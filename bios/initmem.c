#include "headers/types.h" //Types support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/cpu/interrupts.h" //Interrupt support!
#include "headers/interrupts/interrupt13.h" //INT13 support!
#include "headers/bios/io.h" //Basic I/O support!
#include "headers/mmu/mmu.h" //MMU support!
#include "headers/mmu/bda.h" //BDA support!
#include "headers/cpu/callback.h" //CB support!
#include "headers/cpu/80286/protection.h" //Protection support!

//Are we disabled?
#define __HW_DISABLED 0

extern byte EMU_BIOS[0x10000]; //Full custom BIOS from 0xF0000-0xFFFFF for the emulator itself to use!
extern word CB_realoffset; //Real offset we're loaded at within the custom BIOS!

//Sources:
//http://www.bioscentral.com/misc/bda.htm#

extern MMU_type MMU; //MMU!
extern word CB_datasegment; //Segment of data!
extern word CB_dataoffset; //Offset of data!
extern byte mounteddrives[0x100]; //All mounted drives!

void generateDPT(word offset, byte disk)
{
	if (__HW_DISABLED) return; //Abort!
	/*
		00   byte  specify byte 1; step-rate time, head unload time
		01   byte  specify byte 2; head load time, DMA mode
		02   byte  timer ticks to wait before disk motor shutoff
		03   byte  bytes per sector code:

				0 - 128 bytes	2 - 512 bytes
				1 - 256 bytes	3 - 1024 bytes

		04   byte  sectors per track (last sector number)
		05   byte  inter-block gap length/gap between sectors
		06   byte  data length, if sector length not specified
		07   byte  gap length between sectors for format
		08   byte  fill byte for formatted sectors
		09   byte  head settle time in milliseconds
		0A   byte  motor startup time in eighths of a second
	*/

	EMU_BIOS[offset] = 0; //Head unload time!
	EMU_BIOS[offset+1] = 0; //Head load time, DMA mode!
	EMU_BIOS[offset+2] = 0; //Timer ticks to wait till motor poweroff!
	/*switch (floppy_bps(disksize(mounteddrives[disk])))
	{
		case 128:
			MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,segment,offset+3,0); //Bytes per sector (0=128,1=256,2=512,3=1024)!
		case 256:
			MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,segment,offset+3,1); //Bytes per sector (0=128,1=256,2=512,3=1024)!
		case 512:
			MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,segment,offset+3,2); //Bytes per sector (0=128,1=256,2=512,3=1024)!
			break;
		case 1024:
			MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,segment,offset+3,2); //Bytes per sector (0=128,1=256,2=512,3=1024)!
			break;
			
		default: //Unknown? Default to 512 BPS.
		*/
			EMU_BIOS[offset+3] = 2; //Bytes per sector (0=128,1=256,2=512,3=1024)!
	/*		break;		
	}*/
	EMU_BIOS[offset+4] = floppy_spt(disksize(mounteddrives[disk])); //SPT!
	EMU_BIOS[offset+5] = 0; //Gap width between sectors!
	EMU_BIOS[offset+6] = 0; //DTL (Data Transfer Length) max transfer when length not set
	EMU_BIOS[offset+7] = 0; //Gap length for format operation
	EMU_BIOS[offset+8] = 0xF6; //Filler byte for formatted sectors. (normally 0f6h 'o umlaud')
	EMU_BIOS[offset+9] = 0; //Head settle time (in ms)
	EMU_BIOS[offset+10] = 0; //Motor-startup time (in 1/8th-second intervals)
}

void initMEM() //Initialise memory for reset!
{
	if (__HW_DISABLED) return; //Abort!
	BDA_type *BDA; //The BIOS Data area for us to initialise!
	bzero(MMU.memory,MMU.size); //Initialise the memory by size!
	if (!hasmemory())
	{
		raiseError("BIOS::initmem","No memory present!");
	}

	BDA = (BDA_type *)MMU_ptr(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0x0040,0x0000,0,sizeof(*BDA)); //Point to the BDA (Segment 40, offset 0; thus generating offset 400)!

	if (!BDA) //No BIOS Data Area set yet?
	{
		raiseError("BIOS::initmem","No BDA set!");
		return; //Stop: No BDA!
	}

	bzero(BDA,sizeof(*BDA)); //Init BDA to 0's!

//BDA Data:

	int i; //For multiple setup!
	for (i=0; i<4; i++)
	{
		BDA->COM_BaseAdress[i] = 0; //Not used!
		BDA->LPT_BaseAdress[i] = 0; //Not used!
	}


	BDA->Equipment.ParallelPorts = 0; //Ammount!
	BDA->Equipment.SerialPorts = 0; //Ammount!
	BDA->Equipment.FloppyDrives = 1; //Ammount: 1(0b) or 2(1b) floppy drives installed!
	BDA->Equipment.VideoMode = 0; //0: EGA+; 1=color 40x25; 2=color 80x25; 3=mono 80x25
	BDA->Equipment.PS2MouseInstalled = 0; //PS/2 mouse installed?
	BDA->Equipment.MathCOProcessorInstalled = 0; //Math CO-OP installed?
	BDA->Equipment.BootFloppyInstalled = 1; //Boot floppy installed?

	BDA->IF_ManufacturingTest = 0; //FLAG_IF - Manufacturing test

	BDA->MemorySize_KB = ((MEMsize()>>10)>=0xFFFF)?0xFFFF:(MEMsize()/1024); //MMU size in KB!

	BDA->ErrorCodes_AdapterMemorySizePCXT = 0; //Error coes for AT+; Adapter memory size for PC&XT

	BDA->KeyboardShiftFlags1.data = 0; //Keyboard state flags
	BDA->KeyboardShiftFlags2.data = 0;
	BDA->AltNumpadWordArea = 0;
	BDA->NextCharacterInKeyboardBuffer.segment = 0;
	BDA->NextCharacterInKeyboardBuffer.offset = 0;
	BDA->LastCharacterInKeyboardBuffer.segment = 0;
	BDA->LastCharacterInKeyboardBuffer.offset = 0;
	bzero(&BDA->KeyboardBuffer,sizeof(BDA->KeyboardBuffer)); //Reset buffer!

	BDA->Int1ACounter = 0; //# of IRQ0 ticks since boot (int 1Ah)!

	BDA->NumHDDs = 0; //Number of HDD drives!

//Video part!

	BDA->ActiveVideoMode = 0; //Active video mode setting
	BDA->ActiveVideoMode_TextColumnsPerRow = 40; //Number of textcolumns per row for the active video mode!
	BDA->ActiveVideoMode_Size = 0; //Size of active video in page bytes!
	BDA->ActiveVideoMode_Offset = 0; //Offset address of the active video page relative to the start of VRAM!

	for (i=0; i<8; i++)
	{
		bzero(&BDA->CursorPosition[i],sizeof(BDA->CursorPosition[i])); //Reset cursor positions!
	}

	BDA->CursorShape.StartRow = 6; //Start row of cursor!
	BDA->CursorShape.EndRow = 7; //End row of cursor!

	BDA->ActiveVideoPage = 0; //Active video page!
	BDA->VideoDisplayAdapter_IOPort = 0; //Base IO port for video!

	BDA->VideoDisplayAdapter_InternalModeRegister.Notused1 = 0; //Not used!
	BDA->VideoDisplayAdapter_InternalModeRegister.Attribute = 0; //0: attribute=background intensity; 1=attribute=blinking
	BDA->VideoDisplayAdapter_InternalModeRegister.Mode6GraphicsOperation = 0; //1=Mode 6 graphics operation
	BDA->VideoDisplayAdapter_InternalModeRegister.VideoOn =  1; //1=Video signal enabled
	BDA->VideoDisplayAdapter_InternalModeRegister.MonoOperation = 0; //0=Color operation; 1=Monochrome operation
	BDA->VideoDisplayAdapter_InternalModeRegister.Mode45GraphicsOperation = 0; //1=Modr 4/5 graphics operation
	BDA->VideoDisplayAdapter_InternalModeRegister.Mode23TestOperation = 0; //1=Modr 4/5 graphics operation; 1=Mode 2/3 test operation

	BDA->ColorPalette.NotUsed = 0; //Not used!
	BDA->ColorPalette.Mode5ForeGroundColors = 0; //mode 5 foreground colors: 0=Green/red/yellow; 1=cyan/magenta/white
	BDA->ColorPalette.BackgroundIntensified = 0; //background color: 0=normal; 1=Intensified.
	BDA->ColorPalette.BorderOrBgColor = 0; //Intensified border color (mode 2) and bgcolor (mode 5)
	BDA->ColorPalette.Red = 0; //Indicates red
	BDA->ColorPalette.Green = 0; //Indicates green
	BDA->ColorPalette.Blue = 0; //Indicates blue

	BDA->Video_AdapterROM_Segment = 0; //Segment of Video Parameter Control Block
	BDA->Video_AdapterROM_Offset = 0; //Offset of Video Parameter Control Block

//End of Video part!


//END FLAG_OF BDA

//Finally: model and submodel
//SRC: http://flint.cs.yale.edu/feng/research/BIOS/mem.htm

	EMU_BIOS[0xFFFE] = 0xFF; //PC!
	EMU_BIOS[0xFFFF] = 0x00; //--!

//Basic tables!

	//Disk parameter table!


	addCBHandler(CB_DATA,NULL,0); //Reserve a callback for data (16 bytes)! For the first floppy drive!
	CPU_setint(0x1E,CB_datasegment,CB_dataoffset); //Set our Diskette Parameter Table to us!
	generateDPT(CB_realoffset,FLOPPY0); //Generate Floppy0 DPT!
	word backup = CB_realoffset;
	addCBHandler(CB_DATA,NULL,0); //Add a second set for the second floppy drive after the first one (16 bytes isn't enough)!
	generateDPT(backup+11,FLOPPY1); //Generate Floppy1 DPT!
}