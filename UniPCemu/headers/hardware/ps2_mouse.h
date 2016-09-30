#ifndef PS2_MOUSE_H
#define PS2_MOUSE_H

#include "headers/types.h" //Basic types!

typedef struct MOUSE_PACKET
{
sbyte xmove;
sbyte ymove;
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

void updatePS2Mouse(double timepassed); //For stuff requiring timing!
#endif