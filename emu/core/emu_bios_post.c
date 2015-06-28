#include "headers/types.h" //Basic types!

#include "headers/mmu/mmu.h" //For MMU
#include "headers/bios/io.h" //For mounting!
#include "headers/bios/bios.h" //For BIOS!
#include "headers/bios/biosrom.h" //BIOS ROM support!
#include "headers/bios/initmem.h" //BIOS Memory support!
#include "headers/hardware/pic.h" //Interrupt controller support!
#include "headers/emu/timers.h" //Timers!
#include "headers/support/log.h" //Log support!
#include "headers/interrupts/interrupt18.h" //Interrupt 18h support!
#include "headers/interrupts/interrupt13.h" //Interrupt 13h initialising support!
#include "headers/support/zalloc.h" //For final freezall functionality!
#include "headers/hardware/ps2_mouse.h" //PS/2 mouse support!
#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!
#include "headers/emu/emucore.h" //Emulation core!
#include "headers/emu/emu_bios_sound.h" //BIOS sound option!
//Debugging:
#include "headers/emu/graphics_debug.h" //Graphics debug!
#include "headers/emu/file_debug.h" //File debug!

#include "headers/emu/soundtest.h" //Sound test support!

#include "headers/cpu/80286/protection.h" //Basic protection support!

#include "headers/emu/emu_main.h" //Main stuff!
#include "headers/emu/emu_vga_bios.h" //VGA BIOS support for output!

//Bootstrap stuff
#include "headers/cpu/easyregs.h" //Easy register stuff!
#include "headers/cpu/callback.h"
#include "headers/interrupts/interrupt05.h"
#include "headers/interrupts/interrupt11.h"
#include "headers/interrupts/interrupt12.h"
#include "headers/interrupts/interrupt16.h"
#include "headers/interrupts/interrupt19.h"

extern byte reset; //To fully reset emu?
extern byte dosoftreset; //To fully softreset emu (start from booting)

extern uint_32 romsize; //For checking if we're running a ROM!

extern BIOS_Settings_TYPE BIOS_Settings;
extern byte EMU_RUNNING; //Emulator running? 0=Not running, 1=Running, Active CPU, 2=Running, Inactive CPU (BIOS etc.)

//Special flags for the BIOS POST loader!
//To only debug text/graphic mode operations for now (and sleep after)?
#define DEBUG_VGA_ONLY 0
//Don't run the emulator?
#define NOEMU 0
//Allow the BIOS to be run?
#define ALLOW_BIOS

byte allow_debuggerstep; //Do we allow the debugger to step through?

extern byte EMU_BIOS[0x10000]; //Full custom BIOS from 0xF0000-0xFFFFF for the emulator itself to use!
extern word CB_datasegment; //Reserved segment when adding callback!
extern word CB_dataoffset; //Reserved offset when adding callback!
void BIOS_initStart() //Memory defaults for the CPU with our internal BIOS!
{
	debugrow("Setting up the initial emulator JMP to internal BIOS ROM executable...");
	//Our core handlers!
	addCBHandler(CB_UNASSIGNEDINTERRUPT, &BIOS_int19, 0x19); //Second is used by the Bootstrap/BIOS loader! Don't assign to an interrupt!
	//Jump to our BIOS!
}

/* reinitialize the PIC controllers, giving them specified vector offsets
rather than 8h and 70h, as configured by default */

#define PIC1_CMD                    0x20
#define PIC1_DATA                   0x21
#define PIC2_CMD                    0xA0
#define PIC2_DATA                   0xA1
#define PIC_READ_IRR                0x0a    /* OCW3 irq ready next CMD read */
#define PIC_READ_ISR                0x0b    /* OCW3 irq service next CMD read */

#define ICW1_ICW4	0x01		/* ICW4 (not) needed */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */

#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */

/*
arguments:
offset1 - vector offset for master PIC
vectors on the master become offset1..offset1+7
offset2 - same for slave PIC: offset2..offset2+7
*/
void PIC_remap(int offset1, int offset2)
{
	unsigned char a1, a2;

	a1 = PORT_IN_B(PIC1_DATA);                        // save masks
	a2 = PORT_IN_B(PIC2_DATA);

	PORT_OUT_B(PIC1_CMD, ICW1_INIT + ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
	PORT_OUT_B(PIC2_CMD, ICW1_INIT + ICW1_ICW4);
	PORT_OUT_B(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
	PORT_OUT_B(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
	PORT_OUT_B(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	PORT_OUT_B(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)

	PORT_OUT_B(PIC1_DATA, ICW4_8086);
	PORT_OUT_B(PIC2_DATA, ICW4_8086);

	PORT_OUT_B(PIC1_DATA, a1);   // restore saved masks.
	PORT_OUT_B(PIC2_DATA, a2);
}

void POST_memorydefaults() //Memory defaults for the CPU without custom BIOS!
{
	//Finally: interrupt callbacks!
	word NULLsegment, NULLoffset;
	addCBHandler(CB_IRET, NULL, 0x00); //IRET first!
	NULLsegment = CB_datasegment;
	NULLoffset = CB_dataoffset;
	addCBHandler(CB_INTERRUPT, &BIOS_int05, 0x05); //Interrupt 05h overrideable handler!
	addCBHandler(CB_INTERRUPT, &BIOS_int10, 0x10); //Interrupt 10h overrideable handler!
	addCBHandler(CB_INTERRUPT, &BIOS_int11, 0x11); //Interrupt 11h overrideable handler!
	addCBHandler(CB_INTERRUPT, &BIOS_int12, 0x12); //Interrupt 12h overrideable handler!
	addCBHandler(CB_INTERRUPT, &BIOS_int13, 0x13); //Interrupt 13h overrideable handler!
	addCBHandler(CB_INTERRUPT, &BIOS_int18, 0x18); //Interrupt 18h overridable handler!
	addCBHandler(CB_IRET,NULL,0x14); //Async communication services to IRET!
	addCBHandler(CB_IRET,NULL,0x15); //System BIOS services to IRET!
	addCBHandler(CB_IRET,NULL,0x17); //Printer to IRET!
	addCBHandler(CB_IRET,NULL,0x1A); //System and RTC services to IRET!
	addCBHandler(CB_IRET,NULL,0x1B); //BIOS CTRL-BREAK!
	addCBHandler(CB_IRET,NULL,0x1C); //System tick!
	CPU_setint(0x19, MMU_rw(-1, 0xF000, 0xFFF3, 0), MMU_rw(-1, 0xF000, 0xFFF1, 0)); //Interrupt 19 (bootstrap)!

	//1D=Video control parameter table
	//1E=Disk base table
	//1F=High video graphics characters
	//49=Translation table for keyboard-supplement devices

	//Set up interrupt handler base table!
	copyint(0x00, 0x01); //Set int 1 to IRET!
	copyint(0x00, 0x02); //Set int 2 to IRET!
	copyint(0x00, 0x03); //Set int 3 to IRET!
	copyint(0x00, 0x04); //Set int 4 to IRET!
	copyint(0x00, 0x06); //Set int 6 to IRET!
	copyint(0x00, 0x07); //Set int 7 to IRET!
	copyint(0x00, 0x08); //Set int 8 to IRET!
	copyint(0x00, 0x09); //Set int 9 to IRET!
	copyint(0x00, 0x0A); //Set int 10 to IRET!
	copyint(0x00, 0x0B); //Set int 11 to IRET!
	copyint(0x00, 0x0C); //Set int 12 to IRET!
	copyint(0x00, 0x0D); //Set int 13 to IRET!
	copyint(0x00, 0x0E); //Set int 14 to IRET!

	//int 15 isn't used!
	//int 16 is BIOS Video!
	//rest is unset or unused!

	PIC_remap(0x08,0x70); //Remap the PIC for our usage!

	//Now, setup overriding interrupts which are installed now!
	BIOS_SetupKeyboard(); //Setup the Dosbox keyboard handler!

	MMU_ww(CPU_segment_index(CPU_SEGMENT_DS), 0x40, 0x72, 0x1234); //Make sure we boot the disk only, not do the BIOS again!
}

//Result: 0=Continue;1=Reset!
int EMU_BIOSPOST() //The BIOS (INT19h) POST Loader!
{
	allow_debuggerstep = 0; //Default: don't allow to step!

	if (MMU_rw(CPU_segment_index(CPU_SEGMENT_DS), 0x40, 0x72, 0) != 0x1234) //Normal BIOS POST?
	{
		debugrow("Running BIOS POST!");
	#ifdef ALLOW_BIOS
		EMU_RUNNING = 0; //We're not running atm!
		if (CheckBIOSMenu(3000000)) //Run BIOS Menu if needed for a short time!
		{
			EMU_RUNNING = 1; //We're running again!
			return 1; //Reset after the BIOS!
		}
	#endif

		debugrow("Running core BIOS POST...");

		if (BIOS_Settings.executionmode == EXECUTIONMODE_BIOS)
		{
			if (NOEMU)
			{
				dolog("emu", "BIOS is used, but not emulated! Resetting emulator!");
				return 1; //Reboot always: emulation isn't allowed!
			}
			byte verified;
			verified = 0; //Default: not verified!

			if (file_exists("ROM/BIOSROM.BIN")) //Fully custom BIOS ROM, for the entire BIOS segment?
			{
				verified = BIOS_load_custom("ROM/BIOSROM.BIN"); //Try to load a custom BIOS ROM!
				if (verified) goto loadOPTROMS; //Loaded the BIOS?
			}
			
			//Load a normal BIOS ROM, according to the chips!
			if (EMULATED_CPU < CPU_80286) //5160 PC?
			{
				if (!BIOS_load_ROM(18)) //Failed to load u18?
				{
					CPU_INT(0x18); //Error: no ROM!
					EMU_startInput(); //Start input again!
					EMU_RUNNING = 1; //We're running again!
					allow_debuggerstep = 1; //Allow stepping from now on!
					return 0; //No reset!
				}
				if (!BIOS_load_ROM(19)) //Failed to load u19?
				{
					BIOS_free_ROM(19); //Release u27!
					CPU_INT(0x18); //Error: no ROM!
					EMU_startInput(); //Start input again!
					EMU_RUNNING = 1; //We're running again!
					allow_debuggerstep = 1; //Allow stepping from now on!
					return 0; //No reset!
				}
				verified = 1; //Verified!
			}
			else //5170 PC?
			{
				if (!BIOS_load_ROM(27)) //Failed to load u27?
				{
					CPU_INT(0x18); //Error: no ROM!
					EMU_startInput(); //Start input again!
					EMU_RUNNING = 1; //We're running again!
					allow_debuggerstep = 1; //Allow stepping from now on!
					return 0; //No reset!
				}
				if (!BIOS_load_ROM(47)) //Failed to load u47?
				{
					BIOS_free_ROM(27); //Release u27!
					CPU_INT(0x18); //Error: no ROM!
					EMU_startInput(); //Start input again!
					EMU_RUNNING = 1; //We're running again!
					allow_debuggerstep = 1; //Allow stepping from now on!
					return 0; //No reset!
				}
				verified = 1; //Verified!
			}

			loadOPTROMS:

			if (verified) //Ready to boot, but need option ROMS?
			{
				verified = BIOS_checkOPTROMS(); //Try and load OPT roms!
			}

			if (!verified) //Error reading ROM?
			{
				CPU_INT(0x18); //Error: no ROM!
				EMU_startInput(); //Start input again!
				EMU_RUNNING = 1; //We're running again!
				return 0; //No reset!
			}
			else //Boot rom ready?
			{
				BIOS_registerROM(); //Register the BIOS ROMS!
				EMU_startInput(); //Start input again!
				resetCPU(); //Reset the CPU to load the BIOS!
				EMU_RUNNING = 1; //We're running again!
				allow_debuggerstep = 1; //Allow stepping from now on!
				startTimers(0); //Make sure we're running fully!
				startTimers(1); //Make sure we're running fully!
				return 0; //No reset, start the BIOS!
			}
		}

		debugrow("Continuing BIOS POST...");
		EMU_stopInput(); //Stop emulator input!

		debugrow("BIOS Beep...");
		doBIOSBeep(); //Do the beep to signal we're ready to run!	

		//Now for the user visible part:

		int OPcounter = 0;
		OPcounter = 0; //Init!
		if (DEBUG_VGA_ONLY)
		{
			DoDebugTextMode(1); //Text mode debugging only, finally sleep!
		}

#ifdef ALLOW_BIOS
		debugrow("BIOS POST Screen...");
		//Now we're ready to go run the POST!
		CPU[activeCPU].registers->AH = 0x00; //Init video mode!
		CPU[activeCPU].registers->AL = VIDEOMODE_EMU; //80x25 16-color TEXT for EMU mode!
		BIOS_int10(); //Switch!

		BIOS_enableCursor(0); //Disable the cursor!

		delay(200000); //Wait a bit before showing on-screen!

		printmsg(0xF, "x86 EMU\r\n");
		printmsg(0xF, "\r\n"); //A bit of whitespace before output!
#endif

#ifdef ALLOW_BIOS
		BIOS_LoadIO(1); //Load basic BIOS I/O (disks), show checksum errors!
#else
		BIOS_LoadIO(0); //Load basic BIOS I/O (disks), hide checksum errors!
#endif

#ifdef ALLOW_BIOS
		startTimers(0); //Start EMU timers!
		BIOS_ShowBIOS(); //Show BIOS information!
		if (CheckBIOSMenu(0)) //Run BIOS Menu if needed!
		{
			BIOS_enableCursor(1); //Re-enable the cursor!
			EMU_RUNNING = 1; //We're running again!
			EMU_startInput(); //Start input again!
			return 1; //Reset after the BIOS!
		}
#endif

		BIOS_enableCursor(1); //Re-enable the cursor!

		if (DEBUG_VIDEOCARD) //Debugging text mode?
		{
			DoDebugTextMode(0); //Do the debugging!
			EMU_RUNNING = 1; //We're running again!
			return 1; //Full reset emulator!
		}

		if (shuttingdown()) //Shut down?
		{
			EMU_Shutdown(0); //Done shutting down!
			halt(); //Shut down!
		}

		//First debugger step: custom bios check!

		//Emulate anything here!
		FILE *f; //The file to use for loading ROMs.
		//Try booting of different disks:

		switch (BIOS_Settings.executionmode) //What execution mode?
		{
		case EXECUTIONMODE_TEST:
			debugrow("Debugging files!");
			DoDebugFiles(); //Do the debug files!
			EMU_startInput(); //Start input again!
			EMU_RUNNING = 1; //We're running again!
			return 1; //Reboot!

		case EXECUTIONMODE_SOUND:
			debugrow("Starting sound test...");
			dosoundtest(); //Run the sound test!
			return 1; //Reboot!

		case EXECUTIONMODE_TESTROM:
			f = fopen("TESTROM.DAT", "rb"); //Try TESTROM.DAT?
			int verified;
			romsize = 0; //Default: no ROM!

			if (f) //Show boot rom msg?
			{
				printmsg(0x0F, "Booting Test ROM...\r\n");
			}

			if (f) //Boot ROM?
			{
				fseek(f, 0, SEEK_END); //Goto EOF!
				romsize = ftell(f); //ROM size!
				fseek(f, 0, SEEK_SET); //Goto BOF!
				byte *ptr = (byte *)MMU_ptr(-1, 0x0000, 0x0000, 0, romsize); //Read a pointer to test ROM memory!
				if (ptr) //Valid pointer?
				{
					verified = fread(ptr, 1, romsize, f); //Read ROM to memory adress 0!
				}
				else
				{
					verified = 0; //Failed!
				}
				CPU[activeCPU].registers->CS = CPU[activeCPU].registers->DS = CPU[activeCPU].registers->ES = 0;
				CPU[activeCPU].registers->IP = 0; //Run ROM!
				CPU[activeCPU].registers->SS = 0;
				CPU[activeCPU].registers->SP = 0x100; //For ROM specific!
				fclose(f); //Close boot rom!
				if (!verified) //Error reading ROM?
				{
					CPU_INT(0x18); //Error: no ROM!
					EMU_startInput(); //Start input again!
					EMU_RUNNING = 1; //We're running again!
					return 0; //No reset!
				}
				else //Boot rom ready?
				{
					EMU_startInput(); //Start input again!
					EMU_RUNNING = 1; //We're running again!
					allow_debuggerstep = 1; //Allow stepping from now on!
					return 0; //Run the boot rom!
				}
			}
			break;
		default: //Unknwn?
			break;
		}

		if (NOEMU)
		{
			EMU_startInput(); //Start input again!
			EMU_RUNNING = 1; //We're running again!
			return 1; //Don't emulate: just reset!
		}

		debugrow("Starting CPU emulation...");

		//We're starting up normal emulation of our 'BIOS'?

		initMEM(); //Initialise all BIOS stuff in memory!

		POST_memorydefaults(); //Install default handlers for interrupts etc.!

		BIOSKeyboardInit(); //Initialise the BIOS stuff for the keyboard!

		CPU[activeCPU].registers->AX = VIDEOMODE_BOOT; //TEXT mode for booting!
		BIOS_int10(); //Switch modes!

		CPU[activeCPU].registers->AH = 0x06; //Shift down total rows (moving everthing down one line)?
		CPU[activeCPU].registers->AL = 0; //0 for clear >0 for shift!
		CPU[activeCPU].registers->BH = 0xF; //Attribute!
		CPU[activeCPU].registers->CH = 0;
		CPU[activeCPU].registers->CL = 0;
		CPU[activeCPU].registers->DH = 23;
		CPU[activeCPU].registers->DL = 79; //Coordinates of our window!
		BIOS_int10(); //Clear the screen!

		CPU[activeCPU].registers->AH = 0x02;
		CPU[activeCPU].registers->BH = 0; //Page #0!
		CPU[activeCPU].registers->DH = 0; //Y!
		CPU[activeCPU].registers->DL = 0; //X!
		BIOS_int10(); //Move cursor!

		CPU[activeCPU].registers->AX = 0;
		CPU[activeCPU].registers->BX = 0;
		CPU[activeCPU].registers->CX = 0;
		CPU[activeCPU].registers->DX = 0; //Reset basic registers!
	}

	int13_init(1, 1, has_drive(HDD0), has_drive(HDD1), 1, 1); //Initialise interrupt 13h disks! Always floppy0&1 and cdrom0&1. HDD are predefined and mounted.

	//Execute boot sequence, if possible...
	if (EMULATED_CPU >= CPU_80286) //Emulating a CPU with protected mode?
	{
		printmsg(0xF, "You can't use the 80286+ with the default BIOS. Please insert a BIOS ROM.");
		delay(1000000); //Wait 1 second before rebooting!
		MMU_ww(CPU_segment_index(CPU_SEGMENT_DS), 0x0000, 0x0472, 0); //Clear reboot flag!
		REG_CS = 0xF000; //Go back to our bootstrap, by using a simulated jump to ROM!
		REG_IP = 0xFFFF;
	}
	else //We can boot safely?
	{
		//Boot to disk system!
		if (!boot_system()) //System not booted?
		{
			CPU_INT(0x18); //Boot failure!
		}
		else //We're booted?
		{
			BIOS_DUMPSYSTEMROM(); //Dump our system ROM for debugging purposes, if enabled!
			EMU_startInput(); //Start input again!
			allow_debuggerstep = 1; //Allow stepping from now on!
		}
		EMU_RUNNING = 1; //We're running again!
		return 0; //Continue normally: we've booted, or give an error message!
	}

	EMU_RUNNING = 1; //We're running again!
	EMU_startInput(); //Start input again!
	return 0; //Plain run!
}