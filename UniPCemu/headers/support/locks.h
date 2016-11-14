#ifndef __LOCKS_H
#define __LOCKS_H

void initLocks();
byte lock(byte id);
void unlock(byte id);

SDL_sem *getLock(byte id); //For termination of locks!

#define LOCK_VGA 0
#define LOCK_GPU 1
#define LOCK_CPU 2
#define LOCK_VIDEO 3
#define LOCK_TIMERS 4
#define LOCK_INPUT 5
#define LOCK_SHUTDOWN 6
#define LOCK_FRAMERATE 7
//Finally MIDI locks!
#define LOCK_MAINTHREAD 8
#define LOCK_SOUND 9
//#define MIDI_LOCKSTART 10

#endif