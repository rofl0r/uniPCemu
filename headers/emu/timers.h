#ifndef TIMERS_H
#define TIMERS_H

void tickTimers(); //Handler for timers!

void resetTimers(); //Reset all timers to off and turn off handler!
void addtimer(float frequency, Handler timer, char *name, uint_32 counterlimit, byte coretimer, SDL_sem *uselock);
void useTimer(char *name, byte use); //To use the timer (is the timer active?)
void cleartimers(); //Clear all running timers!
void removetimer(char *name); //Removes a timer!
void startTimers(byte core);
void stopTimers(byte core);

#endif