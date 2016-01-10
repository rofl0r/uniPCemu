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
//Speaker buffer size!
#define SPEAKER_BUFFER 512

#define TIME_RATE 1193182.0f

TicksHolder speaker_ticker;
uint_64 speaker_ticktiming, time_ticktiming; //Both current clocks!
uint_64 speaker_tick = (uint_64)(1000000000.0f / SPEAKER_RATE); //Time of a tick in the PC speaker sample!
uint_64 time_tick = (uint_64)(1000000000.0f / TIME_RATE); //Time of a tick in the PIT!

byte oldPCSpeakerPort = 0x00;
extern byte PCSpeakerPort; //Port 0x61 for the PC Speaker! Bit0=Gate, Bit1=Data enable

typedef struct
{
	byte mode; //PC speaker mode!
	word frequency; //Frequency divider that has been set!
	byte status; //Status of the counter!
	FIFOBUFFER *buffer; //Output buffer for rendering!
	word ticker; //16-bit ticks!
	byte reload; //Reload requested?

				 //Output generating timer!
	float samples; //Output samples to process for the current audio tick!
	float samplesleft; //Samples left to process!
	FIFOBUFFER *rawsignal; //The raw signal buffer for the oneshot mode!
} SPEAKER_INFO; //One speaker!

SPEAKER_INFO speaker; //All possible PC speakers, whether used or not!

byte speakerCallback(void* buf, uint_32 length, byte stereo, void *userdata) {
	uint_32 i;
	SPEAKER_INFO *speaker; //Convert to the current speaker!
	short s; //The sample!

	if (__HW_DISABLED) return 0; //Abort!	

								 //First, our information sources!
	speaker = (SPEAKER_INFO *)userdata; //Active speaker!

	i = 0; //Init counter!
	if (stereo) //Stereo samples?
	{
		register sample_stereo_p ubuf_stereo = (sample_stereo_p)buf; //Active buffer!
		for (;;) //Process all samples!
		{ //Process full length!
			if (!readfifobuffer16(speaker->buffer, &s)) //Not readable from the buffer?
			{
				s = 0; //Silence!
			}

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
			if (!readfifobuffer16(speaker->buffer, &s)) //Not readable from the buffer?
			{
				s = 0; //Silence!
			}
			*ubuf_mono = s; //Mono channel!
			++ubuf_mono; //Next item!
			if (++i == length) break; //Next item!
		}
	}

	return SOUNDHANDLER_RESULT_FILLED; //We're filled!
}

void reloadticker()
{
	speaker.ticker = speaker.frequency; //Reload!
}

void tickSpeakers() //Ticks all PC speakers available!
{
	if (__HW_DISABLED) return;
	//const float sampleLength = 1.0f / SPEAKER_RATE;
	const float ticklength = (1.0f / SPEAKER_RATE)*TIME_RATE; //Length of one shot samples to read every sample!
	const float scaleFactor = 2.0f*(SHRT_MAX - 1.0f); //Scaling from -0.5-0.5
	register uint_32 length; //Amount of samples to generate!
	uint_32 i;
	uint_64 timepassed;
	//float oneshot_temptiming, oneshot_timing;
	uint_64 tickcounter;
	//word oneshot_ticktotal; //Total amount of ticks for the oneshot!
	word oldvalue; //Old value before decrement!
	float tempf;
	uint_32 render_ticks; //A one shot tick!
	uint_32 dutycyclei; //Calculated duty cycle!
	uint_32 dutycycle; //Total counted duty cycle!
	byte currentsample; //Saved sample in the 1.19MHz samples!

	timepassed = (uint_64)getnspassed(&speaker_ticker); //Get the amount of time passed!
	speaker_ticktiming += timepassed; //Get the amount of time passed!
	time_ticktiming += timepassed; //Get the amount of time passed!

								   //Ticks the speaker when needed!

	static byte speaker_reload = 0;
	static byte speaker_status = 0; //Current speaker status!

									//Render 1.19MHz samples for the time that has passed!
	length = SAFEDIV(time_ticktiming, time_tick); //How many ticks to tick?
	time_ticktiming -= (length*time_tick); //Rest the amount of ticks!

	switch (speaker.mode) //One-shot mode?
	{
	case 0: //Interrupt on Terminal Count? Is One-Shot without Gate Input?
	case 1: //One-shot mode?
		for (tickcounter = length;tickcounter;--tickcounter) //Tick all needed!
		{
			//Length counts the amount of ticks to render!
			switch (speaker.status) //What status?
			{
			case 1: //Output goes low/high?
				speaker_status = speaker.mode; //We're high when mode 1, else low!
				break;
			case 2: //Wait for next rising edge of gate input?
				if (!speaker.mode) //No wait on mode 0?
				{
					speaker.status = 3;
					goto mode0_3;
				}
				break;
			case 3: //Output goes low and we start counting to rise! After timeout we become 4(inactive) with mode 1!
				mode0_3:
				speaker_status = 0; //We're low during this phase!
				if (--speaker.ticker == 0xFFFF) //Timeout? We're done!
				{
					speaker_status = 1; //We're high again!
					if (speaker.mode) speaker.status = 4; //We're inactive with Single Shot!
				}
				break;
			case 4: //Inactive?
				break;
			default: //Unsupported! Ignore any input!
				break;
			}
			if (!(PCSpeakerPort & 2)) continue; //Speaker is not turned on? Don't generate a signal!
			writefifobuffer(speaker.rawsignal, speaker_status); //Add the data to the raw signal!
		}
		break;
	default: //Unsupported mode Default to square wave mode!
	//Mode 2 is useless for generating sound?
	//mode 2==6 and mode 3==7.
	case 7: //Also Square Wave mode?
	case 3: //Square Wave mode?
		for (tickcounter = length;tickcounter;--tickcounter) //Tick all needed!
		{
			//Length counts the amount of ticks to render!
			switch (speaker.status) //What status?
			{
			case 0: //Output going high! See below! Wait for reload register to be written!
				speaker_status = 1; //We're high!
				speaker_reload = 1; //Request reload next cycle we're active!
				break;
			case 1: //Output goes low and we start counting to rise! After timeout we become 4(inactive)!
				if (speaker_reload)
				{
					speaker_reload = 0; //Not reloading!
					reloadticker(); //Reload the counter without ticking it!
					goto speaker3ready; //Don't tick!
				}
				oldvalue = speaker.ticker; //Save old ticker for checking for overflow!
				if (PCSpeakerPort & 1) --speaker.ticker; //Decrement by 2?
				--speaker.ticker; //Always decrease by 1 at least!
				if (speaker.ticker > oldvalue) //Timeout? We're done!
				{
					speaker_status = !speaker_status; //We're toggling during this phase!
					reloadticker();
				}
				break;
			default: //Unsupported! Ignore any input!
				break;
			}
		speaker3ready: //Speaker #3 ready to process?
			if (!(PCSpeakerPort & 2)) continue; //Speaker is not turned on? Don't generate a signal!
			writefifobuffer(speaker.rawsignal, speaker_status); //Add the data to the raw signal!
		}
		break;
	}

	if (speaker_ticktiming >= speaker_tick) //Enough time passed to render?
	{
		length = SAFEDIV(speaker_ticktiming, speaker_tick); //How many ticks to tick?
		speaker_ticktiming -= (length*speaker_tick); //Rest the amount of ticks!

		if (!FIFOBUFFER_LOCK) //Not locked?
		{
			lockaudio(); //Lock the audio!
		}

		//Ticks the speaker when needed!
		i = 0; //Init counter!
		if (PCSpeakerPort & 2) //Speaker is turned on?
		{
			short s; //Set the channels! We generate 1 sample of output here!
					 //Generate the samples from the output signal!
			for (;;) //Generate samples!
			{
				//Average our input ticks!
				speaker.samplesleft += ticklength; //Add our time to the one-shot samples!
				tempf = floor((double)speaker.samplesleft); //Take the rounded number of samples to process!
				speaker.samplesleft -= tempf; //Take off the samples we've processed!
				render_ticks = (uint_32)tempf; //The ticks to render!

											   //render_ticks contains the samples to process! Calculate the duty cycle and use it to generate a sample!
				dutycycle = 0; //Start with nothing!
				for (dutycyclei = render_ticks;dutycyclei;)
				{
					if (!readfifobuffer(speaker.rawsignal, &currentsample)) break; //Failed to read the sample? Stop counting!
					dutycycle += currentsample; //Add the sample to the duty cycle!
					--dutycyclei; //Decrease!
				}

				if (!render_ticks) //Invalid?
				{
					s = 0; //Nothing to process!
				}
				else
				{
					s = (short)(((((float)((float)dutycycle / (float)render_ticks)) - 0.5f)*scaleFactor)); //Convert duty cycle to full factor!
				}

				//Add the result to our buffer!
				writefifobuffer16(speaker.buffer, s); //Write the sample to the buffer (mono buffer)!
				if (++i == length)
				{
					if (!FIFOBUFFER_LOCK) //Not locked?
					{
						unlockaudio(); //Unlock the audio!
					}
					return; //Next item!
				}
			}
		}
		else //Not on? Generate quiet signal!
		{
		quietness:
			//Generate a quiet signal!
			for (;;) //Generate samples!
			{
				writefifobuffer16(speaker.buffer, 0); //Write the sample to the buffer (mono buffer)!
				if (++i == length)
				{
					if (!FIFOBUFFER_LOCK) //Not locked?
					{
						unlockaudio(); //Lock the audio!
					}
					return; //Next item!
				}
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
	initTicksHolder(&speaker_ticker); //Initialise our ticks holder!
									  //First speaker defaults!
	memset(&speaker, 0, sizeof(speaker)); //Initialise our data!
	speaker.rawsignal = allocfifobuffer(((uint_64)((2048.0f / SPEAKER_RATE)*TIME_RATE)) + 1, 0); //Nonlockable FIFO with 1024 word-sized samples with lock (TICK_RATE)!
	speaker.buffer = allocfifobuffer(2048, FIFOBUFFER_LOCK); //(non-)Lockable FIFO with 1024 word-sized samples with lock!
	addchannel(&speakerCallback, &speaker, "PC Speaker", SPEAKER_RATE, SPEAKER_BUFFER, 0, SMPL16S); //Add the speaker at the hardware rate, mono! Make sure our buffer responds every 2ms at least!
	setVolume(&speakerCallback, &speaker, SPEAKER_VOLUME); //What volume?
}

void doneSpeakers()
{
	if (__HW_DISABLED) return;
	removechannel(&speakerCallback, &speaker, 0); //Remove the speaker!
	free_fifobuffer(&speaker.buffer); //Release the FIFO buffer we use!
									  //Cleanup the speaker info!
	speaker.frequency = 0; //Start the divider with 0!
	speaker.ticker = 0; //Start the ticker with 0!
}

void speakerGateUpdated()
{
	if ((speaker.mode == 1) && (speaker.status == 2)) //Wait for next rising edge of gate input?
	{
		if (((oldPCSpeakerPort ^ PCSpeakerPort) & 0x1) && (PCSpeakerPort & 0x1)) //Risen?
		{
			speaker.status = 3; //Output goes low and we start counting to rise! After timeout we become 4(inactive)!
		}
	}
	oldPCSpeakerPort = PCSpeakerPort; //Save new value!
}

void setSpeakerFrequency(word frequency) //Set the new frequency!
{
	if (__HW_DISABLED) return; //Abort!
	speaker.frequency = frequency;
	if (speaker.status == 0) //First step?
	{
		if ((!speaker.mode) || (speaker.mode == 1) || (speaker.mode == 3) || (speaker.mode == 7)) //Wait for next rising edge of gate input(mode 1) or start counting (mode 0/3/7)?
		{
			speaker.status = 1; //Wait for next rising edge of gate input!
		}
	}
}

void setPCSpeakerMode(byte mode)
{
	if (__HW_DISABLED) return; //Abort!
	speaker.mode = mode; //Set the current PC speaker mode!
	if ((mode == 1) || (mode == 3) || (mode==7)) //Output goes high when set?
	{
		speaker.status = 0; //Output going high! Wait for reload to be set!
	}
}

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
