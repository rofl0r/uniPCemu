#include "headers/types.h"

//x86 doesn't have this?

/*int_32 get_IOdevice(uint_32 offset)
{
	if (EMULATED_CPU==CPU_8086) //8086?
	{
		if ((offset>=0xE0000) && (offset<0xE1000)) //I/O adress space?
		{
			return offset-0xE0000; //Memory adress I/O!
		}
	}
	return -1; //No i/o device!
}*/

/*byte memIO_rb(uint_32 baseoffset, uint_32 reloffset, byte *value)
{
	int_32 portio; //For memory mapped port I/O!
	if ((portio=get_IOdevice(offset))!=-1) //Memory mapped PI/O?
	{
		*value = PORT_IN_B((word)(reloffset&0xFFFF)); //Port In!
		return 1; //Recognised!
	}
	return 0; //NOt recognised!
}

byte memIO_wb(uint_32 baseoffset, uint_32 reloffset, byte value)
{
	int_32 portio; //For memory mapped port I/O!
	if ((portio=get_IOdevice(offset))!=-1) //Memory mapped PI/O?
	{
		PORT_OUT_B((word)(reloffset&0xFFFF),value); //Port In!
		return 1; //Recognised!
	}
	return 0; //NOt recognised!
}*/

void initMemIO()
{
	return; //There's no MMI/O?
	//Reset&register handlers!
	MMU_resetHandlers("MMI/O");
	MMU_registerWriteHandler(0xE0000,0xEFFFF,&memIO_wb,"MMI/O");
	MMU_registerReadHandler(0xE0000,0xEFFFF,&memIO_rb,"MMI/O");
}