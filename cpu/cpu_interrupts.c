#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //CPU!
#include "headers/bios/bios.h" //BIOS!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/cpu/callback.h" //Callback support!
#include "headers/cpu/80286/protection.h" //Protection support!

void CPU_setint(byte intnr, word segment, word offset) //Set real mode IVT entry!
{
	MMU_ww(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0x0000,((intnr<<2)|2),segment); //Copy segment!
	MMU_ww(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0x0000,(intnr<<2),offset); //Copy offset!
}

void CPU_getint(byte intnr, word *segment, word *offset) //Set real mode IVT entry!
{
	*segment = MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0x0000,((intnr<<2)|2),0); //Copy segment!
	*offset = MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0x0000,(intnr<<2),0); //Copy offset!
}

extern uint_32 destEIP;

void CPU_customint(byte intnr, word retsegment, uint_32 retoffset) //Used by soft (below) and exceptions/hardware!
{
	if (getcpumode()==CPU_MODE_REAL) //Use IVT?
	{
		CPU_PUSH16(&REG_FLAGS); //Push flags!
		CPU_PUSH16(&retsegment); //Push segment!
		word retoffset16 = (retoffset&0xFFFF);
		CPU_PUSH16(&retoffset16);
		FLAG_IF = 0; //We're calling the interrupt!
		FLAG_TF = 0; //We're calling an interrupt, resetting debuggers!
//Now, jump to it!
		destEIP = MMU_rw(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, 0x0000, (intnr << 2), 0); //JUMP to position CS:EIP/CS:IP in table.
		segmentWritten(CPU_SEGMENT_CS,MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0x0000,(intnr<<2)|2,0),0); //Interrupt to position CS:EIP/CS:IP in table.
	}
	else //Use Protected mode IVT?
	{
		//TODO
	}
}


void CPU_INT(byte intnr) //Call an software interrupt; WARNING: DON'T HANDLE ANYTHING BUT THE REGISTERS ITSELF!
{
	if (getcpumode()==CPU_MODE_REAL) //Use IVT?
	{
		//Now, jump to it!
		CPU_customint(intnr,REG_CS,REG_IP); //Execute real interrupt, returning to current address!
	}
	else //Use Protected mode IVT?
	{
		//TODO
	}
}

void CPU_IRET()
{
	if (getcpumode()==CPU_MODE_REAL) //Use IVT?
	{
		REG_IP = CPU_POP16();
		REG_CS = CPU_POP16();
		REG_FLAGS = CPU_POP16();
	}
	else //Use protected mode IRET?
	{
		//TODO
	}
}