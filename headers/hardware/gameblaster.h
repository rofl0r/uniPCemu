#ifndef GAMEBLASTER_H
#define GAMEBLASTER_H

void initGameBlaster(word baseaddr);
void doneGameBlaster();

void GameBlaster_setVolume(float volume);
void setGameBlaster_SoundBlaster(byte useSoundBlasterIO); //Sound Blaster compatible I/O?

void updateGameBlaster(double timepassed);

#endif