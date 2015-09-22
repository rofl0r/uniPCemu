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
//-1: Keep running: execute next instruction!
//0:Shutdown
//1:Reset emu
byte cpurun(); //Run the emulator CPU (called from main thread)!
int DoEmulator(); //Run the emulator execution itself!

#endif