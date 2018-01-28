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

			byte STATUSREGISTERA; //CMOS 0Ah

			byte STATUSREGISTERB;

			byte ToDo[116]; //Still todo!
		} info;
		byte data[0x80]; //CMOS Data!
	} DATA80; //The normal CMOS data!
	int_64 timedivergeance; //Time divergeance in seconds!
	int_64 timedivergeance2; //Time diveargeance in us!
	byte s100; //Extra support for 100th seconds!
	byte s10000; //Extra support for 10000th seconds!
	byte extraRAMdata[8]; //Extra RAM data from XT RTC(UM82C8167), for 56 bits of extra RAM!
	byte centuryisbinary; //Century is to be read/written as a binary value?
	byte cycletiming; //Run the CMOS off the CPU clock instead of realtime?
} CMOSDATA;

typedef struct
{
	CMOSDATA DATA;
	byte Loaded; //CMOS loaded?
	byte ADDR; //Internal address in CMOS (7 bits used, 8th bit set=NMI Disable)

	uint_32 RateDivider; //Rate divider, usually set to 1024Hz. Used for Square Wave output and Periodic Interrupt!
	uint_32 currentRate; //The current rate divider outputs(22-bits)!

	byte SquareWave; //Square Wave Output!
} CMOS_Type;

//SRA
//Rate selection bits for interrupt: 0:None;3:122ms(minimum);16:500ms;6:1024Hz(default).
#define SRA_IntRateSelection(SRA) (SRA&0xF)
//2=32768 Time base (default)
#define SRA_DATA_22STAGEDIVIDER(SRA) (SRA>>4&7)
//Time update in progress, data outputs undefined (read-only)
#define SRA_UPDATEINPROGRESS 0x80

//SRB
//DST Enabled?
#define SRB_DSTENABLE 1
//24 hour mode enabled?
#define SRB_ENABLE24HOURMODE 2
//1=Binary, 0=BCD
#define SRB_DATAMODEBINARY 4
//1=Enabled
#define SRB_ENABLESQUAREWAVEOUTPUT 8
#define SRB_ENABLEUPDATEENDEDINTERRUPT 0x10
#define SRB_ENABLEALARMINTERRUPT 0x20
#define SRB_ENABLEPERIODICINTERRUPT 0x40
#define SRB_ENABLECYCLEUPDATE 0x80

void initCMOS(); //Initialises CMOS (apply solid init settings&read init if possible)!
void saveCMOS(); //Saves the CMOS, if any!

void updateCMOS(double timepassed); //Update CMOS timing!

#endif