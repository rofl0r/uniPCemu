#ifndef __LOCKS_H
#define __LOCKS_H

void initLocks();
byte lock(byte id);
void unlock(byte id);

SDL_sem *getLock(byte id); //For termination of locks!

#define LOCK_GPU 0
#define LOCK_VIDEO 1
#define LOCK_CPU 2
#define LOCK_TIMERS 3
#define LOCK_INPUT 4
#define LOCK_SHUTDOWN 5
#define LOCK_FRAMERATE 6
#define LOCK_MAINTHREAD 7
#define LOCK_SOUND 8
//Finally MIDI locks, when enabled!
//#define MIDI_LOCKSTART 10

#endif