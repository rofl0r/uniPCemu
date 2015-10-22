#include "headers/types.h" //Basic types!
#include "headers/emu/sound.h" //Sound!
#include "headers/emu/emu_misc.h" //Random generators!
#include "headers/support/highrestimer.h" //High resolution clock support!

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

typedef struct
{
	byte function; //0=Sine,1=Square,2=Triangle,Else=None!
	float frequency;
	float freq0; //Old frequency!
	float time;
	//float freq0; //Old frequency handler!
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
	const float sampleLength = 1.0f / 44100.0f;
	const float scaleFactor = (SHRT_MAX - 1.0f);
	//float currentfactor;
	uint_32 i;
	SPEAKER_INFO *speaker; //Convert to the current speaker!
	float time;
	float frequency;
	float freq0; //Old frequency!
	register byte function; //What!

	if (__HW_DISABLED) return 0; //Abort!	
	
	/*TicksHolder t;
	startHiresCounting(&t);*/
	//First, our information sources!
	speaker = (SPEAKER_INFO *)userdata; //Active speaker!
	function = speaker->function; //The function to use!
	if (function == 0xFF) return SOUNDHANDLER_RESULT_NOTFILLED; //Inactive speaker!
	//Load current info!
	time = speaker->time; //Take the current time to update!
	frequency = speaker->frequency; //Take the current frequency to sound!
	freq0 = speaker->freq0; //Old frequency!

	if (frequency != freq0) { //Frequency changed?
	        time *= (freq0 / frequency);
	}

	float defaultfreq;
	defaultfreq = 2.0f*PI*frequency; //Change in frequency for all times!

	i = 0; //Init counter!
	if (stereo) //Stereo samples?
	{
		register sample_stereo_p ubuf_stereo = (sample_stereo_p) buf; //Active buffer!
		for (;;) //Process all samples!
		{ //Process full length!
			//currentfactor *= time;
			register short s = (short) (scaleFactor * currentFunction(function,defaultfreq * time)); //Set the channels!
			ubuf_stereo->l = ubuf_stereo->r = s; //Single channel!
			++ubuf_stereo; //Next item!
			time += sampleLength; //Add 1 sample to the time!
			if (++i==length) break; //Next item!
		}
	}
	else //Mono samples?
	{
		register sample_p ubuf_mono = (sample_p) buf; //Active buffer!
		for (;;)
		{ //Process full length!
			short s = (short) (scaleFactor * currentFunction(function,defaultfreq * time)); //Set the channels!
			*ubuf_mono = s; //Mono channel!
			++ubuf_mono; //Next item!
			time += sampleLength; //Add 1 sample to the time!
			if (++i==length) break; //Next item!
		}
	}

	float temp = time*frequency; //Calculate!
	if (temp > 1.0f) {
		double d;
		time = (float)modf(temp, &d) / frequency;
	}
	
	freq0 = frequency;

	//Update changed info!
	speaker->time = time; //Update the time!
	speaker->freq0 = freq0; //Update the frequency!
	//stopHiresCounting("soundgen","pcspeaker",&t); //Log our timing!
	return SOUNDHANDLER_RESULT_FILLED; //We're filled!
}

void initSpeakers()
{
	if (__HW_DISABLED) return; //Abort!
	//First speaker defaults!
	word speaker;
	for (speaker=0;speaker<MAX_SPEAKERS;speaker++)
	{
		speakers[speaker].function = 0xFF; //Off!
		speakers[speaker].frequency = 1.0f; //1, all but 0!
		speakers[speaker].freq0 = 0.0f; //Old frequency!
		speakers[speaker].time = 0.0f; //Reset the time!
		addchannel(&speakerCallback,&speakers[speaker],"PC Speaker",44100.0f,88,0,SMPL16S); //Add the speaker at the hardware rate, mono! Make sure our buffer responds every 2ms at least!
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