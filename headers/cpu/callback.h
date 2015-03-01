#ifndef CALLBACK_H
#define CALLBACK_H

#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //CPU support!

//Info for internal callbacks! (4KB data used)
#define CB_MAX		128
#define CB_SIZE		32
//First callback is reserved for the ROM Basic!
#define CB_SEG		0xF600
#define CB_SOFFSET	0x1000

//Base of our data within our custom BIOS!
#define CB_BASE 0x7000

//Type for below!
//Not defined.
#define CB_NONE 0x00
//Used definitions:
#define CB_INTERRUPT 0x01
#define CB_IRET 0x02
#define CB_DATA 0x03
//Same as CB_INTERRUPT, but not assigned to any yet!
#define CB_UNASSIGNEDINTERRUPT 0x04

//Dosbox interrupt support!
#define CB_DOSBOX_INTERRUPT 0x05
#define CB_DOSBOX_IRQ0 0x06
#define CB_DOSBOX_IRQ1 0x07
#define CB_DOSBOX_IRQ9 0x08
#define CB_DOSBOX_IRQ12 0x09
#define CB_DOSBOX_IRQ12_RET 0x0A
#define CB_DOSBOX_MOUSE 0x0B
#define CB_DOSBOX_INT16 0x0C

void CB_handler(word handlernr); //Call an handler (from CB_Handler)?
void addCBHandler(byte type, Handler CBhandler, uint_32 intnr); //Add a callback!
void clearCBHandlers(); //Reset callbacks!

void CB_handleCallbacks(); //Handle callbacks after CPU/debugger usage!
byte CB_ISCallback(); //Are we a called callback (for callbacked functions)?
#endif