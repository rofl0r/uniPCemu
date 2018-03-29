#ifndef MID_H
#define MID_H

#include "headers/types.h" //Basic types!

//MIDI file player support!

byte playMIDIFile(char *filename, byte showinfo); //Play a MIDI file, CIRCLE to stop playback! Cancelled/error loading returns 0, 1 on success playing.
void updateMIDIPlayer(DOUBLE timepassed); //Update the running MIDI player!

#endif