#ifndef TIMERS_H
#define TIMERS_H

//CPU\timers.c
void Timer_Tick(); //Timer tick Irq (18.2 times/sec)

//support\timers.c

void timer_thread(); //Handler for timer!

void resetTimers(); //Reset all timers to off and turn off handler!
void addtimer(float frequency, Handler timer, char *name, uint_32 counterlimit);
void useTimer(char *name, byte use); //To use the timer (is the timer active?)
void cleartimers(); //Clear all running timers!
void removetimer(char *name); //Removes a timer!
void startTimers();
void stopTimers();

#endif