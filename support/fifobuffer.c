#include "headers/types.h"
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/support/fifobuffer.h" //Our types etc.

//Are we disabled?
#define __HW_DISABLED 1

/*

newbuffer: generates a new buffer to work with.
parameters:
	buffersize: the size of the buffer!
	filledcontainer: pointer to filled variabele for this buffer
	sizecontainer: pointer to size variabele for this buffer
result:
	Buffer for allocated, NULL for error when allocating!

*/


FIFOBUFFER* allocfifobuffer(uint_32 buffersize)
{
	if (__HW_DISABLED) return NULL; //Abort!
	FIFOBUFFER *container = zalloc(sizeof(FIFOBUFFER),"FIFOBuffer"); //Allocate an container!
	if (container) //Allocated container?
	{
		container->buffer = zalloc(buffersize,"FIFOBuffer_Buffer"); //Try to allocate the buffer!
		if (!container->buffer) //No buffer?
		{
			freez((void **)&container,sizeof(FIFOBUFFER),"Failed FIFOBuffer"); //Release the container!
			return NULL; //Not allocated!
		}
		container->size = buffersize; //Set the buffer size!
		//The reset is ready to work with: all 0!
	}
	return container; //Give the allocated FIFO container!
}

void free_fifobuffer(FIFOBUFFER **buffer)
{
	if (__HW_DISABLED) return; //Abort!
	if (buffer) //Valid?
	{
		if (memprotect(*buffer,sizeof(FIFOBUFFER),NULL)) //Valid point?
		{
			FIFOBUFFER *container = *buffer; //Get the buffer itself!
			if (memprotect(container->buffer,container->size,NULL)) //Valid?
			{
				freez((void **)&container->buffer,container->size,"Free FIFOBuffer_buffer"); //Release the buffer!
			}
		}
		freez((void **)buffer,sizeof(FIFOBUFFER),"Free FIFOBuffer"); //Free the buffer container!
	}
}

uint_32 fifobuffer_freesize(FIFOBUFFER *buffer)
{
	if (__HW_DISABLED) return 0; //Abort!
	if (!memprotect(buffer,sizeof(FIFOBUFFER),NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer,buffer->size,NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}

	if (buffer->readpos>=buffer->writepos) //Write after or at read index: we wrap arround? Difficule sum!
	{
		return (buffer->size - buffer->readpos) + buffer->writepos; //Free space!
	}
	//Simple difference!
	return buffer->writepos - buffer->readpos; //Free space!
}

void waitforfreefifobuffer(FIFOBUFFER *buffer, uint_32 size)
{
	if (__HW_DISABLED) return; //Abort!
	while (fifobuffer_freesize(buffer)<size) //Not enough?
	{
		delay(100); //Wait for the buffer to have enough free size!
	}
}

int peekfifobuffer(FIFOBUFFER *buffer, byte *result) //Is there data to be read?
{
	if (__HW_DISABLED) return 0; //Abort!
	if (!memprotect(buffer,sizeof(FIFOBUFFER),NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer,buffer->size,NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}

	if (fifobuffer_freesize(buffer)<buffer->size) //Filled?
	{
		*result = buffer->buffer[buffer->readpos]; //Give the data!
		return 1; //Something to peek at!
	}
	return 0; //Nothing to peek at!
}

int readfifobuffer(FIFOBUFFER *buffer, byte *result)
{
	if (__HW_DISABLED) return 0; //Abort!
	if (!memprotect(buffer,sizeof(FIFOBUFFER),NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer,buffer->size,NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}

	if (fifobuffer_freesize(buffer)<buffer->size) //Filled?
	{
		*result = buffer->buffer[buffer->readpos];
		buffer->readpos = SAFEMOD((buffer->readpos+1),buffer->size); //Update the position!
		return 1; //Read!
	}

	return 0; //Nothing to read!
}

int writefifobuffer(FIFOBUFFER *buffer, byte data)
{
	if (__HW_DISABLED) return 0; //Abort!
	if (!memprotect(buffer,sizeof(FIFOBUFFER),NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer,buffer->size,NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}

	if (fifobuffer_freesize(buffer)<1) //Buffer full?
	{
		return 0; //Error: buffer full!
	}
	
	buffer->buffer[buffer->writepos] = data; //Write!
	buffer->writepos = SAFEMOD((buffer->writepos+1),buffer->size); //Next pos!
	return 1; //Written!
}

void fifobuffer_gotolast(FIFOBUFFER *buffer)
{
	if (__HW_DISABLED) return; //Abort!
	if (!memprotect(buffer,sizeof(FIFOBUFFER),NULL)) //Error?
	{
		return; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer,buffer->size,NULL)) //Error?
	{
		return; //Error: invalid buffer!
	}
	
	if (buffer->writepos-1<0) //Last pos?
	{
		buffer->readpos = buffer->size-1; //Goto end!
	}
	else
	{
		buffer->readpos = buffer->writepos-1; //Last write!
	}
}