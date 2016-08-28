/*

DMA Controller (8237A)

*/

#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //Port support!
#include "headers/mmu/mmu.h" //Memory support!
#include "headers/hardware/8237A.h" //Our own header!

//Are we disabled?
#define __HW_DISABLED 0

//DMA Ticks(bytes)/second!
#define MHZ14_RATE 9
#define DMA_RATE (MHZ14/8.732575)

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
	memset(&DMAController[0],0,sizeof(DMAController[0])); //Init DMA Controller channels 0-3 (0 unused: for DRAM Refresh)
	memset(&DMAController[1],0,sizeof(DMAController[1])); //Init DMA Controller channels 4-7 (4 unused: for DMA Controller coupling)
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
	if (!((port < 0x20) || ((port >= 0xC0) && (port <= 0xE0)) || ((port >= 0x80) && (port <= 0x9F)))) return 0; //Not our DMA!
	byte controller = ((port & 0xC0)==0xC0) ? 1 : 0; //What controller?
	byte reg = (byte)port; //What register is selected, default to 1:1 mapping?
	if (controller) //16-bit register (second DMA controller)?
	{
		reg -= 0xC0; //Take the base!
		if (reg & 1) return 0; //Invalid register: not mapped!
		reg >>= 1; //Every port is on a offset of 2!
		//Now reg is on 1:1 mapping too!
	}
	WaitSem(DMA_Lock)
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
			PostSem(DMA_Lock) //Release!
			return 0; //Invalid register!
			break;
		}
		break;
	}
	PostSem(DMA_Lock)
	return 1; //Correct register!
}

byte DMA_ReadIO(word port, byte *result) //Handles IN instruction from CPU I/O ports
{
	byte channel; //Which channel to use?
	if (__HW_DISABLED) return 0; //Abort!
	if (!((port < 0x10) || ((port >= 0xC0) && (port <= 0xDE)) || ((port >= 0x80) && (port <= 0x8F)))) return 0; //Not our DMA!
	byte controller = (port>=0xC0)?1:0; //What controller?
	byte reg = (byte)port; //What register is selected, default to 1:1 mapping?
	if (controller) //16-bit register (second DMA controller)?
	{
		reg -= 0xC0; //Take the base!
		if (reg & 1) return 0; //Invalid register: not mapped!
		reg >>= 1; //Every port is on a offset of 2!
		//Now reg is on 1:1 mapping too!
	}
	WaitSem(DMA_Lock)
	byte ok = 0;
	switch (port) //What port?
	{
		//Extra 8 bits for addresses:
		case 0x87: //
			*result = DMAController[0].DMAChannel[0].PageAddressRegister; //Get!
			ok = 1;
			break;
		case 0x83: //
			*result = DMAController[0].DMAChannel[1].PageAddressRegister; //Get!
			ok = 1;
			break;
		case 0x81: //
			*result = DMAController[0].DMAChannel[2].PageAddressRegister; //Get!
			ok = 1;
			break;
		case 0x82: //
			*result = DMAController[0].DMAChannel[3].PageAddressRegister; //Get!
			ok = 1;
			break;
		//Extra 8 bits for addresses:
		case 0x8F: //
			*result = DMAController[1].DMAChannel[0].PageAddressRegister; //Get!
			ok = 1;
			break;
		case 0x8B: //
			*result = DMAController[1].DMAChannel[1].PageAddressRegister; //Get!
			ok = 1;
			break;
		case 0x89: //
			*result = DMAController[1].DMAChannel[2].PageAddressRegister; //Get!
			ok = 1;
			break;
		case 0x8A: //
			*result = DMAController[1].DMAChannel[3].PageAddressRegister; //Get!
			ok = 1;
			break;
		default: //Non-page register!
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
			break;
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

/* Main DMA Controller processing ticks */
void DMA_tick()
{
	if (__HW_DISABLED) return; //Abort!
	INLINEREGISTER byte controller,current=lastcycle; //Current controller!
	INLINEREGISTER byte channelindex, MCMReversed;
	byte transferred = 0; //Transferred data this time?
	INLINEREGISTER byte startcurrent = current; //Current backup for checking for finished!
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
				if (moderegister.Mode==3) goto skipdmachannel; //Skip channel: invalid! We don't process a cascade mode channel!
				{
					if (DMAController[controller].DMAChannel[channel].DREQHandler) //Gotten a tick handler?
					{
						DMAController[controller].DMAChannel[channel].DREQHandler(); //Execute the tick handler!
					}

					channelindex = 1; //Load index!
					channelindex <<= channel; //Assign the channel index to use!

					MCMReversed = DMAController[controller].MultiChannelMaskRegister; //Load MCM!
					MCMReversed = ~MCMReversed; //NOT!
					MCMReversed &= channelindex; //For our current channel only!

					if (DMAController[controller].DREQ&MCMReversed) //Requested and not masking?
					{
						if (DMAController[controller].DMAChannel[channel].DACKHandler) //Gotten a DACK handler?
						{
							DMAController[controller].DMAChannel[channel].DACKHandler(); //Send a DACK to the hardware!
						}
						switch (moderegister.Mode)
						{
							case 0: //Single transfer!
							case 1: //Block transfer!
								DMAController[controller].DACK |= channelindex; //Acnowledged!
								break;
							case 2: //Demand transfer?
								//Nothing happens: DREQ affects transfers directly!
								break;
						}
					}
			
					byte processchannel = 0; //To process the channel?
					switch (moderegister.Mode) //What mode?
					{
						case 0: //Demand mode?
							//DREQ determines the transfer!
							processchannel = DMAController[controller].DREQ;
							processchannel &= channelindex; //Demand sets if we're to run!
							break;
						case 1: //Single: DACK determines running time!
						case 2: //Block: TC and DREQ masked determines running time!
							//DACK isn't used in this case!
							processchannel = DMAController[controller].DACK;
							processchannel &= MCMReversed; //We're affected directly by the DACK!
							break;
					}
			
					if (DMAController[controller].RequestRegister&channelindex) //Requested?
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
				
						if (processed&FLAG_TC) //TC resets request register bit?
						{
							DMAController[controller].RequestRegister &= ~channelindex; //Clear the request register!
						}

						if (processed&FLAG_TC) //Complete on Terminal count?
						{
							if (DMAController[controller].DMAChannel[channel].TCHandler) //Gotten a TC handler?
							{
								DMAController[controller].DMAChannel[channel].TCHandler(); //Send hardware TC!
							}
							DMAController[controller].StatusRegister |= channelindex; //Transfer complete!
						}

						//Transfer data!
						switch (moderegister.TransferType)
						{
						case 1: //Writing to memory? (Reading from device)
							if (controller) //16-bits?
							{
								if (DMAController[controller].DMAChannel[channel].ReadWHandler) //Valid handler?
								{
									MMU_directww(address, DMAController[controller].DMAChannel[channel].ReadWHandler()); //Read using handler!
								}
							}
							else //8-bits?
							{
								if (DMAController[controller].DMAChannel[channel].ReadBHandler) //Valid handler?
								{
									MMU_directwb(address, DMAController[controller].DMAChannel[channel].ReadBHandler()); //Read using handler!
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

						switch (moderegister.Mode) //What mode are we processing in?
						{
						case 0: //Demand Transfer Mode
							if (processed&FLAG_TC) //TC?
							{
								DMAController[controller].DACK &= ~channelindex; //Finished!
							}
							break;
						case 1: //Single Transfer Mode
							DMAController[controller].DACK &= ~channelindex; //Finished, wait for the next time we're requested!
						case 2: //Block Transfer Mode
							if (processed&FLAG_TC) //Complete on Terminal count?
							{
								DMAController[controller].DACK &= ~channelindex; //Finished!
								if (moderegister.Auto)
								{
									DMA_autoinit(controller,channel); //Perform autoinit!
								}
							}
							break;
						}
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
		if (transferred) //Transferred data? We're done(This transfers data at the clock rate specified)!
		{
			lastcycle = current; //Save the current item we've left off!
			return; //Back to our original cycle? We don't have anything to transfer!
		}
		goto nextcycle; //Next cycle!!
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
	if (timing >= MHZ14_RATE) //To tick?
	{
		do //While ticking?
		{
			DMA_tick(); //Tick the DMA!
			timing -= MHZ14_RATE; //Tick the DMA!
		} while (timing >= MHZ14_RATE); //Continue ticking?
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