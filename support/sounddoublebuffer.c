#include "headers/support/sounddoublebuffer.h" //Our own typedefs etc.

byte allocDoubleBufferedSound16(uint_32 samplebuffersize, SOUNDDOUBLEBUFFER *buffer)
{
	buffer->outputbuffer = allocfifobuffer(samplebuffersize<<1,0); //Normal output buffer, lock free!
	buffer->sharedbuffer = allocfifobuffer((samplebuffersize+1)<<2,1); //Shared output buffer, uses locks!
	buffer->inputbuffer = allocfifobuffer(samplebuffersize<<1,0); //Normal input buffer, lock free!
	buffer->samplebuffersize = samplebuffersize; //The buffer size used!
	return ((buffer->outputbuffer!=NULL) && (buffer->sharedbuffer!=NULL) && (buffer->inputbuffer!=NULL)); //Gotten the buffers!
}

byte allocDoubleBufferedSound8(uint_32 samplebuffersize, SOUNDDOUBLEBUFFER *buffer)
{
	buffer->outputbuffer = allocfifobuffer(samplebuffersize,0); //Normal output buffer, lock free!
	buffer->sharedbuffer = allocfifobuffer((samplebuffersize+1)<<1,1); //Shared output buffer, uses locks!
	buffer->inputbuffer = allocfifobuffer(samplebuffersize,0); //Normal input buffer, lock free!
	buffer->samplebuffersize = samplebuffersize; //The buffer size used!
	return ((buffer->outputbuffer!=NULL) && (buffer->sharedbuffer!=NULL) && (buffer->inputbuffer!=NULL)); //Gotten the buffers!
}

void freeDoubleBufferedSound(SOUNDDOUBLEBUFFER *buffer)
{
	free_fifobuffer(&buffer->inputbuffer); //Normal output buffer, lock free!
	free_fifobuffer(&buffer->sharedbuffer); //Normal output buffer, lock free!
	free_fifobuffer(&buffer->outputbuffer); //Normal output buffer, lock free!
	buffer->samplebuffersize = 0; //Nothin used anymore!
}

void writeDoubleBufferedSound16(SOUNDDOUBLEBUFFER *buffer, word sample)
{
	writefifobuffer16(buffer->outputbuffer,sample); //Add to the normal buffer!
	movefifobuffer16(buffer->outputbuffer,buffer->sharedbuffer,buffer->samplebuffersize); //Move to the destination if required!
}

void writeDoubleBufferedSound8(SOUNDDOUBLEBUFFER *buffer, byte sample)
{
	writefifobuffer(buffer->outputbuffer,sample); //Add to the normal buffer!
	movefifobuffer8(buffer->outputbuffer,buffer->sharedbuffer,buffer->samplebuffersize); //Move to the destination if required!
}

byte readDoubleBufferedSound16(SOUNDDOUBLEBUFFER *buffer, word *sample)
{
	if (readfifobuffer16(buffer->inputbuffer,sample)) //Read from the normal buffer!
	{
		return 1; //We've read from the normal (fast) buffer!
	}
	//No sample yet? Request samples!
	movefifobuffer16(buffer->sharedbuffer,buffer->inputbuffer,buffer->samplebuffersize); //Move to the destination if required!
	return readfifobuffer16(buffer->inputbuffer,sample); //Try to read from the buffer if possible!
}

byte readDoubleBufferedSound8(SOUNDDOUBLEBUFFER *buffer, byte *sample)
{
	if (readfifobuffer(buffer->inputbuffer,sample)) //Read from the normal buffer!
	{
		return 1; //We've read from the normal (fast) buffer!
	}
	//No sample yet? Request samples!
	movefifobuffer8(buffer->sharedbuffer,buffer->inputbuffer,buffer->samplebuffersize); //Move to the destination if required!
	return readfifobuffer(buffer->inputbuffer,sample); //Try to read from the buffer if possible!
}