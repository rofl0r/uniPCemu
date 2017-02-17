#include "headers/types.h" //Types support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/cpu/interrupts.h" //Interrupt support!
#include "headers/interrupts/interrupt13.h" //INT13 support!
#include "headers/basicio/io.h" //Basic I/O support!
#include "headers/cpu/mmu.h" //MMU support!
#include "headers/cpu/cb_manager.h" //CB support!
#include "headers/cpu/protection.h" //Protection support!
#include "headers/hardware/floppy.h" //Floppy support!
#include "headers/mmu/mmuhandler.h" //Memory available and size support!

//Are we disabled?
#define __HW_DISABLED 0

extern byte EMU_BIOS[0x10000]; //Full custom BIOS from 0xF0000-0xFFFFF for the emulator itself to use!
extern word CB_realoffset; //Real offset we're loaded at within the custom BIOS!

//Sources:
//http://www.bioscentral.com/misc/bda.htm#

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
	if (!hasmemory())
	{
		raiseError("BIOS::initmem","No memory present!");
	}

//BDA Data:

	int i; //For multiple setup!
	for (i=0; i<4; i++)
	{
		MMU_ww(-1,0x40,i<<1,0,0); //Not used COM port address!
		MMU_ww(-1,0x40,(i<<1)|8,0,0); //Not used LPT port address!
	}

	/*
		byte BootFloppyInstalled : 1; //Boot floppy installed (0=Not installed, 1=Installed)
		byte MathCOProcessorInstalled : 1; //Match Coprocessor installed (1=Installed, 0=Not installed)
		byte PS2MouseInstalled : 1; //PS/2 Mouse installed (1=Installed, 0=Not installed)
		byte NotUsedOnPS2 : 1; //Not used on PS/2
		byte VideoMode : 2; //00: EGA+; 1=color 40x25, 2=color 80x25, 3=Mono 80x25
		byte FloppyDrives : 2; //Number of floppy drives installed (plus 1 (0=1,1=2,2=3,3=4 drives)
		byte NoDMAInstalled : 1; //0 if DMA installed.
		byte SerialPorts : 3; //Number of serial ports installed (0+).
		byte GameAdapterInstalled : 1; //1 if installed
		byte InternalPS2Modem : 1; //1 if installed.
		byte ParallelPorts : 2; //Number of parallel ports installed (00=1;01=2;03=3)
	*/ //Equipment word definition!
	word eq=0;

	//eq.ParallelPorts = 0; //Ammount!
	eq |= (1<<8); //Ammount to 2 Serial ports!
	eq |= (1<<6); //Ammount: 1(0b) or 2(1b) floppy drives installed!
	eq |= (0<<8); //0: EGA+; 1=color 40x25; 2=color 80x25; 3=mono 80x25
	eq |= (1<<2); //PS/2 mouse installed?
	eq |= (0<<1); //Math CO-OP installed?
	eq |= (0<<0); //Boot floppy installed?

	MMU_wb(-1, 0x0040, 0x0010, (eq&0xFF),0); //Write the equipment flag!
	MMU_wb(-1, 0x0040, 0x0011, ((eq>>8)&0xFF),0); //Write the equipment flag!

	uint_32 sizeinKB;
	sizeinKB = MEMsize();
	sizeinKB >>= 10; //Size in KB!

	MMU_ww(-1,0x0040,0x0013,(sizeinKB>=640)?640:sizeinKB,0); //MMU size in KB! Limit it to 640K!

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