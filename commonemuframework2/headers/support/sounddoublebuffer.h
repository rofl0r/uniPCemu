#ifndef SOUNDDOUBLEBUFFER_H
#define SOUNDDOUBLEBUFFER_H

#include "headers/types.h" //Basic types!
#include "headers/support/fifobuffer.h" //FIFO buffer support for our buffering needs!

typedef struct
{
	FIFOBUFFER *outputbuffer; //Output buffer for rendering!
	FIFOBUFFER *sharedbuffer; //Shared buffer for transfers!
	FIFOBUFFER *inputbuffer; //Input buffer for playback!
	uint_32 samplebuffersize; //Size of the sample buffers!
} SOUNDDOUBLEBUFFER;

//Basic (de)allocation
byte allocDoubleBufferedSound32(uint_32 samplebuffersize, SOUNDDOUBLEBUFFER *buffer, byte locked, double samplerate);
byte allocDoubleBufferedSound16(uint_32 samplebuffersize, SOUNDDOUBLEBUFFER *buffer, byte locked, double samplerate);
byte allocDoubleBufferedSound8(uint_32 samplebuffersize, SOUNDDOUBLEBUFFER *buffer, byte locked, double samplerate);
void freeDoubleBufferedSound(SOUNDDOUBLEBUFFER *buffer);

//Input&Output
void writeDoubleBufferedSound32(SOUNDDOUBLEBUFFER *buffer, uint_32 sample);
void writeDoubleBufferedSound16(SOUNDDOUBLEBUFFER *buffer, word sample);
void writeDoubleBufferedSound8(SOUNDDOUBLEBUFFER *buffer, byte sample);
byte readDoubleBufferedSound32(SOUNDDOUBLEBUFFER *buffer, uint_32 *sample);
byte readDoubleBufferedSound16(SOUNDDOUBLEBUFFER *buffer, word *sample);
byte readDoubleBufferedSound8(SOUNDDOUBLEBUFFER *buffer, byte *sample);
#endif