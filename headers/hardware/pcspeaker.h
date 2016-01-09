#ifndef PCSPEAKER_H
#define PCSPEAKER_H

//PC speaker!
void setSpeakerFrequency(word newfrequency); //Set the new frequency!
void initSpeakers(); //Initialises the speaker and sets it up!
void doneSpeakers(); //Finishes the speaker and removes it!
void tickSpeakers(); //Ticks all PC speakers available!
void setPCSpeakerMode(byte mode); //Set the current rendering mode!
void speakerGateUpdated(); //Gate has been updated?

#endif