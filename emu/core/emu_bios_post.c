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

extern byte shutdown; //Shut down (default: NO)?
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
//To debug files in the tests folder?
#define ALLOW_DEBUGFILES 1
//Allow the BIOS to be run?
#define ALLOW_BIOS

//Result: 0=Continue;1=Reset!
int EMU_BIOSPOST() //The BIOS (INT19h) POST Loader!
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

	if (BIOS_Settings.debugmode == DEBUGMODE_BIOS)
	{
		if (NOEMU)
		{
			dolog("emu", "BIOS is used, but not emulated! Resetting emulator!");
			return 1; //Reboot always: emulation isn't allowed!
		}
		byte verified;
		verified = 0; //Default: not verified!

		if (EMULATED_CPU < CPU_80286) //5160 PC?
		{
			if (!BIOS_load_ROM(18)) //Failed to load u18?
			{
				CPU_INT(0x18); //Error: no ROM!
				EMU_startInput(); //Start input again!
				EMU_RUNNING = 1; //We're running again!
				return 0; //No reset!
			}
			if (!BIOS_load_ROM(19)) //Failed to load u19?
			{
				BIOS_free_ROM(19); //Release u27!
				CPU_INT(0x18); //Error: no ROM!
				EMU_startInput(); //Start input again!
				EMU_RUNNING = 1; //We're running again!
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
				return 0; //No reset!
			}
			if (!BIOS_load_ROM(47)) //Failed to load u47?
			{
				BIOS_free_ROM(27); //Release u27!
				CPU_INT(0x18); //Error: no ROM!
				EMU_startInput(); //Start input again!
				EMU_RUNNING = 1; //We're running again!
				return 0; //No reset!
			}
			verified = 1; //Verified!
		}

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
			BIOS_registerROM(); //Register the BIOS ROM!
			EMU_startInput(); //Start input again!
			EMU_RUNNING = 1; //We're running again!
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
	CPU.registers->AH = 0x00; //Init video mode!
	CPU.registers->AL = VIDEOMODE_EMU; //80x25 16-color TEXT for EMU mode!
	BIOS_int10(); //Switch!

	BIOS_enableCursor(0); //Disable the cursor!
	
	delay(200000); //Wait a bit before showing on-screen!

	printmsg(0xF,"x86 EMU\r\n");
	printmsg(0xF,"\r\n"); //A bit of whitespace before output!
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

	if (DEBUG_TEXTMODE) //Debugging text mode?
	{
		DoDebugTextMode(0); //Do the debugging!
		EMU_RUNNING = 1; //We're running again!
		return 1; //Full reset emulator!
	}	
	
	if (shutdown) //Shut down?
	{
		shutdown = 0; //Done shutting down!
		halt(); //Shut down!
	}
	
//First debugger step: custom bios check!

	if (ALLOW_DEBUGFILES && BIOS_Settings.debugmode==DEBUGMODE_TEST)
	{
		debugrow("Debugging files!");
		DoDebugFiles(); //Do the debug files!
		EMU_startInput(); //Start input again!
		EMU_RUNNING = 1; //We're running again!
		return 1; //Reboot!
	}

	if (BIOS_Settings.debugmode==DEBUGMODE_SOUND)
	{
		debugrow("Starting sound test...");
		dosoundtest(); //Run the sound test!
	}

	//Emulate anything here!
	FILE *f; //The file to use for loading ROMs.
	//Try booting of different disks:

	if (BIOS_Settings.debugmode==DEBUGMODE_TEST)
	{
		f = fopen("TESTROM.DAT","rb"); //Try TESTROM.DAT?
		int verified;
		romsize = 0; //Default: no ROM!

		if (f) //Show boot rom msg?
		{
			printmsg(0x0F,"Booting Test ROM...\r\n");
		}

		if (f) //Boot ROM?
		{
			fseek(f,0,SEEK_END); //Goto EOF!
			romsize = ftell(f); //ROM size!
			fseek(f,0,SEEK_SET); //Goto BOF!
			byte *ptr = (byte *)MMU_ptr(-1,0x0000,0x0000,0,romsize); //Read a pointer to test ROM memory!
			if (ptr) //Valid pointer?
			{
				verified = fread(ptr,1,romsize,f); //Read ROM to memory adress 0!
			}
			else
			{
				verified = 0; //Failed!
			}
			CPU.registers->CS = CPU.registers->DS = CPU.registers->ES = 0;
			CPU.registers->IP = 0; //Run ROM!
			CPU.registers->SS = 0;
			CPU.registers->SP = 0x100; //For ROM specific!
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
				return 0; //Run the boot rom!
			}
		}
	}
	
	if (NOEMU)
	{
		EMU_startInput(); //Start input again!
		EMU_RUNNING = 1; //We're running again!
		return 1; //Don't emulate: just reset!
	}
	
	debugrow("Starting CPU emulation...");

	//We're starting up normal emulation of our 'BIOS'?

	initMEM(); //Initialise all stuff in memory!
	
	CPU_memorydefaults(); //Install default handlers for interrupts etc.!
	
	BIOSKeyboardInit(); //Initialise the BIOS stuff for the keyboard!
	
	CPU.registers->AX = VIDEOMODE_BOOT; //TEXT mode for booting!
	BIOS_int10(); //Switch modes!

	CPU.registers->AH = 0x06; //Shift down total rows (moving everthing down one line)?
	CPU.registers->AL = 0; //0 for clear >0 for shift!
	CPU.registers->BH = 0xF; //Attribute!
	CPU.registers->CH = 0;
	CPU.registers->CL = 0;
	CPU.registers->DH = 23;
	CPU.registers->DL = 79; //Coordinates of our window!
	BIOS_int10(); //Clear the screen!

	CPU.registers->AH = 0x02;
	CPU.registers->BH = 0; //Page #0!
	CPU.registers->DH = 0; //Y!
	CPU.registers->DL = 0; //X!
	BIOS_int10(); //Move cursor!

	CPU.registers->AX = 0;
	CPU.registers->BX = 0;
	CPU.registers->CX = 0;
	CPU.registers->DX = 0; //Reset basic registers!
	
	int13_init(1,1,has_drive(HDD0),has_drive(HDD1),1,1); //Initialise interrupt 13h disks! Always floppy0&1 and cdrom0&1. HDD are predefined and mounted.
	
	MMU_ww(CPU_segment_index(CPU_SEGMENT_DS),0x0400,0x72,0x1234); //Make sure we boot normally!
	CPU_INT(0x19); //Run bootstrap loader!

	EMU_RUNNING = 1; //We're running again!
	EMU_startInput(); //Start input again!
	return 0; //Plain run!
}