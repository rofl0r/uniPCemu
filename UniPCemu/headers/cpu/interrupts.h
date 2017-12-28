#ifndef BIOS_INTERRUPTS_H
#define BIOS_INTERRUPTS_H

void CPU_setint(byte intnr, word segment, word offset); //Set real mode IVT entry!
byte CPU_INT(byte intnr, int_64 errorcode, byte is_interrupt); //Call an interrupt!
void CPU_IRET();

void BIOS_unkint(); //Unknown/unhandled interrupt (<0x20 only!)
byte CPU_customint(byte intnr, word retsegment, uint_32 retoffset, int_64 errorcode, byte is_interrupt); //Used by soft (below) and exceptions/hardware!

#endif