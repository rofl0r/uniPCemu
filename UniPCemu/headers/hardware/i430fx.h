#ifndef I430FX_H
#define I430FX_H

#include "headers/types.h" //Basic types!

#ifndef IS_I430FX
extern byte is_i430fx; //Are we an i430fx motherboard?
extern byte i430fx_memorymappings_read[16]; //All read memory/PCI! Set=DRAM, clear=PCI!
extern byte i430fx_memorymappings_write[16]; //All write memory/PCI! Set=DRAM, clear=PCI!
#endif

void i430fx__SMIACT(byte active); //SMIACT# signal
void i430fx_writeaddr(byte index, byte *value); //Written an address?
void init_i430fx();
void done_i430fx();
void i430fx_MMUready(); //Memory is ready to use?

#endif
