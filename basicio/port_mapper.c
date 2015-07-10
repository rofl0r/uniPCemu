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
word PORT_IN_COUNT = 0;
PORTOUT PORT_OUT[0x10000]; //For writing to ports!
word PORT_OUT_COUNT = 0;

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
	PORT_IN_COUNT = 0; //Nothing here!
	PORT_OUT_COUNT = 0; //Nothing here!
}

void register_PORTOUT(PORTOUT handler)
{
	if (PORT_OUT_COUNT < NUMITEMS(PORT_OUT))
	{
		PORT_OUT[PORT_OUT_COUNT++] = handler; //Link!
	}
}

void register_PORTIN(PORTIN handler)
{
	if (PORT_IN_COUNT < NUMITEMS(PORT_IN))
	{
		PORT_IN[PORT_IN_COUNT++] = handler; //Link!
	}
}

//Execution CPU functions!

byte EXEC_PORTOUT(word port, byte value)
{
	word i;
	byte executed = 0;
	if (port == 0x61) //Special register: System control port B!
	{
		executed = 1; //Always executed!
		SystemControlPortB = value; //Special case: system control port B!
	}
	#ifdef __LOG_PORT
	dolog("emu","PORT OUT: %02X@%04X",value,port);
	#endif
	for (i = 0; i < PORT_OUT_COUNT; i++) //Process all ports!
	{
		executed |= PORT_OUT[i](port, value); //PORT OUT on this port!
	}
	return !executed; //Have we failed?
}

byte EXEC_PORTIN(word port, byte *result)
{
	word i;
	byte executed = 0, temp, tempresult;
	byte actualresult=0;
	if (port == 0x61) //Special register: System control port B!
	{
		executed = 1; //Always executed!
		actualresult = SystemControlPortB; //Special case: system control port B!
	}
	#ifdef __LOG_PORT
	dolog("emu","PORT IN: %04X",port);
	#endif
	for (i = 0; i < PORT_OUT_COUNT; i++) //Process all ports!
	{
		temp = PORT_IN[i](port, &tempresult); //PORT IN on this port!
		executed |= temp; //OR into the result: we're executed?
		if (temp) actualresult |= tempresult; //Add to the result if we're used!
	}
	if (!executed) *result = PORT_UNDEFINED_RESULT; //Not executed gives all bits set!
	else *result = actualresult; //Give the result!
	#ifdef __LOG_PORT
	dolog("emu","Value read: %02X",*result);
	#endif
	return !executed; //Have we failed?
}