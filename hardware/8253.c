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

//Are we disabled?
#define __HW_DISABLED 0

uint64_t timerticks; //Done externally!
double timerfreq; //Done externally!

void Timer_Tick() //Timer tick Irq (18.2 times/sec)
{
	doirq(0); //Execute IRQ: System timer (55ms intervals)!
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

extern byte EMU_RUNNING; //Emulator running? 0=Not running, 1=Running, Active CPU, 2=Running, Inactive CPU (BIOS etc.)

void pit0handler() //PIT0 timeout handler!
{
	if (__HW_DISABLED) return; //Abort!
	if (EMU_RUNNING==1) //The emulator is running and CPU is active?
	{
		doirq(0); //Do the IRQ, if any!
	}
}

byte in8253(word portnum)
{
	if (__HW_DISABLED) return 0; //Abort!
switch (portnum)
{
		case 0x40:
		if (pit0latch==0) {
			pit0latch = 1;
			return(pit0divisor & 0xFF);
			} else {
			pit0latch = 0;
			return((pit0divisor >> 8) & 0xFF);
		} break;
		case 0x43:
		return(pit0command);
		case 0x61: //PC speaker? From original timer!
			return PCSpeakerPort; //Give the speaker port!
}
return 0; //Disabled!
}

void out8253(word portnum, byte value)
{
	if (__HW_DISABLED) return; //Abort!
	byte old61; //For tracking updates!
	switch (portnum)
	{
		case 0x40: //pit 0 data port
		switch (pit0command) {
			case 0x36:
			if (pit0latch==0) {
				pit0divisor = (pit0divisor & 0xFF00) + (value & 0xFF);
				pit0latch = 1;
				return;
				} else {
				pit0divisor = (pit0divisor & 0xFF) + (value & 0xFF)*256;
				pit0latch = 0;
				if (pit0divisor==0) pit0divisor = 65536;
				timerticks = SAFEDIV(timerfreq,(SAFEDIV(1193180,pit0divisor))); //Init ticks!
				addtimer(SAFEDIV(1193180.0f,pit0divisor),&pit0handler,"PIT0"); //PIT0 handler update!
				return;
			} break;
		} break;
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
		} break;
		case 0x43: //pit 0 command port
		pit0command = value;
		switch (pit0command) {
			case 0x36: //reprogram pit 0 divisor
			pit0latch = 0; break;
			case 0xB6:
			latch42 = 0; break;
		} break;
		//From above original:
	case 0x61: //PC Speaker?
		old61 = PCSpeakerPort; //Old value!
		PCSpeakerPort = value; //Set the new port value!
		if ((old61&0x3)!=(PCSpeakerPort&0x3)) //Port changed?
		{
			updatePCSpeaker(); //The PC Speaker status has been updated!
		}
		break;
	}
}

void init8253() {
	if (__HW_DISABLED) return; //Abort!
	memset (&i8253, 0, sizeof (i8253) );
	register_PORTOUT(0x40,&out8253);
	register_PORTOUT(0x41,&out8253);
	register_PORTOUT(0x42,&out8253);
	register_PORTOUT(0x43,&out8253);
	register_PORTIN(0x40,&in8253);
	register_PORTIN(0x41,&in8253);
	register_PORTIN(0x42,&in8253);
	register_PORTIN(0x43,&in8253);
	//PC Speaker port register!
	register_PORTOUT(0x61,&out8253);
	register_PORTIN(0x61,&in8253);
}