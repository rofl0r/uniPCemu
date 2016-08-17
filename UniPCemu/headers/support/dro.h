#ifndef DRO_H
#define DRO_H

byte playDROFile(char *filename, byte showinfo); //Play a MIDI file, CIRCLE to stop playback!

void stepDROPlayer(double timepassed); //CPU handler for playing DRO files!
#endif