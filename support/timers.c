#include "headers/types.h" //Basic types!
#include "headers/emu/timers.h" //Timer support function data!
#include "headers/emu/threads.h" //Thread for timer item!
#include "headers/support/highrestimer.h" //High-resolution timer for additional precision!

//Are we disabled?
#define __HW_DISABLED 0

//Timer step in us! Originally 100ms now 10000?
#define TIMER_STEP 1

//Log running timers (error/timing search only!)
//#define TIMER_LOG

#ifdef TIMER_LOG
#include "headers/support/log.h" //Logging support!
#endif

typedef struct
{
	float frequency; //The frequency!
	double overflowtime; //The time taken to overflow in us!
	Handler handler; //The handler!
	byte enabled; //Enabled?
	double counter; //Counter for handling the frequency calls!
	char name[256]; //The name of the timer!
	uint_32 calls; //Total ammount of calls so far (for debugging only!)
	double total_timetaken; //Time taken by the function!
	uint_32 counterlimit; //Limit of the ammount of counters to execute!
} TIMER; //A timer's data!

TIMER timers[100]; //We use up to 100 timers!

ThreadParams_p timerthread = NULL; //Our thread!

extern double clockspeed; //Default clockspeed we use, in Hz!

//This handles all current used timers!
void timer_thread() //Handler for timer!
{
	TicksHolder timer_lasttimer; //Last timer ticks holder!
	char name[256];
	int curtimer;
	uint_32 numcounters;
	double clockspeedup;
	double realpassed; //Real timer passed since last call!

	if (__HW_DISABLED) return; //Abort!

	bzero(name,sizeof(name)); //Init name!

	initTicksHolder(&timer_lasttimer); //Init ticks holder for precision!
	for (;;) //Keep running!
	{
		if (!timerthread) return; //To stop running?
		
		//Calculate speedup needed for our timing!
		clockspeedup = 1.0f;
		clockspeedup /= clockspeed; //Take the percentage of 1.0f for clockspeed (time in seconds for the default clock speed)
		clockspeedup *= getCurrentClockSpeed(); //Multiply with current clock speed for the speedup factor!

		realpassed = (double)getuspassed(&timer_lasttimer); //How many time has passed for real!
		realpassed *= clockspeedup; //Speed up the clock as much as needed, according to the actual CPU speed!

		for (curtimer=0; curtimer<NUMITEMS(timers); curtimer++) //Process timers!
		{
			if (timers[curtimer].handler && timers[curtimer].frequency && timers[curtimer].enabled) //Timer set, valid and enabled?
			{
				timers[curtimer].counter += realpassed; //Increase counter using high precision timer!
				numcounters = (uint_32)(timers[curtimer].counter/timers[curtimer].overflowtime); //Ammount of times to count!
				if (numcounters>timers[curtimer].counterlimit) numcounters = timers[curtimer].counterlimit;
				if (numcounters) //Are we to fire?
				{
					timers[curtimer].counter -= (numcounters*timers[curtimer].overflowtime); //Decrease counter by the executions!
					for (;;) //Overflow multi?
					{
						#ifdef TIMER_LOG
						strcpy(name,timers[curtimer].name); //Set name!
						dolog("emu","firing timer: %s",timers[curtimer].name); //Log our timer firing!
						TicksHolder singletimer;
						startHiresCounting(&singletimer); //Start counting!
						#endif
						timers[curtimer].handler(); //Run the handler!
						#ifdef TIMER_LOG
						++timers[curtimer].calls; //For debugging the ammount of calls!
						timers[curtimer].total_timetaken += getuspassed(&singletimer); //Add the time that has passed for this timer!
						dolog("emu","returning timer: %s",timers[curtimer].name); //Log our timer return!
						#endif
						if (!--numcounters) break; //Done? Process next counter!
					}
				}
			}
		}
		delay(TIMER_STEP); //Lousy, take 100ms breaks!
	}
}

void timer_calcfreq(int timer)
{
	if (timers[timer].frequency!=0.0f)
	{
		timers[timer].overflowtime = 1000000.0f;
		timers[timer].overflowtime /= timers[timer].frequency; //Actual time taken to overflow!
		if (!timers[timer].overflowtime)
		{
			goto defaulttime;
		}
	}
	else
	{
		defaulttime: //Apply default time!
		timers[timer].overflowtime = 1.0f; //minimal interval?
	}
}

void addtimer(float frequency, Handler timer, char *name, uint_32 counterlimit)
{
	int i;
	int timerpos = -1; //Timer position to use!
	if (__HW_DISABLED) return; //Abort!
	if (frequency==0.0f)
	{
		removetimer(name); //Remove the timer if it's there!
		return; //Don't add without frequency: 0 times/sec is never!
	}
	for (i=0; i<NUMITEMS(timers); i++) //Check for existing timer!
	{
		if (timers[i].name==name) //Found?
		{
			timerpos = i; //Use this position!
			break; //Quit our search!
		}
	}

//Now for new timers!
	if (timerpos==-1) //New timer?
	{
		for (i=0; i<NUMITEMS(timers); i++)
		{
			if (!timers[i].frequency) //Not set?
			{
				timerpos = i; //Use this position!
				break; //Quit our search!
			}
		}
	}
	
	if (timerpos!=-1) //Found a position to add?
	{
		timers[timerpos].handler = timer; //Set timer!
		timers[timerpos].counter = 0; //Reset counter!
		timers[timerpos].frequency = frequency; //Start timer!
		timers[timerpos].counterlimit = counterlimit; //The counter limit!
		memset(timers[timerpos].name,0,sizeof(timers[timerpos].name)); //Init name!
		strcpy(timers[timerpos].name,name); //Timer name!
		timers[timerpos].enabled = 1; //Set to enabled by default!
		timer_calcfreq(timerpos);
		return; //Finished: we're added!
	}
}

void cleartimers() //Clear all running timers!
{
	int i;
	if (__HW_DISABLED) return; //Abort!
	for (i=0; i<NUMITEMS(timers); i++)
	{
		if (timers[i].frequency!=0.0) //Set?
		{
			if (strcmp(timers[i].name,"")!=0) //Set and has a name?
			{
				removetimer(timers[i].name); //Remove!
			}
		}
	}
}

void useTimer(char *name, byte use)
{
	int i;
	if (__HW_DISABLED) return; //Abort!
	for (i=0; i<NUMITEMS(timers); i++)
	{
		if (timers[i].frequency!=0.0) //Set?
		{
			if (strcmp(timers[i].name,name)==0) //Found?
			{
				timers[i].enabled = use; //To use it?
				return; //Don't search any further: we've found our timer!
			}
		}
	}
	//We only get here when the timer isn't found. Do nothing in this case!
}

void removetimer(char *name) //Removes a timer!
{
	int i;
	if (__HW_DISABLED) return; //Abort!
	for (i=0; i<NUMITEMS(timers); i++)
	{
		if (timers[i].frequency!=0.0) //Enabled?
		{
			if (strcmp(timers[i].name,name)==0) //Timer enabled and selected?
			{
				memset(&timers[i],0,sizeof(timers[i])); //Disable!
				break;
			}
		}
	}
}

void startTimers()
{
	if (__HW_DISABLED) return; //Abort!
	if (!timerthread) //Not already running?
	{
		timerthread = startThread(&timer_thread,"X86EMU_Timing",DEFAULT_PRIORITY); //Timer thread start!
	}
}

void stopTimers()
{
	if (__HW_DISABLED) return; //Abort!
	if (timerthread) //Running already (we can terminate it)?
	{
		ThreadParams_p timerthread_backup = timerthread; //Our thread!
		timerthread = NULL; //Request normal termination!
		waitThreadEnd(timerthread_backup); //Wait for our thread to end!
	}
}

void resetTimers() //Init/Reset all timers to go off and turn off all handlers!
{
	if (__HW_DISABLED) return; //Abort!
	stopTimers(); //Stop timer thread!
	memset(&timers,0,sizeof(timers)); //Reset all!
}