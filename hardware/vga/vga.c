/*

VGA ROM and handling functions.

*/

//Rendering priority: same as input!
#define VGARENDER_PRIORITY 0x20

#include "headers/types.h" //Basic type support!
#include "headers/hardware/ports.h" //Basic PORT compatibility!

#include "headers/bios/bios.h" //For VRAM memory size!
#include "headers/hardware/vga.h" //VGA data!
#include "headers/mmu/mmu.h" //For CPU passtrough!
#include "headers/hardware/vga_screen/vga_sequencer.h" //For precalcs!
#include "headers/hardware/vga_screen/vga_crtcontroller.h" //For getyres for display rate!
#include "headers/hardware/vga_screen/vga_vram.h" //VRAM read!
#include "headers/emu/threads.h" //Multithread support!

#include "headers/emu/gpu/gpu.h" //GPU!
#include "headers/cpu/interrupts.h" //int10 support!
#include "headers/hardware/vga_screen/vga_precalcs.h" //Precalculations!

#include "headers/support/zalloc.h" //Memory allocation!

#include "headers/support/log.h" //Logging support!

#include "headers/emu/gpu/gpu_renderer.h" //GPU emulator support!

//Are we disabled?
#define __HW_DISABLED 0
#define __RENDERER_DISABLED 0

extern GPU_type GPU; //GPU!
VGA_Type *ActiveVGA; //Currently active VGA chipset!

extern BIOS_Settings_TYPE BIOS_Settings; //The BIOS Settings!

//Enable disabling VGA?
#define DISABLE_VGA 0

byte is_loadchartable = 0; //Loading character table?

/*

Info about basic modes:

Other source: http://webpages.charter.net/danrollins/techhelp/0114.HTM


Our base:

http://webpages.charter.net/danrollins/techhelp/0114.HTM

 AL  Type     Format   Cell  Colors        Adapter  Addr  Monitor
                                                                           
      0  text     40x25     8x8* 16/8 (shades) CGA,EGA  b800  Composite
      1  text     40x25     8x8* 16/8          CGA,EGA  b800  Comp,RGB,Enh
      2  text     80x25     8x8* 16/8 (shades) CGA,EGA  b800  Composite
      3  text     80x25     8x8* 16/8          CGA,EGA  b800  Comp,RGB,Enh
      4  graphic  320x200   8x8  4             CGA,EGA  b800  Comp,RGB,Enh
      5  graphic  320x200   8x8  4 (shades)    CGA,EGA  b800  Composite
      6  graphic  640x200   8x8  2             CGA,EGA  b800  Comp,RGB,Enh
      7  text     80x25    9x14* 3 (b/w/bold)  MDA,EGA  b000  TTL Mono
 8,9,0aH  PCjr modes
 0bH,0cH  (reserved; internal to EGA BIOS)
     0dH graphic  320x200   8x8  16            EGA,VGA  a000  Enh,Anlg
     0eH graphic  640x200   8x8  16            EGA,VGA  a000  Enh,Anlg
     0fH graphic  640x350  8x14  3 (b/w/bold)  EGA,VGA  a000  Enh,Anlg,Mono
     10H graphic  640x350  8x14  4 or 16       EGA,VGA  a000  Enh,Anlg
     11H graphic  640x480  8x16  2             VGA      a000  Anlg
     12H graphic  640x480  8x16  16            VGA      a000  Anlg
     13H graphic  640x480  8x16  256           VGA      a000  Anlg

    Notes: With EGA, VGA, and PCjr you can add 80H to AL to initialize a
          video mode without clearing the screen.

        * The character cell size for modes 0-3 and 7 varies, depending on
          the hardware.  On modes 0-3: CGA=8x8, EGA=8x14, and VGA=9x16.
          For mode 7, MDPA and EGA=9x14, VGA=9x16, LCD=8x8.


*/

VGA_Type *VGAalloc(uint_32 custom_vram_size, int update_bios) //Initialises VGA and gives the current set!
{
	if (__HW_DISABLED) return NULL; //Abort!
	VGA_Type *VGA; //The VGA to be allocated!
	//dolog("zalloc","Allocating VGA...");
	VGA = zalloc(sizeof(*VGA),"VGA_Struct"); //Allocate new VGA base to work with!
	if (!VGA)
	{
		raiseError("VGAalloc","Ran out of memory allocating VGA base!");
		return NULL;
	}
	
	uint_32 size;
	if (update_bios) //From BIOS init?
	{
		size = BIOS_Settings.VRAM_size; //Get VRAM size from BIOS!
	}
	else if (custom_vram_size) //Custom VRAM size?
	{
		size = custom_vram_size; //VRAM size from user!
	}
	else //No VRAM size?
	{
		size = 0; //Default VRAM size!
	}
	
	if (size==0) //Default?
	{
		size = VRAM_SIZE; //Default!
	}
	VGA->VRAM_size = size; //Use the selected size!
	
	//dolog("zalloc","Allocating VGA VRAM...");
	VGA->VRAM = (byte *)zalloc(VGA->VRAM_size,"VGA_VRAM"); //The VRAM allocated to 0!
	if (!VGA->VRAM)
	{
		VGA->VRAM_size = VRAM_SIZE; //Try Default VRAM size!
		//dolog("zalloc","Allocating VGA VRAM default...");
		VGA->VRAM = (byte *)zalloc(VGA->VRAM_size,"VGA_VRAM"); //The VRAM allocated to 0!
		if (!VGA->VRAM) //Still not OK?
		{
			freez((void **)&VGA,sizeof(*VGA),"VGA@VGAAlloc_VRAM"); //Release the VGA!
			raiseError("VGAalloc","Ran out of memory allocating VGA VRAM!");
			return NULL;
		}
	}

	if (update_bios) //Auto (for BIOS)
	{
		//dolog("BIOS","VGA requesting update VRAM size in BIOS...");
		BIOS_Settings.VRAM_size = VGA->VRAM_size; //Update VRAM size in BIOS!
		forceBIOSSave(); //Force save of BIOS!
	}

	//dolog("zalloc","Allocating VGA registers...");
	VGA->registers = zalloc(sizeof(*VGA->registers),"VGA_Registers"); //Allocate registers!
	if (!VGA->registers) //Couldn't allocate the registers?
	{
		freez((void **)&VGA->VRAM, VGA->VRAM_size,"VGA_VRAM@VGAAlloc_Registers"); //Release VRAM!
		freez((void **)&VGA,sizeof(*VGA),"VGA@VGAAlloc_Registers"); //Release VGA itself!
		raiseError("VGAalloc","Ran out of memory allocating VGA registers!");
	}

	VGA->CursorOn = 1; //Default: cursor on!
	VGA->TextBlinkOn = 1; //Default: text blink on!

//Stuff from dosbox for comp.
	int i; //Counter!
	for (i=0; i<256; i++) //Init ExpandTable!
	{
		VGA->ExpandTable[i]=i | (i << 8)| (i <<16) | (i << 24); //For Graphics Unit, full 32-bits value of index!
	}

	for (i=0;i<16;i++)
	{
		VGA->FillTable[i] = (((i&1)?0x000000ff:0)
					|((i&2)?0x0000ff00:0)
					|((i&4)?0x00ff0000:0)
					|((i&8)?0xff000000:0)); //Fill the filltable for Graphics Unit!
	}
	
	VGA->Request_Termination = 0; //We're not running a request for termination!
	VGA->Terminated = 1; //We're not running yet, so run nothing yet, if enabled!
	
	VGA->Sequencer = zalloc(sizeof(SEQ_DATA),"SEQ_DATA"); //Sequencer data!
	if (!VGA->Sequencer) //Failed to allocate?
	{
		freez((void **)&VGA->VRAM, VGA->VRAM_size,"VGA_VRAM@VGAAlloc_Registers"); //Release VRAM!
		freez((void **)&VGA,sizeof(*VGA),"VGA@VGAAlloc_Registers"); //Release VGA itself!
		raiseError("VGAalloc","Ran out of memory allocating VGA precalcs!");		
	}
	
	VGA_calcprecalcs(VGA,WHEREUPDATED_ALL); //Init all values to be working with!
	
	//dolog("VGA","Allocation ready.");
	return VGA; //Give the new allocated VGA!
}

void dumpVRAM() //Diagnostic dump of VRAM!
{
	if (__HW_DISABLED) return; //Abort!
	if (ActiveVGA) //Got active VGA?
	{
		FILE *f = fopen("VRAM.dat","wb");
		if (f) //Opened?
		{
			byte plane,c;
			plane = 0; //Start at plane 0!
			for (plane=0;plane<4;plane++)
			{
				for (c=0;c<(ActiveVGA->VRAM_size>>2);c++) //Process all data in VRAM!
				{
					byte data = readVRAMplane(ActiveVGA,plane,c,0); //Read a direct byte from memory!
					fwrite(&data,1,1,f); //Write the VRAM byte!
				}
			}
			fclose(f); //Close the dump!
		}
	}
}

//Read port, write port and both!
#define VGAREGISTER_PORTR(port) register_PORTIN(port,&PORT_readVGA)
#define VGAREGISTER_PORTW(port) register_PORTOUT(port,&PORT_writeVGA)
#define VGAREGISTER_PORTRW(port) VGAREGISTER_PORTR(port);VGAREGISTER_PORTW(port)

void setupVGA() //Sets the VGA up for PC usage (CPU access etc.)!
{
	if (__HW_DISABLED) return; //Abort!
//Our own settings we use:
	VGAREGISTER_PORTRW(0x3B0); //R/W
	VGAREGISTER_PORTRW(0x3B1); //R/W
	VGAREGISTER_PORTRW(0x3B2); //R/W
	VGAREGISTER_PORTRW(0x3B3); //R/W
	VGAREGISTER_PORTRW(0x3B4); //R/W
	VGAREGISTER_PORTRW(0x3B5); //R/W
	VGAREGISTER_PORTRW(0x3B6); //R/W
	VGAREGISTER_PORTRW(0x3B7); //R/W
	VGAREGISTER_PORTRW(0x3BA); //R/W
	VGAREGISTER_PORTRW(0x3C0); //R/W
	VGAREGISTER_PORTRW(0x3C1); //R/W
	VGAREGISTER_PORTRW(0x3C2); //R/W
	VGAREGISTER_PORTRW(0x3C3); //R/W
	VGAREGISTER_PORTRW(0x3C4); //R/W
	VGAREGISTER_PORTRW(0x3C5); //R/W
	VGAREGISTER_PORTRW(0x3C6); //R/W
	VGAREGISTER_PORTRW(0x3C7); //R/W
	VGAREGISTER_PORTRW(0x3C8); //R/W
	VGAREGISTER_PORTRW(0x3C9); //R/W
	VGAREGISTER_PORTR(0x3CA); //R/O
	VGAREGISTER_PORTR(0x3CC); //R/O
	VGAREGISTER_PORTRW(0x3CE); //R/W
	VGAREGISTER_PORTRW(0x3CF); //R/W
	VGAREGISTER_PORTRW(0x3D0); //R/W
	VGAREGISTER_PORTRW(0x3D1); //R/W
	VGAREGISTER_PORTRW(0x3D2); //R/W
	VGAREGISTER_PORTRW(0x3D3); //R/W
	VGAREGISTER_PORTRW(0x3D4); //R/W
	VGAREGISTER_PORTRW(0x3D5); //R/W
	VGAREGISTER_PORTRW(0x3D6); //R/W
	VGAREGISTER_PORTRW(0x3D7); //R/W
	//3D8/9?
	VGAREGISTER_PORTRW(0x3DA); //R/W
	VGAmemIO_reset(); //Initialise/reset memory mapped I/O!
}

/*

Internal terminate and start functions!

*/

void terminateVGA() //Terminate running VGA and disable it! Only to be used by root processes (non-VGA processes!)
{
	if (__HW_DISABLED) return; //Abort!
	if (ActiveVGA==NULL) return; //We can't terminate without a VGA to terminate!
	if (ActiveVGA->Terminated) return; //Already terminated?
	/*
	ActiveVGA->Request_Termination = 1; //We request to terminate!
	while (!ActiveVGA->Terminated) //Still not terminated?
	{
		delay(1); //Wait to be terminated!
	}*/
	//No need to request for termination: we either are rendering in hardware (already not here), or here and not rendering at all!
	ActiveVGA->Terminated = 1; //Terminate VGA!
}

void startVGA() //Starts the current VGA! (See terminateVGA!)
{
	if (__HW_DISABLED) return; //Abort!
	/*if (ActiveVGA->Request_Termination) //We're requesting termination?
	{
		while (!ActiveVGA->Terminated) //Wait to be terminated!
		{
			delay(1); //Wait to be terminated!
		}
	}*/ //Request for termination is not needed!
	//raiseError("VGA","StartVGA0");
	ActiveVGA->Terminated = DISABLE_VGA; //Reset termination flag, effectively starting the rendering!
	VGA_calcprecalcs(ActiveVGA,0); //Update full VGA to make sure we're running!
}

/*

For the emulator: setActiveVGA sets and starts a selected VGA (old one is terminated!)

*/

void setActiveVGA(VGA_Type *VGA) //Sets the active VGA chipset!
{
	if (__HW_DISABLED) return; //Abort!
	terminateVGA(); //Terminate currently running VGA!
	ActiveVGA = VGA; //Set the active VGA to this!
	if (VGA) //Valid?
	{
		startVGA(); //Start the new VGA system!
	}
	//raiseError("VGA","SetActiveVGA: Started!");
}

VGA_Type *getActiveVGA() //Get the active VGA Chipset!
{
	return ActiveVGA; //Give the active VGA!
}

void doneVGA(VGA_Type **VGA) //Cleans up after the VGA operations are done.
{
	if (__HW_DISABLED) return; //Abort!
	VGA_Type *realVGA = *VGA; //The real VGA!
	if (realVGA->VRAM) //Got allocated?
	{
		freez((void **)&realVGA->VRAM,realVGA->VRAM_size,"VGA_VRAM@DoneVGA"); //Free the VRAM!
	}
	if (realVGA->registers) //Got allocated?
	{
		freez((void **)&realVGA->registers,sizeof(*realVGA->registers),"VGA_Registers@DoneVGA"); //Free the registers!
	}
	if (realVGA->Sequencer)
	{
		freez((void **)&realVGA->Sequencer,sizeof(SEQ_DATA),"SEQ_DATA@DoneVGA"); //Free the registers!
	}
	if (VGA) //Valid ptr?
	{
		if (*VGA) //Allocated?
		{
			freez((void **)VGA,sizeof(*realVGA),"VGA@DoneVGA"); //Cleanup the real VGA structure finally!
		}
	}
	//We're cleaned!
}

//Text blink handler!
void textBlinkHandler() //=Vertical Sync Rate/32 (Every two cursor blinks!, since it's at 16. 16*2=32!)
{
	if (ActiveVGA) //Active?
	{
		ActiveVGA->TextBlinkOn = !ActiveVGA->TextBlinkOn; //Blink!
	}
}


//Cursor blink handler!
void cursorBlinkHandler() //Handled every 16 frames!
{
	if (__HW_DISABLED) return; //Abort!
	if (ActiveVGA) //Active?
	{
		ActiveVGA->CursorOn = !ActiveVGA->CursorOn; //Blink!
		if (ActiveVGA->CursorOn) //32 frames processed (we start at ON=1, becomes off first 16, becomes on second 16==32)
		{
			textBlinkHandler(); //32 frames processed!
		}
	}
}

//Now, input/output functions for the emulator.

byte VRAM_readdirect(uint_32 offset)
{
	if (__HW_DISABLED) return 0; //Abort!
	return ActiveVGA->VRAM[SAFEMOD(offset,ActiveVGA->VRAM_size)]; //Give the offset, protected overflow!
}

void VRAM_writedirect(uint_32 offset, byte value)
{
	if (__HW_DISABLED) return; //Abort!
	ActiveVGA->VRAM[SAFEMOD(offset,ActiveVGA->VRAM_size)] = value; //Set the offset, protected overflow!
}

void VGA_VBlankHandler(VGA_Type *VGA)
{
	if (__HW_DISABLED) return; //Abort!
//First: cursor blink handler every 16 frames!
	static byte cursorCounter = 0; //Cursor counter!
	++cursorCounter; //Next cursor!
	if (cursorCounter==16) //To blink cursor?
	{
		cursorCounter = 0; //Reset counter!
		cursorBlinkHandler(); //This is handled every 16 frames!
	}
	
	if (VGA->wait_for_vblank) //Waiting for vblank?
	{
		VGA->wait_for_vblank = 0; //Reset!
		VGA->VGA_vblank = 1; //VBlank occurred!
	}
	
	GPU.doublewidth = VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.DCR; //Apply with double width!
	GPU.doubleheight = VGA_ScanDoubling(VGA); //Apply with double height!
	renderHWFrame(); //Render the GPU a frame!
}

void VGA_waitforVBlank() //Wait for a VBlank to happen?
{
	if (__HW_DISABLED || __RENDERER_DISABLED) return; //Abort!
	ActiveVGA->VGA_vblank = 0; //Reset we've occurred!
	ActiveVGA->wait_for_vblank = 1; //We're waiting for vblank to happen!
	while (!ActiveVGA->VGA_vblank) //Not happened yet?
	{
		delay(1); //Wait a bit for the VBlank to occur!
	}
}