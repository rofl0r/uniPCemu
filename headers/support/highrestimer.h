#ifndef HIGHRESTIMER_H
#define HIGHRESTIMER_H

#include "headers/types.h" //Basic types etc.

typedef struct
{
u64 oldticks; //Old ticks!
u64 newticks; //New ticks!
u64 avg_sumpassed; //Total sum passed of get*spassed!
float ticksrest; //Ticks left after ticks have been processed (ticks left after division to destination time rate (ms/us/ns))
uint_32 avg_oldtimes; //Total times of avg_sumpassed!
char lockname[256]; //Full lock name!
byte avg; //Are we averaging?
SDL_sem *lock; //Our lock when calculating time passed!
} TicksHolder; //Info for checking differences between ticks!

#define MS_SECOND 1000
#define US_SECOND 1000000
#define NS_SECOND 1000000000

void initHighresTimer(); //Global init!

void initTicksHolder(TicksHolder *ticksholder); //Initialise ticks holder!
uint_64 getmspassed(TicksHolder *ticksholder); //Get ammount of ms passed since last use!
uint_64 getuspassed(TicksHolder *ticksholder); //Get ammount of us passed since last use!
uint_64 getnspassed(TicksHolder *ticksholder); //Get ammount of ns passed since last use!
uint_64 getmspassed_k(TicksHolder *ticksholder); //Same as getuspassed, but doesn't update the start of timing, allowing for timekeeping normally.
uint_64 getuspassed_k(TicksHolder *ticksholder); //Same as above, but keep old time data!
uint_64 getnspassed_k(TicksHolder *ticksholder); //Same as above, but keep old time data!

void convertTime(uint_64 time, char *holder); //Convert time to hh:mm:ss:s100.s1000.s1k!

void startHiresCounting(TicksHolder *ticksholder); //Start counting!
void stopHiresCounting(char *src, char *what, TicksHolder *ticksholder); //Stop counting&log!

u64 getcurrentticks(); //Retrieve the current ticks (can be used as seed for random functions)!

void ticksholder_AVG(TicksHolder *ticksholder); //Enable averaging mspassed results!
#endif