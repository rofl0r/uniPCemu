#ifndef SOUNDBLASTER_H
#define SOUNDBLASTER_H

void initSoundBlaster(word port, byte version);
void doneSoundBlaster();
void updateSoundBlaster(double timepassed, uint_32 MHZ14passed);


#endif