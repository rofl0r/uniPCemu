#ifndef HIGHRESTIMER_H
#define HIGHRESTIMER_H

#include "headers/types.h" //Basic types etc.

typedef struct
{
u64 oldticks; //Old ticks!
u64 newticks; //New ticks!
float ticksrest; //Ticks left after ticks have been processed (ticks left after division to destination time rate (ms/us/ns))
char lockname[256]; //Full lock name!
SDL_sem *lock; //Our lock when calculating time passed!
} TicksHolder; //Info for checking differences between ticks!

#define MS_SECOND 1000
#define US_SECOND 1000000
#define NS_SECOND 1000000000

void initHighresTimer(); //Global init!

void initTicksHolder(TicksHolder *ticksholder); //Initialise ticks holder!
float getmspassed(TicksHolder *ticksholder); //Get ammount of ms passed since last use!
float getuspassed(TicksHolder *ticksholder); //Get ammount of us passed since last use!
float getnspassed(TicksHolder *ticksholder); //Get ammount of ns passed since last use!
float getmspassed_k(TicksHolder *ticksholder); //Same as getuspassed, but doesn't update the start of timing, allowing for timekeeping normally.
float getuspassed_k(TicksHolder *ticksholder); //Same as above, but keep old time data!
float getnspassed_k(TicksHolder *ticksholder); //Same as above, but keep old time data!

void convertTime(float time, char *holder); //Convert time to hh:mm:ss:s100.s1000.s1k!

void startHiresCounting(TicksHolder *ticksholder); //Start counting!
void stopHiresCounting(char *src, char *what, TicksHolder *ticksholder); //Stop counting&log!
#endif