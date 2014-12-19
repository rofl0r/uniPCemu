#include "headers/types.h" //Basic type support!
#include "headers/hardware/pic.h" //PIC!
#include "headers/emu/timers.h" //Timing!
#include "headers/support/log.h" //Logging support!
#include "headers/bios/bios.h" //BIOS support!

/*

CMOS&RTC (Combined!)

*/

//Are we disabled?
#define __HW_DISABLED 0

word decodeBCD(word bcd)
{
	return (
			((((bcd&0xF000)>>12)%10)*1000)+ //Factor 1000
			((((bcd&0x0F00)>>8)%10)*100)+ //Factor 100
			((((bcd&0x00F0)>>4)%10)*10)+ //Factor 10
			((bcd&0x000F)%10) //Factor 1
			); //Give decoded value!
}

word encodeBCD(word value)
{
	return ((
			(0x1000*(word)((value%10000)/1000))+ //Factor 1000
			(0x0100*(word)((value%1000)/100))+ //Factor 100
			(0x0010*(word)((value%100)/10))+ //Factor 10
			(value%10) //Factor 1
		)&0xFFFF); //Give encoded BCD, wrap arround!
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
	}; //The data!
	byte IRQ8_Disabled; //IRQ8 not allowed to run for this type? (bits 0x10-0x40 are set for enabled)?
	byte Loaded; //CMOS loaded?
	byte ADDR; //Internal address in CMOS (7 bits used, 8th bit set=NMI Disable)
	byte NMI; //NMI interrupt enabled?
} CMOS;

extern BIOS_Settings_TYPE BIOS_Settings; //The BIOS settings loaded!

#define FLOPPY_NONE 0
#define FLOPPY_360 1
#define FLOPPY_12 2
#define FLOPPY_720 3
#define FLOPPY_144 4
#define FLOPPY_288 5

void loadCMOSDefaults()
{
	memset(&CMOS.data,0,sizeof(CMOS.data)); //Clear/init CMOS!
	CMOS.data[0x10] = ((FLOPPY_144)<<4)|(FLOPPY_144); //High=Master, Low=Slave!
	CMOS.data[0x15] = 0x15; //We have...
	CMOS.data[0x16] = 0x16; //640K base memory!
	CMOS.Loaded = 1; //Loaded: ready to save!
}

void loadCMOS()
{
	if (BIOS_Settings.got_CMOS)
	{
		loadCMOSDefaults(); //No CMOS!
		return;
	}
	else //Load BIOS CMOS!
	{
		memcpy(&CMOS.data,&BIOS_Settings.CMOS,0x80); //Copy to our memory!
	}
	CMOS.Loaded = 1; //The CMOS is loaded!
}

void saveCMOS()
{
	if (!CMOS.Loaded) return; //Don't save when not loaded/initialised!
	memcpy(&BIOS_Settings.CMOS,&CMOS.data,0x80); //Copy the CMOS to BIOS!
	BIOS_Settings.got_CMOS = 1; //We've saved an CMOS!
}

void initCMOS() //Initialises CMOS (apply solid init settings&read init if possible)!
{
	CMOS.ADDR = 0; //Reset!
	CMOS.NMI = 1; //Reset!
	loadCMOS(); //Load the CMOS from disk OR defaults!
}

void RTC_PeriodicInterrupt() //Periodic Interrupt!
{
	CMOS.data[0x0C] |= 0x40; //Periodic Interrupt flag!

	if (CMOS.data[0x0B]&0x40) //Enabled interrupt?
	{
		CMOS.IRQ8_Disabled |= 0x40; //Disable future calls!
		doirq(8); //Run the IRQ!
	}
}

void RTC_UpdateEndedInterrupt() //Update Ended Interrupt!
{
	//Apply time!
	pspTime time;
	if (!sceRtcGetCurrentClockLocalTime(&time)) //Time gotten?
	{
		CMOS.info.RTC_Year = encodeBCD8(time.year);
		CMOS.info.RTC_Month = encodeBCD8(time.month);
		CMOS.info.RTC_DateOfMonth = encodeBCD8(time.day);
		CMOS.info.RTC_Hours = encodeBCD8(time.hour);
		CMOS.info.RTC_Minutes = encodeBCD8(time.minutes);
		CMOS.info.RTC_Seconds = encodeBCD8(time.seconds);
		//Now the time is updated!
	}
	CMOS.data[0x0C] |= 0x10; //Update Ended Interrupt flag!

	if (CMOS.data[0x0B]&0x10) //Enabled interrupt?
	{
	CMOS.IRQ8_Disabled |= 0x10; //Disable future calls!
	doirq(8); //Run the IRQ!
	}
}

void RTC_AlarmInterrupt() //Alarm handler!
{
	CMOS.data[0x0C] |= 0x20; //Alarm Interrupt flag!

	if (CMOS.data[0x0B]&0x20) //Enabled interrupt?
	{
		CMOS.IRQ8_Disabled |= 0x20; //Disable future calls!
		doirq(8); //Run the IRQ!
	}
}

void CMOS_onRead() //When CMOS is being read (special actions).
{
	if (CMOS.ADDR==0x0C) //Enable all interrupts for RTC again?
	{
		CMOS.IRQ8_Disabled = 0; //Enable all!
	}
}

void RTC_Handler() //Handle RTC Timer Tick!
{
	if (1) //Periodic interrupt?
	{
		if (CMOS.info.STATUSREGISTERB.EnablePeriodicInterrupt) //Enabled?
		{
			RTC_PeriodicInterrupt(); //Handle!
		}
	}

	if (1) //Update ended interrupt?
	{
		if (CMOS.info.STATUSREGISTERB.EnabledUpdateEndedInterrupt) //Enabled?
		{
			RTC_UpdateEndedInterrupt(); //Handle!
		}
	}

	if (1) //Alarm interrupt?
	{
			if ((CMOS.info.RTC_Hours==CMOS.info.RTC_HourAlarm) &&
					(CMOS.info.RTC_Minutes==CMOS.info.RTC_MinuteAlarm) &&
					(CMOS.info.RTC_Seconds==CMOS.info.RTC_SecondAlarm) &&
					(CMOS.info.STATUSREGISTERB.EnableAlarmInterrupt)) //Alarm on?
			{
				RTC_AlarmInterrupt(); //Handle!
			}
	}
}

uint_32 getIRQ8Rate()
{
	return 32768>>(CMOS.data[0xA]); //The frequency!
}

void CMOS_onWrite() //When written to CMOS!
{
	if (CMOS.ADDR==0xB) //Might have enabled IRQ8 functions!
	{
		addtimer((float)getIRQ8Rate(),&RTC_Handler,"RTC"); //RTC handler!
		CMOS.IRQ8_Disabled = 0; //Allow IRQ8 to be called by timer: we're enabled!
	}
}

byte PORT_readCMOS(word port) //Read from a port/register!
{
	switch (port)
	{
	case 0x70: //CMOS_ADDR
		return CMOS.ADDR|(CMOS.NMI<<7); //Give the address and NMI!
	case 0x71:
		CMOS_onRead(); //Execute handler!
		byte data =  CMOS.data[CMOS.ADDR]; //Give the data from the CMOS!
		CMOS.ADDR = 0xD; //Reset address!
		return data; //Give the data!
	}
	return 0; //None for now!
}

void PORT_writeCMOS(word port, byte value) //Write to a port/register!
{
	switch (port)
	{
	case 0x70: //CMOS ADDR
		CMOS.ADDR = (value&0x7F); //Take the value!
		CMOS.NMI = ((value&0x80)==0x80); //NMI?
		CMOS_onWrite(); //On write!
		break;
	case 0x71:
		CMOS.data[CMOS.ADDR] = value; //Set value in CMOS!
		CMOS_onWrite(); //On write!
		CMOS.ADDR = 0xD; //Reset address!		
		break;
	default: //Unknown?
		break; //Do nothing!
	}
}