#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //I/O support!
#include "headers/support/fifobuffer.h" //FIFO buffer support!
#include "headers/emu/sound.h" //Sound output support!

//Sound source sample rate and buffer size!
#define __SSOURCE_RATE 7000.0f
//Primary buffer is always 16 bytes large (needed for full detection)!
#define __SSOURCE_BUFFER 16
//Secondary buffer needs to be large enough for a good sound!
#define __SSOURCE_HWBUFFER 1024

byte ssource_ready = 0; //Are we running?
FIFOBUFFER *ssourcestream = NULL, *ssourcestream2 = NULL; //Sound source data stream and secondary buffer!
byte forcefull = 0; //Forced full buffer?

byte getssourcebyte() {
	byte result;
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


void putssourcebyte(byte value) {
	byte transfer;
	if (writefifobuffer(ssourcestream, value)) //Add to the primary buffer!
	{
		if (!fifobuffer_freesize(ssourcestream) && fifobuffer_freesize(ssourcestream2)>=__SSOURCE_BUFFER) //Primary buffer full and enough space to store it in the second buffer?
		{
			lockaudio(); //Make sure the audio thread isn't using our data!
			for (;readfifobuffer(ssourcestream,&transfer);) writefifobuffer(ssourcestream2,transfer); //Transfer data to the second buffer!
			forcefull = 1; //We're forced full to allow detection of 'full' buffer!
			unlockaudio(1); //We're finished locking!
		}
	}
}

byte ssourcefull() {
	if (!fifobuffer_freesize(ssourcestream) || forcefull)
	{
		forcefull = 0; //Not forced full anymore! We're needing filling if possible next check!
		return (0x40);
	}
	else return (0x00);
}

byte outsoundsource(word port, byte value) {
	static byte lastcontrol = 0;
	static byte databuffer = 0;
	switch (port) {
	case 0x378: //Data output?
		databuffer = value; //Last data output!
		return 1; //We're handled!
		break;
	case 0x37A: //Control register?
		if (value&4) //Is this output for the Sound Source?
		{
			if ((value & 8) && !(lastcontrol & 8)) putssourcebyte(databuffer); //Toggling this bit on sends the data to the DAC!
			lastcontrol = value; //Save the last status for checking the bits!
		}
		return 1; //We're handled!
		break;
	default:
		break;
	}
	return 0; //We're not handled!
}

byte insoundsource(word port, byte *result) {
	switch (port)
	{
	case 0x379: //Status?
		*result = ssourcefull();
		return 1; //We're handled!
		break;
	default: //Unknown port?
		break;
	}
	return 0; //Not supported port!
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
	ssourcestream = allocfifobuffer(__SSOURCE_BUFFER); //Our FIFO buffer!
	if (ssourcestream) //Allocated buffer?
	{
		ssourcestream2 = allocfifobuffer(__SSOURCE_HWBUFFER); //Our FIFO hardware buffer!
		if (ssourcestream2) //Allocated buffer?
		{
			if (addchannel(&ssourceoutput, NULL, "Sound Source", __SSOURCE_RATE, __SSOURCE_HWBUFFER, 0, SMPL8U)) //Channel added?
			{
				ssource_setVolume(1.0f); //Default volume: 100%!
				register_PORTIN(&insoundsource); //Register the read handler!
				register_PORTOUT(&outsoundsource); //Register the write handler!
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