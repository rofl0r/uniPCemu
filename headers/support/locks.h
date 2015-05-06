#ifndef __LOCKS_H
#define __LOCKS_H

void initLocks();
byte lock(char *name);
void unlock(char *name);

SDL_sem *getLock(char *name); //For termination of locks!

#endif