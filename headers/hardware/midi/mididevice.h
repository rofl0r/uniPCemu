#ifndef MIDIDEVICE_H
#define MIDIDEVICE_H

typedef struct
{
byte command; //What command?
byte buffer[2]; //The parameter buffer!
void *next; //Next command, if any!
} MIDICOMMAND, *MIDIPTR;

void MIDIDEVICE_addbuffer(byte command, MIDIPTR data); //Add a command to the buffer!
//MIDICOMMAND *MIDIDEVICE_peekbuffer(); //Peek at the buffer!
//int MIDIDEVICE_readbuffer(MIDICOMMAND *result); //Read from the buffer!

void init_MIDIDEVICE(); //Initialise MIDI device for usage!
void done_MIDIDEVICE(); //Finish our midi device!

#endif