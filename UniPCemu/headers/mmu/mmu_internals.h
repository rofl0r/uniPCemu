/*

Copyright (C) 2019  Superfury

This file is part of UniPCemu.

UniPCemu is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

UniPCemu is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UniPCemu.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef MMU_INTERNALS
#define MMU_INTERNALS

#include "headers/types.h" //Basic types!

//Internal communication routines from MMU.c and MMUhandler.c!
byte MMU_INTERNAL_directrb_realaddr(uint_32 realaddress, byte index); //Read without segment/offset translation&protection (from system/interrupt)!
void MMU_INTERNAL_directwb_realaddr(uint_32 realaddress, byte val, byte index); //Write without segment/offset translation&protection (from system/interrupt)!

#endif
