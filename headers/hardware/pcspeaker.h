#ifndef PCSPEAKER_H
#define PCSPEAKER_H

//PC speaker!
void enableSpeaker(byte speaker); //Enables the speaker!
void disableSpeaker(byte speaker); //Disables the speaker!
void setSpeakerFrequency(byte speaker, float newfrequency); //Set the new frequency!
void initSpeakers(); //Initialises the speaker and sets it up!
void doneSpeakers(); //Finishes the speaker and removes it!
void tickSpeakers(); //Ticks all PC speakers available!

#endif