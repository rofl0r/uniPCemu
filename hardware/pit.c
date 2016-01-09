/*

src:http://wiki.osdev.org/Programmable_Interval_Timer#Channel_2
82C54 PIT (Timer) (82C54(8253/8254 in older systems))

*/

#include "headers/types.h"
#include "headers/hardware/ports.h" //IO Port support!
#include "headers/hardware/pic.h" //irq support!
#include "headers/emu/sound.h" //PC speaker support for the current emu!
#include "headers/emu/timers.h" //Timer support!
#include "headers/hardware/8253.h" //Our own typedefs etc.

#include "headers/hardware/pcspeaker.h" //PC speaker support!
#include "headers/support/highrestimer.h" //High resolution timer support for current value!
#include "headers/support/locks.h" //Locking support!

//Are we disabled?
#define __HW_DISABLED 0

TicksHolder timerticks[3];
float timertime[3] = { 1000000000.0f / 18.2f,0.0f,0.0f }; //How much time does it take to expire (default to 0=18.2Hz timer)?
float currenttime[3] = { 0.0f,0.0f,0.0f }; //Current time passed!

extern byte EMU_RUNNING; //Emulator running? 0=Not running, 1=Running, Active CPU, 2=Running, Inactive CPU (BIOS etc.)

void cleanPIT0()
{
	byte channel;
	for (channel = 0;channel < 3;) //process all channels!
	{
		getnspassed(&timerticks[channel]); //Discard the time passed to the counter!
		if ((currenttime[channel] >= timertime[channel]) && timertime[channel]) //Are we to trigger an interrupt?
		{
			currenttime[channel] = fmod(currenttime[channel],timertime[channel]); //Tick until there's nothing left1
		}
		++channel; //Process next channel!
	}
}

void updatePIT0() //Timer tick Irq
{
	byte channel;
	if (EMU_RUNNING==1) //Are we running?
	{
		for (channel = 0;channel < 3;) //process all channels!
		{
			currenttime[channel] += (float)getnspassed(&timerticks[channel]); //Add the time passed to the counter!
			if ((currenttime[channel] >= timertime[channel]) && timertime[channel]) //Are we to trigger an interrupt?
			{
				currenttime[channel] -= timertime[channel]; //Rest!
				if (!channel) doirq(0); //PIT0 executes an IRQ on timeout!
			}
			++channel; //Process next channel!
		}
	}
}

uint_32 pitcurrentlatch[4], pitlatch[4], pitdivisor[4]; //Latches & divisors are 32-bits large!
byte pitcommand[4]; //PIT command is only 1 byte large!

//PC Speaker functionality in PIT

uint_32 PCSpeakerFrequency=0x10000; //PC Speaker Frequency from the PIT!
byte PCSpeakerPort; //Port 0x61 for the PC Speaker!
byte PCSpeakerIsRunning; //Speaker is running?

//NEW HANDLER
uint_64 calculatedpitstate[3]; //Calculate state by time and last time handled!

void updatePITState(byte channel)
{
	//Calculate the current PIT0 state by frequency and time passed!
	static const float tickduration = (1.0f / 1193180.0f)*1000000000.0f; //How long does it take to process one tick in ns?
	calculatedpitstate[channel] = pitdivisor[channel]; //Load the current divisor (1-65536) to count down from!
	calculatedpitstate[channel] -= (uint_64)((float)currenttime[channel] / (float)tickduration); //Count down to the current PIT0 state!
	pitlatch[channel] = (calculatedpitstate[channel] &= 0xFFFF); //Convert it to 16-bits value of the PIT and latch it!
}

byte lastpit = 0;

byte in8253(word portnum, byte *result)
{
	byte pit;
	if (__HW_DISABLED) return 0; //Abort!
	switch (portnum)
	{
		case 0x40:
		case 0x41:
		case 0x42:
			pit = (byte)(portnum&0xFF);
			pit &= 3; //PIT!
			if (pitcommand[pit] & 0x30) //No latch mode?
			{
				updatePITState(pit); //Update the state: effect like a running timer!
			}
			switch (pitcommand[pit] & 0x30) //What input mode currently?
			{
			case 0x10: //Lo mode?
				*result = (pitlatch[pit] & 0xFF);
				break;
			case 0x20: //Hi mode?
				*result = ((pitlatch[pit]>>8) & 0xFF) ;
				break;
			case 0x00: //Latch mode?
			case 0x30: //Lo/hi mode?
				if (pitcurrentlatch[pit] == 0)
				{
					//Give the value!
					pitcurrentlatch[pit] = 1;
					*result = (pitlatch[pit] & 0xFF);
				}
				else
				{
					pitcurrentlatch[pit] = 0;
					*result = ((pitlatch[pit] >> 8) & 0xFF);
				}
				break;
			}
			return 1;
			break;
		case 0x43:
			*result = pitcommand[lastpit]; //Give the last command byte!
			return 1;
		case 0x61: //PC speaker? From original timer!
			*result = PCSpeakerPort; //Give the speaker port!
			return 1;
		default: //Unknown port?
			break; //Unknown port!
	}
	return 0; //Disabled!
}

byte out8253(word portnum, byte value)
{
	if (__HW_DISABLED) return 0; //Abort!
	byte old61; //For tracking updates!
	byte pit;
	switch (portnum)
	{
		case 0x40: //pit 0 data port
		case 0x41: //pit 1 data port
		case 0x42: //speaker data port
			pit = (byte)(portnum&0xFF);
			pit &= 3; //Low 2 bits only!
			switch (pitcommand[pit]&0x30) //What input mode currently?
			{
			case 0x10: //Lo mode?
				pitdivisor[pit] = (pitdivisor[pit] & 0xFF00) + (value & 0xFF);
				break;
			case 0x20: //Hi mode?
				pitdivisor[pit] = (pitdivisor[pit] & 0xFF) + ((value & 0xFF)<<8);
				break;
			case 0x00: //Latch mode?
			case 0x30: //Lo/hi mode?
				if (!pitcurrentlatch[pit])
				{
					pitdivisor[pit] = (pitdivisor[pit] & 0xFF00) + (value & 0xFF);
					pitcurrentlatch[pit] = 1;
				}
				else
				{
					pitdivisor[pit] = (pitdivisor[pit] & 0xFF) + ((value & 0xFF)<<8);
					pitcurrentlatch[pit] = 0;
				}
				break;
			}
			if (!pitdivisor[pit]) pitdivisor[pit] = 0x10000;
			initTicksHolder(&timerticks[pit]); //Initialise the timer ticks!
			timertime[pit] = SAFEDIV(1000000000.0f,SAFEDIV(1193180.0f, pitdivisor[pit])); //How much time do we take to expire?
			if (pit==2) //PC speaker?
			{
				PCSpeakerFrequency = pitdivisor[pit]; //The frequency of the PC speaker!
				setSpeakerFrequency(pitdivisor[pit]); //Set the new divisor!
			}
			return 1;
		case 0x43: //pit command port
			if ((value & 0xC0) == 0xC0) //Read-back command?
			{
				//TODO
			}
			else //Normal command?
			{
				byte channel;
				channel = (value >> 6);
				channel &= 3; //The channel!
				pitcommand[channel] = value; //Set the command for the port!
				if (channel==2) setPCSpeakerMode((value>>1)&7); //Update the PC speaker mode when needed!
				if (!(value&0x30)) //Latch count value?
				{
					updatePITState(channel); //Update the latch!
				}
				lastpit = channel; //The last channel effected!
				pitcurrentlatch[channel] = 0; //Reset the latch always!
			}
			return 1;
		//From above original:
	case 0x61: //PC Speaker?
		old61 = PCSpeakerPort; //Old value!
		PCSpeakerPort = (value&3); //Set the new port value, only low 2 bits are used!
		speakerGateUpdated(); //Gate has been updated!
		return 1;
	default:
		break;
	}
	return 0; //Unhandled!
}

void init8253() {
	if (__HW_DISABLED) return; //Abort!
	register_PORTOUT(&out8253);
	register_PORTIN(&in8253);
}