#include "headers/types.h" //Basic types!
#include "headers/emu/timers.h" //Timer support function data!
#include "headers/emu/threads.h" //Thread for timer item!
#include "headers/support/highrestimer.h" //High-resolution timer for additional precision!
#include "headers/support/log.h" //Logging support!
#include "headers/emu/gpu/gpu_text.h" //Text support!

//Are we disabled?
#define __HW_DISABLED 0

//Timers enabled?
#define TIMER_ENABLED 1
//Debug active timers?
#define TIMER_DEBUG 0
//Log running timers (error/timing search only!)
//#define TIMER_LOG
//Timer step in us! Originally 100ms now 10000?
#define TIMER_STEP 10000
//Base row/column of debugging timers!
#define DEBUG_BASE_ROW 10
#define DEBUG_BASE_COLUMN 10

//The limit in executing counters per step!
#define COUNTER_LIMIT 100

typedef struct
{
	float frequency; //The frequency!
	float overflowtime; //The time taken to overflow in us!
	Handler handler; //The handler!
	byte enabled; //Enabled?
	double counter; //Counter for handling the frequency calls!
	char name[256]; //The name of the timer!
	uint_32 calls; //Total ammount of calls so far (for debugging only!)
	uint_64 total_timetaken; //Time taken by the function!
} TIMER; //A timer's data!

TIMER timers[10]; //We use 10 timers!

int TIMER_RUNNING = 0; //Whether to run timers or pause!
int action_confirmed = 0; //Action confirmed (after stop)?

extern PSP_TEXTSURFACE *frameratesurface; //Framerate surface!

void debug_timers() //Timer debug for timer_thread!
{
	if (__HW_DISABLED) return; //Abort!
	int timer;
	int ctr = 0; //Counter for index!
	for (timer=0;timer<NUMITEMS(timers);timer++) //Process all timers!
	{
		if (timers[timer].handler) //OK?
		{
			GPU_textgotoxy(frameratesurface,DEBUG_BASE_COLUMN,ctr+DEBUG_BASE_ROW); //Goto row/column!
			GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0xFF),"#%i: \"%s\"@%i; AVG Time: %i us!",ctr,timers[timer].name,timers[timer].calls,(uint_32)(timers[timer].total_timetaken/timers[timer].calls)); //Debug info about the timer!
			//dolog("timing","#%i: \"%s\"@%i; AVG Time: %i us!",ctr,timers[timer].name,timers[timer].calls,(uint_32)(timers[timer].total_timetaken/timers[timer].calls));
			++ctr; //Next counter!
		}
	}
	if (ctr==0) //No timers?
	{
		GPU_textgotoxy(frameratesurface,DEBUG_BASE_COLUMN,DEBUG_BASE_ROW);
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0xFF),"No timers running!");
	}
}

//This handles all current used timers!
void timer_thread() //Handler for timer!
{
	if (__HW_DISABLED) return; //Abort!
	TicksHolder timer_lasttimer; //Last timer ticks holder!
	char name[256];
	bzero(name,sizeof(name)); //Init name!
	uint_32 numcounters;

	initTicksHolder(&timer_lasttimer); //Init ticks holder for precision!
	int curtimer;
	uint_64 realpassed; //Real timer passed since last call!
	while (1) //Keep running!
	{
		if (TIMER_RUNNING && !action_confirmed) //Request to run?
		{
			action_confirmed = 1; //Confirmed to run!
		}
		else if (!TIMER_RUNNING && !action_confirmed) //To stop running?
		{
			action_confirmed = 1; //Confirmed!
			return; //Stop timer!
		}
		realpassed = getuspassed(&timer_lasttimer); //How many time has passed for real!

		//if (TIMER_DEBUG) debug_timers(); //Debug the timers!
		if (TIMER_ENABLED) //Are timers enabled?
		{
			for (curtimer=0; curtimer<NUMITEMS(timers); curtimer++) //Process timers!
			{
				if (timers[curtimer].handler && timers[curtimer].frequency && timers[curtimer].enabled) //Timer set, valid and enabled?
				{
					timers[curtimer].counter += realpassed; //Increase counter using high precision timer!
					numcounters = (timers[curtimer].counter/timers[curtimer].overflowtime); //Ammount of times to count!
					if (numcounters>COUNTER_LIMIT) numcounters = COUNTER_LIMIT;
					if (numcounters) //Anything at all to process?
					{
						timers[curtimer].counter -= (numcounters*timers[curtimer].overflowtime); //Decrease counter by the executions!
						for (;;) //Overflow multi?
						{
							#ifdef TIMER_LOG
							strcpy(name,"timer_sub_"); //Root!
							strcat(name,timers[curtimer].name); //Set name!
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
		}
		delay(TIMER_STEP); //Lousy, take 100ms breaks!
	}
}

void timer_calcfreq(int timer)
{
	timers[timer].overflowtime = (1000000.0f/timers[timer].frequency); //Actual time taken to overflow!
}

void addtimer(float frequency, Handler timer, char *name)
{
	if (__HW_DISABLED) return; //Abort!
	if (frequency==0.0f)
	{
		removetimer(name); //Remove the timer if it's there!
		return; //Don't add without frequency: 0 times/sec is never!
	}
	int i;
	for (i=0; i<NUMITEMS(timers); i++) //Check for existing timer!
	{
		if (strcmp(timers[i].name,name)==0) //Found?
		{
			timers[i].handler = timer; //Edit timer if needed!
//Leave counter alone!
			timers[i].frequency = frequency; //Edit frequency!
//Timer name is already set!
			timers[i].enabled = 1; //Set to enabled by default!
			timer_calcfreq(i);
			return; //Done: we've found the timer and updated it!
		}
	}

//Now for new timers!

	for (i=0; i<NUMITEMS(timers); i++)
	{
		if (timers[i].frequency==0.0) //Not set?
		{
			timers[i].handler = timer; //Set timer!
			timers[i].counter = 0; //Reset counter!
			timers[i].frequency = frequency; //Start timer!
			strcpy(timers[i].name,name); //Timer name!
			timers[i].enabled = 1; //Set to enabled by default!
			timer_calcfreq(i);
			break;
		}
	}
}

void cleartimers() //Clear all running timers!
{
	if (__HW_DISABLED) return; //Abort!
	int i;
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
	if (__HW_DISABLED) return; //Abort!
	int i;
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
	if (__HW_DISABLED) return; //Abort!
	int i;
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
	if (!TIMER_RUNNING) //Not already running?
	{
		TIMER_RUNNING = 1; //Start timers!
		startThread(&timer_thread,"Timer",DEFAULT_PRIORITY); //Timer thread start!
	}
}

void stopTimers()
{
	if (__HW_DISABLED) return; //Abort!
	if (TIMER_RUNNING) //Running already (we can terminate it)?
	{
		action_confirmed = 0; //Init!
		TIMER_RUNNING = 0; //Stop timers command for the timer thread!
		while (!action_confirmed) //Wait to stop!
		{
			delay(1); //Wait a bit for the thread to stop!
		}
	}
}

void resetTimers() //Init/Reset all timers to off and turn off all handlers!
{
	if (__HW_DISABLED) return; //Abort!
	stopTimers(); //Stop timer thread!
	memset(&timers,0,sizeof(timers)); //Reset all!
	int i;
	for (i=0;i<NUMITEMS(timers);i++)
	{
		timers[i].handler = NULL; //Default to NULL ptr!
	}
}