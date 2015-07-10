//Ports compatibility (e.g. for keyboard, mouse and joystick), input/output to hw ports from software!

#include "headers/types.h"
#include "headers/hardware/ports.h" //Full PORTIN/OUT compatibility!

/*

We handle direct input/output to/from hardware ports by the CPU!

*/

/*

//Initialises ports support!

*/

void Ports_Init()
{
	reset_ports(); //Passtrough: reset all ports!
}

byte PORT_IN_B(word port)
{
	byte result;
	if (EXEC_PORTIN(port,&result)) //Passtrough!
	{
		if (execNMI(0)) //Execute an NMI from Bus!
		{
			dolog("emu", "Warning: Unhandled PORT IN from port %04X", port);
		}
	}
	return result; //Give the result!
}

word PORT_IN_W(word port) //IN result,port
{
	union
	{
		struct
		{
			byte low;
			byte high;
		};
		word w;
	} splitter;
	splitter.low = PORT_IN_B(port); //Low first
	splitter.high = PORT_IN_B(port+1); //High last!
	return splitter.w; //Give word!
}

uint_32 PORT_IN_DW(word port) //IN result,port
{
	union
	{
		struct
		{
			word low;
			word high;
		};
		word w;
	} splitter;
	splitter.low = PORT_IN_W(port); //Low first
	splitter.high = PORT_IN_W(port+2); //High last!
	return splitter.w; //Give word!
}

void PORT_OUT_B(word port, byte b)
{
	if (EXEC_PORTOUT(port, b)) //Passtrough and error?
	{
		if (execNMI(0)) //Execute an NMI from Bus failed?
		{
			dolog("emu", "Warning: Unhandled PORT OUT to port %04X value %02X", port, b); //Report unhandled NMI!
		}
	}
}

void PORT_OUT_W(word port, word w) //OUT port,w
{
	union
	{
		struct
		{
			byte low;
			byte high;
		} byte;
		word w;
	} splitter;
	splitter.w = w; //Split!
	PORT_OUT_B(port,splitter.byte.low); //First low byte!
	PORT_OUT_B(port+1,splitter.byte.high); //Next high byte!
}

void PORT_OUT_DW(word port, uint_32 dw) //OUT port,w
{
	union
	{
		struct
		{
			word low;
			word high;
		} word;
		uint_32 dw;
	} splitter;
	splitter.dw = dw; //Split!
	PORT_OUT_W(port,splitter.word.low); //First low byte!
	PORT_OUT_W(port+2,splitter.word.high); //Next high byte!
}