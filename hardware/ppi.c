#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //I/O support!
#include "headers/mmu/mmu.h" //MMU support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/hardware/vga/vga.h" //VGA/EGA/CGA/MDA support!

byte SystemControlPortB=0x00; //System control port B!
byte SystemControlPortA=0x00; //System control port A!
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
		*result = ((PPI62|4)&~8); //Read the value! 2 Floppy drives forced!
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

void setupPPI()
{
	//PPI63!
	if (((getActiveVGA()->registers->specialCGAflags&0x81)==1)) //Pure CGA mode?
	{
		PPI62 |= 2; //First bit set: 80x25 CGA!
	}
	else if (((getActiveVGA()->registers->specialMDAflags&0x81)==1)) //Pure MDA mode?
	{
		PPI62 |= 3; //Both bits set: 80x25 MDA!
	}
	else //VGA with floppy?
	{
		//Leave PPI62 at zero!
	}
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
			doneCPU();
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
	SystemControlPortB = 0x7F; //Reset system control port B!
	PPI62 = 0x00; //Set the default switches!
	PPI63 = 0x00; //Set the default switches!

	if (EMULATED_CPU<=CPU_80186) //XT machine?
	{
		//Check for reserved hardware settings!
		setupPPI(); //Setup our initial values!
	}

	register_PORTIN(&PPI_readIO);
	register_PORTOUT(&PPI_writeIO);
}
