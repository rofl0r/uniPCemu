#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //I/O support!
#include "headers/mmu/mmu.h" //MMU support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/hardware/vga/vga.h" //VGA/EGA/CGA/MDA support!

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
		*result = PPI62; //Read the value!
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

void setPPI62()
{
	PPI62 &= 0xF0; //Clear our mode bits: We're filling in now!
	if (((getActiveVGA()->registers->specialMDAflags&0x81)==1)) //Pure MDA mode?
	{
		PPI62 |= 0x3; //MDA 80x25!
	}
	//80x25 color=00, so already set!
	PPI62 |= (1<<2); //Read the value, only 2 floppy drives(this number is the amount of floppy drives minus one)!
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
	SystemControlPortB = 0xFF; //Reset system control port B!
	PPI62 = 0xFF; //Set the default switches!
	PPI63 = 0xFF; //Set the default switches!

	if (EMULATED_CPU<=CPU_80186) //XT machine?
	{
		//Check for reserved hardware settings!
		setPPI62(); //Set PPI62!
	}

	register_PORTIN(&PPI_readIO);
	register_PORTOUT(&PPI_writeIO);
}
