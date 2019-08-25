/*
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

#ifndef ADLIB_H
#define ADLIB_H

void initAdlib(); //Initialise adlib!
void doneAdlib(); //Finish adlib!

void cleanAdlib();
void updateAdlib(uint_32 MHZ14passed); //Sound tick. Executes every instruction.

//Special Sound Blaster support!
byte readadlibstatus();
void writeadlibaddr(byte value);
void writeadlibdata(byte value);

#endif