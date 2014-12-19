#include "headers/types.h"
#include "headers/hardware/ports.h" //Basic port compatibility!
#include "headers/hardware/midi/mididevice.h" //MIDI Device compatibility!
#include "headers/support/fifobuffer.h" //FIFOBUFFER support!
#include "headers/hardware/midi/midi.h" //Our own stuff!
#include "headers/hardware/pic.h" //Interrupt support!

//http://www.oktopus.hu/imgs/MANAGED/Hangtechnikai_tudastar/The_MIDI_Specification.pdf

//HW: MPU-401: http://www.piclist.com/techref/io/serial/midi/mpu.html
//Protocol: http://www.gweep.net/~prefect/eng/reference/protocol/midispec.html

//MIDI ports: 330-331 or 300-301(exception rather than rule)

//Log MIDI output?
//#define __MIDI_LOG

//ACK/NACK!
#define MPU_ACK 0xFE
#define MPU_NACK 0xFF

struct
{
	MIDICOMMAND current;
	byte bufferpos; //Position in the buffer for midi commands.
	int command; //What command are we processing? -1 for none.
	//Internal MIDI support!
	byte has_result; //Do we have a result?
	FIFOBUFFER *inbuffer;
	int MPU_command; //What command of a result!
} MIDIDEV; //Midi device!

void resetMPU() //Fully resets the MPU!
{
	fifobuffer_gotolast(MIDIDEV.inbuffer); //Clear the FIFO buffer!
	byte dummy;
	readfifobuffer(MIDIDEV.inbuffer,&dummy); //Make sure it's cleared by clearing the final byte!
}

/*

Basic input/ouput functionality!

*/

void MIDI_writeStatus(byte data) //Write a status byte to the MIDI device!
{
	if (((data&0xF0)!=0xF0) || MIDIDEV.command==-1) //Not System comamnd?
	{
		MIDIDEV.command = data; //Update the status information!
		//Do something?
	}
	memset(&MIDIDEV.current,0,sizeof(MIDIDEV.current)); //Clear info on the current command!
	switch ((data>>4)&0xF) //What command?
	{
		case 0x8: case 0x9: case 0xA: case 0xB: case 0xC: case 0xD: case 0xE: //Normal commands?
			MIDIDEV.command = data; //Load the command!
			MIDIDEV.bufferpos = 0; //Init buffer position!
			break;
		case 0xF: //System realtime command?
			switch (data&0xF) //What command?
			{
				case 0x0: //SysEx?
				case 0x1: //MTC Quarter Frame Message?
				case 0x2: //Song Position Pointer?
				case 0x3: //Song Select?
					MIDIDEV.command = data; //Load the command to use!
					MIDIDEV.bufferpos = 0; //Initialise buffer pos for the command!
					break;
				case 0x6: //Tune Request?
					//Execute Tune Request!
					break;
				case 0x8: //MIDI Clock?
					//Execute MIDI Clock!
					MIDIDEVICE_addbuffer(0xF8,&MIDIDEV.current); //Add MIDI clock!
					break;
				case 0xA: //MIDI Start?
					//Execute MIDI Start!
					break;
				case 0xB: //MIDI Continue?
					//Execute MIDI Continue!
					break;
				case 0xC: //MIDI Stop?
					//Execute MIDI Stop!
					break;
				case 0xE: //Active Sense?
					//Execute Active Sense!
					break;
				case 0xF: //Reset?
					//Execute Reset!
					MIDIDEV.command = -1;
					MIDIDEV.bufferpos = 0;
					memset(MIDIDEV.current.buffer,0,sizeof(MIDIDEV.current.buffer));
					//We're reset!
					break;
				default: //Unknown?
					//Ignore the data: we're not supported yet!
					break;
			}
			break;
	}
}

void MIDI_writeData(byte data) //Write a data byte to the MIDI device!
{
	switch ((MIDIDEV.command>>4)&0xF) //What command?
	{
		case 0x8: //Note Off?
			MIDIDEV.current.buffer[MIDIDEV.bufferpos++] = data; //Add to the buffer!
			if (MIDIDEV.bufferpos==2) //Done when not giving input anymore!
			{
				//Process Note Off!
				MIDIDEVICE_addbuffer(MIDIDEV.command,&MIDIDEV.current); //Add MIDI command!
				MIDIDEV.bufferpos = 0; //Reset buffer position for the next command!
			}
			break;
		case 0x9: //Note On?
			MIDIDEV.current.buffer[MIDIDEV.bufferpos++] = data; //Add to the buffer!
			if (MIDIDEV.bufferpos==2) //Done when not giving input anymore!
			{
				//Process Note On!
				MIDIDEVICE_addbuffer(MIDIDEV.command,&MIDIDEV.current); //Add MIDI command!
				MIDIDEV.bufferpos = 0; //Reset buffer position for the next command!
			}
			break;			
		case 0xA: //AfterTouch?
			MIDIDEV.current.buffer[MIDIDEV.bufferpos++] = data; //Add to the buffer!
			if (MIDIDEV.bufferpos==2) //Done when not giving input anymore!
			{
				//Process Aftertouch!
				MIDIDEVICE_addbuffer(MIDIDEV.command,&MIDIDEV.current); //Add MIDI command!
				MIDIDEV.bufferpos = 0; //Reset buffer position for the next command!
			}
			break;
		case 0xB: //Control change?
			MIDIDEV.current.buffer[MIDIDEV.bufferpos++] = data; //Add to the buffer!
			if (MIDIDEV.bufferpos==2) //Done when not giving input anymore!
			{
				//Process Control change!
				MIDIDEVICE_addbuffer(MIDIDEV.command,&MIDIDEV.current); //Add MIDI command!
				MIDIDEV.bufferpos = 0; //Reset buffer position for the next command!
			}
			break;
		case 0xC: //Program (patch) change?
			//Process Program change!
			MIDIDEV.current.buffer[0] = data; //Load data!
			MIDIDEVICE_addbuffer(MIDIDEV.command,&MIDIDEV.current); //Add MIDI command!
			break;
		case 0xD: //Channel pressure?
			//Process channel pressure!
			MIDIDEV.current.buffer[0] = data; //Load data!
			MIDIDEVICE_addbuffer(MIDIDEV.command,&MIDIDEV.current); //Add MIDI command!
			break;
		case 0xE: //Pitch Wheel?
			MIDIDEV.current.buffer[MIDIDEV.bufferpos++] = data; //Add to the buffer!
			if (MIDIDEV.bufferpos==2) //Done when not giving input anymore!
			{
				//Process Pitch Wheel!
				//Pitch = ((MIDIDEV.current.buffer[1]<<7)|MIDIDEV.current.buffer[0])
				MIDIDEVICE_addbuffer(MIDIDEV.command,&MIDIDEV.current); //Add MIDI command!
				MIDIDEV.bufferpos = 0; //Reset buffer position for the next command!
			}
			break;
		case 0xF: //System message?
			switch (MIDIDEV.command&0xF) //What kind of message?
			{
				case 0: //SysEx?
					if (data==0xF7) //End of SysEx message?
					{
						//Handle the given SysEx message?
						MIDIDEV.command = -1; //Done!
						return; //Abort processing!
					}
					//Don't do anything with the data yet!
					break;
				case 0x1: //MTC Quarter Frame Message?
					//Process the parameter!
					break;
				case 0x2: //Song Position Pointer?
					MIDIDEV.current.buffer[MIDIDEV.bufferpos++] = data; //Add to the buffer!
					if (MIDIDEV.bufferpos==2) //Done when not giving input anymore!
					{
						//Process Song Position Pointer!
						//Pitch = ((MIDIDEV.current.buffer[1]<<7)|MIDIDEV.current.buffer[0])
						MIDIDEV.bufferpos = 0; //Reset buffer position for the next command!
					}
					break;
				case 0x3: //Song Select?
					//Execute song select with the data!
					break;
				default: //Unknown?
					break;
			}
			//Unknown, don't parse!
			break;
	}
}

byte MIDI_has_data() //Do we have data to be read?
{
	if (MIDIDEV.inbuffer) //Gotten a FIFO buffer?
	{
		byte temp;
		return peekfifobuffer(MIDIDEV.inbuffer,&temp)?1:0; //We're containing a result?
	}
	return 0; //We never have data to be read!
}

byte MIDI_readData() //Read data from the MPU!
{
	if (MIDIDEV.inbuffer) //We're containing a FIFO buffer?
	{
		byte result;
		if (readfifobuffer(MIDIDEV.inbuffer,&result))
		{
			return result; //Give the read result!
		}
	}
	return 0; //Unimplemented yet: we never have anything from hardware to read!
}


//MPU MIDI support!
//MIDI ports: 330-331 or 300-301(exception rather than rule)

void MIDI_OUT(byte data)
{
	#ifdef __MIDI_LOG
	dolog("MIDI","MIDI OUT: %02X",data); //Log it!
	#endif
	if (data&0x80)
	{
		MIDI_writeStatus(data);
	}
	else
	{
		MIDI_writeData(data);
	}
}

byte MIDI_IN()
{
	return MIDI_readData(); //Read data from the MPU!
}

void initMPU() //Initialise function!
{
	memset(&MIDIDEV,0,sizeof(MIDIDEV)); //Clear the MIDI device!
	MIDIDEV.inbuffer = allocfifobuffer(100); //Alloc FIFO buffer of 100 bytes!
	MIDIDEV.command = -1; //Default: no command there!
	init_MIDIDEVICE(); //Initialise the MIDI device!
	resetMPU(); //Reset the MPU!
	MPU401_Init(); //Init the dosbox handler for our MPU-401!
}

void doneMPU() //Finish function!
{
	done_MIDIDEVICE(); //Finish the MIDI device!
	free_fifobuffer(&MIDIDEV.inbuffer); //Free the FIFO buffer!
	MPU401_Done(); //Finish our MPU-401 system: custom!
}

void MPU401_Done() //Finish our MPU system! Custom by superfury1!
{
	removetimer("MPU"); //Remove the timer if it's still there!
	removeirq(2); //Remove the irq if it's still there!
}