#ifndef MIDI_H
#define MIDI_H

#include "headers/types.h" //Basic types!

void resetMPU(); //Fully resets the MPU!
byte MIDI_has_data(); //Do we have data to be read?
void MIDI_OUT(byte data);
byte MIDI_IN();
void initMPU(); //Initialise function!
void doneMPU(); //Finish function!

void MPU401_Init(/*Section* sec*/); //From DOSBox (mpu.c)!

void MPU401_Done(); //Finish our MPU! Custom by superfury1!
#endif