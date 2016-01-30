#include "headers/types.h" //Basic types!
#include "headers/hardware/parallel.h" //Our own interface!
#include "headers/hardware/ports.h" //I/O support!
#include "headers/hardware/pic.h" //Interrupt support!

struct
{
ParallelOutputHandler outputhandler;
ParallelControlOUTHandler controlouthandler;
ParallelControlINHandler controlinhandler;
ParallelStatusHandler statushandler;
byte outputdata; //Mirror of last written data!
byte controldata; //Mirror of last written control!
byte IRQEnabled; //IRQs enabled?
byte IRQraised; //IRQ raised?
} PARALLELPORT[4]; //All parallel ports!

void registerParallel(byte port, ParallelOutputHandler outputhandler, ParallelControlOUTHandler controlouthandler, ParallelControlINHandler controlinhandler, ParallelStatusHandler statushandler)
{
	PARALLELPORT[port].outputhandler = outputhandler;
	PARALLELPORT[port].controlouthandler = controlouthandler;
	PARALLELPORT[port].controlinhandler = controlinhandler;
	PARALLELPORT[port].statushandler = statushandler;
}

void setParallelIRQ(byte port, byte raised)
{
	PARALLELPORT[port].IRQraised = raised; //Raise the IRQ from the hardware if set, lower when cleared!
}

void tickParallel(double timepassed)
{
	byte port;
	for (port=0;port<4;port++)
	{
		if (PARALLELPORT[port].IRQraised && PARALLELPORT[port].IRQEnabled) //Enabled and raised?
		{
			switch (port)
			{
				case 0: //IRQ 7!
					doirq(7);
					break;
				case 1: //IRQ 6!
					doirq(6);
					break;
				case 2: //IRQ 5!
					doirq(5);
				default: //unknown IRQ?
					break;
			}
		}
		else
		{
			switch (port)
			{
				case 0: //IRQ 7!
					removeirq(7);
					break;
				case 1: //IRQ 6!
					removeirq(6);
					break;
				case 2: //IRQ 5!
					removeirq(5);
				default: //unknown IRQ?
					break;
			}
		}
	}
}

byte getParallelport(word port) //What COM port?
{
	byte highnibble = (port>>8); //3 or 2
	byte lownibble = ((port>>2)&0x3F); //2F=0, 1E=1/2
	
	switch (lownibble)
	{
	case 0x2F: //Might be port 0?
		return (highnibble==3)?2:4; //LPT3 or invalid!
	case 0x1E: //Might be port 1/2?
		switch (highnibble)
		{
			case 3: return 0; //LPT1!
			case 2: return 1; //LPT2!
			default: return 4; //Invalid!
		}
		break;
	default:
		break;
	}
	return 4; //Invalid by default!
}

//Offset calculator!
#define ParallelPORT_offset(port) (port&0x3)

byte outparallel(word port, byte value)
{
	byte Parallelport;
	if ((Parallelport = getParallelport(port))==4) //Unknown?
	{
		return 0; //Error: not our port!
	}
	switch (ParallelPORT_offset(port))
	{
	case 0: //Data output?
		if (PARALLELPORT[Parallelport].outputhandler) //Valid?
		{
			PARALLELPORT[Parallelport].outputhandler(value); //Output the new data
		}
		return 1; //We're handled!
		break;
	case 2: //Control register?
		if (PARALLELPORT[Parallelport].controlouthandler) //Valid?
		{
			PARALLELPORT[Parallelport].controlouthandler(value&0xF); //Output the new control
		}
		PARALLELPORT[Parallelport].controldata = (value&0x30); //The new control data last written, only the Bi-Directional pins and IRQ pins!
		PARALLELPORT[Parallelport].IRQEnabled = (value&0x10)?1:0; //Is the IRQ enabled?
		return 1; //We're handled!
		break;
	default: //Unknown port?
		break;
	}
	return 0; //We're not handled!
}

byte inparallel(word port, byte *result)
{
	byte Parallelport;
	if ((Parallelport = getParallelport(port))==4) //Unknown?
	{
		return 0; //Error: not our port!
	}
	switch (ParallelPORT_offset(port))
	{
	case 0: //Data?
		*result = PARALLELPORT[Parallelport].outputdata; //The last written data!
		return 1; //We're handled!
		break;
	case 1: //Status?
		*result = 0x00; //Default: clear input!
		if (PARALLELPORT[Parallelport].statushandler) //Valid?
		{
			*result = PARALLELPORT[Parallelport].statushandler(); //Output the data?
		}
		return 1; //We're handled!
		break;
	case 2: //Control register?
		*result = PARALLELPORT[Parallelport].controlinhandler()|PARALLELPORT[Parallelport].controldata;
		return 1; //We're handled!
		break;
	default: //Unknown port?
		break;
	}
	return 0; //Not supported port!
}

void initParallelPorts()
{
	memset(&PARALLELPORT,0,sizeof(PARALLELPORT)); //Initialise our ports!
	register_PORTIN(&inparallel); //Register the read handler!
	register_PORTOUT(&outparallel); //Register the write handler!	
}