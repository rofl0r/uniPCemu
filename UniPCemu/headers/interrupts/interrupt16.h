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

#ifndef INTERRUPT16_H
#define INTERRUPT16_H

void BIOS_int16(); //Interrupt #16h: (Keyboard)! Overridable!
void Dosbox_RealSetVec(byte interrupt, uint_32 realaddr); //For dosbox compatibility!
void BIOS_SetupKeyboard(); //Sets up the keyboard handler for usage by the CPU!
#endif