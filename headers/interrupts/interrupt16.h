#ifndef INTERRUPT16_H
#define INTERRUPT16_H

void BIOS_int16(); //Interrupt #16h: (Keyboard)! Overridable!
void Dosbox_RealSetVec(byte interrupt, uint_32 realaddr); //For dosbox compatibility!
void BIOS_SetupKeyboard(); //Sets up the keyboard handler for usage by the CPU!
#endif