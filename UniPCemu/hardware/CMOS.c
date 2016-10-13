#include "headers/types.h" //Basic type support!
#include "headers/hardware/pic.h" //PIC!
#include "headers/emu/timers.h" //Timing!
#include "headers/support/log.h" //Logging support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/support/locks.h" //Locking support!
#include "headers/hardware/ports.h" //Port support!

//For time support!
#ifdef IS_PSP
#include <psprtc.h> //PSP Real Time Clock atm!
#endif

#include <time.h>

//Biggest Signed Integer value available!
#define BIGGESTSINT int_64

/*

CMOS&RTC (Combined!)

*/

//Are we disabled?
#define __HW_DISABLED 0

byte XTMode = 0;

OPTINLINE word decodeBCD(word bcd)
{
	INLINEREGISTER word temp, result=0;
	temp = bcd; //Load the BCD value!
	result += (temp&0xF); //Factor 1!
	temp >>= 4;
	result += (temp&0xF)*10; //Factor 10!
	temp >>= 4;
	result += (temp&0xF)*100; //Factor 100!
	temp >>= 4;
	result += (temp&0xF)*1000; //Factor 1000!
	return result; //Give the decoded integer value!
}

OPTINLINE word encodeBCD(word value)
{
	INLINEREGISTER word temp,result=0;
	temp = value; //Load the original value!
	temp %= 10000; //Wrap around!
	result |= (0x1000*(temp/1000)); //Factor 1000!
	temp %= 1000;
	result |= (0x0100*(temp/100)); //Factor 100
	temp %= 100;
	result |= (0x0010*(temp/10)); //Factor 10!
	temp %= 10;
	result |= temp; //Factor 1!
	return result;
}

OPTINLINE byte encodeBCD8(byte value)
{
	return (encodeBCD(value)&0xFF);
}

OPTINLINE byte decodeBCD8(byte value)
{
	return (decodeBCD(value)&0xFF);
}

struct
{
	CMOSDATA DATA;
	byte Loaded; //CMOS loaded?
	byte ADDR; //Internal address in CMOS (7 bits used, 8th bit set=NMI Disable)

	uint_32 RateDivider; //Rate divider, usually set to 1024Hz. Used for Square Wave output and Periodic Interrupt!
	uint_32 currentRate; //The current rate divider outputs(22-bits)!

	byte SquareWave; //Square Wave Output!
	byte UpdatingInterruptSquareWave; //Updating interrupt square wave generation!
} CMOS;

extern byte NMI; //NMI interrupt enabled?

extern BIOS_Settings_TYPE BIOS_Settings; //The BIOS settings loaded!

#define FLOPPY_NONE 0
#define FLOPPY_360 1
#define FLOPPY_12 2
#define FLOPPY_720 3
#define FLOPPY_144 4
#define FLOPPY_288 5

OPTINLINE void loadCMOSDefaults()
{
	memset(&CMOS.DATA,0,sizeof(CMOS.DATA)); //Clear/init CMOS!
	CMOS.DATA.timedivergeance = 0; //No second divergeance!
	CMOS.DATA.timedivergeance2 = 0; //No us divergeance!
	//We don't affect loaded: we're not loaded and invalid by default!
}

OPTINLINE void RTC_raiseIRQ()
{
	raiseirq(8); //We're the cause of the interrupt!
}

OPTINLINE void RTC_PeriodicInterrupt() //Periodic Interrupt!
{
	if (CMOS.DATA.DATA80.data[0x0B]&0x40) //Enabled interrupt?
	{
		if ((CMOS.DATA.DATA80.data[0xC] & 0x40) == 0) //Allowed to raise?
		{
			RTC_raiseIRQ(); //Raise the IRQ!
		}
	}
	CMOS.DATA.DATA80.data[0x0C] |= 0x40; //Periodic Interrupt flag is always set!
}

OPTINLINE void RTC_UpdateEndedInterrupt() //Update Ended Interrupt!
{
	if (CMOS.DATA.DATA80.data[0x0B]&0x10) //Enabled interrupt?
	{
		if ((CMOS.DATA.DATA80.data[0xC] & 0x10) == 0) //Allowed to raise?
		{
			RTC_raiseIRQ(); //Raise the IRQ!
		}
	}
	CMOS.DATA.DATA80.data[0x0C] |= 0x10; //Update Ended Interrupt flag!
}

OPTINLINE void RTC_AlarmInterrupt() //Alarm handler!
{
	if (CMOS.DATA.DATA80.data[0x0B]&0x20) //Enabled interrupt?
	{
		if ((CMOS.DATA.DATA80.data[0xC] & 0x20) == 0) //Allowed to raise?
		{
			RTC_raiseIRQ(); //Raise the IRQ!
		}
	}
	CMOS.DATA.DATA80.data[0x0C] |= 0x20; //Alarm Interrupt flag!
}

OPTINLINE void RTC_Handler(byte lastsecond) //Handle RTC Timer Tick!
{
	uint_32 oldrate, bitstoggled=0; //Old output!
	oldrate = CMOS.currentRate; //Save the old output for comparision!
	++CMOS.currentRate; //Increase the input divider to the next stage(22-bit divider at 64kHz(32kHz square wave))!
	bitstoggled = CMOS.currentRate^oldrate; //What bits have been toggled!

	if (CMOS.DATA.DATA80.info.STATUSREGISTERB.EnablePeriodicInterrupt) //Enabled?
	{
		if (bitstoggled&(CMOS.RateDivider<<1)) //Overflow on Rate(divided by 2 for our rate, since it's square wave signal converted to Hz)?
		{
			RTC_PeriodicInterrupt(); //Handle!
		}
	}

	if (CMOS.DATA.DATA80.info.STATUSREGISTERB.EnableSquareWaveOutput) //Square Wave generator enabled?
	{
		if (bitstoggled&CMOS.RateDivider) //Overflow on Rate? We're generating a square wave at the specified frequency!
		{
			CMOS.SquareWave ^= 1; //Toggle the square wave!
			//It's unknown what the Square Wave output is connected to, if it's connected at all?
		}
	}

	if (CMOS.DATA.DATA80.info.STATUSREGISTERB.EnabledUpdateEndedInterrupt && (CMOS.DATA.DATA80.info.STATUSREGISTERB.EnableCycleUpdate == 0) && (CMOS.UpdatingInterruptSquareWave == 0)) //Enabled and updated?
	{
		if (CMOS.DATA.DATA80.info.RTC_Seconds != lastsecond) //We're updated at all?
		{
			RTC_UpdateEndedInterrupt(0); //Handle!
		}
	}

	if (
			((CMOS.DATA.DATA80.info.RTC_Hours==CMOS.DATA.DATA80.info.RTC_HourAlarm) || ((CMOS.DATA.DATA80.info.RTC_HourAlarm&0xC0)==0xC0)) && //Hour set or ignored?
			((CMOS.DATA.DATA80.info.RTC_Minutes==CMOS.DATA.DATA80.info.RTC_MinuteAlarm) || ((CMOS.DATA.DATA80.info.RTC_MinuteAlarm & 0xC0) == 0xC0)) && //Minute set or ignored?
			((CMOS.DATA.DATA80.info.RTC_Seconds==CMOS.DATA.DATA80.info.RTC_SecondAlarm) || ((CMOS.DATA.DATA80.info.RTC_SecondAlarm & 0xC0) == 0xC0)) && //Second set or ignored?
			(CMOS.DATA.DATA80.info.RTC_Seconds!=lastsecond) && //Second changed and check for alarm?
			(CMOS.DATA.DATA80.info.STATUSREGISTERB.EnableAlarmInterrupt)) //Alarm enabled?
	{
		RTC_AlarmInterrupt(); //Handle the alarm!
	}
}

#ifdef IS_WINDOWS
struct timezone {
	int tz_minuteswest;     /* minutes west of Greenwich */
	int tz_dsttime;         /* type of DST correction */
};

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

//Our accurate time support:

typedef struct
{
	uint_64 year;
	byte month;
	byte day;
	byte hour;
	byte minute;
	byte second;
	byte s100; //100th seconds(use either this or microseconds, since they both give the same time, only this one is rounded down!)
	byte s10000; //10000th seconds!
	uint_64 us; //Microseconds?
	byte dst;
	byte weekday;
} accuratetime;

//Our accuratetime epoch support!

//Epoch time values for supported OS!
#define EPOCH_YR 1970
#define SECS_DAY (3600*24)
#define YEAR0 0
//Is this a leap year?
#define LEAPYEAR(year) ( (year % 4 == 0 && year % 100 != 0) || ( year % 400 == 0))
//What is the size of this year in days?
#define YEARSIZE(year) (LEAPYEAR(year)?366:365)

byte _ytab[2][12] = { //Days within months!
	{ 31,28,31,30,31,30,31,31,30,31,30,31 }, //Normal year
	{ 31,29,31,30,31,30,31,31,30,31,30,31 } //Leap year
};

OPTINLINE byte epochtoaccuratetime(struct timeval *curtime, accuratetime *datetime)
{
	//More accurate timing than default!
	datetime->us = curtime->tv_usec;
	datetime->s100 = (byte)(curtime->tv_usec/10000); //10000us=1/100 second!
	datetime->s10000 = (byte)((curtime->tv_usec%10000)/100); //100us=1/10000th second!

	//Further is directly taken from the http://stackoverflow.com/questions/1692184/converting-epoch-time-to-real-date-time gmtime source code.
	register unsigned long dayclock, dayno;
	int year = EPOCH_YR;

	dayclock = (unsigned long)curtime->tv_sec % SECS_DAY;
	dayno = (unsigned long)curtime->tv_sec / SECS_DAY;

	datetime->second = dayclock % 60;
	datetime->minute = (byte)((dayclock % 3600) / 60);
	datetime->hour = (byte)(dayclock / 3600);
	datetime->weekday = (dayno + 4) % 7;       /* day 0 was a thursday */
	for (;dayno >= (unsigned long)YEARSIZE(year);)
	{
		dayno -= YEARSIZE(year);
		year++;
	}
	datetime->year = year - YEAR0;
	datetime->day = (byte)dayno;
	datetime->month = 0;
	while (dayno >= _ytab[LEAPYEAR(year)][datetime->month]) {
		dayno -= _ytab[LEAPYEAR(year)][datetime->month];
		++datetime->month;
	}
	++datetime->month; //We're one month further(months start at one, not zero)!
	datetime->day = (byte)(dayno + 1);
	datetime->dst = 0;

	return 1; //Always successfully converted!
}

//Sizes of minutes, hours and days in Epoch time units.
#define MINUTESIZE 60
#define HOURSIZE 3600
#define DAYSIZE (3600*24)

OPTINLINE byte accuratetimetoepoch(accuratetime *curtime, struct timeval *datetime)
{
	uint_64 seconds=0;
	if ((curtime->us-(curtime->us%100))!=(((curtime->s100)*10000)+(curtime->s10000*100))) return 0; //Invalid time to convert: 100th&10000th seconds doesn't match us(this is supposed to be the same!)
	if (curtime->year<1970) return 0; //Before 1970 isn't supported!
	datetime->tv_usec = (uint_32)curtime->us; //Save the microseconds directly!
	uint_64 year;
	byte counter;
	byte leapyear;
	for (year=curtime->year;year>1970;) //Process the years!
	{
		--year; //The previous year has passed!
		seconds += YEARSIZE(year)*DAYSIZE; //Add the year that has passed!
	}
	leapyear = LEAPYEAR(curtime->year); //Are we a leap year?
	//Now, only months etc. are left!
	for (counter = curtime->month;counter>1;) //Process the months!
	{
		seconds += _ytab[leapyear][--counter]*DAYSIZE; //Add a month that has passed!
	}
	//Now only days, hours, minutes and seconds are left!
	seconds += DAYSIZE*(curtime->day?(curtime->day-1):0); //Days start at 1!
	seconds += HOURSIZE*curtime->hour;
	seconds += MINUTESIZE*curtime->minute;
	seconds += curtime->second;

	datetime->tv_sec = (uint_32)seconds; //The amount of seconds!
	return 1; //Successfully converted!
}

//CMOS time encoding support!
OPTINLINE void CMOS_decodetime(accuratetime *curtime) //Decode time into the current time!
{
	curtime->year = decodeBCD8(CMOS.DATA.DATA80.info.RTC_Year); //The year to compare to!
	curtime->year += decodeBCD8(CMOS.DATA.DATA80.data[0x32])*100; //Add the century!
	curtime->month = decodeBCD8(CMOS.DATA.DATA80.info.RTC_Month); //The month to compare to!
	curtime->day = decodeBCD8(CMOS.DATA.DATA80.info.RTC_DateOfMonth); //The day to compare to!
	curtime->hour = decodeBCD8(CMOS.DATA.DATA80.info.RTC_Hours); //H
	curtime->minute = decodeBCD8(CMOS.DATA.DATA80.info.RTC_Minutes); //M
	curtime->second = decodeBCD8(CMOS.DATA.DATA80.info.RTC_Seconds); //S
	curtime->weekday = decodeBCD8(CMOS.DATA.DATA80.info.RTC_DayOfWeek); //Day of week!
	curtime->s100 = decodeBCD8(CMOS.DATA.s100); //The 100th seconds!
	curtime->s10000 = decodeBCD8(CMOS.DATA.s10000); //The 10000th seconds!
	curtime->us = (curtime->s100*10000)+(curtime->s10000*100); //The same as above, make sure we match!
}

OPTINLINE void CMOS_encodetime(accuratetime *curtime) //Encode time into the current time!
{
	CMOS.DATA.DATA80.data[0x32] = encodeBCD8((curtime->year/100)%100); //The century!
	CMOS.DATA.DATA80.info.RTC_Year = encodeBCD8(curtime->year%100);
	CMOS.DATA.DATA80.info.RTC_Month = encodeBCD8(curtime->month);
	CMOS.DATA.DATA80.info.RTC_DateOfMonth = encodeBCD8(curtime->day);

	CMOS.DATA.DATA80.info.RTC_Hours = encodeBCD8(curtime->hour);
	CMOS.DATA.DATA80.info.RTC_Minutes = encodeBCD8(curtime->minute);
	CMOS.DATA.DATA80.info.RTC_Seconds = encodeBCD8(curtime->second);
	CMOS.DATA.DATA80.info.RTC_DayOfWeek = encodeBCD8(curtime->weekday); //The day of the week!
	CMOS.DATA.s100 = encodeBCD8(curtime->s100); //The 100th seconds!
	CMOS.DATA.s10000 = encodeBCD8(curtime->s10000); //The 10000th seconds!
}

//Divergeance support!
OPTINLINE byte calcDivergeance(accuratetime *time1, accuratetime *time2, int_64 *divergeance_sec, int_64 *divergeance_usec) //Calculates the difference of time1 compared to time2(reference time)!
{
	struct timeval time1val, time2val; //Our time values!
	if (accuratetimetoepoch(time1, &time1val)) //Converted to universal value?
	{
		if (accuratetimetoepoch(time2, &time2val)) //Converted to universal value?
		{
			BIGGESTSINT applyingtime; //Biggest integer value we have!
			applyingtime = ((((time1val.tv_sec * 1000000) + time1val.tv_usec) - ((time2val.tv_sec * 1000000) + time2val.tv_usec))); //Difference in usec!
			*divergeance_sec = applyingtime/1000000; //Seconds!
			*divergeance_usec = applyingtime%1000000; //Microseconds!
			return 1; //Give the difference time!
		}
	}
	return 0; //Unknown: Don't apply divergeance!
}

OPTINLINE byte applyDivergeance(accuratetime *curtime, int_64 divergeance_sec, int_64 divergeance_usec) //Apply divergeance to accurate time!
{
	struct timeval timeval; //The accurate time value!
	BIGGESTSINT applyingtime; //Biggest integer value we have!
	if (accuratetimetoepoch(curtime, &timeval)) //Converted to epoch?
	{
		applyingtime = ((timeval.tv_sec * 1000000) + timeval.tv_usec); //Direct time conversion!
		applyingtime += (divergeance_sec*1000000); //Add the divergeance: we're applying the destination time!
		applyingtime += divergeance_usec; //Apply usec!

		//Apply the resulting time!
		timeval.tv_sec = (uint_32)(applyingtime/1000000); //Time in seconds!
		applyingtime -= (timeval.tv_sec*1000000); //Substract to get microseconds!
		timeval.tv_usec = (uint_32)applyingtime; //We have the amount of microseconds left!
		if (epochtoaccuratetime(&timeval,curtime)) //Convert back to apply it to the current time!
		{
			return 1; //Success!
		}
	}
	return 0; //Failed!
}

//Calculating relative time from the CMOS!
OPTINLINE void updateTimeDivergeance() //Update relative time to the clocks(time difference changes)! This is called when software changes the time/date!
{
	struct timeval tp;
	struct timezone currentzone;
	accuratetime savedtime,currenttime;
	CMOS_decodetime(&savedtime); //Get the currently stored time in the CMOS!
	if (gettimeofday(&tp,&currentzone)==0) //Time gotten?
	{
		if (epochtoaccuratetime(&tp,&currenttime)) //Convert to accurate time!
		{
			calcDivergeance(&savedtime,&currenttime,&CMOS.DATA.timedivergeance,&CMOS.DATA.timedivergeance2); //Apply the new time divergeance!
		}
	}
}

//Update the current Date/Time (based upon the refresh rate set) to the CMOS this runs at 64kHz!
OPTINLINE void RTC_updateDateTime()
{
	//Update the time itself at the highest frequency of 64kHz!
	//Get time!
	struct timeval tp;
	struct timezone currentzone;
	accuratetime currenttime;
	byte lastsecond = CMOS.DATA.DATA80.info.RTC_Seconds; //Previous second value for alarm!
	CMOS.UpdatingInterruptSquareWave ^= 1; //Toggle the square wave to interrupt us!
	if (CMOS.UpdatingInterruptSquareWave==0) //Toggled twice? Update us!
	{
		if (CMOS.DATA.DATA80.info.STATUSREGISTERB.EnableCycleUpdate==0) //We're allowed to update the time?
		{
			if (gettimeofday(&tp, &currentzone) == 0) //Time gotten?
			{
				if (epochtoaccuratetime(&tp,&currenttime)) //Converted?
				{
					//Apply time!
					applyDivergeance(&currenttime, CMOS.DATA.timedivergeance,CMOS.DATA.timedivergeance2); //Apply the new time divergeance!
					CMOS_encodetime(&currenttime); //Apply the new time to the CMOS!
				}
			}
		}
	}
	RTC_Handler(lastsecond); //Handle anything that the RTC has to handle!
}

double RTC_timepassed = 0.0, RTC_timetick = 0.0;
void updateCMOS(double timepassed)
{
	RTC_timepassed += timepassed; //Add the time passed to get our time passed!
	if (RTC_timetick) //Are we enabled?
	{
		if (RTC_timepassed >= RTC_timetick) //Enough to tick?
		{
			for (;RTC_timepassed>=RTC_timetick;) //Still enough to tick?
			{
				RTC_timepassed -= RTC_timetick; //Ticked once!
				RTC_updateDateTime(); //Call our timed handler!
			}
		}
	}
}

uint_32 getGenericCMOSRate()
{
	byte rate;
	rate = CMOS.DATA.DATA80.data[0xA]; //Load the rate register!
	rate &= 0xF; //Only the rate bits themselves are used!
	if (rate) //To use us?
	{
		--rate; //Rate is one less!
		return (1<<rate); //The tap to look at(as a binary number) for a square wave to change state!
	}
	else //We're disabled?
	{
		return 0; //We're disabled!
	}
}

OPTINLINE void CMOS_onWrite() //When written to CMOS!
{
	if (CMOS.ADDR==0xB) //Might have enabled IRQ8 functions!
	{
		CMOS.RateDivider = getGenericCMOSRate(); //Generic rate!
	}
	else if (CMOS.ADDR < 0xA) //Date/time might have been updated?
	{
		if ((CMOS.ADDR != 1) && (CMOS.ADDR != 3) && (CMOS.ADDR != 5)) //Date/Time has been updated(not Alarm being set)?
		{
			updateTimeDivergeance(); //Update the relative time compared to current time!
		}
	}
	CMOS.Loaded = 1; //We're loaded now!
}

void loadCMOS()
{
	if (!BIOS_Settings.got_CMOS)
	{
		loadCMOSDefaults(); //Load our default requirements!
		updateTimeDivergeance(); //Load the default time divergeance too!
		return;
	}
	else //Load BIOS CMOS!
	{
		memcpy(&CMOS.DATA, &BIOS_Settings.CMOS, sizeof(CMOS.DATA)); //Copy to our memory!
	}
	CMOS.Loaded = 1; //The CMOS is loaded!
}

void saveCMOS()
{
	if (CMOS.Loaded==0) return; //Don't save when not loaded/initialised!
	memcpy(&BIOS_Settings.CMOS, &CMOS.DATA, sizeof(CMOS.DATA)); //Copy the CMOS to BIOS!
	BIOS_Settings.got_CMOS = 1; //We've saved an CMOS!
	forceBIOSSave(); //Save the BIOS data!
}

byte XTRTC_translatetable[0x10] = {
0x80, //00: 1/10000 seconds
0x81, //01: 1/100 seconds
0x00, //02: seconds
0x02, //03: minutes
0x04, //04: hours
0x06, //05: day of week
0x07, //06: day of month
0x08, //07: month
0xFF, //08: RAM
0x09, //09: year(RAM), unexistant on our chip, but map to year anyway!
0xFF, //0A: RAM
0xFF, //0B: RAM
0xFF, //0C: RAM
0xFF, //0D: RAM
0xFF, //0E: RAM
0xFF  //0F: RAM
}; //XT to CMOS translation table!

extern byte is_XT; //Are we an XT machine?

byte PORT_readCMOS(word port, byte *result) //Read from a port/register!
{
	byte data;
	byte isXT = 0;
	switch (port)
	{
	case 0x70: //CMOS_ADDR
		if (is_XT) return 0; //Not existant on XT systems!
		*result = CMOS.ADDR|(NMI<<7); //Give the address and NMI!
		return 1;
	case 0x71:
		if (is_XT) return 0; //Not existant on XT systems!
		readXTRTC: //XT RTC read compatibility
		if ((CMOS.ADDR&0x80)==0x00) //Normal data?
		{
			data = CMOS.DATA.DATA80.data[CMOS.ADDR]; //Give the data from the CMOS!
			if (CMOS.ADDR == 0xD) //Read only status register D?
			{
				if (CMOS.Loaded) //Anything valid in RAM?
				{
					data |= 0x80; //We have power!
				}
			}
			//Status register B&C are read-only!
		}
		else
		{
			switch (CMOS.ADDR & 0x7F) //What extended register?
			{
			case 0: //s10000?
				data = (CMOS.DATA.s10000&0xF0); //10000th seconds, high digit only!
				break;
			case 1: //s100?
				data = CMOS.DATA.s100; //100th seconds!
				break;
			default: //Unknown?
				data = 0; //Unknown register!
				break;
			}
		}
		if (CMOS.ADDR == 0x0C) //Lower any interrupt flags set when this register is read? This allows new interrupts to fire!
		{
			//Enable all interrupts for RTC again?
			lowerirq(8); //Lower the IRQ, if raised!
			acnowledgeIRQrequest(8); //Acnowledge the IRQ, if needed!
			if ((data&0x70)&(CMOS.DATA.DATA80.info.STATUSREGISTERB.value&0x70)) data |= 0x80; //Set the IRQF bit when any interrupt is requested (PF==PIE==1, AF==AIE==1 or UF==UIE==1)
			CMOS.DATA.DATA80.data[0x0C] &= 0xF; //Clear the interrupt raised flags to allow new interrupts to fire!
		}
		CMOS.ADDR = 0xD; //Reset address!
		if ((isXT==0) && CMOS.DATA.DATA80.info.STATUSREGISTERB.DataModeBinary) //To convert to binary?
		{
			data = decodeBCD8(data); //Decode the BCD data!
		}
		*result = data; //Give the data!
		return 1;
	//XT RTC support?
	case 0x240: //1/10000 seconds
	case 0x241: //1/100 seconds
	case 0x242: //seconds
	case 0x243: //minutes
	case 0x244: //hours
	case 0x245: //day of week
	case 0x246: //day of month
	case 0x247: //month
	case 0x249: //1/100 seconds, year in the case of TIMER.COM v1.2!
		if (is_XT == 0) return 0; //Not existant on the AT and higher!
		isXT = 1; //From XT!
		CMOS.ADDR = XTRTC_translatetable[port&0xF]; //Translate the port to a compatible index!
		goto readXTRTC; //Read the XT RTC!
	case 0x248: //1/10000 seconds latch according to documentation, map to RAM for TIMER.COM v1.2!
		if (is_XT == 0) return 0; //Not existant on the AT and higher!
		*result = CMOS.DATA.extraRAMdata[6]; //Map to month instead!
		return 1;
		break;
	//Latches:
	/*
	case 0x249: //1/100 seconds
	case 0x24A: //seconds
	case 0x24B: //minutes
	case 0x24C: //hours
	case 0x24D: //day of week
	case 0x24E: //day of month
	case 0x24F: //month
		*result = CMOS.DATA.extraRAMdata[port-0x248]; //Read the value directly into RAM!
		break;
	*/
	//Rest registers of the chip:
	case 0x250: //Interrupt status Register
	case 0x251: //Interrupt control Register
	case 0x252: //Counter Reset
	case 0x253: //Latch Reset
	case 0x255: //"GO" Command
	case 0x256: //Standby Interrupt
	case 0x257: //Test Mode
		if (is_XT == 0) return 0; //Not existant on the AT and higher!
		*result = 0; //Unimplemented atm!
		return 1; //Simply supported for now(plain RAM read)!
		break;
	case 0x254: //Status Bit
		if (is_XT == 0) return 0; //Not existant on the AT and higher!
		*result = 0; //Not updating the status!
		return 1;
		break;
	}
	return 0; //None for now!
}

byte PORT_writeCMOS(word port, byte value) //Write to a port/register!
{
	byte isXT = 0;
	switch (port)
	{
	case 0x70: //CMOS ADDR
		if (is_XT) return 0; //Not existant on XT systems!
		CMOS.ADDR = (value&0x7F); //Take the value!
		NMI = ((value&0x80)>>7); //NMI?
		return 1;
		break;
	case 0x71:
		if (is_XT) return 0; //Not existant on XT systems!
		writeXTRTC: //XT RTC write compatibility
		if ((isXT==0) && CMOS.DATA.DATA80.info.STATUSREGISTERB.DataModeBinary) //To convert from binary?
		{
			value = encodeBCD8(value); //Encode the binary data!
		}
		//Write back the destination data!
		if ((CMOS.ADDR & 0x80)==0x00) //Normal data?
		{
			if ((CMOS.ADDR!=0xC) && (CMOS.ADDR!=0xD)) //Read only values?
			{
				CMOS.DATA.DATA80.data[CMOS.ADDR] = value; //Give the data from the CMOS!
			}
		}
		else
		{
			switch (CMOS.ADDR & 0x7F) //What extended register?
			{
			case 0: //s10000?
				CMOS.DATA.s10000 = (value&0xF0); //10000th seconds!
				break;
			case 1: //s100?
				CMOS.DATA.s100 = value; //100th seconds!
				break;
			default: //Unknown?
				//Unknown register! Can't write!
				break;
			}
		}
		CMOS_onWrite(); //On write!
		CMOS.ADDR = 0xD; //Reset address!		
		return 1;
		break;
	//XT RTC support!
	//case 0x240: //1/10000 seconds
	case 0x241: //1/100 seconds
	case 0x242: //seconds
	case 0x243: //minutes
	case 0x244: //hours
	case 0x245: //day of week
	case 0x246: //day of month
	case 0x247: //month
	case 0x249: //1/100 seconds, year in the case of TIMER.COM v1.2!
		if (is_XT==0) return 0; //Not existant on the AT and higher!
		isXT = 1; //From XT!
		CMOS.ADDR = XTRTC_translatetable[port & 0xF]; //Translate the port to a compatible index!
		goto writeXTRTC; //Read the XT RTC!
	case 0x248: //1/10000 seconds latch according to documentation, map to RAM for TIMER.COM v1.2!
		if (is_XT == 0) return 0; //Not existant on the AT and higher!
		CMOS.DATA.extraRAMdata[6] = value; //Map to month instead!
		return 1;
		break;
	//Latches to XT CMOS RAM!
	/*
	case 0x24A: //seconds
	case 0x24B: //minutes
	case 0x24C: //hours
	case 0x24D: //day of week
	case 0x24E: //day of month
	case 0x24F: //month
		CMOS.DATA.extraRAMdata[port-0x248] = value; //Save the value directly into RAM!
		return 1;
		break;
	*/
	//Rest registers of the chip:
	case 0x250: //Interrupt status Register
	case 0x251: //Interrupt control Register
	case 0x252: //Counter Reset
	case 0x253: //Latch Reset
	case 0x254: //Status Bit
	case 0x255: //"GO" Command
	case 0x256: //Standby Interrupt
	case 0x257: //Test Mode
		if (is_XT == 0) return 0; //Not existant on the AT and higher!
		//Unimplemented atm!
		return 1;
		break;
	default: //Unknown?
		break; //Do nothing!
	}
	return 0; //Unsupported!
}

void initCMOS() //Initialises CMOS (apply solid init settings&read init if possible)!
{
	CMOS.ADDR = 0; //Reset!
	NMI = 1; //Reset: Disable NMI interrupts!
	memset(&CMOS,0,sizeof(CMOS)); //Make sure we're fully initialized always!
	loadCMOS(); //Load the CMOS from disk OR defaults!

	//Register our I/O ports!
	register_PORTIN(&PORT_readCMOS);
	register_PORTOUT(&PORT_writeCMOS);
	XTMode = 0; //Default: not XT mode!
	RTC_timepassed = 0.0; //Initialize our timing!
	RTC_timetick = 1000000000.0/131072.0; //We're ticking at a frequency of ~65kHz(65535Hz signal, which is able to produce a square wave as well at that frequency?)!
}