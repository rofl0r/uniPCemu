#ifndef MIDI_H
#define MIDI_H

#include "headers/types.h" //Basic types!

void resetMPU(); //Fully resets the MPU!
byte MIDI_has_data(); //Do we have data to be read?
void MIDI_OUT(byte data);
byte MIDI_IN();
byte initMPU(char *filename, byte use_direct_MIDI); //Our own initialise function!
void doneMPU(); //Finish function!

void MPU401_Init(/*Section* sec*/); //From DOSBox (mpu.c)!

void MPU401_Done(); //Finish our MPU! Custom by superfury1!

void updateMPUTimer(double timepassed);

void setMPUTimer(double timeout, Handler handler);
void removeMPUTimer();

#endif