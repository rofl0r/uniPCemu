#include "headers/types.h" //Basic type support etc.
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/emu/gpu/gpu.h" //Need GPU comp!
#include "headers/cpu/cpu_OP8086.h" //8086 interrupt instruction support!
#include "headers/bios/bios.h" //BIOS Support!
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/cpu/easyregs.h" //Easy register addressing!

#include "headers/emu/gpu/gpu_emu.h" //GPU EMU support!
#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!

#include "headers/cpu/protection.h" //Protection support!
#include "headers/cpu/cpu_OP80286.h" //80286 instruction support!

extern BIOS_Settings_TYPE BIOS_Settings; //BIOS Settings!
extern MODRM_PARAMS params;    //For getting all params!
extern MODRM_PTR info; //For storing ModR/M Info!
extern word oper1, oper2; //Buffers!
extern uint_32 oper1d, oper2d; //Buffers!
extern byte immb;
extern word immw;
extern uint_32 imm32;
extern byte thereg; //For function number!
extern byte modrm_addoffset; //Add this offset to ModR/M reads!

/*

Interrupts:
0x00: //Divide error (0x00)
0x01: //Debug Exceptions (0x01)
0x02: //NMI Interrupt
0x03: //Breakpoint: software only!
0x04: //INTO Detected Overflow
0x05: //BOUND Range Exceeded
0x06: //Invalid Opcode
0x07: //Coprocessor Not Available
0x08: //Double Fault
0x09: //Coprocessor Segment Overrun
0x0A: //Invalid Task State Segment
0x0B: //Segment not present
0x0C: //Stack fault
0x0D: //General Protection Fault
0x0E: //Page Fault
0x0F: //Reserved
//0x10:
0x10: //Coprocessor Error
0x11: //Allignment Check

*/

extern Handler CurrentCPU_opcode0F_jmptbl[512]; //Our standard internal standard opcode jmptbl!

void unkOP_286() //Unknown opcode on 286+?
{
	debugger_setcommand("<80286+ #UD>"); //Command is unknown opcode!
	//dolog("unkop","Unknown opcode on NECV30+: %02X",CPU[activeCPU].lastopcode); //Last read opcode!
	CPU_resetOP(); //Go back to the opcode itself!
	CPU086_int(0x06); //Call interrupt with return addres of the OPcode!
}

void CPU286_OP63() //ARPL r/m16,r16
{
	if (getcpumode() == CPU_MODE_REAL) //Real mode? #UD!
	{
		unkOP_286(); //Execute our unk opcode handler!
		return; //Abort!
	}
	word destRPL, srcRPL;
	destRPL = modrm_read16(&params,0); //Read destination RPL!
	CPUPROT1
	srcRPL = modrm_read16(&params,1); //Read source RPL!
	CPUPROT1
		if (getRPL(destRPL) < getRPL(srcRPL))
		{
			FLAG_ZF = 1; //Set ZF!
			setRPL(destRPL,getRPL(srcRPL)); //Set the new RPL!
			modrm_write16(&params,0,destRPL,0); //Set the result!
		}
		else
		{
			FLAG_ZF = 0; //Clear ZF!
		}
	CPUPROT2
	CPUPROT2
}

void CPU286_OPD6() //286+ SALC
{
	REG_AL = FLAG_CF?0xFF:0x00; //Set AL if Carry flag!
}

//0F opcodes for 286+ processors!

//Based on http://ref.x86asm.net/coder32.html

void CPU286_OP0F00() //Various extended 286+ instructions GRP opcode.
{
	modrm_readparams(&params,1,0); //Read our params!
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params, &info, 1); //Store the address for debugging!
	switch (thereg) //What function?
	{
	case 0: //SLDT
		if (getcpumode() != CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("SLDT %s", info.text);
		modrm_write16(&params,0,CPU->registers->LDTR,0); //Try and write it to the address specified!
		break;
	case 1: //STR
		if (getcpumode() != CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("STR %s", info.text);
		modrm_write16(&params, 0, CPU->registers->TR, 0); //Try and write it to the address specified!
		break;
	case 2: //LLDT
		if (getcpumode() != CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("LLDT %s", info.text);
		if (getCPL()) //Privilege level isn't 0?
		{
			THROWDESCGP(0); //Throw #GP!
			return; //Abort!
		}
		oper1 = modrm_read16(&params,0); //Read the descriptor!
		CPUPROT1
			segmentWritten(CPU_SEGMENT_LDTR,oper1,0); //Write the segment!
		CPUPROT2
		break;
	case 3: //LTR
		if (getcpumode() != CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("LTR %s", info.text);
		if (getCPL()) //Privilege level isn't 0?
		{
			THROWDESCGP(0); //Throw #GP!
			return; //Abort!
		}
		oper1 = modrm_read16(&params, 0); //Read the descriptor!
		CPUPROT1
			segmentWritten(CPU_SEGMENT_TR, oper1, 0); //Write the segment!
		CPUPROT2
		break;
	case 4: //VERR
		debugger_setcommand("VERR %s", info.text);
		oper1 = modrm_read16(&params,0); //Read the descriptor!
		CPUPROT1
			SEGDESCRIPTOR_TYPE verdescriptor;
			if (LOADDESCRIPTOR(-1, oper1, &verdescriptor)) //Load the descriptor!
			{
				if (CPU_MMU_checkrights(-1, oper1, 0, 1, &verdescriptor.desc, 0)==0) //Check without address test!
				{
					FLAG_ZF = 1; //We're valid!
				}
				else
				{
					FLAG_ZF = 0; //We're invalid!
				}
			}
			else
			{
				FLAG_ZF = 0; //We're invalid!
			}
		CPUPROT2
		break;
	case 5: //VERW
		debugger_setcommand("VERW %s", info.text);
		oper1 = modrm_read16(&params, 0); //Read the descriptor!
		CPUPROT1
			SEGDESCRIPTOR_TYPE verdescriptor;
			if (LOADDESCRIPTOR(-1, oper1, &verdescriptor)) //Load the descriptor!
			{
				if (CPU_MMU_checkrights(-1, oper1, 0, 0, &verdescriptor.desc, 0)==0) //Check without address test!
				{
					FLAG_ZF = 1; //We're valid!
				}
				else
				{
					FLAG_ZF = 0; //We're invalid!
				}
			}
			else
			{
				FLAG_ZF = 0; //We're invalid!
			}
		CPUPROT2
		break;
	case 6: //--- Unknown Opcode! ---
	case 7: //--- Unknown Opcode! ---
		unkOP0F_286(); //Unknown opcode!
		break;
	default:
		break;
	}
}

void CPU286_OP0F01() //Various extended 286+ instruction GRP opcode.
{
	modrm_readparams(&params, 1, 0); //Read our params!
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params, &info, 1); //Store the address for debugging!
	switch (thereg) //What function?
	{
	case 0: //SGDT
		debugger_setcommand("SGDT %s", info.text);
		if (params.info[0].isreg) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		modrm_write16(&params,0,CPU[activeCPU].registers->GDTR.limit,0); //Store the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			modrm_write32(&params,0,(CPU[activeCPU].registers->GDTR.base&0xFFFFFF)|((EMULATED_CPU>=CPU_80386)?0xFF000000:0x00000000)); //Only 24-bits of limit, high byte is cleared with 386+, set with 286!
		CPUPROT2
		break;
	case 1: //SIDT
		debugger_setcommand("SIDT %s", info.text);
		if (params.info[0].isreg) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		modrm_write16(&params, 0, CPU[activeCPU].registers->IDTR.limit, 0); //Store the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			modrm_write32(&params, 0, (CPU[activeCPU].registers->IDTR.base & 0xFFFFFF) | ((EMULATED_CPU >= CPU_80386) ? 0xFF000000 : 0x00000000)); //Only 24-bits of limit, high byte is cleared with 386+, set with 286!
		CPUPROT2
		break;
	case 2: //LGDT
		debugger_setcommand("LGDT %s", info.text);
		if (params.info[0].isreg) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
		{
			THROWDESCGP(0); //Throw #GP!
			return; //Abort!
		}
		oper1 = modrm_read16(&params, 0); //Read the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			oper1d = ((uint_32)modrm_read16(&params, 0)); //Lower part of the limit!
			CPUPROT1
				modrm_addoffset = 4; //Last byte!
				oper1d |= (((uint_32)modrm_read8(&params,0))<<16); //Higher part of the limit!
				CPUPROT1
					CPU[activeCPU].registers->GDTR.base = oper1d; //Load the base!
					CPU[activeCPU].registers->GDTR.limit = oper1; //Load the limit!
				CPUPROT2
			CPUPROT2
		CPUPROT2
		break;
	case 3: //LIDT
		debugger_setcommand("LIDT %s", info.text);
		if (params.info[0].isreg) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
		{
			THROWDESCGP(0); //Throw #GP!
			return; //Abort!
		}
		oper1 = modrm_read16(&params, 0); //Read the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			oper1d = ((uint_32)modrm_read16(&params, 0)); //Lower part of the limit!
			CPUPROT1
				modrm_addoffset = 4; //Last byte!
				oper1d |= (((uint_32)modrm_read8(&params, 0)) << 16); //Higher part of the limit!
				CPUPROT1
					CPU[activeCPU].registers->IDTR.base = oper1d; //Load the base!
					CPU[activeCPU].registers->IDTR.limit = oper1; //Load the limit!
				CPUPROT2
			CPUPROT2
		CPUPROT2
		break;
	case 4: //SMSW
		debugger_setcommand("SMSW %s", info.text);
		modrm_write16(&params,0,(word)(CPU[activeCPU].registers->CR0_full&0xFFFF),0); //Store the MSW into the specified location!
		break;
	case 6: //LMSW
		debugger_setcommand("LMSW %s", info.text);
		if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
		{
			THROWDESCGP(0); //Throw #GP!
			return; //Abort!
		}
		oper1 = modrm_read16(&params,0); //Read the new register!
		CPUPROT1
		oper1 |= CPU[activeCPU].registers->CR0.PE; //Keep the protected mode bit on, this isn't toggable anymore once set!
		CPU[activeCPU].registers->CR0_full = (CPU[activeCPU].registers->CR0_full&(~0xFFFF))|oper1; //Set the MSW!
		CPUPROT2
		break;
	case 5: //--- Unknown Opcode!
	case 7: //--- Unknown Opcode!
		unkOP0F_286(); //Unknown opcode!
		break;
	default:
		break;
	}
}

void CPU286_OP0F02() //LAR /r
{
	unkOP0F_286(); //TODO!
}

void CPU286_OP0F03() //LSL /r
{
	unkOP0F_286(); //TODO!
}

void CPU286_OP0F06() //CLTS
{
	if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
	{
		THROWDESCGP(0); //Throw #GP!
		return; //Abort!
	}
	CPU[activeCPU].registers->CR0.TS = 0; //Clear the Task Switched flag!
}

void CPU286_OP0F0B() //#UD instruction
{
	unkOP0F_286(); //Delibarately #UD!
}

void CPU286_OP0FB9() //#UD instruction
{
	unkOP0F_286(); //Delibarately #UD!
}