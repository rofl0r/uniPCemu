#ifndef VGA_CGA_MDA_H
#define VGA_CGA_MDA_H

#include "headers/types.h" //Basic types!

void setVGA_CGA(byte enabled); //0=Disabled, 1=Enable with disabled VGA, 2=Enabled with enabled VGA!
void setCGA_NewCGA(byte enabled); //Use New-style CGA emulation?
void setCGA_NTSC(byte enabled); //Use NTSC CGA signal output?
void setVGA_MDA(byte enabled); //0=Disabled, 1=Enable with disabled VGA, 2=Enabled with enabled VGA!

//Initialization call for registering us on the VGA!
void initCGA_MDA();

#endif