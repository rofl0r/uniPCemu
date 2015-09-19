#include "headers/types.h" //Basic types!

#include "headers/mmu/mmu.h" //For MMU
/*
#include "headers/cpu/cpu.h" //For CPU
#include "headers/bios/boot.h" //For booting mounted disks!
#include "headers/basicio/io.h" //For mounting!
*/
#include "headers/bios/bios.h" //For BIOS!
/*
#include "headers/bios/biosmenu.h" //For key support!
#include "headers/emu/gpu/gpu.h" //For GPU!
#include "headers/cpu/interrupts.h" //For int 18 support!
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/emu/input.h" //Input support!
#include "headers/hardware/vga/vga.h" //For savestate support!
#include "headers/emu/state.h" //SaveState holder!
#include "headers/support/crc32.h" //CRC32 function support!
#include "headers/mmu/bda.h" //BDA support!
*/
#include "headers/hardware/pic.h" //Interrupt controller support!
/*
#include "headers/cpu/8086/CPU_OP8086.h" //Basic 8086 control (interrupts!)
#include "headers/cpu/modrm.h" //MODR/M test functionnality!
*/
#include "headers/emu/timers.h" //Timers!
/*
#include "headers/emu/runromverify.h" //RunRomVerify function support!
#include "headers/header_dosbox.h" //For constants!
#include "headers/hardware/8042.h" //For void BIOS_init8042()
#include "headers/hardware/8253.h" //82C54 support!
#include "headers/hardware/ports.h" //Port support!
#include "headers/hardware/vga/vga_displaygeneration_crtcontroller.h" //For get/putpixel
#include "headers/emu/threads.h" //Thead support for errors!
*/
#include "headers/support/log.h" //Log support!
#include "headers/interrupts/interrupt18.h" //Interrupt 18h support!
#include "headers/support/zalloc.h" //For final freezall functionality!
/*
#include "headers/hardware/adlib.h" //Adlib!
#include "headers/support/bmp.h" //Bitmap!
#include "headers/hardware/ps2_keyboard.h" //PS/2 keyboard support!
#include "headers/hardware/ps2_mouse.h" //PS/2 mouse support!
#include "headers/emu/gpu/gpu_text.h" //Text support!

//All graphics now!
*/
#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!
#include "headers/emu/emucore.h" //Emulation core!
#include "headers/emu/emu_bios_sound.h" //BIOS sound option!
//Debugging:
#include "headers/emu/graphics_debug.h" //Graphics debug!
#include "headers/emu/file_debug.h" //File debug!

#include "headers/cpu/protection.h" //Basic protection support!

#include "headers/emu/emu_main.h" //Ourselves!

#include "headers/emu/threads.h" //Thread support!

//Disable BIOS&OS loading for testing?
#define NOEMU 0
#define NOBIOS 0

/*

BIOS Loader&Execution!

*/

byte use_profiler = 0; //To use the profiler?

byte emu_use_profiler()
{
	return use_profiler; //To use or not?
}

//Main loader stuff:

byte reset = 0; //To fully reset emu?
uint_32 romsize = 0; //For checking if we're running a ROM!

extern BIOS_Settings_TYPE BIOS_Settings;
extern byte EMU_RUNNING; //Emulator running? 0=Not running, 1=Running, Active CPU, 2=Running, Inactive CPU (BIOS etc.)

/*

Main thread for emulation!

*/

void cputhread() //The main thread for the emulator!
{
	if (!hasmemory()) //No MMU?
	{
		dolog("BIOS","EMU_BIOSLoader: we have no memory!");
		BIOS_LoadIO(1); //Load basic BIOS I/O (disks), don't show checksum errors!
		autoDetectMemorySize(0); //Check&Save memory size if needed!
		EMU_Shutdown(0); //No shutdown!
		return; //Reset!
	}
	
//At this point everything is ready to go!
	byte emu_status;
	emu_status = DoEmulator(); //Run the emulator!
	debugrow("Checking return status...");
	switch (emu_status) //What to do next?
	{
	case 0: //Shutdown
		debugrow("Shutdown...");
		EMU_Shutdown(1); //Execute shutdown!
	case 1: //Full reset emu
		debugrow("Reset..."); //Not supported yet!
	default: //Unknown status?
		debugrow("Invalid EMU return code OR full reset requested!");
		return; //Shut down our thread, returning to the main processor!
	}
}

void finishEMU() //Called on emulator quit.
{
	doneEMU(); //Finish up the emulator that was still running!
}