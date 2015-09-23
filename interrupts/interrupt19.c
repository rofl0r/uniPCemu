#include "headers/types.h"
#include "headers/bios/bios.h" //BOOT loader!
#include "headers/cpu/interrupts.h" //BOOT loader!
#include "headers/interrupts/interrupt18.h" //Interrupt 18h support!
#include "headers/mmu/mmu.h" //MMU support!
#include "headers/cpu/protection.h" //For CPU_segment_index!
#include "headers/emu/emu_main.h" //For BIOS POST!
#include "headers/emu/threads.h" //Thread support!

extern byte reset; //To reset the emulator?
extern BIOS_Settings_TYPE BIOS_Settings; //Our BIOS Settings!

//Boot strap loader!

/*
note 1) Reads track 0, sector 1 into address 0000h:7C00h, then transfers
        control to that address. If no diskette drive available, looks at
        absolute address C:800 for a valid hard disk or other ROM. If none,
        transfers to ROM-BASIC via int 18h or displays loader error message.
     2) Causes reboot of disk system if invoked while running. (no memory test
        performed).
     3) If location 0000:0472h does not contain the value 1234h, a memory test
        (POST) will be performed before reading the boot sector.
     4) VDISK from DOS 3.0+ traps this vector to determine when the CPU has
        shifted from protected mode to real mode. A detailed discussion can
        be found by Ray Duncan in PC Magazine, May 30, 1989.
     5) Reportedly, some versions of DOS 2.x and all versions of DOS 3.x+
        intercept int 19h in order to restore some interrupt vectors DOS takes
        over, in order to put the machine back to a cleaner state for the
        reboot, since the POST will not be run on the int 19h. These vectors
        are reported to be: 02h, 08h, 09h, 0Ah, 0Bh, 0Ch, 0Dh, 0Eh, 70h, 72h,
        73h, 74h, 75h, 76h, and 77h. After restoring these, it restores the
        original int 19h vector and calls int 19h.
     6) The system checks for installed ROMs by searching memory from 0C000h to
        the beginning of the BIOS, in 2k chunks. ROM memory is identified if it
        starts with the word 0AA55h. It is followed a one byte field length of
        the ROM (divided by 512). If ROM is found, the BIOS will call the ROM
        at an offset of 3 from the beginning. This feature was not supported in
        the earliest PC machines. The last task turns control over to the
        bootstrap loader (assuming the floppy controller is operational).
     7) 8255 port 60h bit 0 = 1 if booting from diskette.
*/

extern ThreadParams_p BIOSMenuThread; //BIOS pause menu thread!

void POSTThread()
{
	reset = EMU_BIOSPOST(); //Execute POST, process emulator reset if needed!
}

void BIOS_int19()
{
	if (BIOSMenuThread) //Gotten a POST thread?
	{
		if (threadRunning(BIOSMenuThread, "BIOSMenu"))
		{
			return; //Don't re-start the POST thread when already running: keep idle looping(waiting for it to finish) while it's running!
		}
	}
	CPU_resetOP(); //Reset the CPU to re-run this opcode by default!
	BIOSMenuThread = startThread(&POSTThread,"BIOSMenu",NULL,DEFAULT_PRIORITY); //Start the POST thread at default priority!
	delay(0); //Wait a bit to start up the thread!
}