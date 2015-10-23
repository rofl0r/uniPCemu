#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //I/O support!
#include "headers/support/fifobuffer.h" //FIFO buffer support!
#include "headers/emu/sound.h" //Sound output support!

//Sound source sample rate!
#define __SSOURCE_RATE 7000.0f

byte ssource_ready = 0; //Are we running?
FIFOBUFFER *ssourcestream = NULL; //Sound source data stream!

byte getssourcebyte() {
	byte result;
	if (!readfifobuffer(ssourcestream, &result)) //Nothing gotten?
	{
		result = 0; //No result, so 0!
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
	writefifobuffer(ssourcestream,value); //Add to the buffer!
}

byte ssourcefull() {
	if (!fifobuffer_freesize(ssourcestream)) return (0x40);
	else return (0x00);
}

byte outsoundsource(word port, byte value) {
	static byte last37a = 0;
	static byte last378 = 0;
	switch (port) {
	case 0x378: //Data output?
		last378 = value; //Last data output!
		putssourcebyte(value);
		return 1; //We're handled!
		break;
	case 0x37A: //Control register?
		if ((value & 4) && !(last37a & 4)) putssourcebyte(last378);
		last37a = value;
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

void doneSoundsource()
{
	if (ssource_ready) //Are we running?
	{
		removechannel(&ssourceoutput, NULL, 0); //Remove the channel!
		free_fifobuffer(&ssourcestream); //Finish the stream if it's there!
		ssource_ready = 0; //We're finished!
	}
}

void initSoundsource() {
	doneSoundsource(); //Make sure we're not already running!
	ssourcestream = allocfifobuffer(16); //Our FIFO buffer!
	if (ssourcestream) //Allocated buffer?
	{
		if (addchannel(&ssourceoutput, NULL, "Sound Source", __SSOURCE_RATE, 1, 0, SMPL8)) //Channel added?
		{
			register_PORTIN(&insoundsource); //Register the read handler!
			register_PORTOUT(&outsoundsource); //Register the write handler!
			ssource_ready = 1; //We're running!
		}
		else
		{
			free_fifobuffer(&ssourcestream); //Finish the stream if it's there!
		}
	}
}