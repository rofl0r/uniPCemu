/*

Copyright (C) 2019 - 2020  Superfury

This file is part of UniPCemu.

UniPCemu is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

UniPCemu is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UniPCemu.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "headers/cpu/cpu.h"
#include "headers/cpu/easyregs.h"
#include "headers/cpu/protection.h"
#include "headers/cpu/cpu_OP8086.h" //8086 memory access support!
#include "headers/cpu/cpu_OP80386.h" //80386 memory access support!
#include "headers/cpu/cpu_execution.h" //Execution phase support!
#include "headers/cpu/cpu_pmtimings.h" //Timing support!
#include "headers/hardware/pic.h" //MSR 1Bh support!

//How many cycles to substract from the documented instruction timings for the raw EU cycles for each BIU access?
#define EU_CYCLES_SUBSTRACT_ACCESSREAD 4
#define EU_CYCLES_SUBSTRACT_ACCESSWRITE 4
#define EU_CYCLES_SUBSTRACT_ACCESSRW 8


//Modr/m support, used when reg=NULL and custommem==0

OPTINLINE byte CPU80586_instructionstepPOPtimeout(word base)
{
	return CPU8086_instructionstepdelayBIU(base, 2);//Delay 2 cycles for POPs to start!
}

void CPU586_CPUID()
{
	CPU_CPUID(); //Common CPUID instruction!
	if (CPU_apply286cycles() == 0) //No 80286+ cycles instead?
	{
		CPU[activeCPU].cycles_OP += 1; //Single cycle!
	}
}

extern uint_32 MSRnumbers[MAPPEDMSRS*2]; //All possible MSR numbers!
extern uint_32 MSRmasklow[MAPPEDMSRS*2]; //Low mask!
extern uint_32 MSRmaskhigh[MAPPEDMSRS*2]; //High mask!
extern uint_32 MSRmaskwritelow_readonly[MAPPEDMSRS*2]; //Low mask for writes changing data erroring out!
extern uint_32 MSRmaskwritehigh_readonly[MAPPEDMSRS*2]; //High mask for writes changing data erroring out!
extern uint_32 MSRmaskreadlow_writeonly[MAPPEDMSRS*2]; //Low mask for reads changing data!
extern uint_32 MSRmaskreadhigh_writeonly[MAPPEDMSRS*2]; //High mask for reads changing data!

//Handle all MSRs as our generic preregistered MSRs!
void CPU586_OP0F30() //WRMSR
{
	CPUMSR* MSR;
	uint_32 validbitslo;
	uint_32 validbitshi;
	uint_32 ROMbitslo;
	uint_32 ROMbitshi;
	uint_32 storagenr;
	uint_32 mapbase;
	uint_32 ECXoffset;
	if (unlikely(cpudebugger)) //Debugger on?
	{
		modrm_generateInstructionTEXT("WRMSR", 0, 0, PARAM_NONE);
	}
	//MSR #ECX = EDX::EAX
	if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Invalid privilege?
	{
		THROWDESCGP(0, 0, 0); //#GP(0)!
		return;
	}

	//Decode the offset!
	ECXoffset = REG_ECX;
	mapbase = 0;
	if (ECXoffset&0x80000000) //High?
	{
		mapbase = MAPPEDMSRS; //Base high!
		ECXoffset &= ~0x80000000; //High offset!
	}
	if (ECXoffset >= MAPPEDMSRS) //Invalid register in ECX?
	{
		handleinvalidregisterWRMSR:
		THROWDESCGP(0, 0, 0); //#GP(0)!
		return;
	}
	if (!MSRnumbers[ECXoffset+mapbase]) //Unmapped?
	{
		goto handleinvalidregisterWRMSR; //Handle it!
	}
	storagenr = MSRnumbers[ECXoffset+mapbase]-1; //Where are we stored?

	validbitshi = MSRmaskhigh[storagenr]; //Valid bits to access!
	validbitslo = MSRmasklow[storagenr]; //Valid bits to access!

	//Inverse the valid bits to get a mask of invalid bits!
	validbitslo = ~validbitslo; //invalid bits calculation!
	validbitshi = ~validbitshi; //invalid bits calculation!
	if ((REG_EDX & validbitshi) || (REG_EAX & validbitslo)) //Invalid register bits in EDX::EAX?
	{
		goto handleinvalidregisterWRMSR; //Handle it!
	}

	MSR = &CPU[activeCPU].registers->genericMSR[storagenr]; //Actual MSR to use!

	ROMbitshi = MSRmaskwritehigh_readonly[storagenr]; //High ROM bits!
	ROMbitslo = MSRmaskwritelow_readonly[storagenr]; //Low ROM bits!
	if (unlikely((REG_ECX == 0x1B) && (EMULATED_CPU==CPU_PENTIUM))) //Sticky bits on this processor?
	{
		ROMbitslo |= ((~MSR->lo) & (1 << 11)); //Bit 11 (APIC global enable) is sticky when disabled!
	}
	MSR->hi = (MSR->hi&MSRmaskwritehigh_readonly[storagenr])|(REG_EDX&~ROMbitshi); //Set high!
	MSR->lo = (MSR->lo&MSRmaskwritelow_readonly[storagenr])|(REG_EAX&~ROMbitslo); //Set low!

	if (unlikely(REG_ECX == 0x1B)) //APIC MSR needs external hardware handling as well?
	{
		APIC_updateWindowMSR(CPU[activeCPU].registers->genericMSR[MSRnumbers[0x1B] - 1].lo, CPU[activeCPU].registers->genericMSR[MSRnumbers[0x1B] - 1].hi); //Update the MSR for the hardware!
	}
}

void CPU586_OP0F31() //RDTSC
{
	if (unlikely(cpudebugger)) //Debugger on?
	{
		modrm_generateInstructionTEXT("RDTSC", 0, 0, PARAM_NONE);
	}
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
	uint_32 storagenr;
	uint_32 mapbase;
	uint_32 ECXoffset;
	CPUMSR* MSR;
	if (unlikely(cpudebugger)) //Debugger on?
	{
		modrm_generateInstructionTEXT("RDMSR", 0, 0, PARAM_NONE);
	}

	if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Invalid privilege?
	{
		THROWDESCGP(0, 0, 0); //#GP(0)!
		return;
	}

	//Decode the offset!
	ECXoffset = REG_ECX;
	mapbase = 0;
	if (ECXoffset&0x80000000) //High?
	{
		mapbase = MAPPEDMSRS; //Base high!
		ECXoffset &= ~0x80000000; //High offset!
	}
	if (ECXoffset >= MAPPEDMSRS) //Invalid register in ECX?
	{
		handleinvalidregisterRDMSR:
		THROWDESCGP(0, 0, 0); //#GP(0)!
		return;
	}
	if (!MSRnumbers[ECXoffset+mapbase]) //Unmapped?
	{
		goto handleinvalidregisterRDMSR; //Handle it!
	}
	storagenr = MSRnumbers[ECXoffset+mapbase]-1; //Where are we stored?

	MSR = &CPU[activeCPU].registers->genericMSR[storagenr]; //Actual MSR to use!

	REG_EDX = (MSR->hi&(~MSRmaskreadhigh_writeonly[storagenr])); //High dword of MSR #ECX
	REG_EAX = (MSR->lo&(~MSRmaskreadhigh_writeonly[storagenr])); //Low dword of MSR #ECX
}

void CPU586_OP0FC7() //CMPXCHG8B r/m32
{
	uint_32 templo;
	uint_32 temphi;
	if (MODRM_REG(params.modrm)!=1)
	{
		CPU_unkOP(); //#UD for reg not being 1!
		return;
	}

	if (unlikely(cpudebugger)) //Debugger on?
	{
		modrm_generateInstructionTEXT("CMPXCHG8B", 32, 0, PARAM_MODRM_0);
	}

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

void CPU80586_OPCD()
{
	byte VMElookup;
	INLINEREGISTER byte theimm = immb;
	INTdebugger80386();
	modrm_generateInstructionTEXT("INT", 0, theimm, PARAM_IMM8);/*INT imm8*/

	//Special handling for the V86 case!
	if (isV86() && (CPU[activeCPU].registers->CR4 & 1)) //V86 mode that's using VME?
	{
		VMElookup = getTSSIRmap(theimm); //Get the IR map bit!
		switch (VMElookup) //What kind of result?
		{
		case 0: //Real mode style interrupt?
			CPU_executionphase_startinterrupt(theimm, 0, -4);/*INT imm8*/
			return; //Abort!
		case 2: //Page fault?
			return; //Abort!
			break;
		case 1: //Legacy when set?
			break; //Just continue using the 80386 method!
		}
	}

	if (isV86() && (FLAG_PL != 3))
	{
		THROWDESCGP(0, 0, 0);
		return;
	}
	CPU_executionphase_startinterrupt(theimm, 0, -2);/*INT imm8*/
}

void CPU80586_OPFA()
{
	modrm_generateInstructionTEXT("CLI", 0, 0, PARAM_NONE);
	if ((FLAG_PL != 3) && (CPU[activeCPU].registers->CR4&1) && (getcpumode()==CPU_MODE_8086)) //Virtual 8086 mode in VME?
	{
		FLAGW_VIF(0); //Clear the virtual interrupt flag instead!
	}
	else //Normal operation!
	{
		if (
			likely(
				(getcpumode() != CPU_MODE_PROTECTED) //Not protected mode has normal behaviour as well
				|| (((getcpumode() == CPU_MODE_PROTECTED) && ((CPU[activeCPU].registers->CR4 & 2)))==0) //PVI==0
				|| ((getcpumode() == CPU_MODE_PROTECTED) && //PVI possible?
						(
							(CPU[activeCPU].registers->CR4 & 2) && //Enabled?
								(
								(getCPL() < 3) //Normal behaviour when PVI 1, CPL < 3
								|| ((getCPL() == 3) && (FLAG_PL == 3)) //Normal behaviour when PVI 1, CPL == 3, IOPL == 3
								)
						)
					)
				)
			)
		{
			if (checkSTICLI())
			{
				FLAGW_IF(0);
			}
		}
		else //PVI=1, CPL=3 and IOPL<3 in protected mode?
		{
			FLAGW_VIF(0); //Clear the Virtual Interrupt Flag!
		}
	}
	if (CPU_apply286cycles() == 0) /* No 80286+ cycles instead? */
	{
		CPU[activeCPU].cycles_OP += 1;
	} /*Special timing!*/
}
void CPU80586_OPFB()
{
	modrm_generateInstructionTEXT("STI", 0, 0, PARAM_NONE);
	if ((FLAG_PL != 3) && (CPU[activeCPU].registers->CR4 & 1) && (getcpumode() == CPU_MODE_8086)) //Virtual 8086 mode in VME?
	{
		if (FLAG_VIP) //VIP already set? Fault!
		{
			THROWDESCGP(0, 0, 0); //#GP(0)!
			return; //Abort!
		}
		FLAGW_VIF(1); //Set the virtual interrupt flag instead!
	}
	else //Normal operation!
	{
		if (
			likely(
				(getcpumode() != CPU_MODE_PROTECTED) //Not protected mode has normal behaviour as well
				|| (((getcpumode() == CPU_MODE_PROTECTED) && ((CPU[activeCPU].registers->CR4 & 2)))==0) //PVI==0
				|| ((getcpumode() == CPU_MODE_PROTECTED) && //PVI possible?
						(
							(CPU[activeCPU].registers->CR4 & 2) && //Enabled?
								(
								(getCPL() < 3) //Normal behaviour when PVI 1, CPL < 3
								|| ((getCPL() == 3) && (FLAG_PL == 3)) //Normal behaviour when PVI 1, CPL == 3, IOPL == 3
								)
						)
					)
				)
			)
		{
			if (checkSTICLI())
			{
				FLAGW_IF(1);
				CPU[activeCPU].allowInterrupts = 0; /* Inhabit all interrupts up to the next instruction */
			}
		}
		else //PVI=1, CPL=3 and IOPL<3 in protected mode?
		{
			if (FLAG_VIP == 0) //No pending interrupts present?
			{	
				FLAGW_VIF(1); //Set the Virtual Interrupt Flag!
			}
			else //Pending interrupt must be handled!
			{
				THROWDESCGP(0, 0, 0); //#GP(0)!
				return; //Abort!
			}
		}
	}
	if (CPU_apply286cycles() == 0) /* No 80286+ cycles instead? */
	{
		CPU[activeCPU].cycles_OP += 1;
	} /*Special timing!*/
}

void CPU80586_OP9C_16()
{
	word theflags;
	theflags = REG_FLAGS; //Default flags that we push!
	modrm_generateInstructionTEXT("PUSHF", 0, 0, PARAM_NONE);/*PUSHF*/
	if (unlikely((getcpumode() == CPU_MODE_8086) && (FLAG_PL != 3)))
	{
		if (CPU[activeCPU].registers->CR4 & 1) //Virtual 8086 mode in VME?
		{
			theflags |= 0x3000; //Set the pushed flags IOPL to 3!
			theflags = (theflags&~F_IF) | (FLAG_VIF ? F_IF : 0); //Replace the pushed interrupt flag with the Virtual Interrupt Flag.
		}
		else //Normal handling!
		{
			THROWDESCGP(0, 0, 0); return; /*#GP fault!*/
		}
	}
	if (unlikely(CPU[activeCPU].stackchecked == 0))
	{
		if (checkStackAccess(1, 1, 0)) return;
		++CPU[activeCPU].stackchecked;
	}
	if (CPU8086_PUSHw(0, &theflags, 0)) return;
	if (CPU_apply286cycles() == 0) /* No 80286+ cycles instead? */
	{
		CPU[activeCPU].cycles_OP += 10 - EU_CYCLES_SUBSTRACT_ACCESSWRITE; /*PUSHF timing!*/
	}
}
void CPU80586_OP9D_16()
{
	modrm_generateInstructionTEXT("POPF", 0, 0, PARAM_NONE);/*POPF*/
	if (unlikely((getcpumode() == CPU_MODE_8086) && (FLAG_PL != 3)))
	{
		if (!(CPU[activeCPU].registers->CR4 & 1)) //Not Virtual 8086 mode in VME?
		{
			THROWDESCGP(0, 0, 0); return; //#GP fault!
		}
	}
	static word tempflags;
	if (unlikely(CPU[activeCPU].stackchecked == 0))
	{
		if (checkStackAccess(1, 0, 0)) return;
		++CPU[activeCPU].stackchecked;
	}
	if (CPU80586_instructionstepPOPtimeout(0)) return; /*POP timeout*/
	if (CPU8086_POPw(2, &tempflags, 0)) return;
	if ((getcpumode()==CPU_MODE_8086) && (CPU[activeCPU].registers->CR4 & 1) && (FLAG_PL!=3)) //VME?
	{
		if (tempflags&F_TF) //If stack image TF=1, Then #GP(0)!
		{
			THROWDESCGP(0, 0, 0); //#GP fault!
			return;
		}
		if (FLAG_VIP && (tempflags&F_IF)) //Virtual interrupt flag set during POPF?
		{
			THROWDESCGP(0, 0, 0); //#GP fault!
			return;
		}
		else //POP Interrupt flag to VIF!
		{
			FLAGW_VIF((tempflags&F_IF)?1:0); //VIF from stack IF!
		}
	}
	if (disallowPOPFI())
	{
		tempflags &= ~0x200;
		tempflags |= REG_FLAGS & 0x200; /* Ignore any changes to the Interrupt flag! */
	}
	if (getCPL())
	{
		tempflags &= ~0x3000;
		tempflags |= REG_FLAGS & 0x3000; /* Ignore any changes to the IOPL when not at CPL 0! */
	}
	REG_FLAGS = tempflags;
	updateCPUmode(); /*POPF*/
	if (CPU_apply286cycles() == 0) /* No 80286+ cycles instead? */
	{
		CPU[activeCPU].cycles_OP += 8 - EU_CYCLES_SUBSTRACT_ACCESSREAD; /*POPF timing!*/
	}
	CPU[activeCPU].allowTF = 0; /*Disallow TF to be triggered after the instruction!*/
	/*CPU[activeCPU].unaffectedRF = 1;*/ //Default: affected!
}

void CPU80586_OP9D_32()
{
	modrm_generateInstructionTEXT("POPFD", 0, 0, PARAM_NONE);/*POPF*/
	if (unlikely((getcpumode() == CPU_MODE_8086) && (FLAG_PL != 3)))
	{
		THROWDESCGP(0, 0, 0);
		return;
	}//#GP fault!
	static uint_32 tempflags;
	if (unlikely(CPU[activeCPU].stackchecked == 0))
	{
		if (checkStackAccess(1, 0, 1)) return;
		++CPU[activeCPU].stackchecked;
	}
	if (CPU80586_instructionstepPOPtimeout(0)) return; /*POP timeout*/
	if (CPU80386_POPdw(2, &tempflags)) return;
	if (disallowPOPFI())
	{
		tempflags &= ~0x200;
		tempflags |= REG_FLAGS & 0x200; /* Ignore any changes to the Interrupt flag! */
	}
	if (getCPL())
	{
		tempflags &= ~0x3000;
		tempflags |= REG_FLAGS & 0x3000; /* Ignore any changes to the IOPL when not at CPL 0! */
	}
	if (getcpumode() == CPU_MODE_8086) //Virtual 8086 mode?
	{
		if (FLAG_PL == 3) //IOPL 3?
		{
			tempflags = ((tempflags&~(0x1B0000 | F_VIP | F_VIF)) | (REG_EFLAGS&(0x1B0000 | F_VIP | F_VIF))); /* Ignore any changes to the VM, RF, IOPL, VIP and VIF ! */
		} //Otherwise, fault is raised!
	}
	else //Protected/real mode?
	{
		if (getCPL())
		{
			tempflags = ((tempflags&~(0x1A0000 | F_VIP | F_VIF)) | (REG_EFLAGS&(0x20000 | F_VIP | F_VIF))); /* Ignore any changes to the IOPL, VM ! VIP/VIF are cleared. */
		}
		else
		{
			tempflags = ((tempflags&~0x1A0000) | (REG_EFLAGS & 0x20000)); /* VIP/VIF are cleared. Ignore any changes to VM! */
		}
	}
	REG_EFLAGS = tempflags;
	updateCPUmode(); /*POPF*/
	if (CPU_apply286cycles() == 0) /* No 80286+ cycles instead? */
	{
		CPU[activeCPU].cycles_OP += 8 - EU_CYCLES_SUBSTRACT_ACCESSREAD; /*POPF timing!*/
	}
	CPU[activeCPU].allowTF = 0; /*Disallow TF to be triggered after the instruction!*/
	/*CPU[activeCPU].unaffectedRF = 1;*/ //Default: affected!
}
