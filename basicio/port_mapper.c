#include "headers/types.h" //Basic type comp.
#include "headers/hardware/ports.h" //Basic type comp.
#include "headers/support/log.h" //Logging support!

/*

We handle mapping input and output to ports!

*/

//Log port input/output?
//#define __LOG_PORT

PORTIN PORT_IN[0x10000]; //For reading from ports!
PORTOUT PORT_OUT[0x10000]; //For writing to ports!

//Reset and register!

void reset_ports()
{
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
	if (PORT_OUT[port]) //Exists?
	{
		#ifdef __LOG_PORT
		dolog("emu","PORT OUT: %02X@%04X",value,port);
		#endif
		PORT_OUT[port](port,value); //PORT OUT!
	}
	else
	{
		dolog("emu","Warning: undefined PORT OUT to port %04X value %02X",port,value);
	}
}

byte EXEC_PORTIN(word port)
{
	byte result;
	if (PORT_IN[port]) //Exists?
	{
		#ifdef __LOG_PORT
		dolog("emu","PORT IN: %04X",port);
		#endif
		result = PORT_IN[port](port); //PORT IN!
		#ifdef __LOG_PORT
		dolog("emu","Value read: %02X",result);
		#endif
		return result;
	}
	else
	{
		dolog("emu","Warning: Undefined PORT IN from port %04X",port);
		return PORT_UNDEFINED_RESULT; //Undefined!
	}
}