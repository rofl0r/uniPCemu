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

#include "headers/support/highrestimer.h" //High resolution timer support for current value!
#include "headers/support/locks.h" //Locking support!

//PC speaker support functionality:
#include "headers/emu/emu_misc.h" //Random generators!
#include "headers/support/fifobuffer.h" //FIFO sample buffer support!

//Are we disabled?
#define __HW_DISABLED 0

/*

PC SPEAKER

*/

//To lock the FIFO buffer during rendering? If disabled, the entire audio thread is locked instead (might have consequences on other audio output).
#define FIFOBUFFER_LOCK 1

//Are we disabled?
#define __HW_DISABLED 0

//What volume, in percent!
#define SPEAKER_VOLUME 100.0f

//Speaker playback rate!
#define SPEAKER_RATE 44100.0f
//Use actual response as speaker rate! 60us responses!
//#define SPEAKER_RATE (1000000.0f/60.0f)
//Speaker buffer size!
#define SPEAKER_BUFFER 1024

#define TIME_RATE 1193182.0f

double speaker_ticktiming; //Both current clocks!
double speaker_tick = (1000000000.0f / SPEAKER_RATE); //Time of a tick in the PC speaker sample!
double time_tick = (1000000000.0f / TIME_RATE); //Time of a tick in the PIT!

byte IRQ0_status = 0; //Current IRQ0 status!

byte oldPCSpeakerPort = 0x00;
extern byte PCSpeakerPort; //Port 0x61 for the PC Speaker! Bit0=Gate, Bit1=Data enable

extern byte EMU_RUNNING; //Current emulator status!

double time_ticktiming; //Current timing!

typedef struct
{
	byte mode; //PC speaker mode!
	word frequency; //Frequency divider that has been set!
	byte status; //Status of the counter!
	FIFOBUFFER *buffer; //Output buffers for rendering!
	word ticker; //16-bit ticks!
	byte reload; //Reload requested?
	byte channel_status; //Current output status!
	byte gatewenthigh; //Gate went high?

	//Output generating timer!
	float samples; //Output samples to process for the current audio tick!
	float samplesleft; //Samples left to process!
	FIFOBUFFER *rawsignal; //The raw signal buffer for the oneshot mode!
} PITCHANNEL; // speaker!

PITCHANNEL PITchannels[3]; //All possible PC speakers, whether used or not!

byte speakerCallback(void* buf, uint_32 length, byte stereo, void *userdata) {
	static sword s = 0; //Last sample!
	uint_32 i;
	PITCHANNEL *speaker; //Convert to the current speaker!

	if (__HW_DISABLED) return 0; //Abort!	

								 //First, our information sources!
	speaker = (PITCHANNEL *)userdata; //Active speaker!

	i = 0; //Init counter!
	if (stereo) //Stereo samples?
	{
		register sample_stereo_p ubuf_stereo = (sample_stereo_p)buf; //Active buffer!
		for (;;) //Process all samples!
		{ //Process full length!
			readfifobuffer16(speaker->buffer, &s); //Not readable from the buffer? Duplicate last sample!

			ubuf_stereo->l = ubuf_stereo->r = s; //Single channel!
			++ubuf_stereo; //Next item!
			if (++i == length) break; //Next item!
		}
	}
	else //Mono samples?
	{
		register sample_p ubuf_mono = (sample_p)buf; //Active buffer!
		for (;;)
		{ //Process full length!
			readfifobuffer16(speaker->buffer, &s); //Not readable from the buffer? Duplicate last sample!
			*ubuf_mono = s; //Mono channel!
			++ubuf_mono; //Next item!
			if (++i == length) break; //Next item!
		}
	}

	return SOUNDHANDLER_RESULT_FILLED; //We're filled!
}

OPTINLINE void reloadticker(byte channel)
{
	PITchannels[channel].ticker = PITchannels[channel].frequency; //Reload the start value!
}

byte channel_reload[3] = {0,0,0}; //To reload the channel next cycle?

OPTINLINE float calcSpeakerLowpassFilter(float cutoff_freq, float samplerate, float currentsample, float previousresult)
{
	float RC = (float)1.0f / (cutoff_freq * (float)2 * (float)3.14);
	float dt = (float)1.0f / samplerate;
	float alpha = dt / (RC + dt);
	return previousresult + (alpha*(currentsample - previousresult));
}

OPTINLINE float calcSpeakerHighpassFilter(float cutoff_freq, float samplerate, float currentsample, float previoussample, float previousresult)
{
	float RC = 1.0 / (cutoff_freq * 2 * 3.14);
	float dt = 1.0 / samplerate;
	float alpha = RC / (RC + dt);
	return alpha * (previousresult + currentsample - previoussample);
}

OPTINLINE void applySpeakerLowpassFilter(sword *currentsample)
{
	static sword last_result = 0, last_sample = 0;
	static byte first_sample = 1;

	if (first_sample) //No last?
	{
		last_result = last_sample = *currentsample; //Save the current sample!
		first_sample = 0;
		return; //Abort: don't filter the first sample!
	}
	last_result = (sword)calcSpeakerLowpassFilter(22050.0f, TIME_RATE, (float)*currentsample, last_result); //20kHz low pass filter!
	last_sample = *currentsample; //The last sample that was processed!
	*currentsample = last_result; //Give the new result!
}

OPTINLINE void applySpeakerHighpassFilter(sword *currentsample)
{
	static sword last_result = 0, last_sample = 0;
	static byte first_sample = 1;

	if (first_sample) //No last?
	{
		last_result = last_sample = *currentsample; //Save the current sample!
		first_sample = 0;
		return; //Abort: don't filter the first sample!
	}
	last_result = (sword)calcSpeakerHighpassFilter(18.2f, TIME_RATE, (float)*currentsample, last_sample,last_result); //1Hz high pass filter!
	last_sample = *currentsample; //The last sample that was processed!
	*currentsample = last_result; //Give the new result!
}

void tickPIT(double timepassed) //Ticks all PIT timers available!
{
	if (__HW_DISABLED) return;
	const float ticklength = (1.0f / SPEAKER_RATE)*TIME_RATE; //Length of PIT samples to read every output sample!
	const float speakerMovement = (USHRT_MAX/((60.0f/1000000.0f)*TIME_RATE)); //Speaker movement (in 16-bits) for every positive/negative PIT sample(linear movement)!
	register uint_32 length; //Amount of samples to generate!
	uint_32 i;
	uint_64 tickcounter;
	word oldvalue; //Old value before decrement!
	float tempf;
	uint_32 render_ticks; //A one shot tick!
	uint_32 dutycyclei; //Calculated duty cycle!
	byte currentsample; //Saved sample in the 1.19MHz samples!
	byte channel; //Current channel?

	time_ticktiming += timepassed; //Add the amount of time passed to the PIT timing!

	//Render 1.19MHz samples for the time that has passed!
	length = (uint_32)SAFEDIV(time_ticktiming, time_tick); //How many ticks to tick?
	time_ticktiming -= (length*time_tick); //Rest the amount of ticks!

	if (length) //Anything to tick at all?
	{
		for (channel=0;channel<3;channel++)
		{
			byte mode,outputmask;
			mode = PITchannels[channel].mode; //Current mode!
			outputmask = (channel==2)?((PCSpeakerPort&2)>>1):1; //Mask output on/off for this timer!

			switch (mode) //What mode are we rendering?
			{
			case 0: //Interrupt on Terminal Count? Is One-Shot without Gate Input?
			case 1: //One-shot mode?
				for (tickcounter = length;tickcounter;--tickcounter) //Tick all needed!
				{
					//Length counts the amount of ticks to render!
					switch (PITchannels[channel].status) //What status?
					{
					case 0: //Output goes low/high?
						PITchannels[channel].channel_status = mode; //We're high when mode 1, else low with mode 0!
						PITchannels[channel].status = 1; //Skip to 1: we're ready to run already!
						break;
					case 1: //Wait for next rising edge of gate input?
						if (!mode) //No wait on mode 0?
						{
							PITchannels[channel].status = 2;
							goto mode0_2;
						}
						else if (PITchannels[channel].gatewenthigh) //Mode 1 waits for gate to become high!
						{
							PITchannels[channel].gatewenthigh = 0; //Not went high anymore!
							PITchannels[channel].status = 2;
							goto mode0_2;
						}
						break;
					case 2: //Output goes low and we start counting to rise! After timeout we become 4(inactive) with mode 1!
						mode0_2:
						if (PITchannels[channel].reload)
						{
							PITchannels[channel].reload = 0; //Not reloading anymore!
							PITchannels[channel].channel_status = 0; //Lower output!
							reloadticker(channel); //Reload the counter!
						}

						oldvalue = PITchannels[channel].ticker; //Save old ticker for checking for overflow!

						if (mode) --PITchannels[channel].ticker; //Mode 1 always ticks?
						else if ((PCSpeakerPort&1) || (channel<2)) --PITchannels[channel].ticker; //Mode 0 ticks when gate is high!

						if ((!PITchannels[channel].ticker) && oldvalue) //Timeout when ticking? We're done!
						{
							PITchannels[channel].channel_status = 1; //We're high again!
						}
						break;
					case 4: //Inactive?
						break;
					default: //Unsupported! Ignore any input!
						break;
					}
					writefifobuffer(PITchannels[channel].rawsignal, PITchannels[channel].channel_status&outputmask); //Add the data to the raw signal!
				}
				break;
			case 2: //Also Rate Generator mode?
			case 6: //Rate Generator mode?
				for (tickcounter = length;tickcounter;--tickcounter) //Tick all needed!
				{
					//Length counts the amount of ticks to render!
					switch (PITchannels[channel].status) //What status?
					{
					case 0: //Output going high! See below! Wait for reload register to be written!
						PITchannels[channel].channel_status = 1; //We're high!
						break;
					case 1: //We're starting the count?
						if (PITchannels[channel].reload)
						{
							reload2:
							PITchannels[channel].reload = 0; //Not reloading!
							reloadticker(channel); //Reload the counter!
							PITchannels[channel].channel_status = 1; //We're high!
							PITchannels[channel].status = 2; //Start counting!
						}
						break;
					case 2: //We start counting to rise!!
						if (PITchannels[channel].gatewenthigh) //Gate went high?
						{
							PITchannels[channel].gatewenthigh = 0; //Not anymore!
							goto reload2; //Reload and execute!
						}
						if (((PCSpeakerPort & 1) && (channel==2)) || (channel<2)) //We're high or undefined?
						{
							--PITchannels[channel].ticker; //Decrement?
							switch (PITchannels[channel].ticker) //Two to one? Go low!
							{
							case 1:
								PITchannels[channel].channel_status = 0; //We're going low during this phase!
								break;
							case 0:
								PITchannels[channel].channel_status = 1; //We're going high again during this phase!
								reloadticker(channel); //Reload the counter!
								break;
							default: //No action taken!
								break;
							}
						}
						else //We're low? Output=High and wait for reload!
						{
							PITchannels[channel].channel_status = 1; //We're going high again during this phase!
						}
						break;
					default: //Unsupported! Ignore any input!
						break;
					}
					writefifobuffer(PITchannels[channel].rawsignal, PITchannels[channel].channel_status&outputmask); //Add the data to the raw signal!
				}
				break;
			//mode 2==6 and mode 3==7.
			case 7: //Also Square Wave mode?
			case 3: //Square Wave mode?
				for (tickcounter = length;tickcounter;--tickcounter) //Tick all needed!
				{
					//Length counts the amount of ticks to render!
					switch (PITchannels[channel].status) //What status?
					{
					case 0: //Output going high! See below! Wait for reload register to be written!
						PITchannels[channel].channel_status = 1; //We're high!
						break;
					case 1: //We start counting to rise!!
						if (PITchannels[channel].reload)
						{
							PITchannels[channel].reload = 0; //Not reloading!
							reloadticker(channel); //Reload the counter!
						}
						oldvalue = PITchannels[channel].ticker; //Save old ticker for checking for overflow!
						if ((PCSpeakerPort & 1) && (channel==2)) --PITchannels[channel].ticker; //Decrement by 2?
						--PITchannels[channel].ticker; //Always decrease by 1 at least!
						if (((PITchannels[channel].ticker == 0xFFFF) && oldvalue) || (!PITchannels[channel].ticker)) //Timeout when ticks to 0 or overflow with two ticks? We're done!
						{
							PITchannels[channel].channel_status = !PITchannels[channel].channel_status; //We're toggling during this phase!
							reloadticker(channel);
						}
						break;
					default: //Unsupported! Ignore any input!
						break;
					}
					writefifobuffer(PITchannels[channel].rawsignal, PITchannels[channel].channel_status&outputmask); //Add the data to the raw signal!
				}
				break;
			case 4: //Software Triggered Strobe?
			case 5: //Hardware Triggered Strobe?
				for (tickcounter = length;tickcounter;--tickcounter) //Tick all needed!
				{
					switch (PITchannels[channel].status) //What status?
					{
					case 0: //Output going high! See below! Wait for reload register to be written!
						PITchannels[channel].channel_status = 1; //We're high!
						break;
					case 1: //We're starting the count or waiting for rising gate(mode 5)?
						if (PITchannels[channel].reload)
						{
						pit45_reload: //Reload PIT modes 4&5!
							if ((mode == 4) || ((PITchannels[channel].gatewenthigh) && (mode == 5))) //Reload when allowed!
							{
								PITchannels[channel].gatewenthigh = 0; //Reset gate high flag!
								PITchannels[channel].reload = 0; //Not reloading!
								reloadticker(channel); //Reload the counter!
								PITchannels[channel].status = 2; //Start counting!
							}
						}
						break;
					case 2: //We start counting to rise!!
					case 3: //We're counting, but ignored overflow?
						if (PITchannels[channel].reload || (((mode==5) && PITchannels[channel].gatewenthigh))) //We're reloaded?
						{
							goto pit45_reload; //Reload when allowed!
						}
						if (((PCSpeakerPort & 1) && (channel == 2)) || (channel<2)) //We're high or undefined?
						{
							--PITchannels[channel].ticker; //Decrement?
							if (!PITchannels[channel].ticker && (PITchannels[channel].status!=3)) //One to zero? Go low when not overflown already!
							{
								PITchannels[channel].channel_status = 0; //We're going low during this phase!
								PITchannels[channel].status = 3; //We're ignoring any further overflows from now on!
							}
							else
							{
								PITchannels[channel].channel_status = 1; //We're going high again any other phase!
							}
						}
						else //We're low? Output=High and wait for reload!
						{
							PITchannels[channel].channel_status = 1; //We're going high again during this phase!
						}
						break;
					default: //Unsupported mode! Ignore any input!
						break;
					}
					writefifobuffer(PITchannels[channel].rawsignal, PITchannels[channel].channel_status&outputmask); //Add the data to the raw signal!
				}
				break;
			}
		}
	}

	//IRQ0 output!
	if (EMU_RUNNING == 1) //Are we running? We allow timers to execute!
	{
		for (;readfifobuffer(PITchannels[0].rawsignal,&currentsample);) //Anything left to process?
		{
			if (((currentsample^IRQ0_status)&1) && currentsample) //Raised?
			{
				doirq(0); //Raise IRQ0!
			}
			IRQ0_status = currentsample; //Update status!
		}
	}

	//Timer 1 output is discarded! We're not connected to anything or unneeded to emulate DRAM refresh!
	fifobuffer_clear(PITchannels[1].rawsignal); //Discard channel 1 output!
	
	//PC speaker output!
	speaker_ticktiming += timepassed; //Get the amount of time passed for the PC speaker (current emulated time passed according to set speed)!
	if (speaker_ticktiming >= speaker_tick) //Enough time passed to render the physical PC speaker?
	{
		length = (uint_32)SAFEDIV(speaker_ticktiming, speaker_tick); //How many ticks to tick?
		speaker_ticktiming -= (length*speaker_tick); //Rest the amount of ticks!

		if (!FIFOBUFFER_LOCK) //Not locked?
		{
			lockaudio(); //Lock the audio!
		}

		//Ticks the speaker when needed!
		i = 0; //Init counter!
		short s; //Set the channels! We generate 1 sample of output here!
		sword sample; //Current sample!
		static float currentduty=0.0f; //Currently calculated duty for the current sample(s)!
		//Generate the samples from the output signal!
		for (;;) //Generate samples!
		{
			//Average our input ticks!
			PITchannels[2].samplesleft += ticklength; //Add our time to the one-shot samples!
			tempf = floorf(PITchannels[2].samplesleft); //Take the rounded number of samples to process!
			PITchannels[2].samplesleft -= tempf; //Take off the samples we've processed!
			render_ticks = (uint_32)tempf; //The ticks to render!

			//render_ticks contains the samples to process! Calculate the duty cycle and use it to generate a sample!
			for (dutycyclei = render_ticks;dutycyclei;)
			{
				if (!readfifobuffer(PITchannels[2].rawsignal, &currentsample)) break; //Failed to read the sample? Stop counting!
				if (currentsample) //Increase to 1?
				{
					if (currentduty<SHRT_MAX) //Not full yet?
					{
						currentduty += speakerMovement; //Move ON!
						if (currentduty>=SHRT_MAX) currentduty = SHRT_MAX; //Limit to maximum voltage!
					}
				}
				else //Decrease to 0?
				{
					if (currentduty>SHRT_MIN) //Not full yet?
					{
						currentduty -= speakerMovement; //Move ON!
						if (currentduty<=SHRT_MIN) currentduty = SHRT_MIN; //Limit to minimum voltage!
					}
				}
			}

			s = (short)currentduty; //Convert available duty cycle to full average factor!
			applySpeakerLowpassFilter(&s); //Low pass filter the signal for safety to output!
			applySpeakerHighpassFilter(&s); //High pass filter for the same reason!

			//Add the result to our buffer!
			writefifobuffer16(PITchannels[2].buffer, s); //Write the sample to the buffer (mono buffer)!
			if (++i == length) //Fully rendered?
			{
				if (!FIFOBUFFER_LOCK) //Not locked?
				{
					unlockaudio(); //Unlock the audio!
				}
				return; //Next item!
			}
		}
		if (!FIFOBUFFER_LOCK) //Not locked?
		{
			unlockaudio(); //Unlock the audio!
		}
	}
}

void initSpeakers()
{
	if (__HW_DISABLED) return; //Abort!
	//First speaker defaults!
	memset(&PITchannels, 0, sizeof(PITchannels)); //Initialise our data!
	byte i;
	for (i=0;i<3;i++)
	{
		PITchannels[i].rawsignal = allocfifobuffer(((uint_64)((2048.0f / SPEAKER_RATE)*TIME_RATE)) + 1, 0); //Nonlockable FIFO with 2048 word-sized samples with lock (TICK_RATE)!
		if (i==2) //Speaker?
		{
			PITchannels[i].buffer = allocfifobuffer((SPEAKER_BUFFER+1)<<1, FIFOBUFFER_LOCK); //(non-)Lockable FIFO with X word-sized samples with lock!
		}
	}
	addchannel(&speakerCallback, &PITchannels[2], "PC Speaker", SPEAKER_RATE, SPEAKER_BUFFER, 0, SMPL16S); //Add the speaker at the hardware rate, mono! Make sure our buffer responds every 2ms at least!
	setVolume(&speakerCallback, &PITchannels[2], SPEAKER_VOLUME); //What volume?
}

void doneSpeakers()
{
	if (__HW_DISABLED) return;
	removechannel(&speakerCallback, &PITchannels[2], 0); //Remove the speaker!
	byte i;
	for (i=0;i<3;i++)
	{
		free_fifobuffer(&PITchannels[i].rawsignal); //Release the FIFO buffer we use!
		if (i==2) //Speaker?
		{
			free_fifobuffer(&PITchannels[i].buffer); //Release the FIFO buffer we use!
		}
	}
}

void speakerGateUpdated()
{
	if (((oldPCSpeakerPort ^ PCSpeakerPort) & 0x1) && (PCSpeakerPort & 0x1)) //Risen?
	{
		PITchannels[2].gatewenthigh = 1; //We went high!
	}
	oldPCSpeakerPort = PCSpeakerPort; //Save new value!
}

void setPITFrequency(byte channel, word frequency) //Set the new frequency!
{
	if (__HW_DISABLED) return; //Abort!
	PITchannels[channel].frequency = frequency;
	PITchannels[channel].reload = 1; //We've been reloaded!
	if (PITchannels[channel].status == 0) //First step?
	{
		//Wait for next rising edge of gate input(mode 1/4) or start counting (mode 0/2/3/5/6/7)?
		PITchannels[channel].status = 1; //Wait for next rising edge of gate input!
	}
}

void setPITMode(byte channel, byte mode)
{
	if (__HW_DISABLED) return; //Abort!
	PITchannels[channel].mode = (mode&7); //Set the current PC speaker mode! Protect against invalid modes!
	PITchannels[channel].status = 0; //Output going high! Wait for reload to be set!
	PITchannels[channel].reload = 0; //Init to be sure!
}

extern byte EMU_RUNNING; //Emulator running? 0=Not running, 1=Running, Active CPU, 2=Running, Inactive CPU (BIOS etc.)

void cleanPIT()
{
	//We don't do anything: we're locked to CPU speed instead!
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
	pitlatch[channel] = PITchannels[channel].ticker; //Convert it to 16-bits value of the PIT and latch it!
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
				pitdivisor[pit] = (value & 0xFF);
				break;
			case 0x20: //Hi mode?
				pitdivisor[pit] = ((value & 0xFF)<<8);
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
			setPITFrequency(pit,pitdivisor[pit]); //Set the new divisor!
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
				if (value&0x30) //Not latching?
				{
					setPITMode(channel,(value>>1)&7); //Update the PIT mode when needed!
				}
				else //Latch count value?
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
