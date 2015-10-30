#include "headers/types.h"
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/support/fifobuffer.h" //Our types etc.

//Are we disabled?
#define __HW_DISABLED 0

/*

newbuffer: generates a new buffer to work with.
parameters:
	buffersize: the size of the buffer!
	filledcontainer: pointer to filled variabele for this buffer
	sizecontainer: pointer to size variabele for this buffer
result:
	Buffer for allocated, NULL for error when allocating!

*/

FIFOBUFFER* allocfifobuffer(uint_32 buffersize, byte lockable)
{
	FIFOBUFFER *container;
	if (__HW_DISABLED) return NULL; //Abort!
	container = (FIFOBUFFER *)zalloc(sizeof(FIFOBUFFER),"FIFOBuffer",NULL); //Allocate an container!
	if (container) //Allocated container?
	{
		container->buffer = (byte *)zalloc(buffersize,"FIFOBuffer_Buffer",NULL); //Try to allocate the buffer!
		if (!container->buffer) //No buffer?
		{
			freez((void **)&container,sizeof(FIFOBUFFER),"Failed FIFOBuffer"); //Release the container!
			return NULL; //Not allocated!
		}
		container->size = buffersize; //Set the buffer size!
		if (lockable) //Lockable FIFO buffer?
		{
			container->lock = SDL_CreateSemaphore(1); //Create our lock!
			if (!container->lock) //Failed to lock?
			{
				freez((void **)&container, sizeof(FIFOBUFFER), "Failed FIFOBuffer"); //Release the container!
				freez((void **)&container->buffer, buffersize, "FIFOBuffer_Buffer"); //Release the buffer!
				return NULL; //Not allocated: can't lock!
			}
		}
		//The reset is ready to work with: all 0!
	}
	return container; //Give the allocated FIFO container!
}

void free_fifobuffer(FIFOBUFFER **buffer)
{
	FIFOBUFFER *container;
	if (__HW_DISABLED) return; //Abort!
	if (buffer) //Valid?
	{
		if (memprotect(*buffer,sizeof(FIFOBUFFER),NULL)) //Valid point?
		{
			container = *buffer; //Get the buffer itself!
			if (memprotect(container->buffer,container->size,NULL)) //Valid?
			{
				freez((void **)&container->buffer,container->size,"Free FIFOBuffer_buffer"); //Release the buffer!
			}
			SDL_DestroySemaphore(container->lock); //Release the lock!
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
	uint_32 result;
	if (buffer->lock) WaitSem(buffer->lock)
	if (buffer->readpos == buffer->writepos) //Either full or empty?
	{
		result = buffer->lastwaswrite ? 0 : buffer->size; //Full when last was write, else empty!
	}
	else if (buffer->readpos>buffer->writepos) //Read after write index? We're a simple difference!
	{
		result = buffer->readpos - buffer->writepos;
	}
	else //The read position is before or at the write position? We wrap arround!
	{
		result = (buffer->size - buffer->writepos) + buffer->readpos;
	}
	if (buffer->lock) PostSem(buffer->lock)
	return result; //Give the result!
}

void waitforfreefifobuffer(FIFOBUFFER *buffer, uint_32 size)
{
	if (__HW_DISABLED) return; //Abort!
	for (;fifobuffer_freesize(buffer)<size;) delay(0); //Wait for the buffer to have enough free size!
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
		if (buffer->lock) WaitSem(buffer->lock)
		*result = buffer->buffer[buffer->readpos]; //Give the data!
		if (buffer->lock) PostSem(buffer->lock)
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
		if (buffer->lock) WaitSem(buffer->lock)
		*result = buffer->buffer[buffer->readpos];
		buffer->readpos = SAFEMOD((buffer->readpos+1),buffer->size); //Update the position!
		buffer->lastwaswrite = 0; //Last operation was a read operation!
		if (buffer->lock) PostSem(buffer->lock)
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
	
	if (buffer->lock) WaitSem(buffer->lock)
	buffer->buffer[buffer->writepos] = data; //Write!
	buffer->writepos = SAFEMOD((buffer->writepos+1),buffer->size); //Next pos!
	buffer->lastwaswrite = 1; //Last operation was a write operation!
	if (buffer->lock) PostSem(buffer->lock)
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
	
	if (fifobuffer_freesize(buffer) == buffer->size) return; //Empty? We can't: there is nothing to go back to!

	if (buffer->lock) WaitSem(buffer->lock)
	if ((((int_64)buffer->writepos)-1)<0) //Last pos?
	{
		buffer->readpos = buffer->size-1; //Goto end!
	}
	else
	{
		buffer->readpos = buffer->writepos-1; //Last write!
	}
	if (buffer->lock) PostSem(buffer->lock)
}