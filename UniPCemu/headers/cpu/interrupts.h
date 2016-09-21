#ifndef BIOS_INTERRUPTS_H
#define BIOS_INTERRUPTS_H

void CPU_setint(byte intnr, word segment, word offset); //Set real mode IVT entry!
void CPU_INT(byte intnr, byte is_HW); //Call an interrupt!
void CPU_IRET();

void BIOS_unkint(); //Unknown/unhandled interrupt (<0x20 only!)
void CPU_customint(byte intnr, word retsegment, uint_32 retoffset, byte is_HW); //Used by soft (below) and exceptions/hardware!

#endif