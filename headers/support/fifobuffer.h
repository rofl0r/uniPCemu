#ifndef VARBUFFER_H
#define VARBUFFER_H

#include "headers/types.h" //Basic type support!

typedef struct
{
byte *buffer; //The buffer itself!
uint_32 size; //The size of the buffer!
uint_32 readpos; //The position to read!
uint_32 writepos; //The position to write!
byte lastwaswrite; //Last operation was a write?
SDL_sem *lock; //Our lock for single access!
} FIFOBUFFER;

/*

allocfifobuffer: generates a new buffer to work with.
parameters:
	buffersize: the size of the buffer!
	lockable: 1 to lock during accesses, 0 to use external locking when needed!
result:
	Buffer when allocated, NULL for error when allocating!

*/


FIFOBUFFER* allocfifobuffer(uint_32 buffersize, byte lockable); //Creates a new FIFO buffer!

/*

free_fifobuffer: Release a fifo buffer!

*/

void free_fifobuffer(FIFOBUFFER **buffer);

/*

fifobuffer_freesize: Determines how much data we have free (in units of buffer items)
paremeters:
	buffer: The buffer.
result:
	The ammount of items free.

*/

uint_32 fifobuffer_freesize(FIFOBUFFER *buffer);

/*

waitforfreefifobuffer: Waits till some space is free. NOT TO BE USED BY THE HANDLER THAT READS IT!
parameters:
	buffer: The buffer.
	size: The size to wait for.

*/

void waitforfreefifobuffer(FIFOBUFFER *buffer, uint_32 size);


/*

peekfifobuffer: Is there data to be read?
returns:
	1 when available, 0 when not available.
	result is set to the value when available.

*/

byte peekfifobuffer(FIFOBUFFER *buffer, byte *result); //Is there data to be read?

/*

readfifobuffer: Tries to read data from a buffer (from the start)
parameters:
	buffer: pointer to the allocated buffer itself.
	result: pointer to variabele for read data
result:
	TRUE for read, FALSE for no data to read.

*/

byte readfifobuffer(FIFOBUFFER *buffer, byte *result);

/*

writefifobuffer: Writes data to the buffer (at the end)
parameters:
	buffer: pointer to the buffer itself.
	data: the data to be written to the buffer!
result:
	TRUE for written, FALSE for buffer full.
*/


byte writefifobuffer(FIFOBUFFER *buffer, byte data);

/*

fifobuffer_gotolast: Set last write to current read, if any!
parameters:
	buffer: pointer to the buffer itself.

*/

void fifobuffer_gotolast(FIFOBUFFER *buffer);

/*

fifobuffer_clear: Remove all FIFO items from the buffer!
parameters:
	buffer: pointer to the buffer itself.

*/


void fifobuffer_clear(FIFOBUFFER *buffer);

/*

movefifobuffer8: Moved threshold items from the source to the destination buffer once threshold bytes are used.
parameters:
	src: pointer to the source buffer.
	dest: pointer to the destination buffer.
	threshold: The threshold, in FIFO buffer items.

*/

void movefifobuffer8(FIFOBUFFER *src, FIFOBUFFER *dest, uint_32 threshold);

/* 16-bit adjustments */

byte peekfifobuffer16(FIFOBUFFER *buffer, word *result); //Is there data to be read?
byte readfifobuffer16(FIFOBUFFER *buffer, word *result);
byte writefifobuffer16(FIFOBUFFER *buffer, word data);
void movefifobuffer16(FIFOBUFFER *src, FIFOBUFFER *dest, uint_32 threshold);

#endif