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

double timerfreq; //Done externally!

TicksHolder timerticks;
float timertime = 1000000.0f/18.2f; //How much time does it take to expire (default to 0=18.2Hz timer)?
float currenttime = 0.0f; //Current time passed!

extern byte EMU_RUNNING; //Emulator running? 0=Not running, 1=Running, Active CPU, 2=Running, Inactive CPU (BIOS etc.)

SDL_sem *PIT0Lock = NULL;

void updatePIT0() //Timer tick Irq
{
	if (EMU_RUNNING==1) //Are we running?
	{
		currenttime += getuspassed(&timerticks); //Add the time that has passed!
		if (currenttime >= timertime) //Are we to trigger an interrupt?
		{
			for (;currenttime >= timertime;) currenttime -= timertime; //Decrease the timer time for as much as possible!
			doirq(0); //Execute IRQ0!
		}
	}
}

uint_32 speakercountdown, latch42, pit0latch, pit0command, pit0divisor, doclocktick, timerack;

//PC Speaker functionality in PIT

word PCSpeakerFrequency; //PC Speaker Frequency from the PIT!
byte PCSpeakerPort; //Port 0x61 for the PC Speaker!
byte PCSpeakerIsRunning; //Speaker is running?

void startOrUpdateSpeaker(float frequency) //Start or update the running frequency!
{
	if (__HW_DISABLED) return; //Abort!
	if (PCSpeakerIsRunning) //Already running: need to update the speaker?
	{
		//Update speaker frequency!
		setSpeakerFrequency(0,frequency);
	}
	else //Start the speaker at the given frequency!
	{
		//Start speaker with given frequency!
		PCSpeakerIsRunning = 1; //We're running!
		setSpeakerFrequency(0,frequency); //Set the frequency first!
		enableSpeaker(0); //Finally: enable the speaker!
	}
}

void stopSpeaker() //Stop the running speaker!
{
	if (__HW_DISABLED) return; //Abort!
	if (PCSpeakerIsRunning) //Running: able to stop?
	{
		//Stop the speaker!
		disableSpeaker(0); //Disable the speaker!
		PCSpeakerIsRunning = 0; //Not running anymore!
	}
}

void updatePCSpeakerFrequency() //PC Speaker frequency has been updated!
{
	if (__HW_DISABLED) return; //Abort!
	uint_32 freq;
	freq = PCSpeakerFrequency; //Default: the freq!
	if (freq==0) //0=>65536!
	{
		freq = 65536; //0=>65536
	}
	float frequency;
	frequency = (1193180.0f/freq); //Calculate the frequency (Hz)
	if ((PCSpeakerPort&0x3)==0x3) //Enabled?
	{
		//Now, do hardware stuff!
		startOrUpdateSpeaker(frequency); //Start or update the speaker!
	}
	else //Disabled?
	{
		//Now, do hardware stuff!
		stopSpeaker(); //Stop a speaker if running!
	}
}

void updatePCSpeaker() //PC Speaker register has been updated!
{
	if (__HW_DISABLED) return; //Abort!
	if ((PCSpeakerPort&0x3)==0x3) //Enabled?
	{
		//Enable PC speaker!
		updatePCSpeakerFrequency(); //Enable it!
	}
	else //Disabled?
	{
		//Disable PC speaker!
		updatePCSpeakerFrequency(); //Disable it!
	}
}

//NEW HANDLER
uint_32 calculatedpit0state; //Calculate state by time and last time handled!

byte in8253(word portnum, byte *result)
{
	if (__HW_DISABLED) return 0; //Abort!
	switch (portnum)
	{
		case 0x40:
		if (pit0latch==0)
		{
			//Calculate the current PIT0 state by frequency and time passed!
			uint_64 uspassed;
			const static float tickduration = (1.0f/1193180.0f)*1000000.0f; //How long does it take to process one tick in us?
			WaitSem(PIT0Lock); //Protect against corrupted values!
			uspassed = getuspassed_k(&timerticks); //How many time has passed since the last full state?
			PostSem(PIT0Lock);
			calculatedpit0state = pit0divisor; //Load the current divisor (1-65536)
			if (calculatedpit0state == 65536) calculatedpit0state = 0; //We start counting from 0 instead of 65536!
			calculatedpit0state -= ((float)uspassed/tickduration); //Count down the current PIT0 state!
			calculatedpit0state &= 0xFFFF; //Convert it to 16-bits value of the PIT!
			//Give the value!
			pit0latch = 1;
			*result = (calculatedpit0state & 0xFF);
		}
		else
		{
			pit0latch = 0;
			*result = ((calculatedpit0state >> 8) & 0xFF);
		}
		return 1;
		case 0x41:
		case 0x42:
			return 0; //Unsupported!
			break;
		case 0x43:
			*result = pit0command;
			return 1;
		case 0x61: //PC speaker? From original timer!
			*result = PCSpeakerPort; //Give the speaker port!
			return 1;
		default: //Unknown port?
			return 0; //Unknown port!
	}
	return 0; //Disabled!
}

byte out8253(word portnum, byte value)
{
	if (__HW_DISABLED) return 0; //Abort!
	byte old61; //For tracking updates!
	switch (portnum)
	{
		case 0x40: //pit 0 data port
		if (pit0latch==0) {
			pit0divisor = (pit0divisor & 0xFF00) + (value & 0xFF);
			pit0latch = 1;
			return 1;
			} else {
			pit0divisor = (pit0divisor & 0xFF) + (value & 0xFF)*256;
			pit0latch = 0;
			if (pit0divisor==0) pit0divisor = 65536;
			initTicksHolder(&timerticks); //Initialise the timer ticks!
			timertime = 1000000.0f / SAFEDIV(1193180.0f, pit0divisor); //How much time do we take to expire?
		} break;
		return 1;
		break;
		case 0x42: //speaker countdown
		if (latch42==0) {
			speakercountdown = (speakercountdown & 0xFF00) + value;
			latch42 = 1;
			} else {
			speakercountdown = (speakercountdown & 0xFF);
			speakercountdown |= (value<<8);
			latch42 = 0;
			PCSpeakerFrequency = speakercountdown; //Set the value of countdown for PC speaker!
			updatePCSpeaker(); //Update the PC speaker!
		}
		return 1;
		break;
		case 0x43: //pit 0 command port
		pit0command = value;
		switch (pit0command) {
			case 0x36: //reprogram pit 0 divisor
			pit0latch = 0; break;
			case 0xB6:
			latch42 = 0; break;
		}
		return 1;
		break;
		//From above original:
	case 0x61: //PC Speaker?
		old61 = PCSpeakerPort; //Old value!
		PCSpeakerPort = (value&3); //Set the new port value, only low 2 bits are used!
		if (old61!=PCSpeakerPort) //Port changed?
		{
			updatePCSpeaker(); //The PC Speaker status has been updated!
		}
		return 1;
		break;
	}
	return 0; //Unhandled!
}

void init8253() {
	if (__HW_DISABLED) return; //Abort!
	register_PORTOUT(&out8253);
	register_PORTIN(&in8253);
	PIT0Lock = getLock("PIT0"); //Get our lock to use!
}