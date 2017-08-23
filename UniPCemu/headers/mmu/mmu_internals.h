#ifndef MMU_INTERNALS
#define MMU_INTERNALS

#include "headers/types.h" //Basic types!

//Internal communication routines from MMU.c and MMUhandler.c!
byte MMU_INTERNAL_directrb_realaddr(uint_32 realaddress, byte index); //Read without segment/offset translation&protection (from system/interrupt)!
void MMU_INTERNAL_directwb_realaddr(uint_32 realaddress, byte val, byte index); //Write without segment/offset translation&protection (from system/interrupt)!

#endif
