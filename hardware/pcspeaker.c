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

TicksHolder speaker_ticker;
uint_64 speaker_ticktiming;
uint_64 speaker_tick = (uint_64)(1000000000.0f/SPEAKER_RATE); //Time of a tick in the PC speaker sample!

typedef struct
{
	byte function; //0=Sine,1=Square,2=Triangle,Else=None!
	float frequency;
	float freq0; //Old frequency!
	float time;
	FIFOBUFFER *buffer; //Output buffer!
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

	speaker_ticktiming += (uint_64)getnspassed(&speaker_ticker); //Get the amount of time passed!
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
			for (;;) //Generate samples!
			{
				short s = (short)(scaleFactor * currentFunction(speakers[speaker].function, defaultfreq * time)); //Set the channels!
				time += sampleLength; //Add 1 sample to the time!
				writefifobuffer16(speakers[speaker].buffer,s); //Write the sample to the buffer (mono buffer)!
				if (++i == length) break; //Next item!
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
		speakers[speaker].function = 0xFF; //Off!
		speakers[speaker].frequency = 1.0f; //1, all but 0!
		speakers[speaker].freq0 = 0.0f; //Original!
		speakers[speaker].time = 0.0f; //Reset the time!
	}
}

void enableSpeaker(byte speaker) //Enables the speaker!
{
	if (__HW_DISABLED) return; //Abort!
	speakers[speaker].function = SPEAKER_METHOD; //We have a square wave generator!
}

void disableSpeaker(byte speaker) //Disables the speaker!
{
	if (__HW_DISABLED) return; //Abort!
	speakers[speaker].function = 0xFF; //We have no speaker!
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
		disableSpeaker(speaker); //Disable the PC speaker!
		speakers[speaker].frequency = 1; //Set to at least some!
	}
}