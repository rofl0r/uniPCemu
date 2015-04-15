/*

DMA Controller (8237A)

*/

#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //Port support!
#include "headers/mmu/mmu.h" //Memory support!
#include "headers/emu/threads.h" //Threads support!
#include "headers/hardware/8237A.h" //Our own header!
#include "headers/emu/timers.h" //Timer support!

//Are we disabled?
#define __HW_DISABLED 0

SDL_sem *DMA_Lock = NULL;

typedef union
{
	struct
	{
		byte SEL : 2; //Which channel are we?
		byte TransferType : 2; //00=Self test, 1=Write to memory, 2=Read from memory, 3=Invalid.
		byte Auto : 1; //After the transfer has completed, Reset to the address and count values originally programmed.
		byte Down : 1; //Top-down processing if set, else increment addresses.
		byte Mode : 2; //Mode: 0=Transfer on Demand, 1=Single DMA Transfer, 2=Block DMA transfer, 3=Cascade Mode (Used to cascade another DMA controller).
	}; //The mode register!
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
	DMATickHandler TickHandler; //Tick handler for DMA ticks!
} DMAChannelTYPE; //Contains all info about an DMA channel!

typedef struct
{
	//Public registers
	DMAChannelTYPE DMAChannel[4]; //4 DMA Channels per Controller!
	union
	{
		struct
		{
			byte TransferComplete : 4; //Transfer complete for 4 channels (high to low, bit 0=TC0.) Set on TC or external EOP. Cleared on read by the CPU.
			byte RequestPending : 4; //Request pending for 4 channels (high to low, bit 0=REQ0.) Set when requesting service.
		};
		byte StatusRegister; //Status register for a DMA controller!
	}; //A Status register!
	byte DREQ; //DREQ for 4 channels!
	byte DACK; //DACK for 4 channels!
	byte EOP; //EOP for 4 channels!
	byte CommandRegister; //Command Register
	byte MultiChannelMaskRegister; //MultiChennel Mask Register, bit0-3=channel 0-3; 0=unmask (enabled), 1=mask (disabled)
	byte RequestRegister; //Request Register for 4 channels!	
	
	//Internal registers!
	byte FlipFlop; //Current flipflop: Cannot be accessed by software!
	byte IntermediateRegister; //Intermediate register!
	//Master reset register doesn't store!
	//MaskResetRegister doesn't store!
} DMAControllerTYPE; //Contains a DMA Controller's registers!

DMAControllerTYPE DMAController[2]; //We have 2 DMA Controllers!

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
	memset(&DMAController[0],0,sizeof(DMAController)); //Init DMA Controller channels 0-3 (0 unused: for DRAM Refresh)
	memset(&DMAController[1],0,sizeof(DMAController)); //Init DMA Controller channels 4-7 (4 unused: for DMA Controller coupling)
}

void DMA_SetDREQ(byte channel, byte DREQ) //Set DREQ from hardware!
{
	if (__HW_DISABLED) return; //Abort!
	DMAController[channel>>2].DREQ &= ~(1<<(channel&3)); //Disable old DREQ!
	DMAController[channel>>2].DREQ |= (DREQ<<channel); //Enable new DREQ!
}

void DMA_SetEOP(byte channel, byte EOP) //Set EOP from hardware!
{
	if (__HW_DISABLED) return; //Abort!
	DMAController[channel>>2].EOP |= (EOP<<channel); //Set EOP!
}

//Easy sets of high and low nibbles (word data)!
#define SETHIGH(b,v) b=((b&0xFF)|(v<<8))
#define SETLOW(b,v) b = ((b&0xFF00)|v)

void DMA_WriteIO(word port, byte value) //Handles OUT instructions to I/O ports.
{
	if (__HW_DISABLED) return; //Abort!
	byte controller = (port>=0xC0)?1:0; //What controller?
	byte reg = port; //What register is selected, default to 1:1 mapping?
	if (controller) //16-bit register (second DMA controller)?
	{
		reg -= 0xC0; //Take the base!
		reg >>= 1; //Every port is on a offset of 2!
		//Now reg is on 1:1 mapping too!
	}
	byte channel; //Which channel to use?
	SDL_SemWait(DMA_Lock);
	switch (port) //What port?
	{
	//Extra 8 bits for addresses:
	case 0x87: //
		DMAController[0].DMAChannel[0].PageAddressRegister = value; //Set!
		break;
	case 0x83: //
		DMAController[0].DMAChannel[1].PageAddressRegister = value; //Set!
		break;
	case 0x81: //
		DMAController[0].DMAChannel[2].PageAddressRegister = value; //Set!
		break;
	case 0x82: //
		DMAController[0].DMAChannel[3].PageAddressRegister = value; //Set!
		break;
	//Extra 8 bits for addresses:
	case 0x8F: //
		DMAController[1].DMAChannel[0].PageAddressRegister = value; //Set!
		break;
	case 0x8B: //
		DMAController[1].DMAChannel[1].PageAddressRegister = value; //Set!
		break;
	case 0x89: //
		DMAController[1].DMAChannel[2].PageAddressRegister = value; //Set!
		break;
	case 0x8A: //
		DMAController[1].DMAChannel[3].PageAddressRegister = value; //Set!
		break;
	default: //Non-page register!
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
			if (DMAController[controller].DMAChannel[value&0x3].ModeRegister.Mode==2) //Only in block mode we take requests?
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
			break;
		}
		break;
	}
	SDL_SemPost(DMA_Lock);
}

byte DMA_ReadIO(word port) //Handles IN instruction from CPU I/O ports
{
	if (__HW_DISABLED) return ~0; //Abort!
	byte result; //To hold the result!
	byte controller = (port>=0xC0)?1:0; //What controller?
	byte reg = port; //What register is selected, default to 1:1 mapping?
	if (controller) //16-bit register (second DMA controller)?
	{
		reg -= 0xC0; //Take the base!
		reg >>= 1; //Every port is on a offset of 2!
		//Now reg is on 1:1 mapping too!
	}
	SDL_SemWait(DMA_Lock);
	switch (port) //What port?
	{
		//Extra 8 bits for addresses:
		case 0x87: //
			result = DMAController[0].DMAChannel[0].PageAddressRegister; //Get!
			break;
		case 0x83: //
			result = DMAController[0].DMAChannel[1].PageAddressRegister; //Get!
			break;
		case 0x81: //
			result = DMAController[0].DMAChannel[2].PageAddressRegister; //Get!
			break;
		case 0x82: //
			result = DMAController[0].DMAChannel[3].PageAddressRegister; //Get!
			break;
		//Extra 8 bits for addresses:
		case 0x8F: //
			result = DMAController[1].DMAChannel[0].PageAddressRegister; //Get!
			break;
		case 0x8B: //
			result = DMAController[1].DMAChannel[1].PageAddressRegister; //Get!
			break;
		case 0x89: //
			result = DMAController[1].DMAChannel[2].PageAddressRegister; //Get!
			break;
		case 0x8A: //
			result = DMAController[1].DMAChannel[3].PageAddressRegister; //Get!
			break;	
		default: //Non-page register!
			switch (reg) //What register is selected?
			{
				//Controller 0!
				case 0x08: //Status Register!
					result = DMAController[0].StatusRegister; //Get!
					DMAController[0].StatusRegister &= ~0xF; //Clear TC bits!
					break;
				case 0x0D: //Intermediate Register!
					result = DMAController[0].IntermediateRegister; //Get!
					break;
				case 0x0F: //MultiChannel Mask Register!
					result = DMAController[0].MultiChannelMaskRegister; //Get!
					break;
				default: //Unknown port?
					result = ~0; //Unknown port!
					break;
			}
			break;
	}
	SDL_SemPost(DMA_Lock);
	return result; //Give the result!
}

void DMA_autoinit(byte controller, byte channel) //Autoinit functionality.
{
	if (__HW_DISABLED) return; //Abort!
	DMAController[controller].DMAChannel[channel].CurrentAddressRegister = DMAController[controller].DMAChannel[channel].BaseAddressRegister; //Reset address register!
	DMAController[controller].DMAChannel[channel].CurrentCountRegister = DMAController[controller].DMAChannel[channel].BaseCountRegister; //Reset count register!
}

//Flags for different responses that might need to be met.
#define FLAG_TC 1

/* Main DMA Controller processing ticks */
void DMA_tick()
{
	if (__HW_DISABLED) return; //Abort!
	static byte current = 0; //Current channel in total (0-7)
	static byte controller; //Current controller!
	byte transferred = 0; //Transferred data this time?
	byte startcurrent = current; //Current backup for checking for finished!
	SDL_SemWait(DMA_Lock);
	//nextcycle: //Next cycle to process!
		controller = ((current&4)>>2); //Init controller
		byte channel = (current&3); //Channel to use! Channels 0 are unused (DRAM memory refresh (controller 0) and cascade DMA controller (controller 1))
		if (!(DMAController[controller].CommandRegister&4) && channel) //Controller not disabled and valid channel to transfer?
		{
			DMAModeRegister moderegister;
			moderegister.data = DMAController[controller].DMAChannel[channel].ModeRegister.data; //Read the mode register to use!
			if (moderegister.Mode==3) goto nextchannel; //Skip channel: invalid! We don't process a cascade mode channel!

			if (DMAController[controller].DMAChannel[channel].TickHandler) //Gotten a tick handler?
			{
				DMAController[controller].DMAChannel[channel].TickHandler(); //Execute the tick handler!
			}

			if (DMAController[controller].DREQ&((~DMAController[controller].MultiChannelMaskRegister)&(1<<channel))) //Requested and not masking?
			{
				switch (moderegister.Mode)
				{
					case 0: //Single transfer!
					case 1: //Block transfer!
						DMAController[controller].DACK |= (1<<channel); //Acnowledged!
						DMAController[controller].DREQ &= ~(1<<channel); //Clear DREQ!
						break;
					case 2: //Demand transfer?
						//Nothing happens: DREQ affects transfers directly!
						break;
				}
			}
			
			byte processchannel = 0; //To process the channel?
			switch (moderegister.Mode) //What mode?
			{
				case 0:
				case 1: //TC(&EOP at 1) determines running time!
						processchannel = !(DMAController[controller].TransferComplete&(1<<channel)); //TC determines processing!
						processchannel &= (DMAController[controller].DACK&(1<<channel)>>channel); //Process acnowledge!
						break;
				case 2: //TC(also caused by EOP) and DREQ masked determines running time!
						//DACK isn't used in this case!
						processchannel = !(DMAController[controller].TransferComplete&(1<<channel)); //TC determines processing!
						processchannel &= (DMAController[controller].DREQ&((~DMAController[controller].MultiChannelMaskRegister)&(1<<channel))); //We're affected directly by the DREQ!
						break;
			}
			
			if (DMAController[controller].RequestRegister&(1<<channel)) //Requested?
			{
				processchannel = 1; //Process: software request!
			}
			
			if (processchannel) //Channel not masked off and requested?
			{
				transferred = 1; //We've transferred a byte of data!
				byte processed = 0; //Default: nothing going on!
				/*
				processed bits:
				bit 0: TC (Terminal Count) occurred.
				bit 1: External EOP encountered.
				*/
				
				//Calculate the address...
				uint_32 address; //The address to use!
				if (controller) //16-bits transfer has a special addressing scheme?
				{
					address = DMAController[controller].DMAChannel[channel].CurrentAddressRegister; //Load the start address!
					address <<= 1; //Shift left by 1 to obtain a multiple of 2!
					address &= 0xFFFF; //Clear the overflowing bit, if any!
				}
				else //8-bit has normal addressing!
				{
					address = DMAController[controller].DMAChannel[channel].CurrentAddressRegister; //Normal addressing!
				}
				address |= (DMAController[controller].DMAChannel[channel].PageAddressRegister<<16); //Apply page address to get the full address!
				
				//Transfer data!
				switch (moderegister.TransferType)
				{
					case 1: //Writing to memory? (Reading from device)
						if (controller) //16-bits?
						{
							if (DMAController[controller].DMAChannel[channel].ReadWHandler) //Valid handler?
							{
								MMU_directww(address,DMAController[controller].DMAChannel[channel].ReadWHandler()); //Read using handler!
							}
						}
						else //8-bits?
						{
							if (DMAController[controller].DMAChannel[channel].ReadBHandler) //Valid handler?
							{
								MMU_directwb(address,DMAController[controller].DMAChannel[channel].ReadBHandler()); //Read using handler!
							}
						}
						break;
					case 2: //Reading from memory? (Writing to device)
						if (controller) //16-bits?
						{
							if (DMAController[controller].DMAChannel[channel].WriteWHandler) //Valid handler?
							{
								DMAController[controller].DMAChannel[channel].WriteWHandler(MMU_directrw(address)); //Read using handler!
							}
						}
						else //8-bits?
						{
							if (DMAController[controller].DMAChannel[channel].WriteBHandler) //Valid handler?
							{
								DMAController[controller].DMAChannel[channel].WriteBHandler(MMU_directrb(address)); //Read using handler!
							}
						}
						break;
					case 0: //Verify? Never used on a PC?
					case 3: //Invalid?
					default: //Invalid?
						break;
				}

				//Process the address counter step: we've been processed and ready to move on!
				if (moderegister.Down) //Decrease address?
				{
					--DMAController[controller].DMAChannel[channel].CurrentAddressRegister; //Decrease counter!
				}
				else //Increase counter?
				{
					++DMAController[controller].DMAChannel[channel].CurrentAddressRegister; //Decrease counter!
				}
				
				//Terminal count!
				--DMAController[controller].DMAChannel[channel].CurrentCountRegister; //Next step calculated!
				if (DMAController[controller].DMAChannel[channel].CurrentCountRegister==0xFFFF) //Finished when overflows below 0!
				{
					processed |= FLAG_TC; //Set flag: terminal count occurred!
				}
				//Process all flags that has occurred!
				
				if (DMAController[controller].EOP&(1<<channel) || processed&FLAG_TC) //EOP or TC resets request register bit?
				{
					DMAController[controller].RequestRegister &= ~(1<<channel); //Clear the request register!
				}
				
				switch (moderegister.Mode) //What mode are we processing in?
				{
					case 0: //Single Transfer Mode
						if (processed&FLAG_TC) //AutoInit&complete on TC!
						{
							DMAController[controller].TransferComplete |= (1<<channel); //Transfer complete!
							if (moderegister.Auto)
							{
								DMA_autoinit(controller,channel); //Perform utoInit!
							}
							else //Allow block mask?
							{
								DMAController[controller].MultiChannelMaskRegister |= (1<<channel); //Set mask!
							}
						}
						break;
					case 1: //Block Transfer Mode
						if (processed&FLAG_TC || (DMAController[controller].EOP&(1<<channel))) //Complete?
						{
							DMAController[controller].TransferComplete |= (1<<channel); //Transfer complete!
							if (processed&FLAG_TC) //Autoinit/mask on TC!
							{
								if (moderegister.Auto)
								{
									DMA_autoinit(controller,channel); //Perform autoinit!
								}
								else //Allow block mask?
								{
									DMAController[controller].MultiChannelMaskRegister |= (1<<channel); //Set mask!
								}
							}
						}
					case 2: //Demand Transfer Mode
						if (processed&FLAG_TC || (DMAController[controller].EOP&(1<<channel))) //Finished by TC or external EOP?
						{
							DMAController[controller].TransferComplete |= (1<<channel); //Transfer complete!
							if (moderegister.Auto && (DMAController[controller].EOP&(1<<channel))) //AutoInit/mask on EOP!
							{
								DMA_autoinit(controller,channel); //Perform autoinit!
							}
							else //Block mask!
							{
								DMAController[controller].MultiChannelMaskRegister |= (1<<channel); //Set mask!
							}
						}
						break;
				}
			}
		}
	nextchannel: //Skipping this channel (used by cascade mode channels)
		if (++current&(~0x7)) //Last controller finished (overflow channel counter)?
		{
			current = 0; //Reset controller!
		}
		if (transferred)
		{
			SDL_SemPost(DMA_Lock);
			return; //Transferred data? We're done!
		}
		if (startcurrent == current)
		{
			SDL_SemPost(DMA_Lock);
			return; //Back to our original cycle? We don't have anything to transfer!
		}
		//goto nextcycle; //Next cycle!!
		SDL_SemPost(DMA_Lock);
}

void initDMA()
{
	doneDMA(); //Stop DMA if needed!
	initDMAControllers(); //Init our DMA controllers!

	//DMA0!
	register_PORTOUT_range(0x00,0x1F,&DMA_WriteIO);
	register_PORTIN_range(0x00,0x1F,&DMA_ReadIO);

	//DMA1
	register_PORTOUT_range(0xC0,0xDF,&DMA_WriteIO);
	register_PORTIN_range(0xC0,0xDF,&DMA_ReadIO);
	
	//Page registers!
	register_PORTOUT_range(0x80,0x9F,&DMA_WriteIO);
	register_PORTIN_range(0x80,0x9F,&DMA_ReadIO);

	DMAController[0].CommandRegister |= 0x4; //Disable controller!
	DMAController[1].CommandRegister |= 0x4; //Disable controller!

	if (!__HW_DISABLED) //Enabled?
	{
		//We're up to 1.6MB/s, so for 1 channel 1.6 million bytes per second, for all channels, to 8 channel 204953.6 bytes per second!
		addtimer(1639628.8f,&DMA_tick,"DMA tick",100,0,NULL); //Just use timers!
	}
}

void doneDMA()
{
	if (__HW_DISABLED) return; //Disabled!
	removetimer("DMA Thread"); //Remove our timer!
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

void registerDMATick(byte channel, DMATickHandler tickhandler)
{
	DMAController[channel >> 2].DMAChannel[channel & 3].TickHandler = tickhandler; //Assign the tick handler!
}