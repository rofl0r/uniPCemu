#ifndef EMUCORE_H
#define EMUCORE_H

void initEMU(int full); //Init EMU!
void doneEMU(); //Finish EMU!

//Pause/resume full emulation
void resumeEMU();
void pauseEMU();

void initEMUreset(); //Simple reset emulator!

//Timers start/stop!
void stopEMUTimers();
void startEMUTimers();

//Input control
void EMU_stopInput();
void EMU_startInput();


//DoEmulator results:
//0:Shutdown
//1:Reset emu
int DoEmulator(); //Run the emulator!

#endif