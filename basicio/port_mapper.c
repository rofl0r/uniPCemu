#include "headers/types.h" //Basic type comp.
#include "headers/hardware/ports.h" //Basic type comp.
#include "headers/support/log.h" //Logging support!
#include "headers/cpu/cpu.h" //NMI support!

/*

We handle mapping input and output to ports!

*/

//Log port input/output?
//#define __LOG_PORT

//Input!
PORTIN PORT_IN[0x10000]; //For reading from ports!
uint_32 PORT_IN_COUNT = 0;
PORTINW PORT_INW[0x10000]; //For reading from ports!
uint_32 PORT_INW_COUNT = 0;
PORTIND PORT_IND[0x10000]; //For reading from ports!
uint_32 PORT_IND_COUNT = 0;

//Output!
PORTOUT PORT_OUT[0x10000]; //For writing to ports!
uint_32 PORT_OUT_COUNT = 0;
PORTOUTW PORT_OUTW[0x10000]; //For writing to ports!
uint_32 PORT_OUTW_COUNT = 0;
PORTOUTD PORT_OUTD[0x10000]; //For writing to ports!
uint_32 PORT_OUTD_COUNT = 0;

//Reset and register!

void reset_ports()
{
	uint_32 i;
	for (i=0;i<NUMITEMS(PORT_IN);i++) //Process all ports!
	{
		PORT_IN[i] = NULL; //Reset PORT IN!
		PORT_INW[i] = NULL; //Reset PORT IN!
		PORT_IND[i] = NULL; //Reset PORT IN!
		PORT_OUT[i] = NULL; //Reset PORT OUT!
		PORT_OUTW[i] = NULL; //Reset PORT OUT!
		PORT_OUTD[i] = NULL; //Reset PORT OUT!
	}
	PORT_IN_COUNT = 0; //Nothing here!
	PORT_INW_COUNT = 0; //Nothing here!
	PORT_IND_COUNT = 0; //Nothing here!
	PORT_OUT_COUNT = 0; //Nothing here!
	PORT_OUTW_COUNT = 0; //Nothing here!
	PORT_OUTD_COUNT = 0; //Nothing here!
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

void register_PORTOUTW(PORTOUTW handler)
{
	if (PORT_OUTW_COUNT < NUMITEMS(PORT_OUTW))
	{
		PORT_OUTW[PORT_OUTW_COUNT++] = handler; //Link!
	}
}

void register_PORTINW(PORTINW handler)
{
	if (PORT_INW_COUNT < NUMITEMS(PORT_INW))
	{
		PORT_INW[PORT_INW_COUNT++] = handler; //Link!
	}
}

void register_PORTOUTD(PORTOUTD handler)
{
	if (PORT_OUTD_COUNT < NUMITEMS(PORT_OUTD))
	{
		PORT_OUTD[PORT_OUTD_COUNT++] = handler; //Link!
	}
}

void register_PORTIND(PORTIND handler)
{
	if (PORT_IND_COUNT < NUMITEMS(PORT_IND))
	{
		PORT_IND[PORT_IND_COUNT++] = handler; //Link!
	}
}


//Execution CPU functions!

byte EXEC_PORTOUT(word port, byte value)
{
	word i;
	byte executed = 0;
	#ifdef __LOG_PORT
	dolog("emu","PORT OUT: %02X@%04X",value,port);
	#endif
	for (i = 0; i < PORT_OUT_COUNT; i++) //Process all ports!
	{
		if (PORT_OUT[i]) //Valid port?
		{
			executed |= PORT_OUT[i](port, value); //PORT OUT on this port!
		}
	}
	return !executed; //Have we failed?
}

byte EXEC_PORTIN(word port, byte *result)
{
	word i;
	byte executed = 0, temp, tempresult=0;
	byte actualresult=0;
#ifdef __LOG_PORT
	dolog("emu","PORT IN: %04X",port);
	#endif
	for (i = 0; i < PORT_IN_COUNT; i++) //Process all ports!
	{
		if (PORT_IN[i]) //Valid port?
		{
			temp = PORT_IN[i](port, &tempresult); //PORT IN on this port!
			executed |= temp; //OR into the result: we're executed?
			if (temp) actualresult |= tempresult; //Add to the result if we're used!
		}
	}
	if (!executed) *result = PORT_UNDEFINED_RESULT; //Not executed gives all bits set!
	else *result = actualresult; //Give the result!
	#ifdef __LOG_PORT
	dolog("emu","Value read: %02X",*result);
	#endif
	return !executed; //Have we failed?
}

byte EXEC_PORTOUTW(word port, word value)
{
	word i;
	byte executed = 0;
#ifdef __LOG_PORT
	dolog("emu", "PORT OUT: %04X@%04X", value, port);
#endif
	for (i = 0; i < PORT_OUTW_COUNT; i++) //Process all ports!
	{
		if (PORT_OUTW[i]) //Valid port?
		{
			executed |= PORT_OUTW[i](port, value); //PORT OUT on this port!
		}
	}
	return !executed; //Have we failed?
}

byte EXEC_PORTINW(word port, word *result)
{
	word i;
	byte executed = 0;
	byte temp;
	word tempresult = 0, actualresult = 0;
#ifdef __LOG_PORT
	dolog("emu", "PORT IN: %04X", port);
#endif
	for (i = 0; i < PORT_INW_COUNT; i++) //Process all ports!
	{
		if (PORT_INW[i]) //Valid port?
		{
			temp = PORT_INW[i](port, &tempresult); //PORT IN on this port!
			executed |= temp; //OR into the result: we're executed?
			if (temp) actualresult |= tempresult; //Add to the result if we're used!
		}
	}
	if (!executed) *result = PORT_UNDEFINED_RESULT; //Not executed gives all bits set!
	else *result = actualresult; //Give the result!
#ifdef __LOG_PORT
	dolog("emu", "Value read: %04X", *result);
#endif
	return !executed; //Have we failed?
}

byte EXEC_PORTOUTD(word port, uint_32 value)
{
	word i;
	byte executed = 0;
#ifdef __LOG_PORT
	dolog("emu", "PORT OUT: %08X@%04X", value, port);
#endif
	for (i = 0; i < PORT_OUTD_COUNT; i++) //Process all ports!
	{
		if (PORT_OUTD[i]) //Valid port?
		{
			executed |= PORT_OUTD[i](port, value); //PORT OUT on this port!
		}
	}
	return !executed; //Have we failed?
}

byte EXEC_PORTIND(word port, uint_32 *result)
{
	word i;
	byte executed = 0;
	byte temp;
	uint_32 tempresult = 0, actualresult = 0;
#ifdef __LOG_PORT
	dolog("emu", "PORT IN: %04X", port);
#endif
	for (i = 0; i < PORT_IND_COUNT; i++) //Process all ports!
	{
		if (PORT_IND[i]) //Valid port?
		{
			temp = PORT_IND[i](port, &tempresult); //PORT IN on this port!
			executed |= temp; //OR into the result: we're executed?
			if (temp) actualresult |= tempresult; //Add to the result if we're used!
		}
	}
	if (!executed) *result = PORT_UNDEFINED_RESULT; //Not executed gives all bits set!
	else *result = actualresult; //Give the result!
#ifdef __LOG_PORT
	dolog("emu", "Value read: %08X", *result);
#endif
	return !executed; //Have we failed?
}
