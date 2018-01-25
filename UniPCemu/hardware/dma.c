/*

DMA Controller (8237A)

*/

#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //Port support!
#include "headers/mmu/mmuhandler.h" //Direct Memory support!
#include "headers/hardware/8237A.h" //Our own header!
#include "headers/cpu/cpu.h" //CPU support for BUS sharing!

//Are we disabled?
#define __HW_DISABLED 0

SDL_sem *DMA_Lock = NULL;

typedef union
{
	byte data; //Mode register byte representation for easy reading/writing!
} DMAModeRegister;

typedef struct
{
	DMAModeRegister ModeRegister; //Our mode register!
	word CurrentAddressRegister; //Start address register!
	word BaseAddressRegister; //Start address base register when resetting (base)! Written together with the start address!
	word CurrentCountRegister; //Count register set by the user and counting!
	word BaseCountRegister; //Current count register when resetting (base)! Written together with the counter!
	byte PageAddressRegister; //Upper 8 bits of the 24-bit transfer memory address!
	
	DMAWriteBHandler WriteBHandler; //Write handler 8-bit!
	DMAReadBHandler ReadBHandler; //Read handler 8-bit
	DMAWriteWHandler WriteWHandler; //Write handler 16-bit!
	DMAReadWHandler ReadWHandler; //Read handler 16-bit!
	DMATickHandler DREQHandler; //Tick handler for DMA DREQ!
	DMATickHandler DACKHandler; //DACK handler for DMA channel!
	DMATickHandler TCHandler; //TC handler for DMA channel!
} DMAChannelTYPE; //Contains all info about an DMA channel!

typedef struct
{
	//Public registers
	DMAChannelTYPE DMAChannel[4]; //4 DMA Channels per Controller!
	byte StatusRegister; //Status register for a DMA controller!
	byte DREQ; //DREQ for 4 channels!
	byte DACK; //DACK for 4 channels!
	byte CommandRegister; //Command Register
	byte MultiChannelMaskRegister; //MultiChennel Mask Register, bit0-3=channel 0-3; 0=unmask (enabled), 1=mask (disabled)
	byte RequestRegister; //Request Register for 4 channels!	
	
	//Internal registers!
	byte FlipFlop; //Current flipflop: Cannot be accessed by software!
	byte IntermediateRegister; //Intermediate register!
	//Master reset register doesn't store!
	//MaskResetRegister doesn't store!
	byte extrastorage[4]; //Extra storage used by the 286 BIOS!
} DMAControllerTYPE; //Contains a DMA Controller's registers!

byte DMA_S=0; //DMA state of transfer(clocks S0-S3), when active!
byte activeDMA=0; //Active DMA!
byte DMA_currenttick=0;

DMAControllerTYPE DMAController[2]; //We have 2 DMA Controllers!

extern byte useIPSclock; //Are we using the IPS clock instead of cycle accurate clock?

void freeDMA(void)
{
	SDL_DestroySemaphore(DMA_Lock); //Free!
}

void initDMAControllers() //Init function for BIOS!
{
	if (__HW_DISABLED) return; //Abort!
	if (!DMA_Lock)
	{
		DMA_Lock = SDL_CreateSemaphore(1);
	}
	memset(&DMAController[0],0,sizeof(DMAController[0])); //Init DMA Controller channels 0-3 (0 unused: for DRAM Refresh)
	memset(&DMAController[1],0,sizeof(DMAController[1])); //Init DMA Controller channels 4-7 (4 unused: for DMA Controller coupling)
	DMA_S = DMA_currenttick = 0; //Init channel state!
}

void DMA_SetDREQ(byte channel, byte DREQ) //Set DREQ from hardware!
{
	if (__HW_DISABLED) return; //Abort!
	INLINEREGISTER byte channel2=channel,channel3=channel,channel4,channel5=DREQ;
	channel2 >>= 2; //Shift to controller!
	channel3 &= 3; //Only what we need!
	channel4 = DMAController[channel2].DREQ; //Load the channel DREQ!
	channel4 &= ~(1<<(channel3)); //Disable old DREQ!
	channel5 <<= channel3; //Apply DREQ location!
	channel4 |= channel5; //Enable new DREQ!
	DMAController[channel2].DREQ = channel4; //Write back the channel DREQ enabled!
}

//Easy sets of high and low nibbles (word data)!
#define SETHIGH(b,v) b=((b&0xFF)|(v<<8))
#define SETLOW(b,v) b = ((b&0xFF00)|v)

byte DMA_WriteIO(word port, byte value) //Handles OUT instructions to I/O ports.
{
	byte channel; //Which channel to use?
	if (__HW_DISABLED) return 0; //Abort!
	if (!((port < 0x10) || ((port >= 0xC0) && (port <= 0xDE)) || ((port >= 0x80) && (port <= 0x8F)))) return 0; //Not our DMA!
	byte controller = (((port & 0xC0)==0xC0) || ((port&0xF8)==0x88)) ? 1 : 0; //What controller?
	byte reg = (byte)port; //What register is selected, default to 1:1 mapping?
	if (controller) //16-bit register (second DMA controller)?
	{
		if ((port&0xC0)==0xC0) //C0 mapping?
		{
			reg -= 0xC0; //Take the base!
			if (reg & 1) return 0; //Invalid register: not mapped!
			reg >>= 1; //Every port is on a offset of 2!
			//Now reg is on 1:1 mapping too!
		}
	}
	WaitSem(DMA_Lock)
	if ((port>=0x80) && (port<=0x8F)) //Page registers?
	{
		reg -= 0x80; //Take the base!
		if (controller) //Second controller?
		{
			reg -= 8; //Take the second controller port!
		}
		switch (reg) //What register?
		{
		//Extra 8 bits for addresses:
		case 0x7: //
			DMAController[controller].DMAChannel[0].PageAddressRegister = value; //Set!
			break;
		case 0x4:
		case 0x5:
		case 0x6: //Extra on 286 BIOS?
			DMAController[controller].extrastorage[reg-0x4] = value; //Set storage!
			break;
		case 0x0:
			DMAController[controller].extrastorage[3] = value; //Set storage!
			break;
		case 0x3: //
			DMAController[controller].DMAChannel[1].PageAddressRegister = value; //Set!
			break;
		case 0x1: //
			DMAController[controller].DMAChannel[2].PageAddressRegister = value; //Set!
			break;
		case 0x2: //
			DMAController[controller].DMAChannel[3].PageAddressRegister = value; //Set!
			break;
		default:
			PostSem(DMA_Lock) //Release!
			return 0; //Invalid register!
			break;
		}
	}
	else //Non-page register!
	{
		switch (reg) //What register is selected?
		{
		case 0x00:
		case 0x02:
		case 0x04:
		case 0x06: //Address register?
			channel = port>>1; //What channel?
			if (DMAController[controller].FlipFlop) //High?
			{
				SETHIGH(DMAController[controller].DMAChannel[channel].CurrentAddressRegister,value); //Set high nibble!
				SETHIGH(DMAController[controller].DMAChannel[channel].BaseAddressRegister,value); //Set high nibble!
			}
			else //Low?
			{
				SETLOW(DMAController[controller].DMAChannel[channel].CurrentAddressRegister,value); //Set low nibble!
				SETLOW(DMAController[controller].DMAChannel[channel].BaseAddressRegister,value); //Set low nibble!
			}
			DMAController[controller].FlipFlop = !DMAController[controller].FlipFlop; //Flipflop!
			break;
			
		case 0x01:
		case 0x03:
		case 0x05:
		case 0x07: //Count register?
			channel = (port-1)>>1; //What channel?
			if (DMAController[controller].FlipFlop) //High?
			{
				SETHIGH(DMAController[controller].DMAChannel[channel].CurrentCountRegister,value); //Set high nibble!
				SETHIGH(DMAController[controller].DMAChannel[channel].BaseCountRegister,value); //Set high nibble!
			}
			else //Low?
			{
				SETLOW(DMAController[controller].DMAChannel[channel].CurrentCountRegister,value); //Set low nibble!
				SETLOW(DMAController[controller].DMAChannel[channel].BaseCountRegister,value); //Set low nibble!
			}
			DMAController[controller].FlipFlop = !DMAController[controller].FlipFlop; //Flipflop!
			break;

		case 0x08: //Command register!
			DMAController[controller].CommandRegister = value; //Set!
			break;
		case 0x09: //Request register!
			//Set DREQs!
			if (((DMAController[controller].DMAChannel[value&0x3].ModeRegister.data>>6)&3)==2) //Only in block mode we take requests?
			{
				DMAController[controller].RequestRegister &= ~(1<<(value&0x3)); //Clear the bit!
				DMAController[controller].RequestRegister |= (((value&0x4)>>2)<<(value&0x3)); //Set the software request bit!
			}
			break;
		case 0x0A: //Single Channel Mask Register!
			DMAController[controller].MultiChannelMaskRegister &= ~(1<<(value&3)); //Turn off the channel!
			DMAController[controller].MultiChannelMaskRegister |= (((value&4)>>2)<<(value&3)); //Mask it if needed!
			break;
		case 0x0B: //Mode Register!
			DMAController[controller].DMAChannel[value&0x3].ModeRegister.data = value; //Set!
			break;
		case 0x0C: //Flip-Flop Reset Register!
			DMAController[controller].FlipFlop = 0; //Reset!
			break;
		case 0x0D: //Master Reset Register!
			DMAController[controller].FlipFlop = 0; //Reset!
			DMAController[controller].StatusRegister = 0; //Reset!
			DMAController[controller].MultiChannelMaskRegister |= 0xF; //Set the masks!
			break;
		case 0x0E: //Mask Reset Register!
			DMAController[controller].MultiChannelMaskRegister &= ~0xF; //Clear the masks!
			break;
		case 0x0F: //MultiChannel Mask Register!
			DMAController[controller].MultiChannelMaskRegister = value; //Set!
			break;
		default:
			PostSem(DMA_Lock) //Release!
			return 0; //Invalid register!
			break;
		}
	}
	PostSem(DMA_Lock)
	return 1; //Correct register!
}

byte DMA_ReadIO(word port, byte *result) //Handles IN instruction from CPU I/O ports
{
	byte channel; //Which channel to use?
	if (__HW_DISABLED) return 0; //Abort!
	if (!((port < 0x10) || ((port >= 0xC0) && (port <= 0xDE)) || ((port >= 0x80) && (port <= 0x8F)))) return 0; //Not our DMA!
	byte controller = (((port & 0xC0)==0xC0) || ((port&0xF8)==0x88)) ? 1 : 0; //What controller?
	byte reg = (byte)port; //What register is selected, default to 1:1 mapping?
	if (controller) //16-bit register (second DMA controller)?
	{
		if ((port&0xC0)==0xC0) //C0 mapping?
		{
			reg -= 0xC0; //Take the base!
			if (reg & 1) return 0; //Invalid register: not mapped!
			reg >>= 1; //Every port is on a offset of 2!
			//Now reg is on 1:1 mapping too!
		}
	}
	WaitSem(DMA_Lock)
	byte ok = 0;
	if ((port>=0x80) && (port<=0x8F)) //Page registers?
	{
		reg -= 0x80; //Take the base!
		if (controller) //Second controller?
		{
			reg -= 8; //Take the second controller port!
		}
		switch (reg) //What register?
		{
		//Extra 8 bits for addresses:
		case 0x7: //
			*result = DMAController[controller].DMAChannel[0].PageAddressRegister; //Get!
			ok = 1;
			break;
		case 0x4:
		case 0x5:
		case 0x6: //Extra on 286 BIOS?
			*result = DMAController[controller].extrastorage[reg-0x4]; //Get storage!
			ok = 1;
			break;
		case 0x0:
			*result = DMAController[controller].extrastorage[3]; //Get storage!
			ok = 1;
			break;
		case 0x3: //
			*result = DMAController[controller].DMAChannel[1].PageAddressRegister; //Get!
			ok = 1;
			break;
		case 0x1: //
			*result = DMAController[controller].DMAChannel[2].PageAddressRegister; //Get!
			ok = 1;
			break;
		case 0x2: //
			*result = DMAController[controller].DMAChannel[3].PageAddressRegister; //Get!
			ok = 1;
			break;
		default: //Unknown port?
			ok = 0; //Unknown port!
			break;
		}
	}
	else //Non-page register?
	{
		switch (reg) //What register is selected?
		{
			//Address/Count registers? Not listed as readable on osdev, but apparently the XT BIOS tries to read it back!
			case 0x00:
			case 0x02:
			case 0x04:
			case 0x06: //Address register?
				channel = port>>1; //What channel?
				if (DMAController[controller].FlipFlop) //High?
				{
					*result = ((DMAController[controller].DMAChannel[channel].CurrentAddressRegister>>8)&0xFF); //Set high nibble!
				}
				else //Low?
				{
					*result = (DMAController[controller].DMAChannel[channel].CurrentAddressRegister&0xFF); //Set low nibble!
				}
				DMAController[controller].FlipFlop = !DMAController[controller].FlipFlop; //Flipflop!
				ok = 1;
				break;
					
			case 0x01:
			case 0x03:
			case 0x05:
			case 0x07: //Count register?
				channel = (port-1)>>1; //What channel?
				if (DMAController[controller].FlipFlop) //High?
				{
					*result = ((DMAController[controller].DMAChannel[channel].CurrentCountRegister>>8)&0xFF); //Set high nibble!
				}
				else //Low?
				{
					*result = (DMAController[controller].DMAChannel[channel].CurrentCountRegister&0xFF); //Set low nibble!
				}
				DMAController[controller].FlipFlop = !DMAController[controller].FlipFlop; //Flipflop!
				ok = 1;
				break;
			//Status registers! This is documented on osdev!
			case 0x08: //Status Register!
				*result = DMAController[controller].StatusRegister; //Get!
				DMAController[controller].StatusRegister &= ~0xF; //Clear TC bits!
				ok = 1;
				break;
			case 0x0D: //Intermediate Register!
				*result = DMAController[controller].IntermediateRegister; //Get!
				ok = 1;
				break;
			case 0x0F: //MultiChannel Mask Register!
				*result = DMAController[controller].MultiChannelMaskRegister; //Get!
				ok = 1;
				break;
			default: //Unknown port?
				ok = 0; //Unknown port!
				break;
		}
	}
	PostSem(DMA_Lock)
	return ok; //Give the result!
}

void DMA_autoinit(byte controller, byte channel) //Autoinit functionality.
{
	if (__HW_DISABLED) return; //Abort!
	DMAController[controller].DMAChannel[channel].CurrentAddressRegister = DMAController[controller].DMAChannel[channel].BaseAddressRegister; //Reset address register!
	DMAController[controller].DMAChannel[channel].CurrentCountRegister = DMAController[controller].DMAChannel[channel].BaseCountRegister; //Reset count register!
}

//Flags for different responses that might need to be met.
#define FLAG_TC 1

byte lastcycle = 0; //Current channel in total (0-7)

extern byte BUSactive; //Are we allowed to control the BUS? 0=Inactive, 2=DMA

/*
	DMA states:
	SI: Sample DRQ lines. Set HRQ if DRQn=1.
	S0: Sample DLDA. Resolve DRQn priorities.
	S1: Present and latch upper address. Present lower address.
	S2: Activate read command or advanced write command. Activate DACKn.
	S3: Activate write command. Activate Mark and TC if apprioriate.
	S3: _Ready_ _Verify_: SW sample ready line keeps us into S3. Else, proceed into S4.
	S4: Reset enable for channel n if TC stop and TC are active. Deactivate commands. Deactivate DACKn, Mark and T0. Sample DRQn and HLDA. Resolve DRQn priorities. Reset HRQ if HLDA=0 or DRQ=0(Goto SI), else Goto S1.
*/

void DMA_SampleDREQ() //Sample all DREQ lines for both controllers!
{
	INLINEREGISTER byte startcurrent = 0; //Current backup for checking for finished!
	INLINEREGISTER byte controller,current=0; //Current controller!
	byte controllerdisabled = 0; //Controller disabled when set, so skip all checks!
	byte controllerdisabled2[2];
	controllerdisabled2[0] = (DMAController[0].CommandRegister & 4);
	controllerdisabled2[1] = (DMAController[1].CommandRegister & 4);
	nextcycle: //Next cycle to process!
		controller = ((current&4)>>2); //Init controller
		if (!controllerdisabled) //Controller not disabled?
		{
			if (controllerdisabled2[controller]) //Controller disabled?
			{
				if ((current & 3) != 3) //Controller is disabled, but not processed yet? Disable next channels if required!
				{
					controllerdisabled = 3;
					controllerdisabled -= (current & 3); //Disable checking for up to 3 more channels(all other channels belonging to the controller)!
				}
			}
			else //Controller enabled?
			{
				byte channel = (current & 3); //Channel to use! Channels 0 are unused (DRAM memory refresh (controller 0) and cascade DMA controller (controller 1))

				//Handle the current channel, since the controller is enabled!
				DMAModeRegister moderegister;
				moderegister.data = DMAController[controller].DMAChannel[channel].ModeRegister.data; //Read the mode register to use!
				if (((moderegister.data>>6)&3)==3) goto skipdmachannel; //Skip channel: invalid! We don't process a cascade mode channel!
				{
					if (DMAController[controller].DMAChannel[channel].DREQHandler) //Gotten a tick handler?
					{
						DMAController[controller].DMAChannel[channel].DREQHandler(); //Execute the tick handler!
					}
				}
			}
		} //Controller not already disabled?
		else //Disabled controller checking?
		{
			--controllerdisabled; //Skipped one channel, continue when done skipping!
		}
		skipdmachannel: //Skipping the channel?
		++current; //Next channel!
		current &= 0x7; //Wrap arround our 2 DMA controllers?
		if (startcurrent==current)
		{
			lastcycle = current; //Save the current item we've left off!
			return; //Back to our original cycle? We don't have anything to transfer!
		}
		goto nextcycle; //Next cycle!!
}

byte activeDMAchannel; //Active DMA channel through states S1+!
byte DMAcontroller; //The DMA controller of the current request!
byte DMAchannel; //The DMA channel of the current request!
byte DMAchannelindex; //The channel index of the current request!
byte DMAprocessed; //Are we processed?
DMAModeRegister DMAmoderegister; //The mode register of the current request!

void DMA_StateHandler_SI()
{
	//SI: Sample DRQ lines. Set HRQ if DRQn=1.
	DMA_SampleDREQ(); //Sample DREQ!
	INLINEREGISTER byte channelindex, MCMReversed;
	INLINEREGISTER byte channel;
	for (channel=0;channel<8;++channel) //Check all channels for both controllers! Apply simple linear priority for now!
	{
		channelindex = 1; //Load index!
		channelindex <<= (channel&3); //Assign the channel index to use!

		MCMReversed = DMAController[(channel>>2)].MultiChannelMaskRegister; //Load MCM!
		MCMReversed = ~MCMReversed; //NOT!
		MCMReversed &= channelindex; //For our current channel only!

		if ((DMAController[(channel>>2)].DREQ&MCMReversed) || (DMAController[DMAcontroller].RequestRegister&channelindex)) //Requested and not masking or requested manually?
		{
			DMAcontroller = (channel>>2); //The controller!

			byte processchannel = 0; //To process the channel?
			DMAmoderegister.data = DMAController[(channel>>2)].DMAChannel[(channel&3)].ModeRegister.data; //The mode register we're using from now on(or any other being overwritten for transfers)!
			switch (((DMAmoderegister.data>>6)&3)) //What mode?
			{
				case 0: //Demand mode?
					//DREQ determines the transfer!
					processchannel = DMAController[DMAcontroller].DREQ;
					processchannel &= channelindex; //Demand sets if we're to run!
					break;
				case 1: //Single: DACK determines running time!
				case 2: //Block: TC and DREQ masked determines running time!
					//DACK isn't used in this case!
					processchannel = DMAController[DMAcontroller].DACK|DMAController[DMAcontroller].DREQ; //DREQ/DACK are requesting?
					processchannel &= MCMReversed; //We're affected directly by the DACK!
					break;
			}
			
			if (DMAController[DMAcontroller].RequestRegister&channelindex) //Requested?
			{
				processchannel = 1; //Process: software request!
			}
			if (processchannel) //Processing this channel?
			{
				++DMA_S; //Proceed into the next DMA state: S0!
			}
		}
	}
}

void DMA_StateHandler_S0()
{
	//S0: Sample DLDA. Resolve DRQn priorities.
	if (likely(useIPSclock==0)) //Cycle-accurate clock used?
	{
		if (CPU[activeCPU].BUSactive!=2) //Bus isn't assigned to ours yet?
		{
			if (CPU[activeCPU].BUSactive==0) //Are we to take the BUS now? The CPU has released the bus(is at T4 state now)!
			{
				CPU[activeCPU].BUSactive = 2; //Take control of the BUS(DLDA is now high). Wait 1 cycle(signal the CPU is this step. Receiving the HLDA the next cycle) before starting the transfer!
			}
			//BUS is taken or waiting the cycle?
			return; //NOP state!
		}
	}
	//We now have control of the BUS! DLDA=1. Resolve DRQn priorities!
	INLINEREGISTER byte channelindex, MCMReversed;
	INLINEREGISTER byte channel,controller;
	for (channel=0;channel<8;++channel) //Check all channels for both controllers! Apply simple linear priority for now!
	{
		channelindex = 1; //Load index!
		channelindex <<= (channel&3); //Assign the channel index to use!

		MCMReversed = DMAController[(channel>>2)].MultiChannelMaskRegister; //Load MCM!
		MCMReversed = ~MCMReversed; //NOT!
		MCMReversed &= channelindex; //For our current channel only!

		controller = (channel>>2); //The controller!

		if (DMAController[(channel>>2)].DREQ&MCMReversed) //Requested and not masking?
		{
			byte processchannel = 0; //To process the channel?
			DMAmoderegister.data = DMAController[(channel>>2)].DMAChannel[(channel&3)].ModeRegister.data; //The mode register we're using from now on(or any other being overwritten for transfers)!
			switch (((DMAmoderegister.data>>6)&3)) //What mode?
			{
				case 0: //Demand mode?
					//DREQ determines the transfer!
					processchannel = DMAController[controller].DREQ;
					processchannel &= channelindex; //Demand sets if we're to run!
					break;
				case 1: //Single: DACK determines running time!
				case 2: //Block: TC and DREQ masked determines running time!
					//DACK isn't used in this case!
					processchannel = DMAController[controller].DACK|DMAController[controller].DREQ; //DREQ/DACK are requesting?
					processchannel &= MCMReversed; //We're affected directly by the DACK!
					break;
			}
			
			if (DMAController[controller].RequestRegister&channelindex) //Requested?
			{
				processchannel = 1; //Process: software request!
			}
			if (processchannel) //Processing this channel?
			{
				activeDMAchannel = channel; //This is the DMA channel that's currently being processed!
				DMAcontroller = (channel>>2); //The controller to use!
				DMAchannel = (channel&3); //The channel to use!
				DMAchannelindex = channelindex; //The channel index to use!
				++DMA_S; //Proceed into the next DMA state: S1!
				return; //Stop searching any more requests!
			}
		}
	}
	if (likely(useIPSclock==0)) //Cycle-accurate clock used?
	{
		CPU[activeCPU].BUSactive = (CPU[activeCPU].BUSactive==2)?0:CPU[activeCPU].BUSactive; //Release the BUS: we've got nothing to do after all!
	}
}

void DMA_StateHandler_S1()
{
	//S1: Present and latch upper address. Present lower address.
	++DMA_S; //Proceed into the next DMA state: S2!
}

void DMA_StateHandler_S2()
{
	//S2: Activate read command or advanced write command. Activate DACKn.
	if (DMAController[DMAcontroller].DMAChannel[DMAchannel].DACKHandler) //Gotten a DACK handler?
	{
		DMAController[DMAcontroller].DMAChannel[DMAchannel].DACKHandler(); //Send a DACK to the hardware!
	}
	switch (((DMAmoderegister.data>>6)&3))
	{
		case 0: //Single transfer!
		case 1: //Block transfer!
			DMAController[DMAcontroller].DACK |= DMAchannelindex; //Acnowledged!
			break;
		case 2: //Demand transfer?
			//Nothing happens: DREQ affects transfers directly!
			break;
	}
	++DMA_S; //Proceed into the next DMA state: S3!
}

void DMA_StateHandler_S3()
{
	//S3: Activate write command. Activate Mark and TC if apprioriate.
	//S3: _Ready_ _Verify_: SW sample ready line keeps us into S3. Else, proceed into S4.
	byte controller;
	byte channelindex;
	//Channel not masked off and requested? We can't be transferring, so transfer now!
	controller = DMAcontroller; //The controller to use!
	channelindex = DMAchannelindex; //The channel index to check!
	DMAprocessed = 0; //Default: nothing going on!
	/*
	processed bits:
	bit 0: TC (Terminal Count) occurred.
	*/
	
	//Calculate the address...
	uint_32 address; //The address to use!
	if (controller) //16-bits transfer has a special addressing scheme?
	{
		address = DMAController[controller].DMAChannel[DMAchannel].CurrentAddressRegister; //Load the start address!
		address <<= 1; //Shift left by 1 to obtain a multiple of 2!
		address &= 0xFFFF; //Clear the overflowing bit, if any!
	}
	else //8-bit has normal addressing!
	{
		address = DMAController[controller].DMAChannel[DMAchannel].CurrentAddressRegister; //Normal addressing!
	}
	address |= (DMAController[controller].DMAChannel[DMAchannel].PageAddressRegister<<16); //Apply page address to get the full address!
				
	//Process the address counter step: we've been processed and ready to move on!
	if (DMAmoderegister.data&0x20) //Decrease address?
	{
		--DMAController[controller].DMAChannel[DMAchannel].CurrentAddressRegister; //Decrease counter!
	}
	else //Increase counter?
	{
		++DMAController[controller].DMAChannel[DMAchannel].CurrentAddressRegister; //Decrease counter!
	}
				
	//Terminal count!
	--DMAController[controller].DMAChannel[DMAchannel].CurrentCountRegister; //Next step calculated!
	if (DMAController[controller].DMAChannel[DMAchannel].CurrentCountRegister==0xFFFF) //Finished when overflows below 0!
	{
		DMAprocessed |= FLAG_TC; //Set flag: terminal count occurred!
	}
	//Process all flags that has occurred!
				
	if (DMAprocessed&FLAG_TC) //TC resets request register bit?
	{
		DMAController[controller].RequestRegister &= ~channelindex; //Clear the request register!
	}

	if (DMAprocessed&FLAG_TC) //Complete on Terminal count?
	{
		if (DMAController[controller].DMAChannel[DMAchannel].TCHandler) //Gotten a TC handler?
		{
			DMAController[controller].DMAChannel[DMAchannel].TCHandler(); //Send hardware TC!
		}
		DMAController[controller].StatusRegister |= channelindex; //Transfer complete!
	}

	//Transfer data!
	switch (DMAmoderegister.data&0xC)
	{
	case 4: //Writing to memory? (Reading from device)
		if (controller) //16-bits?
		{
			if (DMAController[controller].DMAChannel[DMAchannel].ReadWHandler) //Valid handler?
			{
				memory_directww(address, DMAController[controller].DMAChannel[DMAchannel].ReadWHandler()); //Read using handler!
			}
		}
		else //8-bits?
		{
			if (DMAController[controller].DMAChannel[DMAchannel].ReadBHandler) //Valid handler?
			{
				memory_directwb(address, DMAController[controller].DMAChannel[DMAchannel].ReadBHandler()); //Read using handler!
			}
		}
		break;
	case 8: //Reading from memory? (Writing to device)
		if (controller) //16-bits?
		{
			if (DMAController[controller].DMAChannel[DMAchannel].WriteWHandler) //Valid handler?
			{
				DMAController[controller].DMAChannel[DMAchannel].WriteWHandler(memory_directrw(address)); //Read using handler!
			}
		}
		else //8-bits?
		{
			if (DMAController[controller].DMAChannel[DMAchannel].WriteBHandler) //Valid handler?
			{
				DMAController[controller].DMAChannel[DMAchannel].WriteBHandler(memory_directrb(address)); //Read using handler!
			}
		}
		break;
	case 0: //Verify? Never used on a PC?
	case 0xC: //Invalid?
	default: //Invalid?
		break;
	}
	++DMA_S; //Proceed into the next DMA state: S4!
}

void DMA_StateHandler_S4()
{
	//S4: Reset enable for channel n if TC stop and TC are active. Deactivate commands. Deactivate DACKn, Mark and T0. Sample DRQn and HLDA. Resolve DRQn priorities. Reset HRQ if HLDA=0 or DRQ=0(Goto SI), else Goto S1.
	switch ((DMAmoderegister.data>>6)&3) //What mode are we processing in?
	{
	case 0: //Demand Transfer Mode
		if (DMAprocessed&FLAG_TC) //TC?
		{
			DMAController[DMAcontroller].DACK &= ~DMAchannelindex; //Finished!
		}
		break;
	case 1: //Single Transfer Mode
		DMAController[DMAcontroller].DACK &= ~DMAchannelindex; //Finished, wait for the next time we're requested!
	case 2: //Block Transfer Mode
		if (DMAprocessed&FLAG_TC) //Complete on Terminal count?
		{
			DMAController[DMAcontroller].DACK &= ~DMAchannelindex; //Finished!
			if ((DMAmoderegister.data&0x10)) //Auto?
			{
				DMA_autoinit(DMAcontroller,DMAchannel); //Perform autoinit!
			}
		}
		break;
	}
	byte retryclock = 0;
	DMA_S = 0; //Default to SI state!
	if (likely(useIPSclock==0)) //Cycle-accurate clock used?
	{	
		CPU[activeCPU].BUSactive = (CPU[activeCPU].BUSactive==2)?0:CPU[activeCPU].BUSactive; //Release the BUS, when allowed!
		retryclock = (((CPU[activeCPU].BUSactive==2) || (CPU[activeCPU].BUSactive==0))); //BUS available? Sample DREQ and perform steps to get directly into S1!
	}
	else //Using IPS clock?
	{
		retryclock = 1; //BUS always available? Sample DREQ and perform steps to get directly into S1!
	}
	if (retryclock) //Retrying the clock?
	{
		DMA_StateHandler_SI(); //Perform first state to sample DRQn!
		if (DMA_S==1) //State increased? We're ready to process more right away!
		{
			DMA_StateHandler_S0(); //Proceed into S0 directly: Activate the new channel and then let it proceed to S1!
		}
	}
}

Handler DMA_States[6] = {DMA_StateHandler_SI,DMA_StateHandler_S0,DMA_StateHandler_S1,DMA_StateHandler_S2,DMA_StateHandler_S3,DMA_StateHandler_S4}; //All possible DMA cycle states!
char DMA_States_text[6][256] = {"SI","S0","S1","S2","S3","S4"}; //DMA states!

/* Main DMA Controller processing ticks */
void DMA_tick()
{
	if (__HW_DISABLED) return; //Abort!

	//Tick using the current DMA cycle!
	DMA_States[DMA_S](); //Execute the current DMA state!
}

uint_32 DMA_timing = 0; //How much time has passed!

void initDMA()
{
	doneDMA(); //Stop DMA if needed!
	initDMAControllers(); //Init our DMA controllers!

	//DMA0!
	register_PORTOUT(&DMA_WriteIO);
	register_PORTIN(&DMA_ReadIO);

	DMAController[0].CommandRegister |= 0x4; //Disable controller!
	DMAController[1].CommandRegister |= 0x4; //Disable controller!

	DMA_timing = 0; //Initialise DMA timing!
}

void doneDMA()
{
	if (__HW_DISABLED) return; //Disabled!
}

void cleanDMA()
{
	//Skip time passed!
}

void updateDMA(uint_32 MHZ14passed)
{
	INLINEREGISTER uint_32 timing;
	timing = DMA_timing; //Load current timing!
	timing += MHZ14passed; //How many ticks have passed?
	if (timing>=3) //To tick?
	{
		do //While ticking?
		{
			DMA_tick(); //Tick the DMA!
			timing -= 3; //Tick the DMA at 4.77MHz!
		} while (timing>=3); //Continue ticking?
	}
	DMA_timing = timing; //Save the new timing to use!
}

void registerDMA8(byte channel, DMAReadBHandler readhandler, DMAWriteBHandler writehandler)
{
	DMAController[channel >> 2].DMAChannel[channel & 3].ReadBHandler = readhandler; //Assign the read handler!
	DMAController[channel >> 2].DMAChannel[channel & 3].WriteBHandler = writehandler; //Assign the read handler!
}

void registerDMA16(byte channel, DMAReadWHandler readhandler, DMAWriteWHandler writehandler)
{
	DMAController[channel >> 2].DMAChannel[channel & 3].ReadWHandler = readhandler; //Assign the read handler!
	DMAController[channel >> 2].DMAChannel[channel & 3].WriteWHandler = writehandler; //Assign the read handler!
}

void registerDMATick(byte channel, DMATickHandler DREQHandler, DMATickHandler DACKHandler, DMATickHandler TCHandler)
{
	DMAController[channel >> 2].DMAChannel[channel & 3].DREQHandler = DREQHandler; //Assign the tick handler!
	DMAController[channel >> 2].DMAChannel[channel & 3].DACKHandler = DACKHandler; //Assign the tick handler!
	DMAController[channel >> 2].DMAChannel[channel & 3].TCHandler = TCHandler; //Assign the tick handler!
}