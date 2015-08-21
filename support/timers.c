#include "headers/types.h" //Basic types!
#include "headers/emu/timers.h" //Timer support function data!
#include "headers/emu/threads.h" //Thread for timer item!
#include "headers/support/highrestimer.h" //High-resolution timer for additional precision!

#include "headers/support/log.h" //Logging for debugging!
#include "headers/support/locks.h" //Locking support!

//Are we disabled?
#define __HW_DISABLED 0

//Timer step in us! Originally 100ms now 10000?
#define TIMER_STEP 0

//Log running timers (error/timing search only!)
//#define TIMER_LOG

#ifdef TIMER_LOG
#include "headers/support/log.h" //Logging support!
#endif

byte timer_ready = 0; //Ready?

TicksHolder timer_lasttimer; //Last timer ticks holder!
byte timer_init = 0;

typedef struct
{
	byte core; //Is this a core timer?
	float frequency; //The frequency!
	double overflowtime; //The time taken to overflow in us!
	Handler handler; //The handler!
	byte enabled; //Enabled?
	double counter; //Counter for handling the frequency calls!
	char name[256]; //The name of the timer!
	uint_32 calls; //Total ammount of calls so far (for debugging only!)
	double total_timetaken; //Time taken by the function!
	uint_32 counterlimit; //Limit of the ammount of counters to execute!
	SDL_sem *lock; //The sephamore to use when firing (multithreading support)!
} TIMER; //A timer's data!

TIMER timers[100]; //We use up to 100 timers!

ThreadParams_p timerthread = NULL; //Our thread!
byte allow_running = 0;

extern double clockspeed; //Default clockspeed we use, in Hz!

byte EMU_Timers_Enabled = 1; //Are emulator timers enabled?

//This handles all current used timers!
void timer_thread() //Handler for timer!
{
	char name[256];
	int curtimer;
	uint_64 numcounters;
	double clockspeedup;
	double realpassed; //Real timer passed since last call!


	for (; !lock("Timer");) delay(0); //Wait for our lock!
	allow_running = 1; //Set our thread to active!
	unlock("Timer"); //We're finished!

	if (__HW_DISABLED) return; //Abort!

	bzero(name,sizeof(name)); //Init name!

	for (;!lock("Timer");) delay(0);
	if (!timer_init) //Not initialised yet?
	{
		initTicksHolder(&timer_lasttimer); //Init ticks holder for precision!
		timer_init = 1; //Ready!
	}
	unlock("Timer");

	getuspassed(&timer_lasttimer); //Initialise the timer to current time!

	for (;;) //Keep running!
	{
		for (; !lock("Timer");) delay(0); //Wait for our lock!
		if (!allow_running)
		{
			unlock("Timer"); //We're done!
			return; //To stop running?
		}
		
		realpassed = (double)getuspassed(&timer_lasttimer); //How many time has passed for real!

		for (curtimer=0; curtimer<NUMITEMS(timers); curtimer++) //Process timers!
		{
			if (timers[curtimer].handler && timers[curtimer].frequency && timers[curtimer].enabled) //Timer set, valid and enabled?
			{
				if ((timers[curtimer].core&1) || ((!(timers[curtimer].core&1)) && EMU_Timers_Enabled)) //Allowed to run?
				{
					timers[curtimer].counter += realpassed; //Increase counter using high precision timer!
					numcounters = floor(timers[curtimer].counter / timers[curtimer].overflowtime); //Ammount of times to count!
					timers[curtimer].counter -= (numcounters*timers[curtimer].overflowtime); //Decrease counter by the executions! We skip any overflow!
					if (timers[curtimer].counterlimit) //Gotten a limit?
					{
						if (numcounters>timers[curtimer].counterlimit)
						{
							numcounters = timers[curtimer].counterlimit;
						}
					}
					if (numcounters) //Are we to fire?
					{
						if (timers[curtimer].lock) //To wait for using threads?
						{
							//Lock
							WaitSem(timers[curtimer].lock)
						}
						if (!(timers[curtimer].core&2)) //Not counter only timer?
						{
							for (;;) //Overflow multi?
							{
#ifdef TIMER_LOG
								strcpy(name,timers[curtimer].name); //Set name!
								dolog("emu","firing timer: %s",timers[curtimer].name); //Log our timer firing!
								TicksHolder singletimer;
								startHiresCounting(&singletimer); //Start counting!
#endif
								if (timers[curtimer].handler) //Gotten a handler?
								{
									timers[curtimer].handler(); //Run the handler!
								}
#ifdef TIMER_LOG
								++timers[curtimer].calls; //For debugging the ammount of calls!
								timers[curtimer].total_timetaken += getuspassed(&singletimer); //Add the time that has passed for this timer!
								dolog("emu","returning timer: %s",timers[curtimer].name); //Log our timer return!
#endif
								if (!--numcounters) break; //Done? Process next counter!
							}
						}
						else //We're a counter only?
						{
							uint_64 *counter;
							counter = (uint_64 *)timers[curtimer].handler; //Handler is a counter!
							if (counter && numcounters!=0.0f) //Loaded?
							{
								*counter += numcounters; //Add the counter!
							}
						}
						if (timers[curtimer].lock) //To wait for using threads?
						{
							//Unlock
							PostSem(timers[curtimer].lock)
						}
					}
				}
			}
		}
		unlock("Timer"); //Release our lock!
		delay(TIMER_STEP); //Lousy, take 100ms breaks!
	}
}

void timer_calcfreq(int timer)
{
	if (timers[timer].frequency!=0.0f)
	{
		timers[timer].overflowtime = US_SECOND;
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

void addtimer(float frequency, Handler timer, char *name, uint_32 counterlimit, byte coretimer, SDL_sem *uselock)
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
		if (!strcmp(timers[i].name,name)) //Found?
		{
			timerpos = i; //Use this position!
			break; //Quit our search!
		}
	}

//Now for new timers!
	if (timerpos==-1) //New timer?
	{
		i = 0; //Reset current!
		for (; i<NUMITEMS(timers);)
		{
			if (!timers[i].frequency) //Not set?
			{
				timerpos = i; //Use this position!
				break; //Quit our search!
			}
			++i; //Next timer!
		}
	}
	
	if (timerpos!=-1) //Found a position to add?
	{
		for (; !lock("Timers");) delay(0);
		timers[timerpos].handler = timer; //Set timer!
		timers[timerpos].counter = 0; //Reset counter!
		timers[timerpos].frequency = frequency; //Start timer!
		timers[timerpos].counterlimit = counterlimit; //The counter limit!
		memset(timers[timerpos].name,0,sizeof(timers[timerpos].name)); //Init name!
		strcpy(timers[timerpos].name,name); //Timer name!
		timers[timerpos].core = coretimer; //Are we a core timer?
		timers[timerpos].enabled = 1; //Set to enabled by default!
		timers[timerpos].lock = uselock; //The sephamore to use, if any!
		timer_calcfreq(timerpos);
		unlock("Timers"); //Allow running again!
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
				for (; !lock("Timers");) delay(0);
				timers[i].enabled = use; //To use it?
				unlock("Timers");
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
				for (; !lock("Timers");) delay(0);
				memset(&timers[i],0,sizeof(timers[i])); //Disable!
				unlock("Timers");
				break;
			}
		}
	}
}

void startTimers(byte core)
{
	if (__HW_DISABLED) return; //Abort!
	if (core) //Core?
	{
		if (!timerthread) //Not already running?
		{
			timerthread = startThread(&timer_thread, "X86EMU_Timing", NULL, DEFAULT_PRIORITY); //Timer thread start!
		}

	}
	EMU_Timers_Enabled = 1; //Enable timers!
}

void stopTimers(byte core)
{
	if (__HW_DISABLED) return; //Abort!
	if (core) //Are we the core?
	{
		for (;!lock("Timer");) delay(0);
		if (timerthread) //Running already (we can terminate it)?
		{
			unlock("Timer");
			for (; !lock("Timer");) delay(0); //Lock our thread!
			allow_running = 0; //Request normal termination!
			timerthread = NULL; //Finished!
			unlock("Timer"); //We're done!
			waitThreadEnd(timerthread); //Wait for our thread to end!
			for (;!lock("Timer");) delay(0);
		}
		unlock("Timer"); //Finished!
	}
	EMU_Timers_Enabled = 0; //Enable timers!
}

void resetTimers() //Init/Reset all timers to go off and turn off all handlers!
{
	if (__HW_DISABLED) return; //Abort!
	stopTimers(0); //Stop normal timers!
	int i;
	for (i = 0; i < NUMITEMS(timers); i++)
	{
		if (!timers[i].core) //Not a core timer?
		{
			memset(&timers[i], 0, sizeof(timers[i])); //Delete the timer that's not a core timer!
		}
	}
}