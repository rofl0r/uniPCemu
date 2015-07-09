#include "headers/types.h" //Basic type comp.
#include "headers/hardware/ports.h" //Basic type comp.
#include "headers/support/log.h" //Logging support!
#include "headers/cpu/cpu.h" //NMI support!

/*

We handle mapping input and output to ports!

*/

//Log port input/output?
//#define __LOG_PORT

PORTIN PORT_IN[0x10000]; //For reading from ports!
PORTOUT PORT_OUT[0x10000]; //For writing to ports!

extern byte SystemControlPortB; //System control port B!

//Reset and register!

void reset_ports()
{
	SystemControlPortB = 0x00; //Reset system control port B!
	int i;
	for (i=0;i<NUMITEMS(PORT_IN);i++) //Process all ports!
	{
		PORT_IN[i] = NULL; //Reset PORT IN!
		PORT_OUT[i] = NULL; //Reset PORT OUT!
	}
}

void register_PORTOUT(word port, PORTOUT handler)
{
	PORT_OUT[port] = handler; //Link!
}

void register_PORTIN(word port, PORTIN handler)
{
	PORT_IN[port] = handler; //Link!
}

void register_PORTOUT_range(word startport, word endport, PORTOUT handler)
{
	word x;
	for (x=startport;x<=endport;)
	{
		register_PORTOUT(x,handler); //Register the handler!
		++x;
	}
}

void register_PORTIN_range(word startport, word endport, PORTIN handler)
{
	word x;
	x = startport;
	for (x=startport;x<=endport;)
	{
		register_PORTIN(x,handler); //Register the handler!
		++x;
	}
}

//Execution CPU functions!

void EXEC_PORTOUT(word port, byte value)
{
	if (port == 0x61)
	{
		SystemControlPortB = value; //Special case: system control port B!
		return; //Abort!
	}
	if (PORT_OUT[port]) //Exists?
	{
		#ifdef __LOG_PORT
		dolog("emu","PORT OUT: %02X@%04X",value,port);
		#endif
		PORT_OUT[port](port,value); //PORT OUT!
	}
	else
	{
		if (execNMI(0)) //Execute an NMI from Bus!
		{
			dolog("emu", "Warning: Unhandled PORT OUT to port %04X value %02X", port, value); //Report unhandled NMI!
		}
	}
}

byte EXEC_PORTIN(word port)
{
	byte result=0;
	if (port == 0x61)
	{
		return SystemControlPortB; //Special case: system control port B!
	}
	if (PORT_IN[port]) //Exists?
	{
		#ifdef __LOG_PORT
		dolog("emu","PORT IN: %04X",port);
		#endif
		result |= PORT_IN[port](port); //PORT IN!
		#ifdef __LOG_PORT
		dolog("emu","Value read: %02X",result);
		#endif
		return result; //Give the result!
	}
	else
	{
		if (execNMI(0)) //Execute an NMI from Bus!
		{
			dolog("emu", "Warning: Unhandled PORT IN from port %04X", port);
		}
		return PORT_UNDEFINED_RESULT; //Undefined!
	}
}