#include "headers/types.h" //Basic types!
#include "headers/support/highrestimer.h" //Our own typedefs etc.
#include "headers/support/log.h" //Logging support!
#include "headers/support/locks.h" //Locking support!

#ifdef IS_PSP
#include <psprtc.h> //PSP Real Time Clock atm!
#endif

//Allow windows timing to be used?
#define ENABLE_WINTIMING 1
#define ENABLE_PSPTIMING 1

double tickresolution = 0.0f; //Our tick resolution, initialised!
byte tickresolution_type = 0xFF; //What kind of ticks are we using? 0=SDL, 1=gettimeofday, 2=Platform specific

float msfactor, usfactor, nsfactor; //The factors!
float msfactorrev, usfactorrev, nsfactorrev; //The factors reversed!

u64 lastticks=0; //Last ticks passed!

#ifdef IS_WINDOWS
//For cross-platform compatibility!
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;
	INLINEREGISTER uint64_t    time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	time = (((uint64_t)file_time.dwHighDateTime) << 32)|((uint64_t)file_time.dwLowDateTime);

	tp->tv_sec = (long)((time - EPOCH) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
	return 0;
}
#endif
//PSP and Linux already have proper gettimeofday support built into the compiler!

//Normal High-resolution clock support:
OPTINLINE u64 getcurrentticks() //Retrieve the current ticks!
{
	struct timeval tp;
	struct timezone currentzone;
	switch (tickresolution_type) //What type are we using?
	{
	case 0: //SDL?
		return (u64)SDL_GetTicks(); //Give the ticks passed using SDL default handling!
	case 2: //System specific?
#ifdef IS_PSP
	{
		u64 result = 0; //The result!
		if (!sceRtcGetCurrentTick(&result)) //Try to retrieve current ticks as old ticks until we get it!
		{
			return result; //Give the result!
		}
		return lastticks; //Invalid result!		
	}
#endif
#ifdef IS_WINDOWS
	{
		LARGE_INTEGER temp;
		if (QueryPerformanceCounter(&temp)==0) return lastticks; //Invalid result?
		return temp.QuadPart; //Give the result by the performance counter of windows!
	}
#endif
	case 1: //gettimeofday counter?
		if (gettimeofday(&tp,&currentzone)==0) //Time gotten?
		{
			return (tp.tv_sec*1000000)+tp.tv_usec; //Give the result!
		}
		return lastticks; //Invalid result!		
		break;
	default: //Unknown method?
		return lastticks; //Invalid result!		
		break;
	}
}

void initHighresTimer()
{
	if (tickresolution_type!=0xFF) goto resolution_ready; //Already set? Don't detect it again!
	//SDL timing by default?
	tickresolution = 1000.0f; //We have a resolution in ms as given by SDL!
	tickresolution_type = 0; //We're using SDL ticks!
	#ifdef IS_PSP
		//Try PSP timing!
		if (ENABLE_PSPTIMING)
		{
			tickresolution = sceRtcGetTickResolution(); //Get the tick resolution, as defined on the PSP!
			tickresolution_type = 2; //Don't use SDL!
		}
	#endif

	#ifdef IS_WINDOWS
		//Try Windows timing!
		LARGE_INTEGER tickresolution_win;
		if (QueryPerformanceFrequency(&tickresolution_win) && ENABLE_WINTIMING)
		{
			tickresolution = (double)tickresolution_win.QuadPart; //Apply the tick resolution!
			tickresolution_type = 2; //Don't use SDL!
		}
	#endif

	//Finally: gettimeofday provides 10us accuracy at least!
	if (tickresolution_type==0) //We're unchanged? Default to gettimeofday counter!
	{
		tickresolution = 1000000.0f; //Microsecond accuracy!
		tickresolution_type = 1; //Don't use SDL: we're the gettimeofday counter!
	}

	resolution_ready: //Ready?
	//Calculate needed precalculated factors!
	usfactor = (float)(1.0f/tickresolution)*US_SECOND; //US factor!
	nsfactor = (float)(1.0f/tickresolution)*NS_SECOND; //NS factor!
	msfactor = (float)(1.0f/tickresolution)*MS_SECOND; //MS factor!
	usfactorrev = 1.0f/usfactor; //Reverse!
	nsfactorrev = 1.0f/nsfactor; //Reverse!
	msfactorrev = 1.0f/msfactor; //Reverse!
	lastticks = getcurrentticks(); //Initialize the last tick to be something valid!
}

void initTicksHolder(TicksHolder *ticksholder)
{
	memset(ticksholder, 0, sizeof(*ticksholder)); //Clear the holder!}
	ticksholder->newticks = getcurrentticks(); //Initialize the ticks to the current time!
}

OPTINLINE float getrealtickspassed(TicksHolder *ticksholder)
{
    INLINEREGISTER u64 temp;
	INLINEREGISTER u64 currentticks = getcurrentticks(); //Fist: get current ticks to be sure we're right!
	//We're not initialising/first call?
	temp = ticksholder->newticks; //Move new ticks to old ticks! Store it for quicker reference later on!
	ticksholder->oldticks = temp; //Store the old ticks!
	ticksholder->newticks = currentticks; //Set current ticks as new ticks!
	if (currentticks<temp)//Overflown time?
	{
	    //Temp is already equal to oldticks!
	    temp -= currentticks; //Difference between the numbers(old-new=difference)!
		currentticks = (u64)~0; //Max to substract from instead of the current ticks!
		if (tickresolution_type==0) //Are we SDL ticks?
		{
			currentticks &= (u64)(((uint_32)~0)); //We're limited to the uint_32 type, so wrap around it!
		}
	}
	currentticks -= temp; //Substract the old ticks for the difference!
	return (float)currentticks; //Give the result: amount of ticks passed!
}

OPTINLINE float gettimepassed(TicksHolder *ticksholder, float secondfactor, float secondfactorreversed)
{
	INLINEREGISTER float result;
	INLINEREGISTER float tickspassed;
	tickspassed = getrealtickspassed(ticksholder); //Start with checking the current ticks!
	tickspassed += ticksholder->ticksrest; //Add the time we've left unused last time!
	result = floorf(tickspassed*secondfactor); //The ammount of ms that has passed as precise as we can use!
	tickspassed -= (result*secondfactorreversed); //The ticks left unprocessed this call!
	ticksholder->ticksrest = tickspassed; //Add the rest ticks unprocessed to the next time we're counting!
	return result; //Ordinary result!
}

float getmspassed(TicksHolder *ticksholder) //Get ammount of ms passed since last use!
{
	return gettimepassed(ticksholder, msfactor,msfactorrev); //Factor us!
}

float getuspassed(TicksHolder *ticksholder) //Get ammount of ms passed since last use!
{
	return gettimepassed(ticksholder,usfactor,usfactorrev); //Factor us!
}

float getnspassed(TicksHolder *ticksholder)
{
	return gettimepassed(ticksholder,nsfactor,nsfactorrev); //Factor ns!
}

float getmspassed_k(TicksHolder *ticksholder) //Same as getuspassed, but doesn't update the start of timing, allowing for timekeeping normally.
{
	TicksHolder temp;
	memcpy(&temp, ticksholder, sizeof(temp)); //Copy the old one!
	return gettimepassed(&temp, msfactor, msfactorrev); //Factor us!
}

float getuspassed_k(TicksHolder *ticksholder) //Same as getuspassed, but doesn't update the start of timing, allowing for timekeeping normally.
{
	TicksHolder temp;
	memcpy(&temp,ticksholder,sizeof(temp)); //Copy the old one!
	return gettimepassed(&temp, usfactor, usfactorrev); //Factor us!
}

float getnspassed_k(TicksHolder *ticksholder) //Same as getuspassed, but doesn't update the start of timing, allowing for timekeeping normally.
{
	TicksHolder temp;
	memcpy(&temp,ticksholder,sizeof(temp)); //Copy the old one!
	return gettimepassed(&temp, nsfactor, nsfactorrev); //Factor us!
}

void startHiresCounting(TicksHolder *ticksholder)
{
	getrealtickspassed(ticksholder); //Start with counting!
}

void stopHiresCounting(char *src, char *what, TicksHolder *ticksholder)
{
	char time[30]; //Some time holder!
	float passed = getuspassed(ticksholder); //Get the time that has passed!
	cleardata(&time[0],sizeof(time)); //Init holder!
	convertTime(passed,&time[0]); //Convert the time!
	dolog(src,"Counter %s took %s",what,time); //Log it!
}

void convertTime(float time, char *holder) //Convert time to hh:mm:ss:s100.s1000.s1k!
{
	uint_32 h, m, s, s100,sus;
	h = (uint_32)(time/3600000000ll); //Hours!
	time -= h*3600000000ll; //Left!
	m = (uint_32)(time/60000000ll); //Minutes!
	time -= m*60000000ll; //Left!
	s = (uint_32)(time/1000000ll); //Seconds!
	time -= s*1000000ll; //Left!
	s100 = (uint_32)(time/10000ll); //1/100th second!
	time -= (s100*10000ll); //Left!
	sus = (uint_32)time; //Microseconds left (1/1000 and ns)!
	sprintf(holder,"%02u:%02u:%02u:%02u.%05u",h,m,s,s100,sus); //Generate the final text!
}