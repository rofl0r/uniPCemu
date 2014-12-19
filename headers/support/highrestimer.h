#ifndef HIGHRESTIMER_H
#define HIGHRESTIMER_H

#include "headers/types.h" //Basic types etc.

typedef struct
{
u64 oldticks; //Old ticks!
u64 newticks; //New ticks!
byte haveoldticks; //Have old ticks?
u64 tickspassed; //Ticks passed between the two measures (0 at the first call)!

//Average support!
byte avg; //Are we averaging?
u64 avg_sumpassed; //Total sum passed of getmspassed!
uint_32 avg_oldtimes; //Total times of avg_sumpassed!
} TicksHolder; //Info for checking differences between ticks!

OPTINLINE void initTicksHolder(TicksHolder *ticksholder); //Initialise ticks holder!
OPTINLINE uint_64 getmspassed(TicksHolder *ticksholder); //Get ammount of ms passed since last use!
OPTINLINE uint_64 getmspassed_k(TicksHolder *ticksholder); //Same as above, but keep old time data!

OPTINLINE void convertTime(uint_64 time, char *holder); //Convert time to hh:mm:ss:s100.s1000.s1k!

OPTINLINE void startHiresCounting(TicksHolder *ticksholder); //Start counting!
OPTINLINE void stopHiresCounting(char *src, char *what, TicksHolder *ticksholder); //Stop counting&log!

OPTINLINE u64 getcurrentticks(); //Retrieve the current ticks (can be used as seed for random functions)!

OPTINLINE void ticksholder_AVG(TicksHolder *ticksholder); //Enable averaging mspassed results!
#endif