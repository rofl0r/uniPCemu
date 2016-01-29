#ifndef __EMU_VGA_H
#define __EMU_VGA_H

void EMU_update_VGA_Settings(); //Update the VGA settings for the emulator!
void VGA_initTimer(); //Initialise the timer before running!
void updateVGA(double timepassed); //Tick the timer for the CPU accurate cycle emulation!

#endif