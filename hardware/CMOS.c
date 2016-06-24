#include "headers/types.h" //Basic type support!
#include "headers/hardware/pic.h" //PIC!
#include "headers/emu/timers.h" //Timing!
#include "headers/support/log.h" //Logging support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/support/locks.h" //Locking support!
#include "headers/hardware/ports.h" //Port support!

//For time support!
#ifdef __PSP__
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

word decodeBCD(word bcd)
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

word encodeBCD(word value)
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

byte encodeBCD8(byte value)
{
	return (encodeBCD(value)&0xFF);
}

byte decodeBCD8(byte value)
{
	return (decodeBCD(value)&0xFF);
}

struct
{
	CMOSDATA DATA;
	int_64 timedivergeance; //Time divergeance in seconds!
	int_64 timedivergeance2; //Time diveargeance in us!
	byte IRQ8_Disabled; //IRQ8 not allowed to run for this type? (bits 0x10-0x40 are set for enabled)?
	byte Loaded; //CMOS loaded?
	byte ADDR; //Internal address in CMOS (7 bits used, 8th bit set=NMI Disable)
} CMOS;

extern byte NMI; //NMI interrupt enabled?

extern BIOS_Settings_TYPE BIOS_Settings; //The BIOS settings loaded!

#define FLOPPY_NONE 0
#define FLOPPY_360 1
#define FLOPPY_12 2
#define FLOPPY_720 3
#define FLOPPY_144 4
#define FLOPPY_288 5

void loadCMOSDefaults()
{
	memset(&CMOS.DATA.DATA80,0,sizeof(CMOS.DATA)); //Clear/init CMOS!
	CMOS.DATA.DATA80.data[0x10] = ((FLOPPY_144)<<4)|(FLOPPY_144); //High=Master, Low=Slave!
	CMOS.DATA.DATA80.data[0x15] = 0x15; //We have...
	CMOS.DATA.DATA80.data[0x16] = 0x16; //640K base memory!
	CMOS.timedivergeance = 0; //No second divergeance!
	CMOS.timedivergeance2 = 0; //No us divergeance!
	CMOS.Loaded = 1; //Loaded: ready to save!
}

void RTC_PeriodicInterrupt() //Periodic Interrupt!
{
	CMOS.DATA.DATA80.data[0x0C] |= 0x40; //Periodic Interrupt flag!

	if (CMOS.DATA.DATA80.data[0x0B]&0x40) //Enabled interrupt?
	{
		CMOS.IRQ8_Disabled |= 0x40; //Disable future calls!
		doirq(8); //Run the IRQ!
	}
}

void RTC_UpdateEndedInterrupt(byte manualtrigger) //Update Ended Interrupt!
{
	CMOS.DATA.DATA80.data[0x0C] |= 0x10; //Update Ended Interrupt flag!

	if (CMOS.DATA.DATA80.data[0x0B]&0x10) //Enabled interrupt?
	{
		if (!manualtrigger)
		{
			CMOS.IRQ8_Disabled |= 0x10; //Disable future calls!
		}
		doirq(8); //Run the IRQ!
	}
}

void RTC_AlarmInterrupt() //Alarm handler!
{
	CMOS.DATA.DATA80.data[0x0C] |= 0x20; //Alarm Interrupt flag!

	if (CMOS.DATA.DATA80.data[0x0B]&0x20) //Enabled interrupt?
	{
		CMOS.IRQ8_Disabled |= 0x20; //Disable future calls!
		doirq(8); //Run the IRQ!
	}
}

void CMOS_onRead() //When CMOS is being read (special actions).
{
	if (CMOS.ADDR==0x0C) //Enable all interrupts for RTC again?
	{
		lock(LOCK_CMOS);
		CMOS.IRQ8_Disabled = 0; //Enable all!
		unlock(LOCK_CMOS);
	}
}

void RTC_Handler() //Handle RTC Timer Tick!
{
	if (CMOS.DATA.DATA80.info.STATUSREGISTERB.EnablePeriodicInterrupt) //Enabled?
	{
		RTC_PeriodicInterrupt(); //Handle!
	}

	if (CMOS.DATA.DATA80.info.STATUSREGISTERB.EnabledUpdateEndedInterrupt) //Enabled?
	{
		RTC_UpdateEndedInterrupt(0); //Handle!
	}

	if ((CMOS.DATA.DATA80.info.RTC_Hours==CMOS.DATA.DATA80.info.RTC_HourAlarm) &&
			(CMOS.DATA.DATA80.info.RTC_Minutes==CMOS.DATA.DATA80.info.RTC_MinuteAlarm) &&
			(CMOS.DATA.DATA80.info.RTC_Seconds==CMOS.DATA.DATA80.info.RTC_SecondAlarm) &&
			(CMOS.DATA.DATA80.info.STATUSREGISTERB.EnableAlarmInterrupt)) //Alarm on?
	{
		RTC_AlarmInterrupt(); //Handle!
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
	uint64_t    time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	time = ((uint64_t)file_time.dwLowDateTime);
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec = (long)((time - EPOCH) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
	return 0;
}
#else
#ifdef IS_PSP
#ifndef timeval
// MSVC defines this in winsock2.h!?
typedef struct timeval {
	long tv_sec;
	long tv_usec;
} timeval;
#endif

int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
	//Unknown?
	return 1; //Error: couldn't retrieve!
}
#endif
#endif

//Our accurate time support:

typedef struct
{
	uint_64 year;
	byte month;
	byte day;
	byte hour;
	byte minute;
	byte second;
	word s100; //100th seconds(use either this or microseconds, since they both give the same time, only this one is rounded down!)
	uint_64 us; //Microseconds?
} accuratetime;

//Our accuratetime epoch support!

//Epoch time values!
#define EPOCH_YEAR 31556926
#define EPOCH_MONTH 2629743
#define EPOCH_DAY 86400
#define EPOCH_HOUR 3600
#define EPOCH_MINUTE 60
#define EPOCH_SECOND 1

byte epochtoaccuratetime(struct timeval *curtime, accuratetime *datetime)
{
	uint_64 seconds = curtime->tv_sec; //Get the amount of seconds, ignore ms for now(not used?)
	uint_64 usec = curtime->tv_usec;
	datetime->us = curtime->tv_usec;
	datetime->s100 = (curtime->tv_usec/10000); //10000us=1/100 second!
	datetime->year = (seconds/EPOCH_YEAR); //Year(counting since 1-1-1970)!
	seconds -= datetime->year*EPOCH_YEAR; //Rest!
	datetime->year += 1970; //We start at 1970!
	datetime->month = (seconds/EPOCH_MONTH); //Month!
	seconds -= datetime->month*EPOCH_MONTH; //Rest!
	datetime->day = (seconds/EPOCH_DAY);
	seconds -= datetime->day*EPOCH_DAY;
	datetime->second = seconds; //The amount of seconds is left!
	return 1; //Always successfully converted!
}

byte accuratetimetoepoch(accuratetime *curtime, struct timeval *datetime)
{
	uint_64 seconds=0;
	if ((curtime->us/10000)!=(curtime->s100)) return 0; //Invalid time to convert: 100th seconds doesn't match us(this is supposed to be the same!)
	if (curtime->year<1970) return 0; //Before 1970 isn't supported!
	datetime->tv_usec = curtime->us; //Save the microseconds directly!
	seconds += (curtime->year-1970)*EPOCH_YEAR; //Years!
	seconds += curtime->month*EPOCH_MONTH; //Months!
	seconds += curtime->day*EPOCH_DAY; //Day of month!
	seconds += curtime->hour*EPOCH_HOUR; //Hours
	seconds += curtime->minute*EPOCH_MINUTE; //Minutes
	seconds += curtime->second*EPOCH_SECOND; //Seconds!
	datetime->tv_sec = seconds; //The amount of seconds!
	return 1; //Successfully converted!
}

//CMOS time encoding support!
void CMOS_decodetime(accuratetime *curtime) //Decode time into the current time!
{
	curtime->year = decodeBCD8(CMOS.DATA.DATA80.info.RTC_Year); //The year to compare to!
	curtime->month = decodeBCD8(CMOS.DATA.DATA80.info.RTC_Month); //The month to compare to!
	curtime->day = decodeBCD8(CMOS.DATA.DATA80.info.RTC_DateOfMonth); //The day to compare to!
	curtime->hour = decodeBCD8(CMOS.DATA.DATA80.info.RTC_Hours); //H
	curtime->minute = decodeBCD8(CMOS.DATA.DATA80.info.RTC_Minutes); //M
	curtime->second = decodeBCD8(CMOS.DATA.DATA80.info.RTC_Seconds); //S
	curtime->s100 = decodeBCD8(CMOS.DATA.s100); //The 100th seconds!
	curtime->us = curtime->s100*10000; //The same as above, make sure we match!
}

void CMOS_encodetime(accuratetime *curtime) //Encode time into the current time!
{
	CMOS.DATA.DATA80.info.RTC_Year = encodeBCD8(curtime->year%100);
	CMOS.DATA.DATA80.info.RTC_Month = encodeBCD8(curtime->month);
	CMOS.DATA.DATA80.info.RTC_DateOfMonth = encodeBCD8(curtime->day);
	CMOS.DATA.DATA80.info.RTC_Hours = encodeBCD8(curtime->hour);
	CMOS.DATA.DATA80.info.RTC_Minutes = encodeBCD8(curtime->minute);
	CMOS.DATA.DATA80.info.RTC_Seconds = encodeBCD8(curtime->second);
	CMOS.DATA.s100 = encodeBCD8(curtime->s100); //The 100th seconds!
}

//Divergeance support!
byte calcDivergeance(accuratetime *time1, accuratetime *time2, int_64 *divergeance_sec, int_64 *divergeance_usec) //Calculates the difference of time1 compared to time2(reference time)!
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

void applyDivergeance(accuratetime *curtime, int_64 divergeance_sec, int_64 divergeance_usec) //Apply divergeance to accurate time!
{
	struct timeval timeval; //The accurate time value!
	BIGGESTSINT applyingtime; //Biggest integer value we have!
	if (accuratetimetoepoch(curtime, &timeval)) //Converted to epoch?
	{
		applyingtime = ((timeval.tv_sec * 1000000) + timeval.tv_usec); //Direct time conversion!
		applyingtime += (divergeance_sec*1000000); //Add the divergeance: we're applying the destination time!
		applyingtime += divergeance_usec; //Apply usec!

		//Apply the resulting time!
		timeval.tv_sec = (applyingtime/1000000); //Time in seconds!
		applyingtime -= (timeval.tv_sec*1000000); //Substract to get microseconds!
		timeval.tv_usec = applyingtime; //We have the amount of microseconds left!
	}
}

//Calculating relative time from the CMOS!
void updateTimeDivergeance() //Update relative time to the clocks(time difference changes)! This is called when software changes the time/date!
{
	struct timeval tp;
	struct timezone currentzone;
	accuratetime savedtime,currenttime;
	CMOS_decodetime(&savedtime); //Get the currently stored time in the CMOS!
	if (gettimeofday(&tp,&currentzone)==0) //Time gotten?
	{
		if (epochtoaccuratetime(&tp,&currenttime)) //Convert to accurate time!
		{
			lock(LOCK_CMOS); //We're updating critical information!
			calcDivergeance(&savedtime,&currenttime,&CMOS.timedivergeance,&CMOS.timedivergeance2); //Apply the new time divergeance!
			unlock(LOCK_CMOS); //Finished updating!
		}
	}
}

//Update the current Date/Time (based upon the refresh rate set) to the CMOS!
void RTC_updateDateTime()
{
	//Get time!
	struct timeval tp;
	struct timezone currentzone;
	accuratetime currenttime;
	if (gettimeofday(&tp, &currentzone) == 0) //Time gotten?
	{
		if (epochtoaccuratetime(&tp,&currenttime)) //Converted?
		{
			//Apply time!
			lock(LOCK_CMOS);
			applyDivergeance(&currenttime, CMOS.timedivergeance,CMOS.timedivergeance2); //Apply the new time divergeance!
			CMOS_encodetime(&currenttime); //Apply the new time to the CMOS!
			RTC_Handler(); //Handle anything that the RTC has to handle!
			unlock(LOCK_CMOS); //Finished updating!
		}
	}
}

uint_32 getIRQ8Rate()
{
	return 32768>>(CMOS.DATA.DATA80.data[0xA]); //The frequency!
}

void CMOS_onWrite() //When written to CMOS!
{
	if (CMOS.ADDR==0xB) //Might have enabled IRQ8 functions!
	{
		lock(LOCK_CMOS);
		addtimer((float)getIRQ8Rate(),&RTC_updateDateTime,"RTC",10,0,NULL); //RTC handler!
		CMOS.IRQ8_Disabled = 0; //Allow IRQ8 to be called by timer: we're enabled!
		unlock(LOCK_CMOS);
	}
	else if (CMOS.ADDR < 0xA) //Date/time might have been updated?
	{
		if ((CMOS.ADDR != 1) && (CMOS.ADDR != 3) && (CMOS.ADDR != 5)) //Date/Time has been updated?
		{
			updateTimeDivergeance(); //Update the relative time compared to current time!
		}
	}
}

void loadCMOS()
{
	if (!BIOS_Settings.got_CMOS)
	{
		loadCMOSDefaults(); //No CMOS!
		updateTimeDivergeance(); //Load the default time divergeance too!
		return;
	}
	else //Load BIOS CMOS!
	{
		memcpy(&CMOS.DATA.DATA80, &BIOS_Settings.CMOS, sizeof(CMOS.DATA.DATA80)); //Copy to our memory!
	}
	CMOS.timedivergeance = BIOS_Settings.timedivergeance; //Load the divergeance too!
	CMOS.timedivergeance2 = BIOS_Settings.timedivergeance2; //Load the divergeance too!
	CMOS.Loaded = 1; //The CMOS is loaded!
}

void saveCMOS()
{
	if (!CMOS.Loaded) return; //Don't save when not loaded/initialised!
	memcpy(&BIOS_Settings.CMOS, &CMOS.DATA.DATA80.data, 0x80); //Copy the CMOS to BIOS!
	BIOS_Settings.timedivergeance = CMOS.timedivergeance; //Apply the new time divergeance to the existing time!
	BIOS_Settings.timedivergeance2 = CMOS.timedivergeance2; //Apply the new time divergeance to the existing time!
	BIOS_Settings.got_CMOS = 1; //We've saved an CMOS!
	forceBIOSSave(); //Save the BIOS data!
}

byte XTRTC_translatetable[0x10] = {0,
0x80, //0.01 seconds
0x80, //0.1 seconds
0, //seconds
2, //minutes
4, //hours
6, //day of week
7, //day of month
8, //month
0xFF, //-
9, //Year
0xFF, //-
0xFF, //-
0xFF, //-
0xFF //-
}; //XT to CMOS translation table!

byte PORT_readCMOS(word port, byte *result) //Read from a port/register!
{
	byte numberpart = 0; //Number part? 1=Low BCD digit, 2=High BCD digit
	byte isXT = 0;
	switch (port)
	{
	case 0x70: //CMOS_ADDR
		*result = CMOS.ADDR|(NMI<<7); //Give the address and NMI!
		return 1;
	case 0x71:
		readXTRTC: //XT RTC read compatibility
		CMOS_onRead(); //Execute handler!
		lock(LOCK_CMOS); //Lock the CMOS!
		byte data;
		if ((CMOS.ADDR&0x80)==0x00) //Normal data?
		{
			data = CMOS.DATA.DATA80.data[CMOS.ADDR]; //Give the data from the CMOS!
		}
		else
		{
			switch (CMOS.ADDR & 0x7F) //What extended register?
			{
			case 0: //s100?
				data = CMOS.DATA.s100; //100th seconds!
				break;
			default: //Unknown?
				data = 0; //Unknown register!
				break;
			}
		}
		unlock(LOCK_CMOS);
		CMOS.ADDR = 0xD; //Reset address!
		if (numberpart) //Number part (BCD)?
		{
			if (numberpart==2) //High part?
			{
				data >>= 4; //High nibble!
			}
			data &= 0xF; //Low nibble or high nibble!
		}
		if (isXT || CMOS.DATA.DATA80.info.STATUSREGISTERB.DataModeBinary) //To convert to binary?
		{
			data = decodeBCD8(data); //Decode the binary data!
		}
		*result = data; //Give the data!
		return 1;
	//XT RTC support?
	case 0x241: //Read Data?
	case 0x348: //Unknown
		*result = 0; //Nothing!
		return 1;
		break;
	case 0x341: //0.1 seconds
		numberpart = 2; //High digit!
		goto readXTRTCADDR;
	case 0x340: //0.01 seconds
		numberpart = 1; //Low digit!
	case 0x342: //seconds
	case 0x343: //minutes
	case 0x344: //hours
	case 0x345: //day of week
	case 0x346: //day of month
	case 0x347: //month
	case 0x349: //year
		readXTRTCADDR:
		isXT = 1; //From XT!
		CMOS.ADDR = XTRTC_translatetable[port&0xF]; //Translate the port to a compatible index!
		goto readXTRTC; //Read the XT RTC!
		break;
	case 0x350: //Status?
	case 0x354: //Status?
		*result = 0; //Nothing!
		break;
	}
	return 0; //None for now!
}

byte PORT_writeCMOS(word port, byte value) //Write to a port/register!
{
	byte numberpart = 0; //Number part? 1=Low BCD digit, 2=High BCD digit
	byte isXT = 0;
	byte temp; //Temp data!
	switch (port)
	{
	case 0x70: //CMOS ADDR
		CMOS.ADDR = (value&0x7F); //Take the value!
		NMI = ((value&0x80)>>7); //NMI?
		CMOS_onWrite(); //On write!
		return 1;
		break;
	case 0x71:
		writeXTRTC: //XT RTC write compatibility
		lock(LOCK_CMOS); //Lock the CMOS!
		byte data;
		if ((CMOS.ADDR & 0x80) == 0x00) //Normal data?
		{
			data = CMOS.DATA.DATA80.data[CMOS.ADDR]; //Give the data from the CMOS!
		}
		else
		{
			switch (CMOS.ADDR & 0x7F) //What extended register?
			{
			case 0: //s100?
				data = CMOS.DATA.s100; //100th seconds!
				break;
			default: //Unknown?
				data = 0; //Unknown register!
				break;
			}
		}
		if (numberpart) //Only a nibble updated?
		{
			temp = data; //Original data!
			if (numberpart==1) //Low BCD digit?
			{
				temp &= 0xF0; //Clear low BCD digit!
				temp |= value&0xF; //Set the low BCD digit!
			}
			else //High BCD digit?
			{
				temp &= 0xF; //Clear high BCD digit!
				temp |= ((value&0xF)<<8); //Set the high BCD digit!
			}
			value = temp; //Use this value, as specified!
		}
		if ((isXT==0) && CMOS.DATA.DATA80.info.STATUSREGISTERB.DataModeBinary) //To convert from binary?
		{
			value = encodeBCD8(value); //Encode the binary data!
		}

		//Write back the destination data!
		if ((CMOS.ADDR & 0x80) == 0x00) //Normal data?
		{
			CMOS.DATA.DATA80.data[CMOS.ADDR] = value; //Give the data from the CMOS!
		}
		else
		{
			switch (CMOS.ADDR & 0x7F) //What extended register?
			{
			case 0: //s100?
				CMOS.DATA.s100 = value; //100th seconds!
				break;
			default: //Unknown?
				//Unknown register! Can't write!
				break;
			}
		}
		unlock(LOCK_CMOS);
		CMOS_onWrite(); //On write!
		CMOS.ADDR = 0xD; //Reset address!		
		return 1;
		break;
	//XT RTC support?
	case 0x241: //Trigger timer?
		lock(LOCK_CMOS);
		XTMode = 1; //Enable the XT mode!
		RTC_UpdateEndedInterrupt(1); //Manually trigger the update ended interrupt!
		unlock(LOCK_CMOS); //Ready!
		return 1; //Triggered!
		break;
	case 0x348: //Unknown
		return 1;
		break;
	case 0x341: //0.1 seconds
		numberpart = 2; //High digit!
		goto writeXTRTCADDR;
	case 0x340: //0.01 seconds
		numberpart = 1; //Low digit!
	case 0x342: //seconds
	case 0x343: //minutes
	case 0x344: //hours
	case 0x345: //day of week
	case 0x346: //day of month
	case 0x347: //month
	case 0x349: //year
		writeXTRTCADDR:
		isXT = 1; //From XT!
		CMOS.ADDR = XTRTC_translatetable[port&0xF]; //Translate the port to a compatible index!
		value = encodeBCD8(value); //Encode the data to BCD format!
		goto writeXTRTC; //Read the XT RTC!
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
	loadCMOS(); //Load the CMOS from disk OR defaults!

	//Register our I/O ports!
	register_PORTIN(&PORT_readCMOS);
	register_PORTOUT(&PORT_writeCMOS);
	addtimer((float)getIRQ8Rate(), &RTC_updateDateTime, "RTC", 10, 0, NULL); //RTC handler!
	XTMode = 0; //Default: not XT mode!
}