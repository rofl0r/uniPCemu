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
#define LOCK_CMOS 4
#define LOCK_TIMERS 5
#define LOCK_INPUT 6
#define LOCK_SHUTDOWN 7
#define LOCK_FRAMERATE 8
//Finally MIDI locks!
#define MIDI_LOCKSTART 9

#endif