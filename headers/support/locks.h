#ifndef __LOCKS_H
#define __LOCKS_H

void initLocks();
int lock(char *name);
void unlock(char *name);

#endif;