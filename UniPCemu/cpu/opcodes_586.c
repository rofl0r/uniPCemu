#include "headers/cpu/cpu.h"
#include "headers/cpu/easyregs.h"
#include "headers/cpu/protection.h"

extern MODRM_PARAMS params;    //For getting all params!
extern int_32 modrm_addoffset; //Add this offset to ModR/M reads!

//Modr/m support, used when reg=NULL and custommem==0
extern byte MODRM_src0; //What destination operand in our modr/m? (1/2)
extern byte MODRM_src1; //What source operand in our modr/m? (2/2)

void CPU586_CPUID()
{
	switch (EMULATED_CPU)
	{
	case CPU_PENTIUM: //PENTIUM (586)?
		switch (REG_EAX)
		{
		case 0x00: //Highest function parameter!
			REG_EAX = 1; //Maximum 1 parameter supported!
			//GenuineIntel!
			REG_EBX = 0x756e6547;
			REG_EDX = 0x49656e69;
			REG_ECX = 0x6c65746e;
			break;
		case 0x01: //Processor info and feature bits!
			REG_EAX = 0x00000000; //Defaults!
			REG_EDX = 0x13C; //Just Debugging Extensions, Page Size Extensions, TSC, MSR, CMPXCHG8 have been implemented!
			REG_ECX = 0x00000000; //No features!
			break;
		default: //Unknown parameter?
			break;
		}
		break;
	default:
		break;
	}
}

void CPU586_OP0F30() //WRMSR
{
	//MSR #ECX = EDX::EAX
	if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Invalid privilege?
	{
		THROWDESCGP(0, 0, 0); //#GP(0)!
		return;
	}
	if (1) //Invalid register in ECX?
	{
		THROWDESCGP(0, 0, 0); //#GP(0)!
		return;
	}
	if (1) //Invalid register bits in EDX::EAX?
	{
		THROWDESCGP(0, 0, 0); //#GP(0)!
		return;
	}
}

void CPU586_OP0F31() //RSTDC
{
	if (getCPL() && (CPU[activeCPU].registers->CR4 & 4) && (getcpumode()!=CPU_MODE_REAL)) //Time-stamp disable set and not PL0?
	{
		THROWDESCGP(0, 0, 0); //#GP(0)!
		return;
	}
	REG_EDX = (uint_32)(CPU[activeCPU].TSC>>32); //High dword of the TSC
	REG_EAX = (uint_32)(CPU[activeCPU].TSC & 0xFFFFFFFFULL); //Low dword of the TSC
}

void CPU586_OP0F32() //RDMSR
{
	if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Invalid privilege?
	{
		THROWDESCGP(0,0,0); //#GP(0)!
		return;
	}
	if (1) //Invalid register in ECX?
	{
		THROWDESCGP(0, 0, 0); //#GP(0)!
		return;
	}
	REG_EDX = 0; //High dword of MSR #ECX
	REG_EAX = 0; //Low dword of MSR #ECX
}

void CPU586_OP0FC7() //CMPXCHG8B r/m32
{
	uint_32 templo;
	uint_32 temphi;
	modrm_addoffset = 0; //Low dword
	if (modrm_check32(&params, MODRM_src0, 1 | 0x40)) return;
	modrm_addoffset = 4; //High dword
	if (modrm_check32(&params, MODRM_src0, 1 | 0x40)) return;
	modrm_addoffset = 0; //Low dword
	if (modrm_check32(&params, MODRM_src0, 1 | 0xA0)) return;
	modrm_addoffset = 4; //High dword
	if (modrm_check32(&params, MODRM_src0, 1 | 0xA0)) return;
	modrm_addoffset = 0; //Low dword
	templo = modrm_read32(&params, MODRM_src0);
	modrm_addoffset = 4; //High dword
	temphi = modrm_read32(&params, MODRM_src0);
	if ((REG_EAX == templo) && (REG_EDX==temphi)) //EDX::EAX == r/m?
	{
		modrm_addoffset = 0; //Low dword
		if (modrm_check32(&params, MODRM_src0, 0 | 0x40)) return;
		modrm_addoffset = 4; //High dword
		if (modrm_check32(&params, MODRM_src0, 0 | 0x40)) return;
		modrm_addoffset = 0; //Low dword
		if (modrm_check32(&params, MODRM_src0, 0 | 0xA0)) return;
		modrm_addoffset = 4; //High dword
		if (modrm_check32(&params, MODRM_src0, 0 | 0xA0)) return;
		FLAGW_ZF(1);
		modrm_addoffset = 0; //Low dword
		modrm_write32(&params, MODRM_src0, REG_EBX); /* r/m32=low dword(EBX) */
		modrm_addoffset = 4; //High dword
		modrm_write32(&params, MODRM_src0, REG_ECX); /* r/m32=high dword(ECX) */
	}
	else
	{
		FLAGW_ZF(0);
		REG_EAX = templo; /* EAX=low dword r/m */
		REG_EDX = temphi; /* EDX=high dword r/m */
	}
}
