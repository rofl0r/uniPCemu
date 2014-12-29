#include "headers/support/highrestimer.h" //Our own typedefs etc.
#include "headers/support/log.h" //Logging support!

OPTINLINE void initTicksHolder(TicksHolder *ticksholder)
{
	byte avg;
	avg = ticksholder->avg; //Averaging!
	u64 oldpassed;
	uint_32 oldtimes;
	if (avg) //Averaging?
	{
		oldpassed = ticksholder->avg_sumpassed;
		oldtimes = ticksholder->avg_oldtimes;
	}
	memset(ticksholder,0,sizeof(*ticksholder)); //Clear the holder!
	//No ticks passed!
	//We don't have old ticks yet!
	if (avg) //Averaging? Restore our data!
	{
		ticksholder->avg_sumpassed = oldpassed; //Total passed!
		ticksholder->avg_oldtimes = oldtimes; //Times passed!
		ticksholder->avg = 1; //Average flag: keep intact!
	}
}

OPTINLINE void ticksholder_AVG(TicksHolder *ticksholder)
{
	ticksholder->avg = 1; //Enable average meter!
}

OPTINLINE u64 getcurrentticks() //Retrieve the current ticks!
{
	u64 result = 0; //The result!
	if (!sceRtcGetCurrentTick(&result)) //Try to retrieve current ticks as old ticks until we get it!
	{
		return result; //Give the result: ticks passed!
	}
	return 0; //Give the result: error!
}

OPTINLINE u64 getrealtickspassed(TicksHolder *ticksholder)
{
	u64 currentticks = getcurrentticks(); //Fist: get current ticks to be sure we're right!
	if (!ticksholder->haveoldticks) //Not initialized yet?
	{
		ticksholder->oldticks = ticksholder->newticks = currentticks; //Update old&new to current!
		ticksholder->haveoldticks = 1; //We have old ticks loaded!
		ticksholder->tickspassed = 0; //No time passed yet!
		return 0; //None passed yet!
	}
	//We're not initialising/first call?
	ticksholder->oldticks = ticksholder->newticks; //Move new ticks to old ticks!
	ticksholder->newticks = currentticks; //Get current ticks as new ticks!
	if (ticksholder->newticks>ticksholder->oldticks) //Not overflown?
	{
	    	ticksholder->tickspassed = ticksholder->newticks;
	    	ticksholder->tickspassed -= ticksholder->oldticks; //Ticks passed!
	}
	else //Overflow?
	{
	    u64 temp;
	    temp = ticksholder->oldticks;
	    temp -= ticksholder->newticks; //Difference between the numbers!
	    ticksholder->tickspassed = ~0; //Max!
	    ticksholder->tickspassed -= temp; //Substract difference from max to get ticks passed!
	}
	return ticksholder->tickspassed; //Give the result: ammount of ticks passed!
}

OPTINLINE uint_64 getmspassed(TicksHolder *ticksholder) //Get ammount of ms passed since last use!
{
	u64 tickspassed = getrealtickspassed(ticksholder); //Start with checking the current ticks!
	uint_64 result;
	result = (uint_64)(((double)tickspassed/(double)sceRtcGetTickResolution())*1000000.0f); //The ammount of ms that has passed as precise as we can!
	if (ticksholder->avg) //Average enabled?
	{
		ticksholder->avg_sumpassed += result; //Add to the sum!
		++ticksholder->avg_oldtimes; //One time more!
		return SAFEDIV(ticksholder->avg_sumpassed,ticksholder->avg_oldtimes); //Give the ammount passed averaged!
	}
	return result; //Ordinary result!
}

OPTINLINE uint_64 getmspassed_k(TicksHolder *ticksholder) //Same as getmspassed, but doesn't update the start of timing, allowing for timekeeping normally.
{
	TicksHolder temp;
	memcpy(&temp,ticksholder,sizeof(temp)); //Copy the old one!
	return getmspassed(&temp); //Give the ammount of time passed!
}

OPTINLINE void startHiresCounting(TicksHolder *ticksholder)
{
	initTicksHolder(ticksholder); //Init!
	getrealtickspassed(ticksholder); //Start with counting!
}

OPTINLINE void stopHiresCounting(char *src, char *what, TicksHolder *ticksholder)
{
	uint_64 passed = getmspassed(ticksholder); //Get the time that has passed!
	char time[30]; //Some time holder!
	bzero(time,sizeof(time)); //Init holder!
	convertTime(passed,&time[0]); //Convert the time!
	dolog(src,"Counter %s took %s",what,time); //Log it!
}

OPTINLINE void convertTime(uint_64 time, char *holder) //Convert time to hh:mm:ss:s100.s1000.s1k!
{
	uint_32 h, m, s, s100, s1000, s1k;
	h = (uint_32)(time/3600000000ll); //Hours!
	time -= h*3600000000ll; //Left!
	m = (uint_32)(time/60000000ll); //Minutes!
	time -= m*60000000ll; //Left!
	s = (uint_32)(time/1000000ll); //Seconds!
	time -= s*1000000ll; //Left!
	s100 = (uint_32)(time/10000ll); //s100!
	time -= s100*10000ll;
	s1000 = (uint_32)(time/1000ll); //s1000!
	time -= s1000*1000ll; //Left!
	s1k = time; //All that's left!
	sprintf(holder,"%i:%02i:%02i:%02i.%i.%04i",h,m,s,s100,s1000,s1k); //Generate the final text!
}