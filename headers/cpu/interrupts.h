#ifndef BIOS_INTERRUPTS_H
#define BIOS_INTERRUPTS_H

#include "headers/hardware/vga/vga.h" //VGA support!

void CPU_setint(byte intnr, word segment, word offset); //Set real mode IVT entry!
void CPU_INT(byte intnr); //Call an interrupt!
void CPU_IRET();

void BIOS_unkint(); //Unknown/unhandled interrupt (<0x20 only!)
void CPU_customint(byte intnr, word retsegment, uint_32 retoffset); //Used by soft (below) and exceptions/hardware!

#endif