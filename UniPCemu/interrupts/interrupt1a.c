#include "headers/types.h"
#include "headers/cpu/cpu.h" //CPU module!
#include "headers/cpu/easyregs.h" //Easy register access!
#include "headers/hardware/ports.h" //CMOS support!
#include "headers/cpu/cb_manager.h" //Callback support!

/*
OPTINLINE byte CMOS_readAutoBCD(byte number)
{
	PORT_OUT_B(0x70, 0xB); //Status register B!
	byte SREGB = PORT_IN_B(0x71); //Read Status register B!
	PORT_OUT_B(0x70, number); //Select our register!
	byte data;
	data = PORT_IN_B(0x71); //Read the original value!
	byte PM = 0;
	if ((SREGB & 1) && (number == 4)) //12 hour format and getting hours?
	{
		PM = data; //Load the original data!
		PM &= 0x80; //Are we PM?
		data &= 0x7F; //Mask the PM bit off to get 12 hour clock correctly!
	}
	if (!(SREGB & 2)) //BCD mode?
	{
		data = ((data & 0xF0) >> 1) + ((data & 0xF0) >> 3) + (data & 0xf); //Convert to binary!
	}
	if ((SREGB & 1) && (number == 4)) //12 hour format and getting hours?
	{
		if (PM) //Are we PM?
		{
			data += 12; //Add 12 hours for PM!
			data %= 24; //Modulo 24 hours!
		}
	}
	return data; //Give the correct data!
}
*/

//Our IRQ0 handler!
void BIOS_IRQ0()
{
	uint_32 result;
	result = MMU_rdw(CPU_SEGMENT_DS, 0x0040, 0x6C, 0,1); //Read the result!
	++result; //Increase the number!
	if (result == 0x1800B0) //Midnight count reached?
	{
		MMU_wb(CPU_SEGMENT_DS, 0x0040, 0x0070, 0x01,1); //Set Midnight flag!
		result = 0; //Clear counter!
	}
	MMU_wdw(CPU_SEGMENT_DS, 0x0040, 0x6c, result,1); //Write data!
}

void BIOS_int1A() //Handler!
{
	uint_32 result;
	switch (REG_AH) //What function
	{
	case 0x00: //Get system clock?
		CALLBACK_SCF(0); //Clear carry flag to indicate no error!
		result = MMU_rdw(CPU_SEGMENT_DS, 0x0040, 0x6C, 0,1); //Read the result!
		REG_DX = (result >> 16); //High value!
		REG_CX = (result & 0xFFFF); //Low value!
		REG_AL = MMU_rb(CPU_SEGMENT_DS, 0x0040, 0x0070,0,1); //Midnight flag!
		MMU_wb(CPU_SEGMENT_DS, 0x0040, 0x0070, 0x00,1); //Clear Midnight flag!
		break;
	case 0x001: //Set system clock!
		CALLBACK_SCF(0); //Clear carry flag to indicate no error!; / Clear error flag!
		result = ((REG_DX << 16) | REG_CX); //Calculate result!
		MMU_wdw(CPU_SEGMENT_DS, 0x0040, 0x6c, result,1); //Write data!
		MMU_wb(CPU_SEGMENT_DS, 0x0040, 0x0070, 0x00,1); //Clear Midnight flag!
		break;
	default: //Unknown function?
		CALLBACK_SCF(1); //Set carry flag to indicate an error!
		break;
	}
}