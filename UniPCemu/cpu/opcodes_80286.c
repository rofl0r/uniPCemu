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

extern char modrm_param1[256]; //Contains param/reg1
extern char modrm_param2[256]; //Contains param/reg2
extern byte cpudebugger; //CPU debugger active?
extern byte custommem; //Custom memory address?

OPTINLINE void modrm286_generateInstructionTEXT(char *instruction, byte debuggersize, uint_32 paramdata, byte type) //Copy of 8086 version!
{
	if (cpudebugger) //Gotten no debugger to process?
	{
		//Process debugger!
		char result[256];
		bzero(result, sizeof(result));
		strcpy(result, instruction); //Set the instruction!
		switch (type)
		{
		case PARAM_MODRM1: //Param1 only?
		case PARAM_MODRM2: //Param2 only?
		case PARAM_MODRM12: //param1,param2
		case PARAM_MODRM21: //param2,param1
							//We use modr/m decoding!
			switch (debuggersize)
			{
			case 8:
				modrm_debugger8(&params, 0, 1);
				break;
			case 16:
				modrm_debugger16(&params, 0, 1);
				break;
			default: //None?
					 //Don't use modr/m!
				break;
			}
			break;
		}
		switch (type)
		{
		case PARAM_NONE: //No params?
			debugger_setcommand(result); //Nothing!
			break;
		case PARAM_MODRM1: //Param1 only?
			strcat(result, " %s"); //1 param!
			debugger_setcommand(result, modrm_param1);
			break;
		case PARAM_MODRM2: //Param2 only?
			strcat(result, " %s"); //1 param!
			debugger_setcommand(result, modrm_param2);
			break;
		case PARAM_MODRM12: //param1,param2
			strcat(result, " %s,%s"); //2 params!
			debugger_setcommand(result, modrm_param1, modrm_param2);
			break;
		case PARAM_MODRM21: //param2,param1
			strcat(result, " %s,%s"); //2 params!
			debugger_setcommand(result, modrm_param2, modrm_param1);
			break;
		case PARAM_IMM8: //imm8
			strcat(result, " %02X"); //1 param!
			debugger_setcommand(result, paramdata);
			break;
		case PARAM_IMM16: //imm16
			strcat(result, " %04X"); //1 param!
			debugger_setcommand(result, paramdata);
			break;
		case PARAM_IMM32: //imm32
			strcat(result, " %08X"); //1 param!
			debugger_setcommand(result, paramdata);
		default: //Unknown?
			break;
		}
	}
}

void unkOP_286() //Unknown opcode on 286+?
{
	debugger_setcommand("<80286+ #UD>"); //Command is unknown opcode!
	//dolog("unkop","Unknown opcode on NECV30+: %02X",CPU[activeCPU].lastopcode); //Last read opcode!
	CPU_resetOP(); //Go back to the opcode itself!
	CPU086_int(0x06); //Call interrupt with return addres of the OPcode!
}

void CPU286_OP63() //ARPL r/m16,r16
{
	modrm286_generateInstructionTEXT("ARPL",16,0,PARAM_MODRM21); //Our instruction text!
	if (getcpumode() == CPU_MODE_REAL) //Real mode? #UD!
	{
		unkOP_286(); //Execute our unk opcode handler!
		return; //Abort!
	}
	word destRPL, srcRPL;
	destRPL = modrm_read16(&params,1); //Read destination RPL!
	CPUPROT1
	srcRPL = modrm_read16(&params,0); //Read source RPL!
	CPUPROT1
		if (getRPL(destRPL) < getRPL(srcRPL))
		{
			FLAG_ZF = 1; //Set ZF!
			setRPL(destRPL,getRPL(srcRPL)); //Set the new RPL!
			modrm_write16(&params,1,destRPL,0); //Set the result!
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
	debugger_setcommand("SALC");
	REG_AL = FLAG_CF?0xFF:0x00; //Set AL if Carry flag!
}

//0F opcodes for 286+ processors!

//Based on http://ref.x86asm.net/coder32.html

void CPU286_OP0F00() //Various extended 286+ instructions GRP opcode.
{
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params, &info, 1); //Store the address for debugging!
	switch (thereg) //What function?
	{
	case 0: //SLDT
		if (getcpumode() == CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("SLDT %s", info.text);
		modrm_write16(&params,1,CPU[activeCPU].registers->LDTR,0); //Try and write it to the address specified!
		break;
	case 1: //STR
		if (getcpumode() == CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("STR %s", info.text);
		modrm_write16(&params,1, CPU[activeCPU].registers->TR, 0); //Try and write it to the address specified!
		break;
	case 2: //LLDT
		if (getcpumode() == CPU_MODE_REAL)
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
		oper1 = modrm_read16(&params,1); //Read the descriptor!
		CPUPROT1
			segmentWritten(CPU_SEGMENT_LDTR,oper1,0); //Write the segment!
		CPUPROT2
		break;
	case 3: //LTR
		if (getcpumode() == CPU_MODE_REAL)
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
		oper1 = modrm_read16(&params, 1); //Read the descriptor!
		CPUPROT1
			segmentWritten(CPU_SEGMENT_TR, oper1, 0); //Write the segment!
		CPUPROT2
		break;
	case 4: //VERR
		if (getcpumode() == CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("VERR %s", info.text);
		oper1 = modrm_read16(&params,1); //Read the descriptor!
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
		if (getcpumode() == CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("VERW %s", info.text);
		oper1 = modrm_read16(&params, 1); //Read the descriptor!
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
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params, &info, 1); //Store the address for debugging!
	switch (thereg) //What function?
	{
	case 0: //SGDT
		debugger_setcommand("SGDT %s", info.text);
		if (params.info[1].isreg==1) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		modrm_addoffset = 0; //Add no bytes to the offset!
		modrm_write16(&params, 1, CPU[activeCPU].registers->GDTR.limit, 0); //Store the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			modrm_write16(&params, 1, (CPU[activeCPU].registers->GDTR.base & 0xFFFF),0); //Only 24-bits of limit, high byte is cleared with 386+, set with 286!
			CPUPROT1
				modrm_addoffset = 4; //Add 4 bytes to the offset!
				modrm_write8(&params, 1, ((CPU[activeCPU].registers->GDTR.base >> 16) & 0xFF)); //Write rest value!
				CPUPROT1
					//Just store the high byte too, no matter what the CPU!
					modrm_addoffset = 5; //Add 5 bytes to the offset!
					modrm_write8(&params, 1, ((CPU[activeCPU].registers->GDTR.base >> 24) & 0xFF)); //Write rest value!
				CPUPROT2
			CPUPROT2
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 1: //SIDT
		debugger_setcommand("SIDT %s", info.text);
		if (params.info[1].isreg==1) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		modrm_addoffset = 0; //Add no bytes to the offset!
		modrm_write16(&params, 1, CPU[activeCPU].registers->IDTR.limit, 0); //Store the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			modrm_write16(&params, 1, (CPU[activeCPU].registers->IDTR.base & 0xFFFF),0); //Only 24-bits of limit, high byte is cleared with 386+, set with 286!
			CPUPROT1
				modrm_addoffset = 4; //Add 4 bytes to the offset!
				modrm_write8(&params, 1, ((CPU[activeCPU].registers->IDTR.base>>16) & 0xFF)); //Write rest value!
				CPUPROT1
					//Just store the high byte too, no matter what the CPU!
					modrm_addoffset = 5; //Add 5 bytes to the offset!
					modrm_write8(&params, 1, ((CPU[activeCPU].registers->IDTR.base >> 24) & 0xFF)); //Write rest value!
				CPUPROT2
			CPUPROT2
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 2: //LGDT
		debugger_setcommand("LGDT %s", info.text);
		if (params.info[1].isreg==1) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
		{
			THROWDESCGP(0); //Throw #GP!
			return; //Abort!
		}
		modrm_addoffset = 0; //Add no bytes to the offset!
		oper1 = modrm_read16(&params, 1); //Read the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			oper1d = ((uint_32)modrm_read16(&params, 1)); //Lower part of the limit!
			CPUPROT1
				modrm_addoffset = 4; //Last byte!
				oper1d |= (((uint_32)modrm_read8(&params,1))<<16); //Higher part of the limit!
				CPUPROT1
					CPU[activeCPU].registers->GDTR.base = oper1d; //Load the base!
					CPU[activeCPU].registers->GDTR.limit = oper1; //Load the limit!
				CPUPROT2
			CPUPROT2
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 3: //LIDT
		debugger_setcommand("LIDT %s", info.text);
		if (params.info[1].isreg==1) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
		{
			THROWDESCGP(0); //Throw #GP!
			return; //Abort!
		}
		modrm_addoffset = 0; //Add no bytes to the offset!
		oper1 = modrm_read16(&params, 1); //Read the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			oper1d = ((uint_32)modrm_read16(&params, 1)); //Lower part of the limit!
			CPUPROT1
				modrm_addoffset = 4; //Last byte!
				oper1d |= (((uint_32)modrm_read8(&params, 1)) << 16); //Higher part of the limit!
				CPUPROT1
					CPU[activeCPU].registers->IDTR.base = oper1d; //Load the base!
					CPU[activeCPU].registers->IDTR.limit = oper1; //Load the limit!
				CPUPROT2
			CPUPROT2
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 4: //SMSW
		debugger_setcommand("SMSW %s", info.text);
		modrm_write16(&params,1,(word)(CPU[activeCPU].registers->CR0_full&0xFFFF),0); //Store the MSW into the specified location!
		break;
	case 6: //LMSW
		debugger_setcommand("LMSW %s", info.text);
		if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
		{
			THROWDESCGP(0); //Throw #GP!
			return; //Abort!
		}
		CPU[activeCPU].cycles_OP = 4*16; //Make sure we last long enough for the required JMP to be fully buffered!
		oper1 = modrm_read16(&params,1); //Read the new register!
		CPUPROT1
		oper1 |= CPU[activeCPU].registers->CR0.PE; //Keep the protected mode bit on, this isn't toggable anymore once set!
		CPU[activeCPU].registers->CR0_full = (CPU[activeCPU].registers->CR0_full&(~0xFFFF))|oper1; //Set the MSW!
		updateCPUmode(); //Update the CPU mode to reflect the new mode set, if required!
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
	byte isconforming = 1;
	SEGDESCRIPTOR_TYPE verdescriptor;
	if (getcpumode() == CPU_MODE_REAL)
	{
		unkOP0F_286(); //We're not recognized in real mode!
		return;
	}
	modrm286_generateInstructionTEXT("LAR", 16, 0, PARAM_MODRM12); //Our instruction text!
	oper1 = modrm_read16(&params,1); //Read the segment to check!
	CPUPROT1
		if (LOADDESCRIPTOR(-1, oper1, &verdescriptor)) //Load the descriptor!
		{
			switch (verdescriptor.desc.Type)
			{
			case AVL_SYSTEM_RESERVED_0: //Invalid type?
			case AVL_SYSTEM_INTERRUPTGATE16BIT:
			case AVL_SYSTEM_TRAPGATE16BIT:
			case AVL_SYSTEM_RESERVED_1:
			case AVL_SYSTEM_RESERVED_2:
			case AVL_SYSTEM_RESERVED_3:
			case AVL_SYSTEM_INTERRUPTGATE32BIT:
			case AVL_SYSTEM_TRAPGATE32BIT:
				FLAG_ZF = 0; //Invalid descriptor type!
				break;
			default: //Valid type?
				switch (verdescriptor.desc.AccessRights) //What type?
				{
				case AVL_CODE_EXECUTEONLY_CONFORMING:
				case AVL_CODE_EXECUTEONLY_CONFORMING_ACCESSED:
				case AVL_CODE_EXECUTE_READONLY_CONFORMING:
				case AVL_CODE_EXECUTE_READONLY_CONFORMING_ACCESSED: //Conforming?
					isconforming = 1;
					break;
				default: //Not conforming?
					isconforming = 0;
					break;
				}
				if ((MAX(getCPL(), getRPL(oper1)) <= verdescriptor.desc.DPL) || isconforming) //Valid privilege?
				{
					modrm_write16(&params,0,(word)(verdescriptor.desc.AccessRights<<8),0); //Write our result!
					CPUPROT1
						FLAG_ZF = 1; //We're valid!
					CPUPROT2
				}
				else
				{
					FLAG_ZF = 0; //Not valid!
				}
				break;
			}
		}
		else //Couldn't be loaded?
		{
			FLAG_ZF = 0; //Default: not loaded!
		}
	CPUPROT2
}

void CPU286_OP0F03() //LSL /r
{
	uint_32 limit;
	byte isconforming = 1;
	SEGDESCRIPTOR_TYPE verdescriptor;
	if (getcpumode() == CPU_MODE_REAL)
	{
		unkOP0F_286(); //We're not recognized in real mode!
		return;
	}
	modrm286_generateInstructionTEXT("LSL", 16, 0, PARAM_MODRM12); //Our instruction text!
	oper1 = modrm_read16(&params, 1); //Read the segment to check!
	CPUPROT1
		if (LOADDESCRIPTOR(-1, oper1, &verdescriptor)) //Load the descriptor!
		{
			switch (verdescriptor.desc.Type)
			{
			case AVL_SYSTEM_RESERVED_0: //Invalid type?
			case AVL_SYSTEM_INTERRUPTGATE16BIT:
			case AVL_SYSTEM_TRAPGATE16BIT:
			case AVL_SYSTEM_RESERVED_1:
			case AVL_SYSTEM_RESERVED_2:
			case AVL_SYSTEM_RESERVED_3:
			case AVL_SYSTEM_INTERRUPTGATE32BIT:
			case AVL_SYSTEM_TRAPGATE32BIT:
				FLAG_ZF = 0; //Invalid descriptor type!
				break;
			default: //Valid type?
				switch (verdescriptor.desc.AccessRights) //What type?
				{
				case AVL_CODE_EXECUTEONLY_CONFORMING:
				case AVL_CODE_EXECUTEONLY_CONFORMING_ACCESSED:
				case AVL_CODE_EXECUTE_READONLY_CONFORMING:
				case AVL_CODE_EXECUTE_READONLY_CONFORMING_ACCESSED: //Conforming?
					isconforming = 1;
					break;
				default: //Not conforming?
					isconforming = 0;
					break;
				}

				limit = verdescriptor.desc.limit_low|(verdescriptor.desc.limit_high<<16); //Limit!
				if ((verdescriptor.desc.G&CPU[activeCPU].G_Mask) && (EMULATED_CPU >= CPU_80386)) //Granularity?
				{
					limit = ((limit << 12) | 0xFFF); //4KB for a limit of 4GB, fill lower 12 bits with 1!
				}

				if ((MAX(getCPL(), getRPL(oper1)) <= verdescriptor.desc.DPL) || isconforming) //Valid privilege?
				{
					modrm_write16(&params, 0, (word)(limit&0xFFFF), 0); //Write our result!
					CPUPROT1
						FLAG_ZF = 1; //We're valid!
					CPUPROT2
				}
				else
				{
					FLAG_ZF = 0; //Not valid!
				}
				break;
			}
		}
		else //Couldn't be loaded?
		{
			FLAG_ZF = 0; //Default: not loaded!
		}
	CPUPROT2
}

void CPU286_OP0F06() //CLTS
{
	debugger_setcommand("CLTS"); //Our instruction text!
	if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
	{
		THROWDESCGP(0); //Throw #GP!
		return; //Abort!
	}
	CPU[activeCPU].registers->CR0.TS = 0; //Clear the Task Switched flag!
}

void CPU286_OP0F0B() //#UD instruction
{
	unkOP0F_286(); //Deliberately #UD!
}

void CPU286_OP0FB9() //#UD instruction
{
	unkOP0F_286(); //Deliberately #UD!
}

void CPU286_OPF1() //Undefined opcode, Don't throw any exception!
{
	//Ignore this opcode!
}

//FPU non-existant Coprocessor support!

void FPU80287_OPDBE3(){debugger_setcommand("<UNKOP8087: FNINIT>");}
void FPU80287_OPDFE0() { debugger_setcommand("<UNKOP8087: FNINIT>"); }
void FPU80287_OPDDslash7() { debugger_setcommand("<UNKOP8087: FNSTSW>"); }
void FPU80287_OPD9slash7() { debugger_setcommand("<UNKOP8087: FNSTCW>"); }


void FPU80287_OPDB(){if (CPU[activeCPU].registers->CR0.EM) { FPU80287_noCOOP(); return; /* Emulate! */ } if (CPU[activeCPU].registers->CR0.MP && CPU[activeCPU].registers->CR0.TS) { FPU80287_noCOOP(); return; } byte subOP = immb; CPUPROT1 word oldCS = REG_CS; word oldIP = REG_IP; if (subOP==0xE3){FPU80287_OPDBE3();} else{REG_CS = oldCS; REG_IP = oldIP; FPU80287_noCOOP();} CPUPROT2 }
void FPU80287_OPDF(){if (CPU[activeCPU].registers->CR0.EM) { FPU80287_noCOOP(); return; /* Emulate! */ } if (CPU[activeCPU].registers->CR0.MP && CPU[activeCPU].registers->CR0.TS) { FPU80287_noCOOP(); return; } CPUPROT1 byte subOP = immb; CPUPROT1 word oldCS = REG_CS; word oldIP = REG_IP; if (subOP==0xE0){FPU80287_OPDFE0();} else {REG_CS = oldCS; REG_IP = oldIP; FPU80287_noCOOP();} CPUPROT2 CPUPROT2 }
void FPU80287_OPDD(){word oldCS; word oldIP; oldCS = REG_CS; oldIP = REG_IP; if (CPU[activeCPU].registers->CR0.EM) { FPU80287_noCOOP(); return; /* Emulate! */ } if (CPU[activeCPU].registers->CR0.MP && CPU[activeCPU].registers->CR0.TS) { FPU80287_noCOOP(); return; } CPUPROT1 if (MODRM_REG(params.modrm)==7){FPU80287_OPDDslash7();}else {REG_CS = oldCS; REG_IP = oldIP; FPU80287_noCOOP();} CPUPROT2}
void FPU80287_OPD9(){word oldCS; word oldIP; oldCS = REG_CS; oldIP = REG_IP; if (CPU[activeCPU].registers->CR0.EM) { FPU80287_noCOOP(); return; /* Emulate! */ } if (CPU[activeCPU].registers->CR0.MP && CPU[activeCPU].registers->CR0.TS) { FPU80287_noCOOP(); return; } CPUPROT1 if (MODRM_REG(params.modrm)==7){FPU80287_OPD9slash7();} else {REG_CS = oldCS; REG_IP = oldIP; FPU80287_noCOOP();} CPUPROT2}

void FPU80287_noCOOP() {
	debugger_setcommand("<No COprocessor OPcodes implemented!>");
	if ((CPU[activeCPU].registers->CR0.EM) || (CPU[activeCPU].registers->CR0.MP && CPU[activeCPU].registers->CR0.TS)) //To be emulated or task switched?
	{
		CPU_resetOP();
		CPU_COOP_notavailable(); //Only on 286+!
	}
	CPU[activeCPU].cycles_OP = MODRM_EA(params) ? 8 + MODRM_EA(params) : 2; //No hardware interrupt to use anymore!
}