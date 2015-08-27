#ifndef __LOCKS_H
#define __LOCKS_H

void initLocks();
byte lock(byte id);
void unlock(byte id);

SDL_sem *getLock(byte id); //For termination of locks!

#define LOCK_VGA 0
#define LOCK_GPU 1
#define LOCK_CPU 2
#define LOCK_IPS 3
#define LOCK_8042 4
#define LOCK_SERMOUSE 5
#define LOCK_CMOS 6
#define LOCK_TIMERS 7

#endif