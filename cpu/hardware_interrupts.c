#include "headers/cpu/cpu.h" //CPU support!
#include "headers/cpu/interrupts.h" //Interrupt handlers!

Handler soft_interrupt_jmptbl[0x100] =   //Our standard internal hardware interrupt jmptbl!
{
//0x00-0x07: Processor interrupts. Also int instructions.
	NULL, //00h: hardware
	NULL, //01h: hardware
	NULL, //02h: hardware
	NULL, //03h: hardware
	NULL, //04h: hardware
	NULL, //05h: BIOS overrideable: Print screen
	NULL, //06h: hardware
	NULL, //07h: hardware
//0x08-0x0F: Hardware interrupts (IRQs).
	NULL, //08h: hardware: IRQ0: System timer IC
	NULL, //09h: hardware: IRQ1: Keyboard controller IC
	NULL, //0Ah: hardware: IRQ2: Second IRQ Controller IC
	NULL, //0Bh: hardware: IRQ3: Serial port 2 (COM2: 2F8-2FF, and COM4: 2E8-2EF) or hardware modem use
	NULL, //0Ch: hardware: IRQ4: Serial port 1 (COM1: 3F8-3FF, and COM3: 3E8-3EF) or serial port mouse use
	NULL, //0Dh: hardware: IRQ5: Parallel port 2 (LPT2: 378h or 278h) or general adapter use (typically used for sound cards)
	NULL, //0Eh: software: IRQ6: Floppy disk controller
	NULL, //0Fh: software: IRQ7: Parallel port 1 (LPT1: 3BCh[mono] or 378h[color])

//0x10:
	NULL, //10h: build in: video interrupt: overridable!
	NULL, //11h: build in: Equipment list: overridable!
	NULL, //12h: build in: Memory size
	NULL, //13h: BIOS overridable Fixed disk/Diskette
	NULL, //14h: build in: Serial communication
	NULL, //15h: build in: System services
	NULL, //16h: BIOS overridable: Keyboard
	NULL, //17h: build in: Parallel Printer
	NULL, //18h: build in: Process Boot Failure (XT,AT)
	NULL, //19h: build in: Boot strap loader
	NULL, //1Ah: build in: Time of Day
	NULL, //1Bh: build in: Keyboard Break
	NULL, //1Ch: User: User Timer Tick
	NULL, //1Dh: BIOS: Video Parameter Table
	NULL, //1Eh: BIOS: Diskette Parameter Table
	NULL, //1Fh: BIOS: Video Graphics Characters

//0x20
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0x30
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0x40
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0x50
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0x60
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0x70
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0x80
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0x90
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0xA0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0xB0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0xC0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0xD0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0xE0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

//0xF0
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};