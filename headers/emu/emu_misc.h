#ifndef EMU_MISC_H
#define EMU_MISC_H

int FILE_EXISTS(char *filename);
void BREAKPOINT(); //Break point!
int move_file(char *fromfile, char *tofile); //Move a file, gives an error code or 0!
float frand(); //Floating point random
float RandomFloat(float min, float max); //Random float within range!
short shortrand(); //Short random
short RandomShort(short min, short max);
void EMU_update_DACColorScheme(); //Update our DAC color scheme (runtime-changable)!
#endif