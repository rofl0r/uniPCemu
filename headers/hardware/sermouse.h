#ifndef __SERMOUSE_H
#define __SERMOUSE_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/ps2_mouse.h" //PS/2 mouse packet support!

byte useSERMouse(); //Serial mouse enabled?
void SERmouse_packet_handler(MOUSE_PACKET *packet);
void initSERmouse(byte enabled); //Inititialise serial mouse!

#endif