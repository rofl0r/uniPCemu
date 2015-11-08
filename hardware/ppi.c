#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //I/O support!

byte SystemControlPortB; //System control port B!
byte SystemControlPortA; //System control port A!
byte PPI62, PPI63; //Default PPI switches!

byte PPI_readIO(word port, byte *result)
{
	switch (port) //Special register: System control port B!
	{
	case 0x61: //System control port B?
		*result = SystemControlPortB; //Read the value!
		return 1;
		break;
	case 0x62: //PPI62?
		*result = ((PPI62|4)&~8); //Read the value, only 2 floppy drives (bit 3=0)!
		return 1;
		break;
	case 0x63: //PPI63?
		*result = PPI63; //Read the value!
		return 1;
		break;
	case 0x92: //System control port A?
		*result = SystemControlPortA; //Read the value!
		return 1;
		break;
	default: //unknown port?
		break;
	}
	return 0; //No PPI!
}

byte PPI_writeIO(word port, byte value)
{
	switch (port)
	{
	case 0x61: //System control port B?
		SystemControlPortB = (value&0x7F); //Set the port, highest bit isn't ours!
		return 1;
		break;
	case 0x62: //PPI62?
		PPI62 = value; //Set the value!
		return 1;
		break;
	case 0x63: //PPI63?
		PPI63 = value; //Set the value!
		return 1;
		break;
	case 0x92: //System control port A?
		MMU_setA20(1,value&2); //Fast A20!
		if (value&1) //Fast reset?
		{
			resetCPU(); //Reset the CPU!
		}
		SystemControlPortA = (value&(~1)); //Set the port!
		return 1;
		break;
	default: //unknown port?
		break;
	}
	return 0; //No PPI!
}

void initPPI()
{
	SystemControlPortB = 0xFF; //Reset system control port B!
	PPI62 = 0xFF; //Set the default switches!
	PPI63 = 0xFF; //Set the default switches!

	register_PORTIN(&PPI_readIO);
	register_PORTOUT(&PPI_writeIO);
}
