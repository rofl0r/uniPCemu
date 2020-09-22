/*

Copyright (C) 2020 - 2020  Superfury

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

#include "headers/cpu/cpu.h" //Basic CPU!
#include "headers/cpu/cpu_OP80786.h" //i786 support!
#include "headers/cpu/cpu_OP8086.h" //16-bit support!
#include "headers/cpu/cpu_OP80386.h" //32-bit support!
#include "headers/cpu/cpu_pmtimings.h" //Timing support!
#include "headers/cpu/easyregs.h" //Easy register support!

MODRM_PARAMS params; //For getting all params for the CPU!
extern byte cpudebugger; //The debugging is on?
extern byte thereg; //For function number!

//Modr/m support, used when reg=NULL and custommem==0
extern byte MODRM_src0; //What source is our modr/m? (1/2)
extern byte MODRM_src1; //What source is our modr/m? (1/2)

