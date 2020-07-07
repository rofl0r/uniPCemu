#ifndef I430FX_H
#define I430FX_H

#include "headers/types.h" //Basic types!

#ifndef IS_I430FX
extern byte is_i430fx; //Are we an i430fx motherboard?
extern byte i430fx_memorymappings_read[16]; //All read memory/PCI! Set=DRAM, clear=PCI!
extern byte i430fx_memorymappings_write[16]; //All write memory/PCI! Set=DRAM, clear=PCI!
#endif

void init_i430fx(byte enabled);
void done_i430fx();

#endif