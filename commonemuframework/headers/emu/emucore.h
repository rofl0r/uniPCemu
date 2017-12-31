#ifndef EMUCORE_H
#define EMUCORE_H

void initEMU(int full); //Init EMU!
void doneEMU(); //Finish EMU!

//Pause/resume full emulation
void resumeEMU(byte startinput);
void pauseEMU();

void BIOSMenuResumeEMU(); //BIOS menu specific variant of resuming!

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
int DoEmulator(); //Run the emulator execution itself!

void EMU_drawBusy(byte disk); //Draw busy on-screen!
void EMU_drawRecording(byte location); //Draw recording identifier on the screen!

void updateSpeedLimit(); //Prototype!

void EMU_onCPUReset(); //Emu handling for hardware on CPU reset!

void updateEMUSingleStep(); //Update our single-step address!
#endif
