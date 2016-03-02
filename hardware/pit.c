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
#include "headers/support/wave.h" //Wave support!

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
#define SPEAKER_RATE 44100
//Use actual response as speaker rate! 60us responses!
//#define SPEAKER_RATE (1000000.0f/60.0f)
//Speaker buffer size!
#define SPEAKER_BUFFER 4096
//The double buffering threshold!
#define PITDOUBLE_THRESHOLD SPEAKER_BUFFER
//Speaker low pass filter values (if defined, it's used)!
#define SPEAKER_LOWPASS (1000000.0f/60.0f)

//Precise timing rate!
//The clock speed of the PIT (14.31818MHz divided by 12)!
#define TIME_RATE (14318180.0f/12.0f)

//Log the speaker to this .wav file when defined (raw and duty cycles log)!
#define SPEAKER_LOGRAW "captures/speakerraw.wav"
#define SPEAKER_LOGDUTY "captures/speakerduty.wav"

//End of defines!

byte enablespeaker = 0; //Are we sounding the PC speaker?

#ifdef SPEAKER_LOGRAW
	WAVEFILE *speakerlograw = NULL; //The log file for the speaker output!
#endif
#ifdef SPEAKER_LOGDUTY
	WAVEFILE *speakerlogduty = NULL; //The log file for the speaker output!
#endif

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
	FIFOBUFFER *buffer, *doublebuffer; //Output buffers for rendering!
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
			readfifobuffer16(speaker->buffer, (word *)&s); //Not readable from the buffer? Duplicate last sample!

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
			readfifobuffer16(speaker->buffer, (word *)&s); //Not readable from the buffer? Duplicate last sample!
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

float speaker_currentsample = 0, speaker_last_result = 0, speaker_last_sample = 0;
byte speaker_first_sample = 1;

void tickPIT(double timepassed) //Ticks all PIT timers available!
{
	if (__HW_DISABLED) return;
	const float ticklength = (1.0f / SPEAKER_RATE)*TIME_RATE; //Length of PIT samples to process every output sample!
	register uint_32 length; //Amount of samples to generate!
	uint_32 i;
	uint_64 tickcounter;
	word oldvalue; //Old value before decrement!
	float tempf;
	uint_32 render_ticks; //A one shot tick!
	byte currentsample; //Saved sample in the 1.19MHz samples!
	byte channel; //Current channel?
	byte getIRQ; //IRQ triggered?

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
						if (PITchannels[channel].reload)
						{
							PITchannels[channel].reload = 0; //Not reloading!
							reloadticker(channel); //Reload the counter!
							PITchannels[channel].status = 1; //Next status: we're loaded and ready to run!
						}
						break;
					case 1: //We start counting to rise!!
						if (PITchannels[channel].gatewenthigh)
						{
							PITchannels[channel].gatewenthigh = 0; //Not anymore!
							PITchannels[channel].reload = 0; //Reloaded!
							reloadticker(channel); //Gate going high reloads the ticker immediately!
						}
						if ((PCSpeakerPort&1) || (channel<2)) //To tick at all?
						{
							PITchannels[channel].ticker -= 2; //Decrement by 2 instead?
							switch (PITchannels[channel].ticker)
							{
							case 0: //Even counts decreased to 0!
							case 0xFFFF: //Odd counts decreased to -1/0xFFFF.
								PITchannels[channel].channel_status ^= 1; //We're toggling during this phase!
								PITchannels[channel].reload = 0; //Reloaded!
								reloadticker(channel); //Reload the next value to tick!
								break;
							default: //No action taken!
								break;
							}
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
		getIRQ = 0; //Default: no IRQ yet!
		for (;readfifobuffer(PITchannels[0].rawsignal,&currentsample);) //Anything left to process?
		{
			if (((currentsample^IRQ0_status)&1) && currentsample) //Raised?
			{
				doirq(0); //Raise IRQ0!
				getIRQ = 1; //We've gotten an IRQ!
			}
			IRQ0_status = currentsample; //Update status!
			if (getIRQ) break; //IRQ gotten? Abort to receive the IRQ at the full speed possible! Take any other IRQs the next time we check for IRQs!
		}
	}

	//Timer 1 output is discarded! We're not connected to anything or unneeded to emulate DRAM refresh!
	//fifobuffer_clear(PITchannels[1].rawsignal); //Discard channel 1 output!
	
	//PC speaker output!
	speaker_ticktiming += timepassed; //Get the amount of time passed for the PC speaker (current emulated time passed according to set speed)!
	if ((speaker_ticktiming >= speaker_tick) && enablespeaker) //Enough time passed to render the physical PC speaker and enabled?
	{
		length = (uint_32)SAFEDIV(speaker_ticktiming, speaker_tick); //How many ticks to tick?
		speaker_ticktiming -= (length*speaker_tick); //Rest the amount of ticks!

		if (!FIFOBUFFER_LOCK) //Not locked?
		{
			lockaudio(); //Lock the audio!
		}

		//Ticks the speaker when needed!
		i = 0; //Init counter!
		uint_32 dutycyclei; //Input samples to process!
		//Generate the samples from the output signal!
		for (;;) //Generate samples!
		{
			//Average our input ticks!
			PITchannels[2].samplesleft += ticklength; //Add our time to the sample time processed!
			tempf = floorf(PITchannels[2].samplesleft); //Take the rounded number of samples to process!
			PITchannels[2].samplesleft -= tempf; //Take off the samples we've processed!
			render_ticks = (uint_32)tempf; //The ticks to render!

			//render_ticks contains the output samples to process! Calculate the duty cycle by low pass filter and use it to generate a sample!
			for (dutycyclei = render_ticks;dutycyclei;)
			{
				if (!readfifobuffer(PITchannels[2].rawsignal, &currentsample)) break; //Failed to read the sample? Stop counting!
				speaker_currentsample = currentsample?SHRT_MAX:SHRT_MIN; //Convert the current result to the 16-bit data, signed instead of unsigned!
				#ifdef SPEAKER_LOGRAW
					writeWAVMonoSample(speakerlograw,(short)speaker_currentsample); //Log the mono sample to the WAV file, converted as needed!
				#endif
				#ifdef SPEAKER_LOWPASS
					//We're applying the low pass filter for the speaker!
					applySoundLowpassFilter(SPEAKER_LOWPASS, TIME_RATE, &speaker_currentsample, &speaker_last_result, &speaker_last_sample, &speaker_first_sample);
				#endif
				#ifdef SPEAKER_LOGDUTY
					writeWAVMonoSample(speakerlogduty,(short)speaker_currentsample); //Log the mono sample to the WAV file, converted as needed!
				#endif
			}

			//Add the result to our buffer!
			writefifobuffer16(PITchannels[2].doublebuffer, (short)speaker_currentsample); //Write the sample to the buffer (mono buffer)!
			movefifobuffer16(PITchannels[2].doublebuffer,PITchannels[2].buffer,PITDOUBLE_THRESHOLD); //Move any data to the destination once filled!
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

void initSpeakers(byte soundspeaker)
{
	if (__HW_DISABLED) return; //Abort!
	//First speaker defaults!
	memset(&PITchannels, 0, sizeof(PITchannels)); //Initialise our data!
	enablespeaker = soundspeaker; //Are we to sound the speaker?
	byte i;
	for (i=0;i<3;i++)
	{
		PITchannels[i].rawsignal = allocfifobuffer(((uint_64)((2048.0f / SPEAKER_RATE)*TIME_RATE)) + 1, 0); //Nonlockable FIFO with 2048 word-sized samples with lock (TICK_RATE)!
		if (i==2 && enablespeaker) //Speaker?
		{
			PITchannels[i].buffer = allocfifobuffer(SPEAKER_BUFFER<<1, FIFOBUFFER_LOCK); //(non-)Lockable FIFO with X word-sized samples with lock!
			PITchannels[i].doublebuffer = allocfifobuffer((PITDOUBLE_THRESHOLD+1)<<1, 0); //FIFO with X word-sized samples without lock!
		}
	}
	speaker_ticktiming = time_ticktiming = 0.0f; //Initialise our timing!
	if (enablespeaker)
	{
		addchannel(&speakerCallback, &PITchannels[2], "PC Speaker", SPEAKER_RATE, SPEAKER_BUFFER, 0, SMPL16S); //Add the speaker at the hardware rate, mono! Make sure our buffer responds every 2ms at least!
		setVolume(&speakerCallback, &PITchannels[2], SPEAKER_VOLUME); //What volume?

#ifdef SPEAKER_LOGRAW
		domkdir("captures"); //Captures directory!
		speakerlograw = createWAV(SPEAKER_LOGRAW,1,(uint_32)TIME_RATE); //Start raw wave file logging!
#endif
#ifdef SPEAKER_LOGDUTY
		domkdir("captures"); //Captures directory!
		speakerlogduty = createWAV(SPEAKER_LOGDUTY,1,(uint_32)TIME_RATE); //Start duty wave file logging!
#endif
	}
}

void doneSpeakers()
{
	if (__HW_DISABLED) return;
	removechannel(&speakerCallback, &PITchannels[2], 0); //Remove the speaker!
	byte i;
	for (i=0;i<3;i++)
	{
		free_fifobuffer(&PITchannels[i].rawsignal); //Release the FIFO buffer we use!
		if (i==2 && enablespeaker) //Speaker?
		{
			free_fifobuffer(&PITchannels[i].buffer); //Release the FIFO buffer we use!
			free_fifobuffer(&PITchannels[i].doublebuffer); //Release the FIFO buffer we use!
		}
	}
	#ifdef SPEAKER_LOGRAW
		if (enablespeaker)
		{
			closeWAV(&speakerlograw); //Stop wave file logging!
		}
	#endif
	#ifdef SPEAKER_LOGDUTY
		if (enablespeaker)
		{
			closeWAV(&speakerlogduty); //Stop wave file logging!
		}
	#endif
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
