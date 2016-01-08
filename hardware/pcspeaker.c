#include "headers/types.h" //Basic types!
#include "headers/emu/sound.h" //Sound!
#include "headers/emu/emu_misc.h" //Random generators!
#include "headers/support/highrestimer.h" //High resolution clock support!
#include "headers/support/fifobuffer.h" //FIFO sample buffer support!

/*

PC SPEAKER

*/

//Are we disabled?
#define __HW_DISABLED 0

//Set to 0xFF to disable the speaker output!
#define SPEAKER_METHOD 1

//What volume, in percent!
#define SPEAKER_VOLUME 100.0f

//Maximum ammount of active speakers!
#define MAX_SPEAKERS 3

//Speaker playback rate!
#define SPEAKER_RATE 44100.0f
//Speaker buffer size!
#define SPEAKER_BUFFER 512

#define TIME_RATE 1190000.0f

TicksHolder speaker_ticker;
uint_64 speaker_ticktiming, oneshot_ticktiming;
uint_64 speaker_tick = (uint_64)(1000000000.0f/SPEAKER_RATE); //Time of a tick in the PC speaker sample!

byte oldPCSpeakerPort=0x00;
extern byte PCSpeakerPort; //Port 0x61 for the PC Speaker! Bit0=Gate, Bit1=Data enable

typedef struct
{
	byte function; //0=Sine,1=Square,2=Triangle,Else=None!
	byte mode; //PC speaker mode!
	float frequency;
	float freq0; //Old frequency!
	float time;
	byte status; //Status of the counter!
	FIFOBUFFER *buffer; //Output buffer for rendering!
	FIFOBUFFER *rawsignal; //The raw signal buffer!
} SPEAKER_INFO; //One speaker!

SPEAKER_INFO speakers[MAX_SPEAKERS]; //All possible PC speakers, whether used or not!

SPEAKER_INFO *pcspeaker = &speakers[0]; //The default, PC speaker to use by software!

OPTINLINE float currentFunction(byte how,const float time) {
        double x;
	float t = (float)modf(time / (2 * PI), &x);
	switch(how) {
	case 0: // SINE
		return sinf(time);
	case 1: // SQUARE
		if (t < 0.5f) {
			return -0.2f;
		} else {
			return 0.2f;
		}
	case 2: // TRIANGLE
		if (t < 0.5f) {
			return t * 2.0f - 0.5f;
		} else {
			return 0.5f - (t - 0.5f) * 2.0f;
		}
	case 3: // NOISE
		return RandomFloat(-0.2f,0.2f); //Random noise!
	default:
		return 0.0f;
	}
}

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
		register sample_stereo_p ubuf_stereo = (sample_stereo_p) buf; //Active buffer!
		for (;;) //Process all samples!
		{ //Process full length!
			if (!readfifobuffer16(speaker->buffer,&s)) //Not readable from the buffer?
			{
				s = 0; //Silence!
			}

			ubuf_stereo->l = ubuf_stereo->r = s; //Single channel!
			++ubuf_stereo; //Next item!
			if (++i==length) break; //Next item!
		}
	}
	else //Mono samples?
	{
		register sample_p ubuf_mono = (sample_p) buf; //Active buffer!
		for (;;)
		{ //Process full length!
			if (!readfifobuffer16(speaker->buffer, &s)) //Not readable from the buffer?
			{
				s = 0; //Silence!
			}
			*ubuf_mono = s; //Mono channel!
			++ubuf_mono; //Next item!
			if (++i==length) break; //Next item!
		}
	}

	return SOUNDHANDLER_RESULT_FILLED; //We're filled!
}

void tickSpeakers() //Ticks all PC speakers available!
{
	if (__HW_DISABLED) return;
	const float sampleLength = 1.0f / 44100.0f;
	const float scaleFactor = (SHRT_MAX - 1.0f);
	byte speaker; //Current speaker!
	register uint_32 length; //Amount of samples to generate!
	uint_32 i;
	register float time;
	float frequency;
	float freq0; //Old frequency!
	float temp; //Overflow detection!
	register byte function; //What!
	uint_64 timepassed;

	timepassed = (uint_64)getnspassed(&speaker_ticker); //Get the amount of time passed!
	speaker_ticktiming += timepassed; //Get the amount of time passed!

	//Ticks the speaker when needed!

	byte speaker0_status = 0; //Default: empty!

	if (speakers[0].mode == 1) //One-shot mode?
	{
		switch (speakers[0].status) //What status?
		{
		case 1: //Output goes high?
			speaker0_status = 1; //We're high!
			break;
		case 2: //Wait for next rising edge of gate input?
			speaker0_status = 1; //We're high!
			break;
		case 3: //Output goes low and we start counting to rise! After timeout we become 4(inactive)!
			speaker0_status = 0; //We're low!
			oneshot_ticktiming += timepassed; //Count the one-shot ticks!
			if (oneshot_ticktiming >= (1000000000.0f / speakers[0].frequency)) //Timeout? We're done!
			{
				speaker0_status = 1; //We're high again!
				speakers[0].mode = 4; //We're inactive!
			}
			break;
		case 4: //Inactive?
			speaker0_status = 1; //We're high!
			break;
		default: //Unsupported!
			break;
		}
	}

	if (speaker_ticktiming >= speaker_tick) //Enough time passed?
	{
		length = SAFEDIV(speaker_ticktiming,speaker_tick); //How many ticks to tick?
		speaker_ticktiming -= (length*speaker_tick); //Rest the amount of ticks!

		//Ticks the speaker when needed!
		for (speaker = 0;speaker<MAX_SPEAKERS;speaker++)
		{
			//First, our information sources!
			function = speakers[speaker].function; //The function to use!
			//Load current info!
			time = speakers[speaker].time; //Take the current time to update!
			frequency = speakers[speaker].frequency; //Take the current frequency to sound!
			freq0 = speakers[speaker].freq0; //Old frequency!

			if (frequency != freq0) { //Frequency changed?
				time *= (freq0 / frequency);
			}

			float defaultfreq;
			defaultfreq = 2.0f*PI*frequency; //Change in frequency for all times!

			i = 0; //Init counter!
			if (PCSpeakerPort&2) //Speaker is turned on?
			{
				switch (speakers[speaker].mode) //What rendering mode are we using?
				{
				case 1: //Hardware Re-triggerable One-shot?
					if (speaker) goto quietness; //Generate a quiet signal: we're not supported!
					short s = speaker0_status ? 32767 : -32768; //Set the channels! Not yet known how to do this. We generate 1 sample of output here!
					for (;;) //Generate samples!
					{
						time += sampleLength; //Add 1 sample to the time!
						writefifobuffer16(speakers[speaker].buffer, s); //Write the sample to the buffer (mono buffer)!
						if (++i == length) break; //Next item!
					}
					break;
				case 3: //Simple Square Wave Generator?
					for (;;) //Generate samples!
					{
						short s = (short)(scaleFactor * currentFunction(speakers[speaker].function, defaultfreq * time)); //Set the channels!
						time += sampleLength; //Add 1 sample to the time!
						writefifobuffer16(speakers[speaker].buffer,s); //Write the sample to the buffer (mono buffer)!
						if (++i == length) break; //Next item!
					}
					break;
				default: //Not supported!
					goto quietness; //Generatea a quiet signal!
					break;

				}
			}
			else //Not on? Generate quiet signal!
			{
				quietness:
				//Generate a quiet signal!
				time += sampleLength*length; //Process this many samples!
				for (;;) //Generate samples!
				{
					writefifobuffer16(speakers[speaker].buffer, 0); //Write the sample to the buffer (mono buffer)!
					if (++i == length) break; //Next item!
				}
			}

			finishedsamples:
			//We've processed all samples!
			temp = time*frequency; //Calculate!
			if (temp > 1.0f) { //End of wave?
				double d;
				time = (float)modf(temp, &d) / frequency;
			}

			//Update changed info!
			speakers[speaker].time = time; //Update the time!
			speakers[speaker].freq0 = frequency; //Update the frequency!
			//Speaker is up-to-date!
		}
	}
}

void initSpeakers()
{
	if (__HW_DISABLED) return; //Abort!
	initTicksHolder(&speaker_ticker); //Initialise our ticks holder!
	//First speaker defaults!
	word speaker;
	for (speaker=0;speaker<MAX_SPEAKERS;speaker++)
	{
		speakers[speaker].buffer = allocfifobuffer(2048,1); //Lockable FIFO with 1024 word-sized samples with lock!
		speakers[speaker].rawsignal = allocfifobuffer(2048, 0); //Nonlockable FIFO with 1024 word-sized samples with lock ((2048/SPEAKER_RATE)*TICK_RATE)!
		speakers[speaker].function = 0xFF; //Off!
		speakers[speaker].frequency = 1.0f; //1, all but 0!
		speakers[speaker].freq0 = 0.0f; //Old frequency!
		speakers[speaker].time = 0.0f; //Reset the time!
		addchannel(&speakerCallback,&speakers[speaker],"PC Speaker",SPEAKER_RATE,SPEAKER_BUFFER,0,SMPL16S); //Add the speaker at the hardware rate, mono! Make sure our buffer responds every 2ms at least!
		setVolume(&speakerCallback,&speakers[speaker],SPEAKER_VOLUME); //What volume?
	}
}

void doneSpeakers()
{
	if (__HW_DISABLED) return;
	word speaker;
	for (speaker=0;speaker<MAX_SPEAKERS;speaker++)
	{
		removechannel(&speakerCallback,&speakers[speaker],0); //Remove the speaker!
		free_fifobuffer(&speakers[speaker].buffer); //Release the FIFO buffer we use!
		//Cleanup the speaker info!
		speakers[speaker].function = SPEAKER_METHOD; //Off!
		speakers[speaker].frequency = 1.0f; //1, all but 0!
		speakers[speaker].freq0 = 0.0f; //Original!
		speakers[speaker].time = 0.0f; //Reset the time!
	}
}

void enableSpeaker(byte speaker) //Enables the speaker!
{
	if (__HW_DISABLED) return; //Abort!
	//Gate&Enable of speaker is set to 1!
}

void disableSpeaker(byte speaker) //Disables the speaker!
{
	if (__HW_DISABLED) return; //Abort!
	//Gate&Enable of speaker is set to 0!
}

void speakerGateUpdated()
{
	if ((speakers[0].mode == 1) && (speakers[0].status == 2)) //Wait for next rising edge of gate input?
	{
		if (((oldPCSpeakerPort ^ PCSpeakerPort) & 0x1) && (PCSpeakerPort & 0x1)) //Risen?
		{
			speakers[0].status = 3; //Output goes low and we start counting to rise! After timeout we become 4(inactive)!
			oneshot_ticktiming = 0; //Reset tick timing to start counting!
		}
	}
	oldPCSpeakerPort = PCSpeakerPort; //Save new value!
}

void setSpeakerFrequency(byte speaker, float newfrequency) //Set the new frequency!
{
	if (__HW_DISABLED) return; //Abort!
	if (newfrequency) //Gotten a frequency?
	{
		speakers[speaker].frequency = newfrequency; //Set the new frequency!
	}
	else //No frequency?
	{
		speakers[speaker].frequency = 1; //Set to at least some!
	}
	if ((speakers[speaker].mode == 1) && (speakers[speaker].status == 1)) //Wait for next rising edge of gate input?
	{
		speakers[speaker].status = 2; //Wait for next rising edge of gate input!
	}
}

void setPCSpeakerMode(byte speaker, byte mode)
{
	if (__HW_DISABLED) return; //Abort!
	speakers[speaker].mode = mode; //Set the current PC speaker mode!
	if (mode == 1) //Output goes high when set?
	{
		speakers[speaker].status = 1; //Output going high!
	}
}