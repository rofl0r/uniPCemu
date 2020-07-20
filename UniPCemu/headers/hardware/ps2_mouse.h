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

#ifndef PS2_MOUSE_H
#define PS2_MOUSE_H

#include "headers/types.h" //Basic types!

//Timeout between commands or parameters and results being buffered! Use 5ms timings!
#ifdef IS_LONGDOUBLE
#define MOUSE_DEFAULTTIMEOUT 100000.0L
#else
#define MOUSE_DEFAULTTIMEOUT 100000.0
#endif

typedef struct MOUSE_PACKET
{
sbyte xmove;
sbyte ymove;
sbyte xmovemickey;
sbyte ymovemickey;
byte buttons; //1=Left, 2=Right, 4=Middle bitmask.
sbyte scroll; //scroll up(MAX -8)/down(MAX +7); Used during 4-byte packets only! After setsamplerate 200,200,80 and request ID 4.
struct MOUSE_PACKET *next; //Next packet!
} MOUSE_PACKET;

void PS2_initMouse(byte enabled); //Initialise the mouse to reset mode?
byte PS2mouse_packet_handler(MOUSE_PACKET *packet); //A packet has arrived (on mouse!)
int useMouseTimer(); //Use the mouse timer?
float HWmouse_getsamplerate(); //Which repeat rate to use after the repeat delay! (packets/second)

void EMU_enablemouse(byte enabled); //Enable mouse input (disable during EMU, enable during CPU emulation (not paused))?
void BIOS_doneMouse(); //Finish the mouse.

void updatePS2Mouse(DOUBLE timepassed); //For stuff requiring timing!
#endif
