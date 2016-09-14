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
byte numparallelports = 0; //How many ports?

void registerParallel(byte port, ParallelOutputHandler outputhandler, ParallelControlOUTHandler controlouthandler, ParallelControlINHandler controlinhandler, ParallelStatusHandler statushandler)
{
	PARALLELPORT[port].outputhandler = outputhandler;
	PARALLELPORT[port].controlouthandler = controlouthandler;
	PARALLELPORT[port].controlinhandler = controlinhandler;
	PARALLELPORT[port].statushandler = statushandler;
}

void setParallelIRQ(byte port, byte raised)
{
	PARALLELPORT[port].IRQraised = (PARALLELPORT[port].IRQraised&~1)|(raised&1); //Raise the IRQ from the hardware if set, lower when cleared!
}

void tickParallel(double timepassed)
{
	INLINEREGISTER byte port=0;
	if (numparallelports) //Something to do?
	{
		do //Only process the ports we have!
		{
			if (PARALLELPORT[port].IRQEnabled) //Enabled IRQ?
			{
				if ((PARALLELPORT[port].IRQraised & 3) == 1) //Are we raised high?
				{
					switch (port)
					{
						case 0: //IRQ 7!
							raiseirq(7); //Throw the IRQ!
							break;
						case 1: //IRQ 6!
							raiseirq(6); //Throw the IRQ!
							break;
						case 2: //IRQ 5!
							raiseirq(5); //Throw the IRQ!
						default: //unknown IRQ?
							//Don't handle: we're an unknown IRQ!
							break;
					}
					PARALLELPORT[port].IRQraised |= 2; //Not raised anymore! Set to a special bit value to detect by software!
				}
			}
		} while (++port<numparallelports); //Loop while not done!
	}
}

byte getParallelport(word port) //What COM port?
{
	byte result=4;
	byte highnibble = (port>>8); //3 or 2
	byte lownibble = ((port>>2)&0x3F); //2F=0, 1E=1/2
	
	switch (lownibble)
	{
	case 0x2F: //Might be port 0?
		result = (highnibble==3)?2:4; break; //LPT3 or invalid!
	case 0x1E: //Might be port 1/2?
		switch (highnibble)
		{
			case 3: result = 0; break; //LPT1!
			case 2: result = 1; break; //LPT2!
			default: result = 4; //Invalid!
		}
		break;
	default: result = 4; break;
	}
	return ((result<numparallelports) && (result<4))?result:4; //Invalid by default!
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

		if (PARALLELPORT[Parallelport].IRQraised & 2) //Are we raised? Lower it(Writing here acnowledges the interrupt)!
		{
			switch (Parallelport) //What port are we?
			{
			case 0: //IRQ 7!
				lowerirq(7); //Throw the IRQ!
				break;
			case 1: //IRQ 6!
				lowerirq(6); //Throw the IRQ!
				break;
			case 2: //IRQ 5!
				lowerirq(5); //Throw the IRQ!
			default: //unknown IRQ?
				//Don't handle: we're an unknown IRQ!
				break;
			}
		}

		PARALLELPORT[Parallelport].outputdata = value; //We've written data on this port!
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
		*result &= ~4; //Clear IRQ status bit by default(IRQ occurred)!
		*result |= ((~PARALLELPORT[Parallelport].IRQraised)&1)<<2; //Set the nIRQ bit if an interrupt didn't occurred!
		PARALLELPORT[Parallelport].IRQraised &= ~2; //Clear the interrupt raised flag! We've been acnowledged if existant!
		return 1; //We're handled!
		break;
	case 2: //Control register?
		if (PARALLELPORT[Parallelport].controlinhandler)
		{
			*result = (PARALLELPORT[Parallelport].controlinhandler()&0xF);
		}
		*result |= PARALLELPORT[Parallelport].controldata; //Our own control data!
		return 1; //We're handled!
		break;
	default: //Unknown port?
		break;
	}
	return 0; //Not supported port!
}

void initParallelPorts(byte numports)
{
	memset(&PARALLELPORT,0,sizeof(PARALLELPORT)); //Initialise our ports!
	numparallelports = MIN(numports, NUMITEMS(PARALLELPORT)); //Set with safeguard!
	register_PORTIN(&inparallel); //Register the read handler!
	register_PORTOUT(&outparallel); //Register the write handler!	
}
