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
	/*if (!memprotect(buffer,sizeof(FIFOBUFFER),NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer,buffer->size,NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}*/
	if (!buffer) return 0; //Error: invalid buffer!
	if (!buffer->buffer) return 0; //Error invalid: buffer!
	uint_32 result;
	if (buffer->lock) WaitSem(buffer->lock)
	if (buffer->position[0].readpos == buffer->position[0].writepos) //Either full or empty?
	{
		result = buffer->position[0].lastwaswrite ? 0 : buffer->size; //Full when last was write, else empty!
	}
	else if (buffer->position[0].readpos>buffer->position[0].writepos) //Read after write index? We're a simple difference!
	{
		result = buffer->position[0].readpos - buffer->position[0].writepos;
	}
	else //The read position is before or at the write position? We wrap arround!
	{
		result = (buffer->size - buffer->position[0].writepos) + buffer->position[0].readpos;
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
	/*if (!memprotect(buffer,sizeof(FIFOBUFFER),NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer,buffer->size,NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}*/
	if (!buffer) return 0; //Error: invalid buffer!
	if (!buffer->buffer) return 0; //Error invalid: buffer!


	if (fifobuffer_freesize(buffer)<buffer->size) //Filled?
	{
		if (buffer->lock) WaitSem(buffer->lock)
		*result = buffer->buffer[buffer->position[0].readpos]; //Give the data!
		if (buffer->lock) PostSem(buffer->lock)
		return 1; //Something to peek at!
	}
	return 0; //Nothing to peek at!
}

OPTINLINE static void readfifobufferunlocked(FIFOBUFFER *buffer, byte *result)
{
	*result = buffer->buffer[buffer->position[0].readpos++]; //Read and update!
	if (buffer->position[0].readpos >= buffer->size) buffer->position[0].readpos = 0; //Wrap arround when needed!
	buffer->position[0].lastwaswrite = 0; //Last operation was a read operation!
}

int readfifobuffer(FIFOBUFFER *buffer, byte *result)
{
	if (__HW_DISABLED) return 0; //Abort!
	/*if (!memprotect(buffer,sizeof(FIFOBUFFER),NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer,buffer->size,NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}*/
	if (!buffer) return 0; //Error: invalid buffer!
	if (!buffer->buffer) return 0; //Error invalid: buffer!

	if (fifobuffer_freesize(buffer)<buffer->size) //Filled?
	{
		if (buffer->lock) WaitSem(buffer->lock)
		readfifobufferunlocked(buffer,result); //Read the FIFO buffer without lock!
		if (buffer->lock) PostSem(buffer->lock)
		return 1; //Read!
	}

	return 0; //Nothing to read!
}

OPTINLINE static void writefifobufferunlocked(FIFOBUFFER *buffer, byte data)
{
	buffer->buffer[buffer->position[0].writepos++] = data; //Write and update!
	if (buffer->position[0].writepos >= buffer->size) buffer->position[0].writepos = 0; //Wrap arround when needed!
	buffer->position[0].lastwaswrite = 1; //Last operation was a write operation!
}

int writefifobuffer(FIFOBUFFER *buffer, byte data)
{
	if (__HW_DISABLED) return 0; //Abort!
	/*if (!memprotect(buffer,sizeof(FIFOBUFFER),NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}
	
	if (!memprotect(buffer->buffer,buffer->size,NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}*/

	if (!buffer) return 0; //Error: invalid buffer!
	if (!buffer->buffer) return 0; //Error invalid: buffer!

	if (fifobuffer_freesize(buffer)<1) //Buffer full?
	{
		return 0; //Error: buffer full!
	}
	
	if (buffer->lock) WaitSem(buffer->lock)
	writefifobufferunlocked(buffer,data); //Write the FIFO buffer without lock!
	if (buffer->lock) PostSem(buffer->lock)
	return 1; //Written!
}

int peekfifobuffer16(FIFOBUFFER *buffer, word *result) //Is there data to be read?
{
	if (__HW_DISABLED) return 0; //Abort!
	/*if (!memprotect(buffer, sizeof(FIFOBUFFER), NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer, buffer->size, NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}*/
	if (!buffer) return 0; //Error: invalid buffer!
	if (!buffer->buffer) return 0; //Error invalid: buffer!

	if (fifobuffer_freesize(buffer)<(buffer->size-1)) //Filled?
	{
		uint_32 readpos;
		readpos = buffer->position[0].readpos; //Current reading position!
		if (buffer->lock) WaitSem(buffer->lock)
		*result = buffer->buffer[readpos++]; //Read and update!
		if (readpos >= buffer->size) readpos = 0; //Wrap arround when needed!
		*result <<= 8; //Shift high!
		*result |= buffer->buffer[readpos]; //Read and update!
		if (buffer->lock) PostSem(buffer->lock)
		return 1; //Something to peek at!
	}
	return 0; //Nothing to peek at!
}

OPTINLINE static void readfifobuffer16unlocked(FIFOBUFFER *buffer, word *result)
{
	*result = buffer->buffer[buffer->position[0].readpos++]; //Read and update high!
	if (buffer->position[0].readpos >= buffer->size) buffer->position[0].readpos = 0; //Wrap arround when needed!
	*result <<= 8; //Shift high!
	*result |= buffer->buffer[buffer->position[0].readpos++]; //Read and update low!
	if (buffer->position[0].readpos >= buffer->size) buffer->position[0].readpos = 0; //Wrap arround when needed!
	buffer->position[0].lastwaswrite = 0; //Last operation was a read operation!
}

int readfifobuffer16(FIFOBUFFER *buffer, word *result)
{
	if (__HW_DISABLED) return 0; //Abort!
	/*if (!memprotect(buffer, sizeof(FIFOBUFFER), NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer, buffer->size, NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}*/
	if (!buffer) return 0; //Error: invalid buffer!
	if (!buffer->buffer) return 0; //Error invalid: buffer!

	if (fifobuffer_freesize(buffer)<(buffer->size-1)) //Filled?
	{
		if (buffer->lock) WaitSem(buffer->lock)
		readfifobuffer16unlocked(buffer,result); //Read the FIFO buffer without lock!
		if (buffer->lock) PostSem(buffer->lock)
		return 1; //Read!
	}

	return 0; //Nothing to read!
}

OPTINLINE static void writefifobuffer16unlocked(FIFOBUFFER *buffer, word data)
{
	buffer->buffer[buffer->position[0].writepos++] = (data >> 8); //Write high and update!
	if (buffer->position[0].writepos >= buffer->size) buffer->position[0].writepos = 0; //Wrap arround when needed!
	buffer->buffer[buffer->position[0].writepos++] = (data & 0xFF); //Write low and update!
	if (buffer->position[0].writepos >= buffer->size) buffer->position[0].writepos = 0; //Wrap arround when needed!
	buffer->position[0].lastwaswrite = 1; //Last operation was a write operation!
}

int writefifobuffer16(FIFOBUFFER *buffer, word data)
{
	if (__HW_DISABLED) return 0; //Abort!
	/*if (!memprotect(buffer, sizeof(FIFOBUFFER), NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer, buffer->size, NULL)) //Error?
	{
		return 0; //Error: invalid buffer!
	}*/
	if (!buffer) return 0; //Error: invalid buffer!
	if (!buffer->buffer) return 0; //Error invalid: buffer!

	if (fifobuffer_freesize(buffer)<2) //Buffer full?
	{
		return 0; //Error: buffer full!
	}

	if (buffer->lock) WaitSem(buffer->lock)
	writefifobuffer16unlocked(buffer,data); //Write the FIFO buffer without lock!
	if (buffer->lock) PostSem(buffer->lock)
	return 1; //Written!
}


void fifobuffer_gotolast(FIFOBUFFER *buffer)
{
	if (__HW_DISABLED) return; //Abort!
	/*if (!memprotect(buffer,sizeof(FIFOBUFFER),NULL)) //Error?
	{
		return; //Error: invalid buffer!
	}
	if (!memprotect(buffer->buffer,buffer->size,NULL)) //Error?
	{
		return; //Error: invalid buffer!
	}*/
	if (!buffer) return; //Error: invalid buffer!
	if (!buffer->buffer) return; //Error invalid: buffer!
	
	if (fifobuffer_freesize(buffer) == buffer->size) return; //Empty? We can't: there is nothing to go back to!

	if (buffer->lock) WaitSem(buffer->lock)
	if ((((int_64)buffer->position[0].writepos)-1)<0) //Last pos?
	{
		buffer->position[0].readpos = buffer->size-1; //Goto end!
	}
	else
	{
		buffer->position[0].readpos = buffer->position[0].writepos-1; //Last write!
	}
	if (buffer->lock) PostSem(buffer->lock)
}

void fifobuffer_save(FIFOBUFFER *buffer)
{
	/*if (!memprotect(buffer, sizeof(FIFOBUFFER), NULL)) //Error?
	{
		return; //Error: invalid buffer!
	}*/
	if (!buffer) return; //Error: invalid buffer!

	if (buffer->lock) WaitSem(buffer->lock)
	memcpy(&buffer->position[1],&buffer->position[0],sizeof(buffer->position[1])); //Backup!
	if (buffer->lock) PostSem(buffer->lock)
}

void fifobuffer_restore(FIFOBUFFER *buffer)
{
	/*if (!memprotect(buffer, sizeof(FIFOBUFFER), NULL)) //Error?
	{
		return; //Error: invalid buffer!
	}*/
	if (!buffer) return; //Error: invalid buffer!

	if (buffer->lock) WaitSem(buffer->lock)
	memcpy(&buffer->position[0], &buffer->position[1], sizeof(buffer->position[0])); //Restore!
	if (buffer->lock) PostSem(buffer->lock)
}

void fifobuffer_clear(FIFOBUFFER *buffer)
{
	byte temp; //Saved data to discard!
	/*if (!memprotect(buffer, sizeof(FIFOBUFFER), NULL)) //Error?
	{
		return; //Error: invalid buffer!
	}*/
	if (!buffer) return; //Error: invalid buffer!

	fifobuffer_gotolast(buffer); //Goto last!
	readfifobuffer(buffer,&temp); //Clean out the last byte if it's there!
}

void movefifobuffer8(FIFOBUFFER *src, FIFOBUFFER *dest, uint_32 threshold)
{
	if ((src == dest) || (!threshold)) return; //Can't move to itself!
	uint_32 current; //Current thresholded data index!
	byte buffer; //our buffer for the transfer!
	if (!src) return; //Invalid source!
	if (!dest) return; //Invalid destination!
	if (fifobuffer_freesize(src) < (src->size - threshold)) //Buffered enough words of data?
	{
		if (fifobuffer_freesize(dest) >= threshold) //Enough free space left?
		{
			//Transfer the data!
			if (src->lock) WaitSem(src->lock) //Lock the source!
			if (dest->lock) WaitSem(dest->lock) //Lock the destination!
			//Now quickly move the thesholded data from the source to the destination!
			current = threshold; //Move threshold items!
			do //Process all items fast!
			{
				readfifobufferunlocked(src, &buffer); //Read 8-bit data!
				writefifobufferunlocked(dest, buffer); //Write 8-bit data!
			} while (--current);
			if (dest->lock) PostSem(dest->lock) //Unlock the destination!
			if (src->lock) PostSem(src->lock) //Unlock the source!
		}
	}
}

void movefifobuffer16(FIFOBUFFER *src, FIFOBUFFER *dest, uint_32 threshold)
{
	if ((src==dest) || (!threshold)) return; //Can't move to itself!
	uint_32 current; //Current thresholded data index!
	word buffer; //our buffer for the transfer!
	if (!src) return; //Invalid source!
	if (!dest) return; //Invalid destination!
	threshold <<= 1; //Make the threshold word-sized, since we're moving word items!
	if (fifobuffer_freesize(src) < (src->size - threshold)) //Buffered enough words of data?
	{
		if (fifobuffer_freesize(dest) >= threshold) //Enough free space left?
		{
			threshold >>= 1; //Make it into actual data items!
			//Transfer the data!
			if (src->lock) WaitSem(src->lock) //Lock the source!
			if (dest->lock) WaitSem(dest->lock) //Lock the destination!
			//Now quickly move the thesholded data from the source to the destination!
			current = threshold; //Move threshold items!
			do //Process all items fast!
			{
				readfifobuffer16unlocked(src,&buffer); //Read 16-bit data!
				writefifobuffer16unlocked(dest,buffer); //Write 16-bit data!
			} while (--current);
			if (dest->lock) PostSem(dest->lock) //Unlock the destination!
			if (src->lock) PostSem(src->lock) //Unlock the source!
		}
	}
}
