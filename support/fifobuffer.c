#include "headers/types.h"
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/support/fifobuffer.h" //Our types etc.

//Are we disabled?
#define __HW_DISABLED 0

extern byte allcleared; //Are all pointers cleared?

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

OPTINLINE uint_32 fifobuffer_INTERNAL_freesize(FIFOBUFFER *buffer)
{
	if (__HW_DISABLED) return 0; //Abort!
	INLINEREGISTER uint_32 readpos, writepos;
	if ((readpos = buffer->readpos)!=(writepos = buffer->writepos)) //Not at the same position to read&write?
	{
		if (readpos>writepos) //Read after write index? We're a simple difference!
		{
			return readpos - writepos;
		}
		else //Read before write index? We're a complicated difference!
		{
			//The read position is before or at the write position? We wrap arround!
			return (buffer->size -writepos) + readpos;
		}
	}
	else //Readpos = Writepos? Either full or empty?
	{
		if (buffer->lastwaswrite) //Last was write?
			return 0;
		else //Last was read?
			return buffer->size; //Full when last was write, else empty!
	}
}

uint_32 fifobuffer_freesize(FIFOBUFFER *buffer)
{
	INLINEREGISTER uint_32 result;
	if (buffer==0) return 0; //Error: invalid buffer!
	if (buffer->buffer==0) return 0; //Error invalid: buffer!
	if (allcleared) return 0; //Abort: invalid buffer!
	if (buffer->lock) //Locked buffer?
	{
		WaitSem(buffer->lock)
		result = fifobuffer_INTERNAL_freesize(buffer); //Original wrapper!
		PostSem(buffer->lock)
	}
	else //Lockless?
	{
		return fifobuffer_INTERNAL_freesize(buffer); //Original wrapper!
	}
	return result; //Give the result!
}

void waitforfreefifobuffer(FIFOBUFFER *buffer, uint_32 size)
{
	if (__HW_DISABLED) return; //Abort!
	for (;fifobuffer_freesize(buffer)<size;) delay(0); //Wait for the buffer to have enough free size!
}

byte peekfifobuffer(FIFOBUFFER *buffer, byte *result) //Is there data to be read?
{
	if (__HW_DISABLED) return 0; //Abort!
	if (buffer==0) return 0; //Error: invalid buffer!
	if (buffer->buffer==0) return 0; //Error invalid: buffer!
	if (allcleared) return 0; //Abort: invalid buffer!

	if (buffer->lock)
	{
		WaitSem(buffer->lock)
		if (fifobuffer_INTERNAL_freesize(buffer)<buffer->size) //Filled?
		{
			*result = buffer->buffer[buffer->readpos]; //Give the data!
			PostSem(buffer->lock)
			return 1; //Something to peek at!
		}
		PostSem(buffer->lock)
	}
	else
	{
		if (fifobuffer_INTERNAL_freesize(buffer)<buffer->size) //Filled?
		{
			*result = buffer->buffer[buffer->readpos]; //Give the data!
			return 1; //Something to peek at!
		}
	}
	return 0; //Nothing to peek at!
}

OPTINLINE static void readfifobufferunlocked(FIFOBUFFER *buffer, byte *result)
{
	INLINEREGISTER uint_32 readpos;
	readpos = buffer->readpos; //Load the old read position!
	*result = buffer->buffer[readpos++]; //Read and update!
	if (readpos >= buffer->size) readpos = 0; //Wrap arround when needed!
	buffer->readpos = readpos; //Update the read position!
	buffer->lastwaswrite = 0; //Last operation was a read operation!
}

byte readfifobuffer(FIFOBUFFER *buffer, byte *result)
{
	if (__HW_DISABLED) return 0; //Abort!
	if (buffer==0) return 0; //Error: invalid buffer!
	if (allcleared) return 0; //Abort: invalid buffer!
	if (buffer->buffer) //Valid buffer?
	{
		if (buffer->lock)
		{
			WaitSem(buffer->lock)
			if (fifobuffer_INTERNAL_freesize(buffer)<buffer->size) //Filled?
			{
				readfifobufferunlocked(buffer,result); //Read the FIFO buffer without lock!
				PostSem(buffer->lock)
				return 1; //Read!
			}
			PostSem(buffer->lock)
		}
		else
		{
			if (fifobuffer_INTERNAL_freesize(buffer)!=buffer->size) //Filled?
			{
				readfifobufferunlocked(buffer, result); //Read the FIFO buffer without lock!
				return 1; //Read!
			}
		}
	}
	return 0; //Nothing to read or invalid buffer!
}

OPTINLINE static void writefifobufferunlocked(FIFOBUFFER *buffer, byte data)
{
	INLINEREGISTER uint_32 writepos;
	writepos = buffer->writepos; //Load the old write position!
	buffer->buffer[writepos++] = data; //Write and update!
	if (writepos >= buffer->size) writepos = 0; //Wrap arround when needed!
	buffer->writepos = writepos; //Update the write position!
	buffer->lastwaswrite = 1; //Last operation was a write operation!
}

byte writefifobuffer(FIFOBUFFER *buffer, byte data)
{
	if (__HW_DISABLED) return 0; //Abort!
	if (buffer==0) return 0; //Error: invalid buffer!
	if (buffer->buffer==0) return 0; //Error invalid: buffer!
	if (allcleared) return 0; //Abort: invalid buffer!

	if (buffer->lock)
	{
		WaitSem(buffer->lock)
		if (fifobuffer_INTERNAL_freesize(buffer)<1) //Buffer full?
		{
			PostSem(buffer->lock)
			return 0; //Error: buffer full!
		}
		writefifobufferunlocked(buffer,data); //Write the FIFO buffer without lock!
		PostSem(buffer->lock)
	}
	else
	{
		if (fifobuffer_INTERNAL_freesize(buffer)<1) //Buffer full?
		{
			return 0; //Error: buffer full!
		}
		writefifobufferunlocked(buffer, data); //Write the FIFO buffer without lock!
	}
	return 1; //Written!
}

byte peekfifobuffer16(FIFOBUFFER *buffer, word *result) //Is there data to be read?
{
	if (__HW_DISABLED) return 0; //Abort!
	if (buffer==0) return 0; //Error: invalid buffer!
	if (buffer->buffer==0) return 0; //Error invalid: buffer!
	if (allcleared) return 0; //Abort: invalid buffer!

	if (buffer->lock)
	{
		WaitSem(buffer->lock)
		if (fifobuffer_INTERNAL_freesize(buffer)<(buffer->size-1)) //Filled?
		{
			INLINEREGISTER uint_32 readpos;
			readpos = buffer->readpos; //Current reading position!
			*result = buffer->buffer[readpos++]; //Read and update!
			if (readpos >= buffer->size) readpos = 0; //Wrap arround when needed!
			*result <<= 8; //Shift high!
			*result |= buffer->buffer[readpos]; //Read and update!
			PostSem(buffer->lock)
			return 1; //Something to peek at!
		}
		PostSem(buffer->lock)
	}
	else
	{
		if (fifobuffer_INTERNAL_freesize(buffer)<(buffer->size - 1)) //Filled?
		{
			INLINEREGISTER uint_32 readpos;
			readpos = buffer->readpos; //Current reading position!
			*result = buffer->buffer[readpos++]; //Read and update!
			if (readpos >= buffer->size) readpos = 0; //Wrap arround when needed!
			*result <<= 8; //Shift high!
			*result |= buffer->buffer[readpos]; //Read and update!
			return 1; //Something to peek at!
		}
	}
	return 0; //Nothing to peek at!
}

OPTINLINE static void readfifobuffer16unlocked(FIFOBUFFER *buffer, word *result)
{
	INLINEREGISTER uint_32 readpos,size;
	size = buffer->size; //Size of the buffer to wrap around!
	readpos = buffer->readpos; //Load the old read position!
	*result = buffer->buffer[readpos++]; //Read and update high!
	if (readpos >= size) readpos = 0; //Wrap arround when needed!
	*result <<= 8; //Shift high!
	*result |= buffer->buffer[readpos++]; //Read and update low!
	if (readpos >= buffer->size) readpos = 0; //Wrap arround when needed!
	buffer->readpos = readpos; //Update our the position!
	buffer->lastwaswrite = 0; //Last operation was a read operation!
}

byte readfifobuffer16(FIFOBUFFER *buffer, word *result)
{
	if (__HW_DISABLED) return 0; //Abort!
	if (buffer==0) return 0; //Error: invalid buffer!
	if (buffer->buffer==0) return 0; //Error invalid: buffer!
	if (allcleared) return 0; //Abort: invalid buffer!

	if (buffer->lock)
	{
		WaitSem(buffer->lock)
		if (fifobuffer_INTERNAL_freesize(buffer)<(buffer->size-1)) //Filled?
		{
			readfifobuffer16unlocked(buffer,result); //Read the FIFO buffer without lock!
			PostSem(buffer->lock)
			return 1; //Read!
		}
		PostSem(buffer->lock)
	}
	else
	{
		if (fifobuffer_INTERNAL_freesize(buffer)<(buffer->size - 1)) //Filled?
		{
			readfifobuffer16unlocked(buffer, result); //Read the FIFO buffer without lock!
			return 1; //Read!
		}
	}
	return 0; //Nothing to read!
}

OPTINLINE static void writefifobuffer16unlocked(FIFOBUFFER *buffer, word data)
{
	INLINEREGISTER uint_32 writepos, size;
	size = buffer->size; //Load the size!
	writepos = buffer->writepos; //Load the write position!
	buffer->buffer[writepos++] = (data >> 8); //Write high and update!
	if (writepos >= size) writepos = 0; //Wrap arround when needed!
	buffer->buffer[writepos++] = (data & 0xFF); //Write low and update!
	if (writepos >= size) writepos = 0; //Wrap arround when needed!
	buffer->writepos = writepos; //Update the write position!
	buffer->lastwaswrite = 1; //Last operation was a write operation!
}

byte writefifobuffer16(FIFOBUFFER *buffer, word data)
{
	if (__HW_DISABLED) return 0; //Abort!
	if (buffer==0) return 0; //Error: invalid buffer!
	if (buffer->buffer==0) return 0; //Error invalid: buffer!
	if (allcleared) return 0; //Error: invalid buffer!

	if (buffer->lock)
	{
		WaitSem(buffer->lock)
		if (fifobuffer_INTERNAL_freesize(buffer)<2) //Buffer full?
		{
			PostSem(buffer->lock)
			return 0; //Error: buffer full!
		}

		writefifobuffer16unlocked(buffer,data); //Write the FIFO buffer without lock!
		PostSem(buffer->lock)
	}
	else
	{
		if (fifobuffer_INTERNAL_freesize(buffer)<2) //Buffer full?
		{
			return 0; //Error: buffer full!
		}

		writefifobuffer16unlocked(buffer, data); //Write the FIFO buffer without lock!
	}
	return 1; //Written!
}


void fifobuffer_gotolast(FIFOBUFFER *buffer)
{
	if (__HW_DISABLED) return; //Abort!
	if (buffer==0) return; //Error: invalid buffer!
	if (buffer->buffer==0) return; //Error invalid: buffer!
	if (allcleared) return; //Abort: invalid buffer!

	if (buffer->lock)
	{
		WaitSem(buffer->lock)
		if (fifobuffer_INTERNAL_freesize(buffer) == buffer->size)
		{
			PostSem(buffer->lock)
			return; //Empty? We can't: there is nothing to go back to!
		}

		if ((((int_64)buffer->writepos)-1)<0) //Last pos?
		{
			buffer->readpos = buffer->size-1; //Goto end!
		}
		else
		{
			buffer->readpos = buffer->writepos-1; //Last write!
		}
		PostSem(buffer->lock)
	}
	else
	{
		if (fifobuffer_INTERNAL_freesize(buffer) == buffer->size)
		{
			return; //Empty? We can't: there is nothing to go back to!
		}

		if ((((int_64)buffer->writepos) - 1)<0) //Last pos?
		{
			buffer->readpos = buffer->size - 1; //Goto end!
		}
		else
		{
			buffer->readpos = buffer->writepos - 1; //Last write!
		}
	}
}

void fifobuffer_clear(FIFOBUFFER *buffer)
{
	byte temp; //Saved data to discard!
	if (buffer==0) return; //Error: invalid buffer!

	fifobuffer_gotolast(buffer); //Goto last!
	readfifobuffer(buffer,&temp); //Clean out the last byte if it's there!
}

void movefifobuffer8(FIFOBUFFER *src, FIFOBUFFER *dest, uint_32 threshold)
{
	if (allcleared) return; //Abort: invalid buffer!
	if ((src == dest) || (!threshold)) return; //Can't move to itself!
	INLINEREGISTER uint_32 current; //Current thresholded data index!
	byte buffer; //our buffer for the transfer!
	if (!src) return; //Invalid source!
	if (!dest) return; //Invalid destination!
	if (src->lock) WaitSem(src->lock) //Lock the source!
	if (fifobuffer_INTERNAL_freesize(src) <= (src->size - threshold)) //Buffered enough words of data?
	{
		if (dest->lock) WaitSem(dest->lock) //Lock the destination!
		if (fifobuffer_INTERNAL_freesize(dest) >= threshold) //Enough free space left?
		{
			//Now quickly move the thesholded data from the source to the destination!
			current = threshold; //Move threshold items!
			do //Process all items as fast as possible!
			{
				readfifobufferunlocked(src, &buffer); //Read 8-bit data!
				writefifobufferunlocked(dest, buffer); //Write 8-bit data!
			} while (--current);
		}
		if (dest->lock) PostSem(dest->lock) //Unlock the destination!
	}
	if (src->lock) PostSem(src->lock) //Unlock the source!
}

void movefifobuffer16(FIFOBUFFER *src, FIFOBUFFER *dest, uint_32 threshold)
{
	if (allcleared) return; //Abort: invalid buffer!
	if ((src==dest) || (!threshold)) return; //Can't move to itself!
	INLINEREGISTER uint_32 current; //Current thresholded data index!
	word buffer; //our buffer for the transfer!
	if (!src) return; //Invalid source!
	if (!dest) return; //Invalid destination!
	threshold <<= 1; //Make the threshold word-sized, since we're moving word items!
	if (src->lock) WaitSem(src->lock) //Lock the source!
	if (fifobuffer_INTERNAL_freesize(src) <= (src->size - threshold)) //Buffered enough words of data?
	{
		if (dest->lock) WaitSem(dest->lock) //Lock the destination!
		if (fifobuffer_INTERNAL_freesize(dest) >= threshold) //Enough free space left?
		{
			threshold >>= 1; //Make it into actual data items!
			//Now quickly move the thesholded data from the source to the destination!
			current = threshold; //Move threshold items!
			do //Process all items as fast as possible!
			{
				readfifobuffer16unlocked(src,&buffer); //Read 16-bit data!
				writefifobuffer16unlocked(dest,buffer); //Write 16-bit data!
			} while (--current);
		}
		if (dest->lock) PostSem(dest->lock) //Unlock the destination!
	}
	if (src->lock) PostSem(src->lock) //Unlock the source!
}