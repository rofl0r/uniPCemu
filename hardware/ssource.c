#include "headers/types.h" //Basic types!
#include "headers/support/fifobuffer.h" //FIFO buffer support!
#include "headers/emu/sound.h" //Sound output support!
#include "headers/hardware/parallel.h" //Parallel port support!

//Sound source sample rate and buffer size!
#define __SSOURCE_RATE 7000.0f
//Primary buffer is always 16 bytes large (needed for full detection)!
#define __SSOURCE_BUFFER 16
//Secondary buffer needs to be large enough for a good sound!
#define __SSOURCE_HWBUFFER 1024

byte ssource_ready = 0; //Are we running?
FIFOBUFFER *ssourcestream = NULL, *ssourcestream2 = NULL; //Sound source data stream and secondary buffer!
byte forcefull = 0; //Forced full buffer?

OPTINLINE static byte getssourcebyte() {
	static byte result=0;
	if (!readfifobuffer(ssourcestream2, &result)) //Nothing gotten from the secondary buffer?
	{
		if (!readfifobuffer(ssourcestream, &result)) //Primary buffer has no data as well?
		{
			result = 0x80; //No result, so 0 converted from signed to unsigned!
		}
	}
	return result;
}

byte ssourceoutput(void* buf, uint_32 length, byte stereo, void *userdata)
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


byte outbuffer = 0x00; //Our outgoing data buffer!
byte lastcontrol = 0x00; //Our current control data!

void soundsourceoutput(byte data)
{
	outbuffer = data; //Last data set on our lines!
}

void soundsourcecontrolout(byte control)
{
	if (control&4) //Is this output for the Sound Source?
	{
		if ((control & 8) && !(lastcontrol & 8)) //Toggling this bit on sends the data to the DAC!
		{
			if (writefifobuffer(ssourcestream, outbuffer)) //Add to the primary buffer!
			{
				forcefull = !fifobuffer_freesize(ssourcestream); //Buffer full?
				movefifobuffer8(ssourcestream,ssourcestream2,__SSOURCE_BUFFER); //Move data to the destination buffer once we're full!
			}
			else //Write failed with buffer full or error?
			{
				forcefull = !fifobuffer_freesize(ssourcestream); //Buffer full?
			}
		}
	}
	lastcontrol = control; //Save the last status for checking the bits!
}

byte soundsourcecontrolin()
{
	return lastcontrol; //Give our last control byte!
}

byte soundsourcestatus()
{
	if (!fifobuffer_freesize(ssourcestream) || forcefull) //Buffer full or forced full?
	{
		forcefull = 0; //Not forced full anymore! We're needing filling if possible next check!
		return (0x40); //We have a full buffer!
	}
	return (0x00); //We have an empty buffer!
}

void ssource_setVolume(float volume)
{
	setVolume(&ssourceoutput, NULL, volume); //Set the volume!
}

void doneSoundsource()
{
	if (ssource_ready) //Are we running?
	{
		removechannel(&ssourceoutput, NULL, 0); //Remove the channel!
		free_fifobuffer(&ssourcestream); //Finish the stream if it's there!
		free_fifobuffer(&ssourcestream2); //Finish the stream if it's there!
		ssource_ready = 0; //We're finished!
	}
}

void initSoundsource() {
	doneSoundsource(); //Make sure we're not already running!
	ssourcestream = allocfifobuffer(__SSOURCE_BUFFER,0); //Our FIFO buffer! Don't lock: This is done using a sound lock!
	if (ssourcestream) //Allocated buffer?
	{
		ssourcestream2 = allocfifobuffer(__SSOURCE_HWBUFFER,0); //Our FIFO hardware buffer! Don't lock: This is done using a sound lock!
		if (ssourcestream2) //Allocated buffer?
		{
			if (addchannel(&ssourceoutput, NULL, "Sound Source", __SSOURCE_RATE, __SSOURCE_HWBUFFER, 0, SMPL8U)) //Channel added?
			{
				outbuffer = lastcontrol = 0; //Reset output buffer and last control!
				ssource_setVolume(1.0f); //Default volume: 100%!
				registerParallel(0,&soundsourceoutput,&soundsourcecontrolout,&soundsourcecontrolin,&soundsourcestatus); //Register out parallel port for handling!
				ssource_ready = 1; //We're running!
			}
			else
			{
				free_fifobuffer(&ssourcestream2); //Finish the stream if it's there!
				free_fifobuffer(&ssourcestream); //Finish the stream if it's there!
			}
		}
		else
		{
			free_fifobuffer(&ssourcestream); //Finish the stream if it's there!
		}
	}
}