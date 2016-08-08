#include "headers/types.h" //Basic types!
#include "headers/support/sounddoublebuffer.h" //Double buffered sound support!
#include "headers/emu/sound.h" //Sound output support!
#include "headers/support/signedness.h" //Sign support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/ports.h" //I/O support!

#define __SOUNDBLASTER_SAMPLERATE 22050.0f
#define __SOUNDBLASTER_SAMPLEBUFFERSIZE 2048
#define SOUNDBLASTER_VOLUME 100.0f

struct
{
	word baseaddr;
	SOUNDDOUBLEBUFFER soundbuffer; //Outputted sound to render!
} SOUNDBLASTER; //The Sound Blaster data!

double soundblaster_soundtiming = 0.0, soundblaster_soundtick = 1000000000.0 / __SOUNDBLASTER_SAMPLERATE;
void updateSoundBlaster(double timepassed)
{
	if (SOUNDBLASTER.baseaddr == 0) return; //No game blaster?

	//Finally, render any rendered Sound Blaster output to the renderer at the correct rate!
	//Sound Blaster sound output
	soundblaster_soundtiming += timepassed; //Get the amount of time passed!
	if (soundblaster_soundtiming >= soundblaster_soundtick)
	{
		for (;soundblaster_soundtiming >= soundblaster_soundtick;)
		{
			byte leftsample, rightsample; //Two stereo samples!
			//Generate the sample!
			leftsample = rightsample = 0; //Nothing to generate yet!

			//Now push the samples to the output!
			writeDoubleBufferedSound32(&SOUNDBLASTER.soundbuffer, (signed2unsigned16(rightsample) << 16) | signed2unsigned16(leftsample)); //Output the sample to the renderer!
			soundblaster_soundtiming -= soundblaster_soundtick; //Decrease timer to get time left!
		}
	}
}

byte SoundBlaster_soundGenerator(void* buf, uint_32 length, byte stereo, void *userdata) //Generate a sample!
{
	uint_32 c;
	c = length; //Init c!

	static uint_32 last = 0;
	INLINEREGISTER uint_32 buffer;

	SOUNDDOUBLEBUFFER *doublebuffer = (SOUNDDOUBLEBUFFER *)userdata; //Our double buffered sound input to use!
	int_32 mono_converter;
	sample_stereo_p data_stereo;
	sword *data_mono;
	if (stereo) //Stereo processing?
	{
		data_stereo = (sample_stereo_p)buf; //The data in correct samples!
		for (;;) //Fill it!
		{
			//Left and right samples are the same: we're a mono signal!
			readDoubleBufferedSound32(doublebuffer, &last); //Generate a stereo sample if it's available!
			buffer = last; //Load the last sample for processing!
			data_stereo->l = unsigned2signed16((word)buffer); //Load the last generated sample(left)!
			buffer >>= 16; //Shift low!
			data_stereo->r = unsigned2signed16((word)buffer); //Load the last generated sample(right)!
			++data_stereo; //Next stereo sample!
			if (!--c) return SOUNDHANDLER_RESULT_FILLED; //Next item!
		}
	}
	else //Mono processing?
	{
		data_mono = (sword *)buf; //The data in correct samples!
		for (;;) //Fill it!
		{
			//Left and right samples are the same: we're a mono signal!
			readDoubleBufferedSound32(doublebuffer, &last); //Generate a stereo sample if it's available!
			buffer = last; //Load the last sample for processing!
			mono_converter = unsigned2signed16((word)buffer); //Load the last generated sample(left)!
			buffer >>= 16; //Shift low!
			mono_converter += unsigned2signed16((word)buffer); //Load the last generated sample(right)!
			mono_converter = LIMITRANGE(mono_converter, SHRT_MIN, SHRT_MAX); //Clip our data to prevent overflow!
			*data_mono++ = mono_converter; //Save the sample and point to the next mono sample!
			if (!--c) return SOUNDHANDLER_RESULT_FILLED; //Next item!
		}
	}
}

byte inSoundBlaster(word port, byte *result)
{
	if ((port&~0xF)!=SOUNDBLASTER.baseaddr) return 0; //Not our base address?
	switch (port & 0xF) //What port?
	{
	default:
		break;
	}
	return 0; //Not supported yet!
}

byte outSoundBlaster(word port, byte value)
{
	if ((port&~0xF) != SOUNDBLASTER.baseaddr) return 0; //Not our base address?
	switch (port & 0xF) //What port?
	{
	default:
		break;
	}
	return 0; //Not supported yet!
}

void initSoundBlaster(word baseaddr)
{
	if (allocDoubleBufferedSound16(__SOUNDBLASTER_SAMPLEBUFFERSIZE, &SOUNDBLASTER.soundbuffer, 0)) //Valid buffer?
	{
		if (!addchannel(&SoundBlaster_soundGenerator, NULL, "SoundBlaster", __SOUNDBLASTER_SAMPLERATE, __SOUNDBLASTER_SAMPLEBUFFERSIZE, 0, SMPL16S)) //Start the sound emulation (mono) with automatic samples buffer?
		{
			dolog("adlib", "Error registering sound channel for output!");
		}
		else
		{
			setVolume(&SoundBlaster_soundGenerator, NULL, SOUNDBLASTER_VOLUME);
		}
	}
	else
	{
		dolog("adlib", "Error registering double buffer for output!");
	}

	SOUNDBLASTER.baseaddr = baseaddr; //The base address to use!

	register_PORTIN(&inSoundBlaster); //Status port (R)
	//All output!
	register_PORTOUT(&outSoundBlaster); //Address port (W)
}

void doneSoundBlaster()
{
	removechannel(&SoundBlaster_soundGenerator, NULL, 0); //Stop the sound emulation?
	freeDoubleBufferedSound(&SOUNDBLASTER.soundbuffer); //Free our double buffered sound!
}