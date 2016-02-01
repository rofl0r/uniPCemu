#include "headers/types.h" //Basic types!
#include "headers/support/fifobuffer.h" //FIFO buffer support!
#include "headers/emu/sound.h" //Sound output support!
#include "headers/hardware/parallel.h" //Parallel port support!

//Sound source sample rate and buffer size!
#define __SSOURCE_RATE 7000.0f
#define __COVOX_RATE 44100.0f
//Primary buffer is always 16 bytes large (needed for full detection of the Sound Source)!
#define __SSOURCE_BUFFER 16
#define __COVOX_BUFFER 256
//Treshold for primary to secondary buffer!
#define __SSOURCE_TRESHOLD 4
#define __COVOX_TRESHOLD 16
//Secondary buffer needs to be large enough for a good sound!
#define __SSOURCE_HWBUFFER 1024
#define __COVOX_HWBUFFER 4096

double ssourcetiming = 0.0f, covoxtiming = 0.0f, ssourcetick=(1000000000.0f/__SSOURCE_RATE), covoxtick=(1000000000.0f/__COVOX_RATE);
byte ssource_ready = 0; //Are we running?
FIFOBUFFER *ssourcestream = NULL, *ssourcestream2 = NULL, *ssourcestream3 = NULL, *covoxstream = NULL, *covoxstream2 = NULL; //Sound and covox source data stream and secondary buffer!
byte covox_left=0x80, covox_right=0x80; //Current status for the covox speech thing output (any rate updated)!

//Current buffers for the Parallel port!
byte outbuffer = 0x00; //Our outgoing data buffer!
byte lastcontrol = 0x00; //Our current control data!

OPTINLINE static byte getssourcebyte() {
	static byte result=0;
	sbyte temp;
	if (!readfifobuffer(ssourcestream3, &result)) //Nothing gotten from the tertiary buffer?
	{
		result = 0x80; //No result, so 0 converted from signed to unsigned!
		//Data goes from 0-128=>-128-0, 129-255=1-127. Convert to signed value!
		//result = (result&0x80)?result&0x7F:signed2unsigned8((sbyte)((sword)(-128)+((sword)result&0x7F))); //Convert data from 0-255 to -128-127!
	}
	return result;
}

byte ssource_output(void* buf, uint_32 length, byte stereo, void *userdata)
{
	if (stereo) return SOUNDHANDLER_RESULT_NOTFILLED; //Stereo not supported!
	byte *sample = (byte *)buf; //Sample buffer!
	uint_32 lengthleft = length; //Load the length!
	for (;lengthleft--;) //While length left!
	{
		*sample++ = getssourcebyte(); //Fill the output buffer!
	}
	return SOUNDHANDLER_RESULT_FILLED; //We're filled!
}

byte covox_output(void* buf, uint_32 length, byte stereo, void *userdata)
{
	if (!stereo) return SOUNDHANDLER_RESULT_NOTFILLED; //Stereo needs to be supported!
	byte *sample = (byte *)buf; //Sample buffer!
	uint_32 lengthleft = length; //Our stereo samples!
	static byte lastcovoxsample = 0x8080; //Last sample read for both channels!
	for (;lengthleft--;)
	{
		readfifobuffer16(covoxstream2,&lastcovoxsample); //Try to read the left sample if it's there! If it doesn't exist, use the last samples!
		*sample++ = (lastcovoxsample&0xFF); //Left channel!
		*sample++ = (lastcovoxsample>>8); //Right channel!
	}
	return SOUNDHANDLER_RESULT_FILLED; //We're filled!
}

void soundsource_covox_output(byte data)
{
	outbuffer = data; //Last data set on our lines!
}

void soundsource_covox_controlout(byte control)
{
	//bit0=Covox left channel tick, bit1=Covox right channel tick, bit 3=Sound source mono channel tick. Bit 2=Sound source ON.
	if (control&4) //Is this output for the Sound Source?
	{
		if ((control & 8) && !(lastcontrol & 8)) //Toggling this bit on sends the data to the DAC!
		{
			writefifobuffer(ssourcestream, outbuffer); //Add to the primary buffer when possible!
		}
	}
	if ((control&1) && (!lastcontrol&1)) //Covox speech thing left channel pulse?
	{
		covox_left = outbuffer; //Set left channel value!
	}
	if ((control&2) && (!lastcontrol&2)) //Covox speech thing right channel pulse?
	{
		covox_right = outbuffer; //Set right channel value!
	}	
	lastcontrol = control; //Save the last status for checking the bits!
}

byte soundsource_covox_controlin()
{
	return lastcontrol; //Give our last control byte!
}

byte soundsource_covox_status()
{
	//Bits 0-3 is set to detect. Bit 2 is cleared with a full buffer. Bit 6 is set with a full buffer. Output buffer bit 7(pin 9) is wired to status bit 0(pin 11). According to Dosbox.
	if (!fifobuffer_freesize(ssourcestream)) //Buffer full?
	{
		return (0x43)|(outbuffer&0x80); //We have a full buffer!
	}
	return (0x07)|(outbuffer&0x80); //We have an empty buffer!
}

void tickssourcecovox(double timepassed)
{
	//HW emulation of ticking the sound source in time! Not supported yet!
	
	ssourcetiming += timepassed; //Tick the sound source!
	if (ssourcetiming>=ssourcetick) //Timeout?
	{
		for (;ssourcetiming>=ssourcetick;)
		{
			movefifobuffer8(ssourcestream,ssourcestream2,1); //Move data to the destination buffer one at a time!
			ssourcetiming -= ssourcetick; //Ticked!
		}
	}
	
	covoxtiming += covoxtick; //Tick the Covox Speech Thing!
	if (covoxtiming>=covoxtick)
	{
		covoxtiming = modf(covoxtiming,covoxtick); //Rest!
		//Write both left and right channels at the destination to get the sample rate converted, since we don't have a input buffer(just a state at any moment in time)!
		writefifobuffer16(covoxstream, covox_left|(covox_right<<8)); //Add to the primary buffer when possible!
	}
	
	//Move to renderer when needed!
	movefifobuffer8(ssourcestream2,ssourcestream3,__SSOURCE_TRESHOLD); //Move data to the destination buffer once we're full enough!
	movefifobuffer16(covoxstream,covoxstream2,__COVOX_TRESHOLD); //Move data to the destination buffer once we're full enough, both channels at the same time (left and right)!
}

void ssource_setVolume(float volume)
{
	setVolume(&ssourceoutput, NULL, volume); //Set the volume!
	setVolume(&covoxoutput, NULL, volume); //Set the volume!
}

void doneSoundsource()
{
	if (ssource_ready) //Are we running?
	{
		removechannel(&ssourceoutput, NULL, 0); //Remove the channel!
		free_fifobuffer(&ssourcestream); //Finish the stream if it's there!
		free_fifobuffer(&ssourcestream2); //Finish the stream if it's there!
		free_fifobuffer(&ssourcestream3); //Finish the stream if it's there!
		free_fifobuffer(&covoxstream); //Finish the stream if it's there!
		free_fifobuffer(&covoxstream2); //Finish the stream if it's there!
		ssource_ready = 0; //We're finished!
	}
}

void initSoundsource() {
	doneSoundsource(); //Make sure we're not already running!
	ssourcestream = allocfifobuffer(__SSOURCE_BUFFER,0); //Our FIFO buffer! Don't lock: This is done using a sound lock!
	ssourcestream2 = allocfifobuffer(__SSOURCE_HWBUFFER,1); //Our FIFO hardware buffer! Do lock!
	ssourcestream3 = allocfifobuffer(__SSOURCE_HWBUFFER,1); //Our FIFO hardware buffer! Do lock!
	covoxstream = allocfifobuffer(__COVOX_BUFFER<<1,0); //Our FIFO buffer! Don't lock: This is done using a sound lock!
	covoxstream2 = allocfifobuffer(__COVOX_HWBUFFER<<1,1); //Our FIFO hardware buffer! Do lock!

	ssourcetiming = covoxtiming = 0.0f; //Initialise our timing!

	if (ssourcestream && ssourcestream2 && covox_leftstream && covox_leftstream2 && covox_rightstream && covox_rightstream2) //Allocated buffer?
	{
		if (addchannel(&covox_output, NULL, "Covox Speech Thing", __COVOX_RATE, __COVOX_HWBUFFER, 1, SMPL8U)) //Covox channel added?
		{
			if (addchannel(&ssource_output, NULL, "Sound Source", __SSOURCE_RATE, __SSOURCE_HWBUFFER, 0, SMPL8U)) //Sound source channel added?
			{
				outbuffer = lastcontrol = 0; //Reset output buffer and last control!
				ssource_setVolume(1.0f); //Default volume: 100%!
				registerParallel(0,&soundsource_covox_output,&soundsource_covox_controlout,&soundsource_covox_controlin,&soundsource_covox_status); //Register out parallel port for handling!
				ssource_ready = 1; //We're running!
			}
		}
	}
	if (!ssource_ready) //Failed to allocate anything?
	{
		free_fifobuffer(&covoxstream2); //Finish the stream if it's there!
		free_fifobuffer(&covoxstream); //Finish the stream if it's there!
		free_fifobuffer(&ssourcestream3); //Finish the stream if it's there!
		free_fifobuffer(&ssourcestream2); //Finish the stream if it's there!
		free_fifobuffer(&ssourcestream); //Finish the stream if it's there!
	}
}