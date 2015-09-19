#ifndef COLORCONVERSION_H
#define COLORCOLVERSION_H

#include "headers/types.h"

uint_32 getcolX(byte r, byte g, byte b, byte size); //Get relative color RGB color!

uint_32 getcol64k(byte r, byte g, byte b); //64k color convert!
int col16_blink(byte color);
uint_32 getcol16bw(byte color); //Convert color to B/W RGB!
uint_32 getcol4(byte color); //Convert color to RGB!
uint_32 getcol2(byte color); //Convert color to RGB!

#endif