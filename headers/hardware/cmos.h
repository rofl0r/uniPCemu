#ifndef CMOS_H
#define CMOS_H

typedef struct
{
	union
	{
		struct
		{
			byte RTC_Seconds; //BCD 00-59
			byte RTC_SecondAlarm; //BCD 00-59, hex 00-3B, "don't care" if C0-FF
			byte RTC_Minutes; //BCD 00-59
			byte RTC_MinuteAlarm; //See secondalarm
			byte RTC_Hours; //BCD 00-23, Hex 00-17 if 24hr; BCD 01-12, Hex 01-0C if 12hr AM; BCD 82-92, Hex 81-8C if 12hr PM
			byte RTC_HourAlarm; //Same as Hours, "Don't care" if C0-FF
			byte RTC_DayOfWeek; //01-07; Sunday=1
			byte RTC_DateOfMonth; //BCD 01-31, Hex 01-1F
			byte RTC_Month; //BCD 01-12, Hex 01-0C
			byte RTC_Year; //BCD 00-99, Hex 00-63

			//On-chip status information:

			union
			{
				byte value;
				struct
				{
					byte IntRateSelection : 4; //Rate selection bits for interrupt: 0:None;3:122ms(minimum);16:500ms;6:1024Hz(default).
					byte Data_22StageDivider : 3; //2=32768 Time base (default)
					byte UpdateInProgress : 1; //Time update in progress, data outputs undefined (read-only)
				};
			} STATUSREGISTERA; //CMOS 0Ah

			union
			{
				byte value;
				struct
				{
					byte DSTEnable : 1; //DST Enabled?
					byte Enable24HourMode : 1; //24 hour mode enabled?
					byte DataModeBinary : 1; //1=Binary, 0=BCD
					byte EnableSquareWaveOutput : 1; //1=Enabled
					byte EnabledUpdateEndedInterrupt : 1;
					byte EnableAlarmInterrupt : 1;
					byte EnablePeriodicInterrupt : 1;
					byte EnableCycleUpdate : 1;
				};
			} STATUSREGISTERB;

			byte ToDo[116]; //Still todo!
		} info;
		byte data[0x80]; //CMOS Data!
	} DATA80; //The normal CMOS data!
	byte s100; //Extra support for 100th seconds!
	byte s10000; //Extra support for 10000th seconds!
	byte extraRAMdata[0x10]; //Extra RAM data from XT RTC(UM82C8167)!
} CMOSDATA;

void initCMOS(); //Initialises CMOS (apply solid init settings&read init if possible)!
void saveCMOS(); //Saves the CMOS, if any!

#endif