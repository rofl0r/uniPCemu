#include "headers/types.h" //Basic types
#include "headers/cpu/cpu.h" //CPU needed!
#include "headers/cpu/mmu.h" //MMU needed!
#include "headers/cpu/easyregs.h" //Easy register compatibility!
#include "headers/cpu/modrm.h" //MODR/M compatibility!
#include "headers/support/signedness.h" //CPU support functions!
#include "headers/hardware/ports.h" //Ports compatibility!
#include "headers/cpu/cpu_OP8086.h" //Our own opcode presets!
#include "headers/cpu/fpu_OP8087.h" //Our own opcode presets!
#include "headers/cpu/flags.h" //Flag support!
#include "headers/cpu/8086_grpOPs.h" //GRP Opcode extensions!
#include "headers/cpu/interrupts.h" //Basic interrupt support!
#include "headers/emu/debugger/debugger.h" //CPU debugger support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/cpu/protection.h"
#include "headers/mmu/mmuhandler.h" //MMU_invaddr support!

MODRM_PARAMS params; //For getting all params for the CPU!
extern byte cpudebugger; //The debugging is on?
extern byte blockREP; //Block the instruction from executing (REP with (E)CX=0

//When using http://www.mlsite.net/8086/: G=Modr/m mod&r/m adress, E=Reg field in modr/m

//INFO: http://www.mlsite.net/8086/
//Extra info about above: Extension opcodes (GRP1 etc) are contained in the modr/m
//Ammount of instructions in the completed core: 123

//Aftercount: 60-6F,C0-C1, C8-C9, D6, D8-DF, F1, 0F(has been implemented anyways)
//Total count: 30 opcodes undefined.

//Info: Ap = 32-bit segment:offset pointer (data: param 1:word segment, param 2:word offset)

//Simplifier!

extern uint_32 destEIP; //Destination address for CS JMP instruction!

extern byte immb; //For CPU_readOP result!
extern word immw; //For CPU_readOPw result!
extern uint_32 imm32; //For CPU_readOPdw result!
extern uint_64 imm64; //For CPU_readOPdw result!
extern byte oper1b, oper2b; //Byte variants!
extern word oper1, oper2; //Word variants!
uint_32 oper1d, oper2d; //DWord variants!
extern byte res8; //Result 8-bit!
extern word res16; //Result 16-bit!
uint_32 res32; //Result 32-bit!
extern byte thereg; //For function number!
extern uint_32 ea; //From RM OFfset (GRP5 Opcodes only!)
extern byte tempCF2;

VAL64Splitter temp1, temp2, temp3, temp4, temp5; //All temporary values!
extern uint_32 temp32, tempaddr32; //Defined in opcodes_8086.c

extern byte debuggerINT; //Interrupt special trigger?

//Prototypes for GRP code extensions!
void op386_grp3_32(); //Prototype!
uint_32 op386_grp2_32(byte cnt, byte varshift); //Prototype!
void op386_grp5_32(); //Prototype

OPTINLINE void INTdebugger80386() //Special INTerrupt debugger!
{
	if (DEBUGGER_LOG==DEBUGGERLOG_INT) //Interrupts only?
	{
		debuggerINT = 1; //Debug this instruction always!
	}
}

/*

Start of help for debugging

*/

extern char modrm_param1[256]; //Contains param/reg1
extern char modrm_param2[256]; //Contains param/reg2

char LEAtext[256];
OPTINLINE char *getLEAtext32(MODRM_PARAMS *theparams)
{
	modrm_lea32_text(theparams,1,&LEAtext[0]);    //Help function for LEA instruction!
	return &LEAtext[0];
}

/*

Start of help for opcode processing

*/

extern byte CPU_databussize; //0=16/32-bit bus! 1=8-bit bus when possible (8088/80188)!
extern uint_32 wordaddress; //Word address used during memory access!

OPTINLINE void CPU80386_software_int(byte interrupt, int_64 errorcode) //See int, but for hardware interrupts (IRQs)!
{
	call_soft_inthandler(interrupt,errorcode); //Save adress to stack (We're going soft int!)!
}

OPTINLINE void CPU80386_INTERNAL_int(byte interrupt, byte type3) //Software interrupt from us(internal call)!
{
	CPUPROT1
		CPU80386_software_int(interrupt, -1);
	CPUPROT2
	if (type3) //Type-3 interrupt?
		CPU[activeCPU].cycles_OP = 52; /* Type-3 interrupt */
	else //Normal interrupt?
		CPU[activeCPU].cycles_OP = 51; /* Normal interrupt */
}

void CPU80386_int(byte interrupt) //Software interrupt (external call)!
{
	CPU80386_INTERNAL_int(interrupt,0); //Direct call!
}

OPTINLINE void CPU80386_IRET()
{
	CPUPROT1
	CPU_IRET(); //IRET!
	CPUPROT2
	CPU[activeCPU].cycles_OP = 24; /*Timings!*/
}

/*

List of hardware interrupts:
0: Division by 0: Attempting to execute DIV/IDIV with divisor==0: IMPLEMENTED
1: Debug/Single step: Breakpoint hit, also after instruction when TRAP flag is set.
3: Breakpoint: INT 3 call: IMPLEMENTED
4: Overflow: When performing arithmetic instructions with signed operands. Called with INTO.
5: Bounds Check: BOUND instruction exceeds limit.
6: Invalid OPCode: Invalid LOCK prefix or invalid OPCode: IMPLEMENTED
7: Device not available: Attempt to use floating point instruction (8087) with no COProcessor.
8: Double fault: Interrupt occurs with no entry in IVT or exception within exception handler.
12: Stack exception: Stack operation exceeds offset FFFFh or a selector pointing to a non-present segment is loaded into SS.
13: CS,DS,ES,FS,GS Segment Overrun: Word memory access at offset FFFFh or an attempt to execute past the end of the code segment.
16: Floating point error: An error with the numeric coprocessor (Divide-by-Zero, Underflow, Overflow...)

*/


//5 Override prefixes! (LOCK, CS, SS, DS, ES)

//Prefix opcodes:
/*
void CPU80386_OPF0() {} //LOCK
void CPU80386_OP2E() {} //CS:
void CPU80386_OP36() {} //SS:
void CPU80386_OP3E() {} //DS:
void CPU80386_OP26() {} //ES:
void CPU80386_OPF2() {} //REPNZ
void CPU80386_OPF3() {} //REPZ
*/

/*

WE START WITH ALL HELP FUNCTIONS

*/

//First CMP instruction (for debugging) and directly related.

//CMP: Substract and set flags according (Z,S,O,C); Help functions

OPTINLINE void op_adc32() {
	res32 = oper1d + oper2d + FLAG_CF;
	flag_adc32 (oper1d, oper2d, FLAG_CF);
}

OPTINLINE void op_add32() {
	res32 = oper1d + oper2d;
	flag_add32 (oper1d, oper2d);
}

OPTINLINE void op_and32() {
	res32 = oper1d & oper2d;
	flag_log32 (res32);
}

OPTINLINE void op_or32() {
	res32 = oper1d | oper2d;
	flag_log32 (res32);
}

OPTINLINE void op_xor32() {
	res32 = oper1d ^ oper2d;
	flag_log32 (res32);
}

OPTINLINE void op_sub32() {
	res32 = oper1d - oper2d;
	flag_sub32 (oper1d, oper2d);
}

OPTINLINE void op_sbb32() {
	res32 = oper1d - (oper2d + FLAG_CF);
	flag_sbb32 (oper1d, oper2d, FLAG_CF);
}

OPTINLINE void CMP_dw(uint_32 a, uint_32 b, byte flags) //Compare instruction!
{
	CPUPROT1
	flag_sub32(a,b); //Flags only!
	switch (flags & 7)
	{
	case 0: //Default?
		break; //Unused!
	case 1: //Accumulator?
		CPU[activeCPU].cycles_OP = 4; //Imm-Reg
		break;
	case 2: //Determined by ModR/M?
		if (params.EA_cycles) //Memory is used?
		{
			CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //Mem->Reg!
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	case 3: //ModR/M+imm?
		if (params.EA_cycles) //Memory is used?
		{
			CPU[activeCPU].cycles_OP = 10 + MODRM_EA(params); //Mem->Reg!
		}
		else //Imm->Reg?
		{
			CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
		}
		break;
	case 4: //Mem-Mem instruction?
		CPU[activeCPU].cycles_OP = 18; //Assume two times Reg->Mem
		break;
	}
	CPUPROT2
}

//Modr/m support, used when reg=NULL and custommem==0
extern byte MODRM_src0; //What source is our modr/m? (1/2)
extern byte MODRM_src1; //What source is our modr/m? (1/2)

//Custom memory support!
extern byte custommem ; //Used in some instructions!
extern uint_32 customoffset; //Offset to use!

//Help functions:
OPTINLINE void CPU80386_internal_INC32(uint_32 *reg)
{
	if (MMU_invaddr() || (reg==NULL))
	{
		return;
	}
	//Check for exceptions first!
	if (!reg) if (modrm_check32(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!reg) if (modrm_check32(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	INLINEREGISTER byte tempcf = FLAG_CF;
	oper1d = reg?*reg:modrm_read32(&params,MODRM_src0);
	oper2d = 1;
	op_add32();
	FLAGW_CF(tempcf);
	if (reg) //Register?
	{
		*reg = res32;
		CPU[activeCPU].cycles_OP = 2; //16-bit reg!
	}
	else //Memory?
	{
		modrm_write32(&params,MODRM_src0,res32); //Write the result to memory!
		CPU[activeCPU].cycles_OP = 15+MODRM_EA(params); //Mem
	}
	CPUPROT2
}
OPTINLINE void CPU80386_internal_DEC32(uint_32 *reg)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!reg) if (modrm_check32(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!reg) if (modrm_check32(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	INLINEREGISTER byte tempcf = FLAG_CF;
	oper1d = reg?*reg:modrm_read32(&params,MODRM_src0);
	oper2d = 1;
	op_sub32();
	FLAGW_CF(tempcf);
	if (reg) //Register?
	{
		*reg = res32;
		CPU[activeCPU].cycles_OP = 2; //16-bit reg!
	}
	else //Memory?
	{
		modrm_write32(&params,MODRM_src0,res32); //Write the result to memory!
		CPU[activeCPU].cycles_OP = 15+MODRM_EA(params); //Mem
	}
	CPUPROT2
}

OPTINLINE void timing_AND_OR_XOR_ADD_SUB32(uint_32 *dest, byte flags)
{
	switch (flags) //What type of operation?
	{
	case 0: //Reg+Reg?
		CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		break;
	case 1: //Reg+imm?
		CPU[activeCPU].cycles_OP = 4; //Accumulator!
		break;
	case 2: //Determined by ModR/M?
		if (params.EA_cycles) //Memory is used?
		{
			if (dest) //Mem->Reg?
			{
				CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //Mem->Reg!
			}
			else //Reg->Mem?
			{
				CPU[activeCPU].cycles_OP = 16 + MODRM_EA(params); //Mem->Reg!
			}
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	case 3: //ModR/M+imm?
		if (params.EA_cycles) //Memory is used?
		{
			if (dest) //Imm->Reg?
			{
				CPU[activeCPU].cycles_OP = 4; //Imm->Reg!
			}
			else //Imm->Mem?
			{
				CPU[activeCPU].cycles_OP = 17 + MODRM_EA(params); //Mem->Reg!
			}
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	}
}

//For ADD
OPTINLINE void CPU80386_internal_ADD32(uint_32 *dest, uint_32 addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check32(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check32(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1d = dest?*dest:modrm_read32(&params,MODRM_src0);
	oper2d = addition;
	op_add32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params,MODRM_src0,res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}

//For ADC
OPTINLINE void CPU80386_internal_ADC32(uint_32 *dest, uint_32 addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check32(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check32(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1d = dest?*dest:modrm_read32(&params,MODRM_src0);
	oper2d = addition;
	op_adc32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params,MODRM_src0,res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}


//For OR
OPTINLINE void CPU80386_internal_OR32(uint_32 *dest, uint_32 src, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check32(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check32(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1d = dest?*dest:modrm_read32(&params,MODRM_src0);
	oper2d = src;
	op_or32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params,MODRM_src0,res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}

//For AND
OPTINLINE void CPU80386_internal_AND32(uint_32 *dest, uint_32 src, byte flags)
{
	if (!dest) if (modrm_check32(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check32(&params,MODRM_src0,0)) return; //Abort on fault on write only!
	CPUPROT1
	oper1d = dest?*dest:modrm_read32(&params,MODRM_src0);
	oper2d = src;
	op_and32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params,MODRM_src0,res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}


//For SUB
OPTINLINE void CPU80386_internal_SUB32(uint_32 *dest, uint_32 addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check32(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check32(&params,MODRM_src0,0)) return; //Abort on fault on write only!
	CPUPROT1
	oper1d = dest?*dest:modrm_read32(&params,MODRM_src0);
	oper2d = addition;
	op_sub32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params,MODRM_src0,res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}

//For SBB
OPTINLINE void CPU80386_internal_SBB32(uint_32 *dest, uint_32 addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check32(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check32(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1d = dest?*dest:modrm_read32(&params,MODRM_src0);
	oper2d = addition;
	op_sbb32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params,MODRM_src0,res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}

//For XOR
//See AND, but XOR
OPTINLINE void CPU80386_internal_XOR32(uint_32 *dest, uint_32 src, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check32(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check32(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1d = dest?*dest:modrm_read32(&params,MODRM_src0);
	oper2d = src;
	op_xor32();
	if (dest) //Register?
	{
		*dest = res32;
	}
	else //Memory?
	{
		modrm_write32(&params,MODRM_src0,res32); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB32(dest, flags);
	CPUPROT2
}

//TEST : same as AND, but discarding the result!
OPTINLINE void CPU80386_internal_TEST32(uint_32 dest, uint_32 src, byte flags)
{
	uint_32 tmpdest = dest;
	CPU80386_internal_AND32(&tmpdest,src,0);
	//Adjust timing for TEST!
	switch (flags) //What type of operation?
	{
	case 0: //Reg+Reg?
		CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		break;
	case 1: //Reg+imm?
		CPU[activeCPU].cycles_OP = 4; //Accumulator!
		break;
	case 2: //Determined by ModR/M?
		if (params.EA_cycles) //Memory is used?
		{
			//Mem->Reg/Reg->Mem?
			CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //Mem->Reg!
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	case 3: //ModR/M+imm?
		if (params.EA_cycles) //Memory is used?
		{
			if (dest) //Imm->Reg?
			{
				CPU[activeCPU].cycles_OP = 5; //Imm->Reg!
			}
			else //Imm->Mem?
			{
				CPU[activeCPU].cycles_OP = 11 + MODRM_EA(params); //Mem->Reg!
			}
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	}
}

//MOV
OPTINLINE void CPU80386_internal_MOV8(byte *dest, byte val, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	if (dest) //Register?
	{
		*dest = val;
		switch (flags) //What type are we?
		{
		case 0: //Reg+Reg?
			break; //Unused!
		case 1: //Accumulator from immediate memory address?
			CPU[activeCPU].cycles_OP = 10; //[imm16]->Accumulator!
			break;
		case 2: //ModR/M Memory->Reg?
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP = 8+MODRM_EA(params); //Mem->Reg!
			}
			else //Reg->Reg?
			{
				CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
			}
			break;
		case 3: //ModR/M Memory immediate->Reg?
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP = 10 + MODRM_EA(params); //Mem->Reg!
			}
			else //Reg->Reg?
			{
				CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
			}
			break;
		case 4: //Register immediate->Reg?
			CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
			break;
		case 8: //SegReg->Reg?
			if (MODRM_src0 || (MODRM_EA(params)==0)) //From register?
			{
				CPU[activeCPU].cycles_OP = 2; //Reg->SegReg!
			}
			else //From memory?
			{
				CPU[activeCPU].cycles_OP = 8+MODRM_EA(params); //Mem->SegReg!
			}
			break;
		}
	}
	else //Memory?
	{
		if (custommem)
		{
			if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset,0,getCPL())) //Error accessing memory?
			{
				return; //Abort on fault!
			}
			MMU_wb(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset,val); //Write to memory directly!
			CPU[activeCPU].cycles_OP = 10; //Accumulator->[imm16]!
		}
		else //ModR/M?
		{
			if (modrm_check8(&params,MODRM_src0,0)) return; //Abort on fault!
			modrm_write8(&params,MODRM_src0,val); //Write the result to memory!
			switch (flags) //What type are we?
			{
			case 0: //Reg+Reg?
				break; //Unused!
			case 1: //Accumulator from immediate memory address?
				CPU[activeCPU].cycles_OP = 10; //Accumulator->[imm16]!
				break;
			case 2: //ModR/M Memory->Reg?
				if (MODRM_EA(params)) //Memory?
				{
					CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //Mem->Reg!
				}
				else //Reg->Reg?
				{
					CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
				}
				break;
			case 3: //ModR/M Memory immediate->Reg?
				if (MODRM_EA(params)) //Memory?
				{
					CPU[activeCPU].cycles_OP = 10 + MODRM_EA(params); //Mem->Reg!
				}
				else //Reg->Reg?
				{
					CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
				}
				break;
			case 4: //Register immediate->Reg (Non-existant!!!)?
				CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
				break;
			case 8: //Reg->SegReg?
				if (MODRM_src0 || (MODRM_EA(params) == 0)) //From register?
				{
					CPU[activeCPU].cycles_OP = 2; //SegReg->Reg!
				}
				else //From memory?
				{
					CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //SegReg->Mem!
				}
				break;
			}
		}
	}
	CPUPROT2
}
OPTINLINE void CPU80386_internal_MOV16(word *dest, word val, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	if (dest) //Register?
	{
		destEIP = REG_EIP; //Store (E)IP for safety!
		modrm_updatedsegment(dest,val,0); //Check for an updated segment!
		CPUPROT1
		*dest = val;
		switch (flags) //What type are we?
		{
		case 0: //Reg+Reg?
			break; //Unused!
		case 1: //Accumulator from immediate memory address?
			CPU[activeCPU].cycles_OP = 10; //[imm16]->Accumulator!
			break;
		case 2: //ModR/M Memory->Reg?
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP = 8 + MODRM_EA(params); //Mem->Reg!
			}
			else //Reg->Reg?
			{
				CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
			}
			break;
		case 3: //ModR/M Memory immediate->Reg?
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP = 10 + MODRM_EA(params); //Mem->Reg!
			}
			else //Reg->Reg?
			{
				CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
			}
			break;
		case 4: //Register immediate->Reg?
			CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
			break;
		case 8: //SegReg->Reg?
			if (MODRM_src0 || (MODRM_EA(params) == 0)) //From register?
			{
				CPU[activeCPU].cycles_OP = 2; //Reg->SegReg!
			}
			else //From memory?
			{
				CPU[activeCPU].cycles_OP = 8 + MODRM_EA(params); //Mem->SegReg!
			}
			break;
		}
		CPUPROT2
	}
	else //Memory?
	{
		if (custommem)
		{
			if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset,0,getCPL())) //Error accessing memory?
			{
				return; //Abort on fault!
			}
			if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset+1,0,getCPL())) //Error accessing memory?
			{
				return; //Abort on fault!
			}
			MMU_ww(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset,val); //Write to memory directly!
			CPU[activeCPU].cycles_OP = 10; //Accumulator->[imm16]!
		}
		else //ModR/M?
		{
			if (modrm_check16(&params,MODRM_src0,0)) return; //Abort on fault!
			modrm_write16(&params,MODRM_src0,val,0); //Write the result to memory!
			switch (flags) //What type are we?
			{
			case 0: //Reg+Reg?
				break; //Unused!
			case 1: //Accumulator from immediate memory address?
				CPU[activeCPU].cycles_OP = 10; //Accumulator->[imm16]!
				break;
			case 2: //ModR/M Memory->Reg?
				if (MODRM_EA(params)) //Memory?
				{
					CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //Mem->Reg!
				}
				else //Reg->Reg?
				{
					CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
				}
				break;
			case 3: //ModR/M Memory immediate->Reg?
				if (MODRM_EA(params)) //Memory?
				{
					CPU[activeCPU].cycles_OP = 10 + MODRM_EA(params); //Mem->Reg!
				}
				else //Reg->Reg?
				{
					CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
				}
				break;
			case 4: //Register immediate->Reg (Non-existant!!!)?
				CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
				break;
			case 8: //Reg->SegReg?
				if (MODRM_src0 || (MODRM_EA(params) == 0)) //From register?
				{
					CPU[activeCPU].cycles_OP = 2; //SegReg->Reg!
				}
				else //From memory?
				{
					CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //SegReg->Mem!
				}
				break;
			}
		}
	}
	CPUPROT2
}

OPTINLINE void CPU80386_internal_MOV32(uint_32 *dest, uint_32 val, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	if (dest) //Register?
	{
		CPUPROT1
		*dest = val;
		switch (flags) //What type are we?
		{
		case 0: //Reg+Reg?
			break; //Unused!
		case 1: //Accumulator from immediate memory address?
			CPU[activeCPU].cycles_OP = 10; //[imm32]->Accumulator!
			break;
		case 2: //ModR/M Memory->Reg?
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP = 8 + MODRM_EA(params); //Mem->Reg!
			}
			else //Reg->Reg?
			{
				CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
			}
			break;
		case 3: //ModR/M Memory immediate->Reg?
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP = 10 + MODRM_EA(params); //Mem->Reg!
			}
			else //Reg->Reg?
			{
				CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
			}
			break;
		case 4: //Register immediate->Reg?
			CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
			break;
		case 8: //SegReg->Reg?
			if (MODRM_src0 || (MODRM_EA(params) == 0)) //From register?
			{
				CPU[activeCPU].cycles_OP = 2; //Reg->SegReg!
			}
			else //From memory?
			{
				CPU[activeCPU].cycles_OP = 8 + MODRM_EA(params); //Mem->SegReg!
			}
			break;
		}
		CPUPROT2
	}
	else //Memory?
	{
		if (custommem)
		{
			if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset,0,getCPL())) //Error accessing memory?
			{
				return; //Abort on fault!
			}
			if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset+1,0,getCPL())) //Error accessing memory?
			{
				return; //Abort on fault!
			}
			MMU_wdw(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),customoffset,val); //Write to memory directly!
			CPU[activeCPU].cycles_OP = 10; //Accumulator->[imm32]!
		}
		else //ModR/M?
		{
			if (modrm_check32(&params,MODRM_src0,0)) return; //Abort on fault!
			modrm_write32(&params,MODRM_src0,val); //Write the result to memory!
			switch (flags) //What type are we?
			{
			case 0: //Reg+Reg?
				break; //Unused!
			case 1: //Accumulator from immediate memory address?
				CPU[activeCPU].cycles_OP = 10; //Accumulator->[imm32]!
				break;
			case 2: //ModR/M Memory->Reg?
				if (MODRM_EA(params)) //Memory?
				{
					CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //Mem->Reg!
				}
				else //Reg->Reg?
				{
					CPU[activeCPU].cycles_OP = 2; //Reg->Reg!
				}
				break;
			case 3: //ModR/M Memory immediate->Reg?
				if (MODRM_EA(params)) //Memory?
				{
					CPU[activeCPU].cycles_OP = 10 + MODRM_EA(params); //Mem->Reg!
				}
				else //Reg->Reg?
				{
					CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
				}
				break;
			case 4: //Register immediate->Reg (Non-existant!!!)?
				CPU[activeCPU].cycles_OP = 4; //Reg->Reg!
				break;
			case 8: //Reg->SegReg?
				if (MODRM_src0 || (MODRM_EA(params) == 0)) //From register?
				{
					CPU[activeCPU].cycles_OP = 2; //SegReg->Reg!
				}
				else //From memory?
				{
					CPU[activeCPU].cycles_OP = 9 + MODRM_EA(params); //SegReg->Mem!
				}
				break;
			}
		}
	}
	CPUPROT2
}

//LEA for LDS, LES
OPTINLINE word getLEA32(MODRM_PARAMS *theparams)
{
	return modrm_lea32(theparams,1);
}


/*

Non-logarithmic opcodes!

*/


OPTINLINE void CPU80386_internal_DAA()
{
	CPUPROT1
	if (((REG_AL&0xF)>9) || FLAG_AF)
	{
		oper1 = REG_AL+6;
		REG_AL = (oper1&0xFF);
		FLAGW_CF(((oper1&0xFF00)>0));
		FLAGW_AF(1);
	}
	else FLAGW_AF(0);
	if (((REG_AL&0xF0)>0x90) || FLAG_CF)
	{
		REG_AL += 0x60;
		FLAGW_CF(1);
	}
	else
	{
		FLAGW_CF(0);
	}
	flag_szp8(REG_AL);
	CPUPROT2
	CPU[activeCPU].cycles_OP = 4; //Timings!
}
OPTINLINE void CPU80386_internal_DAS()
{
	INLINEREGISTER byte tempCF, tempAL;
	tempAL = REG_AL;
	tempCF = FLAG_CF; //Save old values!
	CPUPROT1
	if (((REG_AL&0xF)>9) || FLAG_AF)
	{
		oper1 = REG_AL-6;
		REG_AL = oper1&255;
		FLAGW_CF(tempCF|((oper1&0xFF00)>0));
		FLAGW_AF(1);
	}
	else FLAGW_AF(0);

	if ((tempAL>0x99) || tempCF)
	{
		REG_AL -= 0x60;
		FLAGW_CF(1);
	}
	else
	{
		FLAGW_CF(0);
	}
	flag_szp8(REG_AL);
	CPUPROT2
	CPU[activeCPU].cycles_OP = 4; //Timings!
}
OPTINLINE void CPU80386_internal_AAA()
{
	CPUPROT1
	if (((REG_AL&0xF)>9) || FLAG_AF)
	{
		REG_AL += 6;
		++REG_AH;
		FLAGW_AF(1);
		FLAGW_CF(1);
	}
	else
	{
		FLAGW_AF(0);
		FLAGW_CF(0);
	}
	REG_AL &= 0xF;
	flag_szp8(REG_AL); //Basic flags!
	CPUPROT2
	CPU[activeCPU].cycles_OP = 4; //Timings!
}
OPTINLINE void CPU80386_internal_AAS()
{
	CPUPROT1
	if (((REG_AL&0xF)>9) || FLAG_AF)
	{
		REG_AL -= 6;
		--REG_AH;
		FLAGW_AF(1);
		FLAGW_CF(1);
	}
	else
	{
		FLAGW_AF(0);
		FLAGW_CF(0);
	}
	REG_AL &= 0xF;
	flag_szp8(REG_AL); //Basic flags!
	CPUPROT2
	CPU[activeCPU].cycles_OP = 4; //Timings!
}

OPTINLINE void CPU80386_internal_CWDE()
{
	CPUPROT1
	if ((REG_AX&0x80)==0x80)
	{
		REG_EAX |= 0xFFFF;
	}
	else
	{
		REG_AX &= 0xFFFF;
	}
	CPU[activeCPU].cycles_OP = 2; //Clock cycles!
	CPUPROT2
}
OPTINLINE void CPU80386_internal_CDQ()
{
	CPUPROT1
	if ((REG_EAX&0x80000000)==0x80000000)
	{
		REG_EDX = 0xFFFFFFFF;
	}
	else
	{
		REG_EDX = 0;
	}
	CPU[activeCPU].cycles_OP = 5; //Clock cycles!
	CPUPROT2
}

//Now the repeatable instructions!

extern byte newREP; //Are we a new repeating instruction (REP issued for a new instruction, not repeating?)

OPTINLINE void CPU80386_internal_MOVSD()
{
	INLINEREGISTER uint_32 data;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),REG_ESI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),REG_ESI+1,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),REG_ESI+2,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),REG_ESI+3,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES,REG_ES,REG_EDI,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES,REG_ES,REG_EDI+1,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES,REG_ES,REG_EDI+2,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES,REG_ES,REG_EDI+3,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	data = MMU_rdw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_ESI, 0); //Try to read the data!
	CPUPROT1
	MMU_wdw(CPU_SEGMENT_ES,REG_ES,REG_DI,data); //Try to write the data!
	CPUPROT1
	if (FLAG_DF)
	{
		REG_ESI -= 2;
		REG_EDI -= 2;
	}
	else
	{
		REG_ESI += 2;
		REG_EDI += 2;
	}
	CPUPROT2
	CPUPROT2
	if (CPU[activeCPU].repeating) //Are we a repeating instruction?
	{
		if (newREP) //Include the REP?
		{
			CPU[activeCPU].cycles_OP = 9 + 17; //Clock cycles including REP!
		}
		else //Repeating instruction itself?
		{
			CPU[activeCPU].cycles_OP = 17; //Clock cycles excluding REP!
		}
	}
	else //Plain non-repeating instruction?
	{
		CPU[activeCPU].cycles_OP = 18; //Clock cycles!
	}
}

OPTINLINE void CPU80386_internal_CMPSD()
{
	INLINEREGISTER uint_32 data1, data2;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS),REG_ESI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS),REG_ESI+1,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS),REG_ESI+2,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS),REG_ESI+3,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}

	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_EDI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_EDI+1,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_EDI+2,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_EDI+3,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	data1 = MMU_rdw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_ESI, 0); //Try to read the first data!
	CPUPROT1
	data2 = MMU_rdw(CPU_SEGMENT_ES, REG_ES, REG_EDI, 0); //Try to read the second data!
	CPUPROT1
	CMP_dw(data1,data2,4);
	if (FLAG_DF)
	{
		REG_ESI -= 2;
		REG_EDI -= 2;
	}
	else
	{
		REG_ESI += 2;
		REG_EDI += 2;
	}
	CPUPROT2
	CPUPROT2
	if (CPU[activeCPU].repeating) //Are we a repeating instruction?
	{
		if (newREP) //Include the REP?
		{
			CPU[activeCPU].cycles_OP = 9 + 22; //Clock cycles including REP!
		}
		else //Repeating instruction itself?
		{
			CPU[activeCPU].cycles_OP = 22; //Clock cycles excluding REP!
		}
	}
	else //Plain non-repeating instruction?
	{
		CPU[activeCPU].cycles_OP = 22; //Clock cycles!
	}
}

OPTINLINE void CPU80386_internal_STOSD()
{
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_EDI,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_EDI+1,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_EDI+2,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_EDI+3,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	MMU_wdw(CPU_segment_index(CPU_SEGMENT_ES),REG_ES,REG_DI,REG_EAX);
	CPUPROT1
	if (FLAG_DF)
	{
		REG_EDI -= 2;
	}
	else
	{
		REG_EDI += 2;
	}
	CPUPROT2
	if (CPU[activeCPU].repeating) //Are we a repeating instruction?
	{
		if (newREP) //Include the REP?
		{
			CPU[activeCPU].cycles_OP = 9 + 10; //Clock cycles including REP!
		}
		else //Repeating instruction itself?
		{
			CPU[activeCPU].cycles_OP = 10; //Clock cycles excluding REP!
		}
	}
	else //Plain non-repeating instruction?
	{
		CPU[activeCPU].cycles_OP = 11; //Clock cycles!
	}
}
//OK so far!

OPTINLINE void CPU80386_internal_LODSD()
{
	INLINEREGISTER uint_32 value;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_ESI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_ESI+1,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_ESI+2,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_ESI+3,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}

	value = MMU_rdw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the result!
	CPUPROT1
	REG_EAX = value;
	if (FLAG_DF)
	{
		REG_ESI -= 2;
	}
	else
	{
		REG_ESI += 2;
	}
	CPUPROT2
	if (CPU[activeCPU].repeating) //Are we a repeating instruction?
	{
		if (newREP) //Include the REP?
		{
			CPU[activeCPU].cycles_OP = 9 + 13; //Clock cycles including REP!
		}
		else //Repeating instruction itself?
		{
			CPU[activeCPU].cycles_OP = 13; //Clock cycles excluding REP!
		}
	}
	else //Plain non-repeating instruction?
	{
		CPU[activeCPU].cycles_OP = 12; //Clock cycles!
	}
}

OPTINLINE void CPU80386_internal_SCASD()
{
	INLINEREGISTER uint_32 cmp1;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_EDI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_EDI+1,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_EDI+2,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_EDI+3,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	cmp1 = MMU_rw(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_EDI, 0); //Try to read the data to compare!
	CPUPROT1
	CMP_dw(REG_EAX,cmp1,4);
	if (FLAG_DF)
	{
		REG_EDI -= 2;
	}
	else
	{
		REG_EDI += 2;
	}
	CPUPROT2
	if (CPU[activeCPU].repeating) //Are we a repeating instruction?
	{
		if (newREP) //Include the REP?
		{
			CPU[activeCPU].cycles_OP = 9 + 15; //Clock cycles including REP!
		}
		else //Repeating instruction itself?
		{
			CPU[activeCPU].cycles_OP = 15; //Clock cycles excluding REP!
		}
	}
	else //Plain non-repeating instruction?
	{
		CPU[activeCPU].cycles_OP = 15; //Clock cycles!
	}
}

OPTINLINE void CPU80386_internal_RET(word popbytes, byte isimm)
{
	if (checkStackAccess(1,0,1)) //Error accessing stack?
	{
		return; //Abort on fault!
	}
	INLINEREGISTER uint_32 val = CPU_POP32();    //Near return
	CPUPROT1
	REG_IP = val;
	CPU_flushPIQ(); //We're jumping to another address!
	REG_SP += popbytes;
	CPUPROT2
	if (isimm)
		CPU[activeCPU].cycles_OP = 12; /* Intrasegment with constant */
	else
		CPU[activeCPU].cycles_OP = 8; /* Intrasegment */
}
OPTINLINE void CPU80386_internal_RETF(word popbytes, byte isimm)
{
	if (checkStackAccess(2,0,1)) //Error accessing stack?
	{
		return; //Abort on fault!
	}
	INLINEREGISTER word val = CPU_POP32(); //Far return
	word destCS;
	CPUPROT1
	destCS = CPU_POP32(); //POP CS!
	CPUPROT1
	destEIP = val; //Load IP!
	segmentWritten(CPU_SEGMENT_CS,destCS,4); //CS changed, we're a RETF instruction!
	CPU_flushPIQ(); //We're jumping to another address!
	CPUPROT1
	REG_SP += popbytes; //Process SP!
	if (isimm)
		CPU[activeCPU].cycles_OP = 17; /* Intersegment with constant */
	else
		CPU[activeCPU].cycles_OP = 18; /* Intersegment */
	CPUPROT2
	CPUPROT2
	CPUPROT2
}

void external80386RETF(word popbytes)
{
	CPU80386_internal_RETF(popbytes,1); //Return immediate variant!
}

OPTINLINE void CPU80386_internal_INTO()
{
	CPUPROT1
	if (FLAG_OF)
	{
		CPU80386_INTERNAL_int(EXCEPTION_OVERFLOW,0);
		CPU[activeCPU].cycles_OP = 53; //Timings!
	}
	else
	{
		CPU[activeCPU].cycles_OP = 4; //Timings!
	}
	CPUPROT2
}

OPTINLINE void CPU80386_internal_AAM(byte data)
{
	CPUPROT1
	if (!data)
	{
		CPU_exDIV0();    //AAM
		return;
	}
	REG_AH = (((byte)SAFEDIV(REG_AL,data))&0xFF);
	REG_AL = (SAFEMOD(REG_AL,data)&0xFF);
	flag_szp32(REG_AX);
	FLAGW_OF(0);FLAGW_CF(0);FLAGW_AF(0); //Clear these!
	CPUPROT2
	CPU[activeCPU].cycles_OP = 83; //Timings!
}
OPTINLINE void CPU80386_internal_AAD(byte data)
{
	CPUPROT1
	REG_AX = ((REG_AH*data)+REG_AL);    //AAD
	REG_AH = 0;
	flag_szp8(REG_AL); //Update the flags!
	FLAGW_OF(0);FLAGW_CF(0);FLAGW_AF(0); //Clear these!
	CPUPROT2
	CPU[activeCPU].cycles_OP = 60; //Timings!
}

OPTINLINE void CPU80386_internal_XLAT()
{
	if (cpudebugger) //Debugger on?
	{
		debugger_setcommand("XLAT");    //XLAT
	}
	CPUPROT1
	INLINEREGISTER byte value = MMU_rb(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),REG_BX+REG_AL,0);    //XLAT
	CPUPROT1
	REG_AL = value;
	CPUPROT2
	CPUPROT2
	CPU[activeCPU].cycles_OP = 11; //XLAT timing!
}

OPTINLINE void CPU80386_internal_XCHG8(byte *data1, byte *data2, byte flags)
{
	if (!data1) if (modrm_check8(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!data1) if (modrm_check8(&params,MODRM_src0,0)) return; //Abort on fault!
	if (!data2) if (modrm_check8(&params,MODRM_src1,1)) return; //Abort on fault!
	if (!data2) if (modrm_check8(&params,MODRM_src1,0)) return; //Abort on fault!
	CPUPROT1
	oper1b = data1?*data1:modrm_read8(&params,MODRM_src0);
	CPUPROT1
	oper2b = data2?*data2:modrm_read8(&params,MODRM_src1);
	CPUPROT1
	INLINEREGISTER byte temp = oper1b; //Copy!
	oper1b = oper2b; //We're ...
	oper2b = temp; //Swapping this!
	if (data1)
	{
		*data1 = oper1b;
	}
	else
	{
		modrm_write8(&params,MODRM_src0,oper1b);
	}
	CPUPROT1
	if (data2)
	{
		*data2 = oper2b;
	}
	else
	{
		modrm_write8(&params,MODRM_src1,oper2b);
	}
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	switch (flags)
	{
	case 0: //Unknown?
		break;
	case 1: //Acc<->Reg?
		CPU[activeCPU].cycles_OP = 3; //Acc<->Reg!
		break;
	case 2: //Mem<->Reg?
		if (MODRM_EA(params)) //Reg<->Mem?
		{
			CPU[activeCPU].cycles_OP = 17 + MODRM_EA(params); //SegReg->Mem!
		}
		else //Reg<->Reg?
		{
			CPU[activeCPU].cycles_OP = 4; //SegReg->Mem!
		}
		break;
	}
}

OPTINLINE void CPU80386_internal_XCHG32(uint_32 *data1, uint_32 *data2, byte flags)
{
	if (!data1) if (modrm_check32(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!data1) if (modrm_check32(&params,MODRM_src0,0)) return; //Abort on fault!
	if (!data2) if (modrm_check32(&params,MODRM_src1,1)) return; //Abort on fault!
	if (!data2) if (modrm_check32(&params,MODRM_src1,0)) return; //Abort on fault!
	CPUPROT1
	oper1d = data1?*data1:modrm_read32(&params,MODRM_src0);
	CPUPROT1
	oper2d = data2?*data2:modrm_read32(&params,MODRM_src1);
	CPUPROT1
	//Do a simple swap!
	word temp = oper1d; //Copy!
	oper1d = oper2d; //We're ...
	oper2d = temp; //Swapping this!
	if (data1)
	{
		*data1 = oper1d;
	}
	else
	{
		modrm_write32(&params,MODRM_src0,oper1d);
	}
	CPUPROT1
	if (data2)
	{
		*data2 = oper2d;
	}
	else
	{
		modrm_write32(&params,MODRM_src1,oper2d);
	}
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	switch (flags)
	{
	case 0: //Unknown?
		break;
	case 1: //Acc<->Reg?
		CPU[activeCPU].cycles_OP = 3; //Acc<->Reg!
		break;
	case 2: //Mem<->Reg?
		if (MODRM_EA(params)) //Reg<->Mem?
		{
			CPU[activeCPU].cycles_OP = 17 + MODRM_EA(params); //SegReg->Mem!
		}
		else //Reg<->Reg?
		{
			CPU[activeCPU].cycles_OP = 4; //SegReg->Mem!
		}
		break;
	}
}

extern byte modrm_addoffset; //Add this offset to ModR/M reads!

OPTINLINE void CPU80386_internal_LXS(int segmentregister) //LDS, LES etc.
{
	modrm_addoffset = 0; //Add this to the offset to use!
	if (modrm_check32(&params,1,1)) return; //Abort on fault!
	modrm_addoffset = 2; //Add this to the offset to use!
	if (modrm_check16(&params,1,1)) return; //Abort on fault!
	if (modrm_check16(&params,0,0)) return; //Abort on fault for the used segment itself!

	CPUPROT1
	modrm_addoffset = 0; //Add this to the offset to use!
	word offset = modrm_read32(&params,1);
	CPUPROT1
	modrm_addoffset = 2; //Add this to the offset to use!
	word segment = modrm_read16(&params,1);
	modrm_addoffset = 0; //Reset again!
	CPUPROT1
		destEIP = REG_EIP; //Save EIP for transfers!
		segmentWritten(segmentregister, segment,0); //Load the new segment!
	CPUPROT1
		modrm_write16(&params, 0, offset, 0); //Try to load the new register with the offset!
	CPUPROT2
	CPUPROT2
	CPUPROT2
	CPUPROT2
	if (MODRM_EA(params)) //Memory?
	{
		CPU[activeCPU].cycles_OP = 16+MODRM_EA(params); /* LXS based on MOV Mem->SS, DS, ES */
	}
	else //Register? Should be illegal?
	{
		CPU[activeCPU].cycles_OP = 2; /* LXS based on MOV Mem->SS, DS, ES */
	}
}

void CPU80386_CALLF(word segment, word offset)
{
	destEIP = offset;
	segmentWritten(CPU_SEGMENT_CS, segment, 2); /*CS changed, call version!*/
	CPU_flushPIQ(); //We're jumping to another address!
}

/*

NOW THE REAL OPCODES!

*/

extern byte didJump; //Did we jump this instruction?


void CPU80386_OP01() {modrm_generateInstructionTEXT("ADDD",32,0,PARAM_MODRM21); if (modrm_check32(&params,0,1)) return; CPU80386_internal_ADD32(modrm_addr32(&params,1,0),modrm_read32(&params,0),2); }
void CPU80386_OP03() {modrm_generateInstructionTEXT("ADDD",32,0,PARAM_MODRM12); if (modrm_check32(&params,1,1)) return; CPU80386_internal_ADD32(modrm_addr32(&params,0,0),modrm_read32(&params,1),2); }
void CPU80386_OP05() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("ADDD EAX,",0,theimm,PARAM_IMM32); CPU80386_internal_ADD32(&REG_EAX,theimm,1); }
void CPU80386_OP06() {modrm_generateInstructionTEXT("PUSH ES",0,0,PARAM_NONE); CPU_PUSH16(&REG_ES);/*PUSH ES*/ CPU[activeCPU].cycles_OP = 10; /*Push Segreg!*/}
void CPU80386_OP07() {modrm_generateInstructionTEXT("POP ES",0,0,PARAM_NONE); segmentWritten(CPU_SEGMENT_ES,CPU_POP16(),0); /*CS changed!*/ CPU[activeCPU].cycles_OP = 8; /*Pop Segreg!*/}
void CPU80386_OP09() {modrm_generateInstructionTEXT("ORD",32,0,PARAM_MODRM21); if (modrm_check32(&params,0,1)) return; CPU80386_internal_OR32(modrm_addr32(&params,1,0),modrm_read32(&params,0),2); }
void CPU80386_OP0B() {modrm_generateInstructionTEXT("ORD",32,0,PARAM_MODRM12); if (modrm_check32(&params,1,1)) return; CPU80386_internal_OR32(modrm_addr32(&params,0,0),modrm_read32(&params,1),2); }
void CPU80386_OP0D() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("ORD EAX,",0,theimm,PARAM_IMM32); CPU80386_internal_OR32(&REG_EAX,theimm,1); }
void CPU80386_OP0E() {modrm_generateInstructionTEXT("PUSH CS",0,0,PARAM_NONE); CPU_PUSH16(&REG_CS);/*PUSH CS*/ CPU[activeCPU].cycles_OP = 10; /*Push Segreg!*/}
void CPU80386_OP11() {modrm_generateInstructionTEXT("ADCD",32,0,PARAM_MODRM21); if (modrm_check32(&params,0,1)) return; CPU80386_internal_ADC32(modrm_addr32(&params,1,0),modrm_read32(&params,0),2); }
void CPU80386_OP13() {modrm_generateInstructionTEXT("ADCD",32,0,PARAM_MODRM12); if (modrm_check32(&params,1,1)) return; CPU80386_internal_ADC32(modrm_addr32(&params,0,0),modrm_read32(&params,1),2); }
void CPU80386_OP15() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("ADC AX,",0,theimm,PARAM_IMM32); CPU80386_internal_ADC32(&REG_EAX,theimm,1); }
void CPU80386_OP19() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("SBBD",32,0,PARAM_MODRM21); if (modrm_check32(&params,0,1)) return; CPU80386_internal_SBB32(modrm_addr32(&params,1,0),(modrm_read32(&params,0)),2); }
void CPU80386_OP1B() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("SBBD",32,0,PARAM_MODRM12); if (modrm_check32(&params,1,1)) return; CPU80386_internal_SBB32(modrm_addr32(&params,0,0),(modrm_read32(&params,1)),2); }
void CPU80386_OP1D() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("SBBD EAX,",0,theimm,PARAM_IMM32); CPU80386_internal_SBB32(&REG_EAX,theimm,1); }
void CPU80386_OP21() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("ANDD",32,0,PARAM_MODRM21); if (modrm_check32(&params,0,1)) return; CPU80386_internal_AND32(modrm_addr32(&params,1,0),modrm_read32(&params,0),2); }
void CPU80386_OP23() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("ANDD",32,0,PARAM_MODRM12); if (modrm_check32(&params,1,1)) return; CPU80386_internal_AND32(modrm_addr32(&params,0,0),modrm_read32(&params,1),2); }
void CPU80386_OP25() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("ANDD EAX,",0,theimm,PARAM_IMM32); CPU80386_internal_AND32(&REG_EAX,theimm,1); }
void CPU80386_OP27() {modrm_generateInstructionTEXT("DAA",0,0,PARAM_NONE);/*DAA?*/ CPU80386_internal_DAA();/*DAA?*/ }
void CPU80386_OP29() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("SUBD",32,0,PARAM_MODRM21); if (modrm_check32(&params,0,1)) return; CPU80386_internal_SUB32(modrm_addr32(&params,1,0),(modrm_read32(&params,0)),2); }
void CPU80386_OP2B() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("SUBD",32,0,PARAM_MODRM12); if (modrm_check32(&params,1,1)) return; CPU80386_internal_SUB32(modrm_addr32(&params,0,0),(modrm_read32(&params,1)),2); }
void CPU80386_OP2D() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("SUBD EAX,",0,theimm,PARAM_IMM32);/*5=AX,imm32*/ CPU80386_internal_SUB32(&REG_EAX,theimm,1);/*5=AX,imm32*/ }
void CPU80386_OP2F() {modrm_generateInstructionTEXT("DAS",0,0,PARAM_NONE);/*DAS?*/ CPU80386_internal_DAS();/*DAS?*/ }
void CPU80386_OP31() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("XORD",32,0,PARAM_MODRM21); if (modrm_check32(&params,0,1)) return; CPU80386_internal_XOR32(modrm_addr32(&params,1,0),modrm_read32(&params,0),2); }
void CPU80386_OP33() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("XORD",32,0,PARAM_MODRM12); if (modrm_check32(&params,1,1)) return; CPU80386_internal_XOR32(modrm_addr32(&params,0,0),modrm_read32(&params,1),2); }
void CPU80386_OP35() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("XORD EAX,",0,theimm,PARAM_IMM32); CPU80386_internal_XOR32(&REG_EAX,theimm,1); }
void CPU80386_OP37() {modrm_generateInstructionTEXT("AAA",0,0,PARAM_NONE);/*AAA?*/ CPU80386_internal_AAA();/*AAA?*/ }
void CPU80386_OP39() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("CMPD",32,0,PARAM_MODRM21); if (modrm_check32(&params,0,1)) return; if (modrm_check32(&params,1,1)) return; CMP_dw(modrm_read32(&params,1),modrm_read32(&params,0),2); }
void CPU80386_OP3B() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("CMPD",32,0,PARAM_MODRM12); if (modrm_check32(&params,0,1)) return; if (modrm_check32(&params,1,1)) return; CMP_dw(modrm_read32(&params,0),modrm_read32(&params,1),2); }
void CPU80386_OP3D() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("CMP EAX,",0,theimm,PARAM_IMM32);/*CMP AX, imm32*/ CMP_dw(REG_EAX,theimm,1);/*CMP AX, imm32*/ }
void CPU80386_OP3F() {modrm_generateInstructionTEXT("AAS",0,0,PARAM_NONE);/*AAS?*/ CPU80386_internal_AAS();/*AAS?*/ }
void CPU80386_OP40() {modrm_generateInstructionTEXT("INC EAX",0,0,PARAM_NONE);/*INC AX*/ CPU80386_internal_INC32(&REG_EAX);/*INC AX*/ }
void CPU80386_OP41() {modrm_generateInstructionTEXT("INC ECX",0,0,PARAM_NONE);/*INC CX*/ CPU80386_internal_INC32(&REG_ECX);/*INC CX*/ }
void CPU80386_OP42() {modrm_generateInstructionTEXT("INC EDX",0,0,PARAM_NONE);/*INC DX*/ CPU80386_internal_INC32(&REG_EDX);/*INC DX*/ }
void CPU80386_OP43() {modrm_generateInstructionTEXT("INC EBX",0,0,PARAM_NONE);/*INC BX*/ CPU80386_internal_INC32(&REG_EBX);/*INC BX*/ }
void CPU80386_OP44() {modrm_generateInstructionTEXT("INC ESP",0,0,PARAM_NONE);/*INC SP*/ CPU80386_internal_INC32(&REG_ESP);/*INC SP*/ }
void CPU80386_OP45() {modrm_generateInstructionTEXT("INC EBP",0,0,PARAM_NONE);/*INC BP*/ CPU80386_internal_INC32(&REG_EBP);/*INC BP*/ }
void CPU80386_OP46() {modrm_generateInstructionTEXT("INC ESI",0,0,PARAM_NONE);/*INC SI*/ CPU80386_internal_INC32(&REG_ESI);/*INC SI*/ }
void CPU80386_OP47() {modrm_generateInstructionTEXT("INC EDI",0,0,PARAM_NONE);/*INC DI*/ CPU80386_internal_INC32(&REG_EDI);/*INC DI*/ }
void CPU80386_OP48() {modrm_generateInstructionTEXT("DEC EAX",0,0,PARAM_NONE);/*DEC AX*/ CPU80386_internal_DEC32(&REG_EAX);/*DEC AX*/ }
void CPU80386_OP49() {modrm_generateInstructionTEXT("DEC ECX",0,0,PARAM_NONE);/*DEC CX*/ CPU80386_internal_DEC32(&REG_ECX);/*DEC CX*/ }
void CPU80386_OP4A() {modrm_generateInstructionTEXT("DEC EDX",0,0,PARAM_NONE);/*DEC DX*/ CPU80386_internal_DEC32(&REG_EDX);/*DEC DX*/ }
void CPU80386_OP4B() {modrm_generateInstructionTEXT("DEC EBX",0,0,PARAM_NONE);/*DEC BX*/ CPU80386_internal_DEC32(&REG_EBX);/*DEC BX*/ }
void CPU80386_OP4C() {modrm_generateInstructionTEXT("DEC ESP",0,0,PARAM_NONE);/*DEC SP*/ CPU80386_internal_DEC32(&REG_ESP);/*DEC SP*/ }
void CPU80386_OP4D() {modrm_generateInstructionTEXT("DEC EBP",0,0,PARAM_NONE);/*DEC BP*/ CPU80386_internal_DEC32(&REG_EBP);/*DEC BP*/ }
void CPU80386_OP4E() {modrm_generateInstructionTEXT("DEC ESI",0,0,PARAM_NONE);/*DEC SI*/ CPU80386_internal_DEC32(&REG_ESI);/*DEC SI*/ }
void CPU80386_OP4F() {modrm_generateInstructionTEXT("DEC EDI",0,0,PARAM_NONE);/*DEC DI*/ CPU80386_internal_DEC32(&REG_EDI);/*DEC DI*/ }
void CPU80386_OP50() {modrm_generateInstructionTEXT("PUSH EAX",0,0,PARAM_NONE);/*PUSH AX*/ if (checkStackAccess(1,1,1)) return; CPU_PUSH32(&REG_EAX);/*PUSH AX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ }
void CPU80386_OP51() {modrm_generateInstructionTEXT("PUSH ECX",0,0,PARAM_NONE);/*PUSH CX*/ if (checkStackAccess(1,1,1)) return; CPU_PUSH32(&REG_ECX);/*PUSH CX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ }
void CPU80386_OP52() {modrm_generateInstructionTEXT("PUSH EDX",0,0,PARAM_NONE);/*PUSH DX*/ if (checkStackAccess(1,1,1)) return; CPU_PUSH32(&REG_EDX);/*PUSH DX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ }
void CPU80386_OP53() {modrm_generateInstructionTEXT("PUSH EBX",0,0,PARAM_NONE);/*PUSH BX*/ if (checkStackAccess(1,1,1)) return; CPU_PUSH32(&REG_EBX);/*PUSH BX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ }
void CPU80386_OP54() {modrm_generateInstructionTEXT("PUSH ESP",0,0,PARAM_NONE);/*PUSH SP*/ if (checkStackAccess(1,1,1)) return; CPU_PUSH32(&REG_ESP);/*PUSH SP*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ }
void CPU80386_OP55() {modrm_generateInstructionTEXT("PUSH EBP",0,0,PARAM_NONE);/*PUSH BP*/ if (checkStackAccess(1,1,1)) return; CPU_PUSH32(&REG_EBP);/*PUSH BP*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ }
void CPU80386_OP56() {modrm_generateInstructionTEXT("PUSH ESI",0,0,PARAM_NONE);/*PUSH SI*/ if (checkStackAccess(1,1,1)) return; CPU_PUSH32(&REG_ESI);/*PUSH SI*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ }
void CPU80386_OP57() {modrm_generateInstructionTEXT("PUSH EDI",0,0,PARAM_NONE);/*PUSH DI*/ if (checkStackAccess(1,1,1)) return; CPU_PUSH32(&REG_EDI);/*PUSH DI*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ }
void CPU80386_OP58() {modrm_generateInstructionTEXT("POP EAX",0,0,PARAM_NONE);/*POP AX*/ if (checkStackAccess(1,0,1)) return; REG_EAX = CPU_POP32();/*POP AX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ }
void CPU80386_OP59() {modrm_generateInstructionTEXT("POP ECX",0,0,PARAM_NONE);/*POP CX*/ if (checkStackAccess(1,0,1)) return; REG_ECX = CPU_POP32();/*POP CX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ }
void CPU80386_OP5A() {modrm_generateInstructionTEXT("POP EDX",0,0,PARAM_NONE);/*POP DX*/ if (checkStackAccess(1,0,1)) return; REG_EDX = CPU_POP32();/*POP DX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ }
void CPU80386_OP5B() {modrm_generateInstructionTEXT("POP EBX",0,0,PARAM_NONE);/*POP BX*/ if (checkStackAccess(1,0,1)) return; REG_EBX = CPU_POP32();/*POP BX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ }
void CPU80386_OP5C() {modrm_generateInstructionTEXT("POP ESP",0,0,PARAM_NONE);/*POP SP*/ if (checkStackAccess(1,0,1)) return; REG_ESP = MMU_rw(CPU_SEGMENT_SS,REG_SS,REG_SP,0);/*POP SP*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ }
void CPU80386_OP5D() {modrm_generateInstructionTEXT("POP EBP",0,0,PARAM_NONE);/*POP BP*/ if (checkStackAccess(1,0,1)) return; REG_EBP = CPU_POP32();/*POP BP*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ }
void CPU80386_OP5E() {modrm_generateInstructionTEXT("POP ESI",0,0,PARAM_NONE);/*POP SI*/ if (checkStackAccess(1,0,1)) return; REG_ESI = CPU_POP32();/*POP SI*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ }
void CPU80386_OP5F() {modrm_generateInstructionTEXT("POP EDI",0,0,PARAM_NONE);/*POP DI*/ if (checkStackAccess(1,0,1)) return; REG_EDI = CPU_POP32();/*POP DI*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ }
void CPU80386_OP85() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("TESTD",32,0,PARAM_MODRM12); if (modrm_check32(&params,0,1)) return; if (modrm_check32(&params,1,1)) return; CPU80386_internal_TEST32(modrm_read32(&params,0),modrm_read32(&params,1),2); }
void CPU80386_OP87() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("XCHGD",32,0,PARAM_MODRM12); CPU80386_internal_XCHG32(modrm_addr32(&params,0,0),modrm_addr32(&params,1,1),2); /*XCHG reg32,r/m32*/ }
void CPU80386_OP89() {modrm_debugger32(&params,1,0); modrm_generateInstructionTEXT("MOVD",32,0,PARAM_MODRM21); if (modrm_check32(&params,0,1)) return; CPU80386_internal_MOV32(modrm_addr32(&params,1,0),modrm_read32(&params,0),2); }
void CPU80386_OP8B() {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("MOVD",32,0,PARAM_MODRM12); if (modrm_check32(&params,1,1)) return; CPU80386_internal_MOV32(modrm_addr32(&params,0,0),modrm_read32(&params,1),2); }
void CPU80386_OP8D() {modrm_debugger32(&params,0,1); debugger_setcommand("LEA %s,%s",modrm_param1,getLEAtext32(&params)); CPU80386_internal_MOV32(modrm_addr32(&params,0,0),getLEA32(&params),0); CPU[activeCPU].cycles_OP = 2+MODRM_EA(params); /* Load effective address */}
void CPU80386_OP90() /*NOP*/ {modrm_generateInstructionTEXT("NOP",0,0,PARAM_NONE);/*NOP (XCHG AX,AX)*/ CPU80386_internal_XCHG32(&REG_EAX,&REG_EAX,1); CPU[activeCPU].cycles_OP = 3; /* NOP */}
void CPU80386_OP91() {modrm_generateInstructionTEXT("XCHGD ECX,EAX",0,0,PARAM_NONE);/*XCHG AX,CX*/ CPU80386_internal_XCHG32(&REG_ECX,&REG_EAX,1); /*XCHG CX,AX*/ }
void CPU80386_OP92() {modrm_generateInstructionTEXT("XCHGD EDX,EAX",0,0,PARAM_NONE);/*XCHG AX,DX*/ CPU80386_internal_XCHG32(&REG_EDX,&REG_EAX,1); /*XCHG DX,AX*/ }
void CPU80386_OP93() {modrm_generateInstructionTEXT("XCHGD EBX,EAX",0,0,PARAM_NONE);/*XCHG AX,BX*/ CPU80386_internal_XCHG32(&REG_EBX,&REG_EAX,1); /*XCHG BX,AX*/ }
void CPU80386_OP94() {modrm_generateInstructionTEXT("XCHGD ESP,EAX",0,0,PARAM_NONE);/*XCHG AX,SP*/ CPU80386_internal_XCHG32(&REG_ESP,&REG_EAX,1); /*XCHG SP,AX*/ }
void CPU80386_OP95() {modrm_generateInstructionTEXT("XCHGD EBP,EAX",0,0,PARAM_NONE);/*XCHG AX,BP*/ CPU80386_internal_XCHG32(&REG_EBP,&REG_EAX,1); /*XCHG BP,AX*/ }
void CPU80386_OP96() {modrm_generateInstructionTEXT("XCHGD ESI,EAX",0,0,PARAM_NONE);/*XCHG AX,SI*/ CPU80386_internal_XCHG32(&REG_ESI,&REG_EAX,1); /*XCHG SI,AX*/ }
void CPU80386_OP97() {modrm_generateInstructionTEXT("XCHGD EDI,EAX",0,0,PARAM_NONE);/*XCHG AX,DI*/ CPU80386_internal_XCHG32(&REG_EDI,&REG_EAX,1); /*XCHG DI,AX*/ }
void CPU80386_OP98() {modrm_generateInstructionTEXT("CWDE",0,0,PARAM_NONE);/*CBW : sign extend AX to EAX*/ CPU80386_internal_CWDE();/*CWDE : sign extend AX to EAX (80386+)*/ }
void CPU80386_OP99() {modrm_generateInstructionTEXT("CWQ",0,0,PARAM_NONE);/*CDQ : sign extend EAX to EDX::EAX*/ CPU80386_internal_CDQ();/*CDQ : sign extend EAX to EDX::EAX (80386+)*/ }
void CPU80386_OP9A() {/*CALL Ap*/ INLINEREGISTER uint_64 segmentoffset = imm64; debugger_setcommand("CALL %04x:%04x", (segmentoffset>>32), (segmentoffset&0xFFFF)); CPU80386_CALLF((segmentoffset>>16)&0xFFFF,segmentoffset&0xFFFF); CPU[activeCPU].cycles_OP = 28; /* Intersegment direct */ }
//Different addressing modes affect us!
void CPU80386_OPA0_8() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVB AL,[%s:%08X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AL,[imm32]*/ if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,1,getCPL())) return; CPU80386_internal_MOV8(&REG_AL,MMU_rb(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,0),1);/*MOV AL,[imm32]*/ }
void CPU80386_OPA1_8() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVW AX,[%s:%08X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AX,[imm32]*/  if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,1,getCPL())) return; if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm+1,1,getCPL())) return; CPU80386_internal_MOV16(&REG_AX,MMU_rw(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,0),1);/*MOV AX,[imm32]*/ }
void CPU80386_OPA2_8() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVB [%s:%08X],AL",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm32],AL*/ custommem = 1; customoffset = theimm; CPU80386_internal_MOV8(NULL,REG_AL,1);/*MOV [imm32],AL*/ custommem = 0; }
void CPU80386_OPA3_8() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVW [%s:%08X],AX",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm32], AX*/ custommem = 1; customoffset = theimm; CPU80386_internal_MOV16(NULL,REG_AX,1);/*MOV [imm32], AX*/ custommem = 0; }
//16/32 depending on address size!
void CPU80386_OPA0_16() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVB AL,[%s:%08X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AL,[imm32]*/ if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,1,getCPL())) return; CPU80386_internal_MOV8(&REG_AL,MMU_rb(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,0),1);/*MOV AL,[imm32]*/ }
void CPU80386_OPA1_16() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVW AX,[%s:%08X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AX,[imm32]*/  if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,1,getCPL())) return; if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm+1,1,getCPL())) return; CPU80386_internal_MOV16(&REG_AX,MMU_rw(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,0),1);/*MOV AX,[imm32]*/ }
void CPU80386_OPA2_16() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVB [%s:%08X],AL",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm32],AL*/ custommem = 1; customoffset = theimm; CPU80386_internal_MOV8(NULL,REG_AL,1);/*MOV [imm32],AL*/ custommem = 0; }
void CPU80386_OPA3_16() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVW [%s:%08X],AX",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm32], AX*/ custommem = 1; customoffset = theimm; CPU80386_internal_MOV32(NULL,REG_EAX,1);/*MOV [imm32], AX*/ custommem = 0; }
void CPU80386_OPA0_32() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVB AL,[%s:%08X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AL,[imm32]*/ if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,1,getCPL())) return; CPU80386_internal_MOV8(&REG_AL,MMU_rb(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,0),1);/*MOV AL,[imm32]*/ }
void CPU80386_OPA1_32() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVW AX,[%s:%08X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AX,[imm32]*/  if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,1,getCPL())) return; if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm+1,1,getCPL())) return; CPU80386_internal_MOV32(&REG_EAX,MMU_rdw(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,0),1);/*MOV AX,[imm32]*/ }
void CPU80386_OPA2_32() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVB [%s:%08X],AL",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm32],AL*/ custommem = 1; customoffset = theimm; CPU80386_internal_MOV8(NULL,REG_AL,1);/*MOV [imm32],AL*/ custommem = 0; }
void CPU80386_OPA3_32() {INLINEREGISTER uint_32 theimm = imm32; debugger_setcommand("MOVW [%s:%08X],AX",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm32], AX*/ custommem = 1; customoffset = theimm; CPU80386_internal_MOV32(NULL,REG_EAX,1);/*MOV [imm32], AX*/ custommem = 0; }
//Normal instruction again!
void CPU80386_OPA5() {modrm_generateInstructionTEXT("MOVSD",0,0,PARAM_NONE);/*MOVSW*/ CPU80386_internal_MOVSD();/*MOVSD*/ }
void CPU80386_OPA7() {debugger_setcommand("CMPSD [%s:ESI],[ES:EDI]",CPU_textsegment(CPU_SEGMENT_DS));/*CMPSD*/ CPU80386_internal_CMPSD();/*CMPSD*/ }
void CPU80386_OPA9() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("TESTD EAX,",0,theimm,PARAM_IMM32);/*TEST EAX,imm32*/ CPU80386_internal_TEST32(REG_EAX,theimm,1);/*TEST EAX,imm32*/ }
void CPU80386_OPAB() {modrm_generateInstructionTEXT("STOSD",0,0,PARAM_NONE);/*STOSW*/ CPU80386_internal_STOSD();/*STOSW*/ }
void CPU80386_OPAD() {modrm_generateInstructionTEXT("LODSD",0,0,PARAM_NONE);/*LODSW*/ CPU80386_internal_LODSD();/*LODSW*/ }
void CPU80386_OPAF() {modrm_generateInstructionTEXT("SCASD",0,0,PARAM_NONE);/*SCASW*/ CPU80386_internal_SCASD();/*SCASW*/ }
void CPU80386_OPB8() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("MOVW EAX,",0,theimm,PARAM_IMM32);/*MOV AX,imm32*/ CPU80386_internal_MOV32(&REG_EAX,theimm,4);/*MOV AX,imm32*/ }
void CPU80386_OPB9() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("MOVW ECX,",0,theimm,PARAM_IMM32);/*MOV CX,imm32*/ CPU80386_internal_MOV32(&REG_ECX,theimm,4);/*MOV CX,imm32*/ }
void CPU80386_OPBA() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("MOVW EDX,",0,theimm,PARAM_IMM32);/*MOV DX,imm32*/ CPU80386_internal_MOV32(&REG_EDX,theimm,4);/*MOV DX,imm32*/ }
void CPU80386_OPBB() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("MOVW EBX,",0,theimm,PARAM_IMM32);/*MOV BX,imm32*/ CPU80386_internal_MOV32(&REG_EBX,theimm,4);/*MOV BX,imm32*/ }
void CPU80386_OPBC() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("MOVW ESP,",0,theimm,PARAM_IMM32);/*MOV SP,imm32*/ CPU80386_internal_MOV32(&REG_ESP,theimm,4);/*MOV SP,imm32*/ }
void CPU80386_OPBD() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("MOVW EBP,",0,theimm,PARAM_IMM32);/*MOV BP,imm32*/ CPU80386_internal_MOV32(&REG_EBP,theimm,4);/*MOV BP,imm32*/ }
void CPU80386_OPBE() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("MOVW ESI,",0,theimm,PARAM_IMM32);/*MOV SI,imm32*/ CPU80386_internal_MOV32(&REG_ESI,theimm,4);/*MOV SI,imm32*/ }
void CPU80386_OPBF() {INLINEREGISTER uint_32 theimm = imm32; modrm_generateInstructionTEXT("MOVW EDI,",0,theimm,PARAM_IMM32);/*MOV DI,imm32*/ CPU80386_internal_MOV32(&REG_EDI,theimm,4);/*MOV DI,imm32*/ }
void CPU80386_OPC2() {INLINEREGISTER int_32 popbytes = imm32();/*RET imm32 (Near return to calling proc and POP imm32 bytes)*/ modrm_generateInstructionTEXT("RET",0,popbytes,PARAM_IMM8); /*RET imm32 (Near return to calling proc and POP imm32 bytes)*/ CPU80386_internal_RET(popbytes,1); }
void CPU80386_OPC3() {modrm_generateInstructionTEXT("RET",0,0,PARAM_NONE);/*RET (Near return to calling proc)*/ /*RET (Near return to calling proc)*/ CPU80386_internal_RET(0,0); }
void CPU80386_OPC4() /*LES modr/m*/ {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("LES",0,0,PARAM_MODRM12); CPU80386_internal_LXS(CPU_SEGMENT_ES); /*Load new ES!*/ }
void CPU80386_OPC5() /*LDS modr/m*/ {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("LDS",0,0,PARAM_MODRM12); CPU80386_internal_LXS(CPU_SEGMENT_DS); /*Load new DS!*/ }
void CPU80386_OPC7() {uint_32 val = imm32; modrm_debugger32(&params,0,1); debugger_setcommand("MOVW %s,%08x",modrm_param2,val); if (modrm_check32(&params,1,0)) return; modrm_write32(&params,1,val); if (MODRM_EA(params)) { CPU[activeCPU].cycles_OP = 10+MODRM_EA(params); /* Imm->Mem */ } else CPU[activeCPU].cycles_OP = 4; /* Imm->Reg */ }
void CPU80386_OPCA() {INLINEREGISTER int_32 popbytes = imm32();/*RETF imm32 (Far return to calling proc and pop imm32 bytes)*/ modrm_generateInstructionTEXT("RETF",0,popbytes,PARAM_IMM32); /*RETF imm32 (Far return to calling proc and pop imm32 bytes)*/ CPU80386_internal_RETF(popbytes,1); }
void CPU80386_OPCB() {modrm_generateInstructionTEXT("RETF",0,0,PARAM_NONE); /*RETF (Far return to calling proc)*/ CPU80386_internal_RETF(0,0); }
void CPU80386_OPCC() {modrm_generateInstructionTEXT("INT 3",0,0,PARAM_NONE); /*INT 3*/ CPU80386_INTERNAL_int(EXCEPTION_CPUBREAKPOINT,1);/*INT 3*/ }
void CPU80386_OPCD() {INLINEREGISTER byte theimm = immb; INTdebugger80386(); modrm_generateInstructionTEXT("INT",0,theimm,PARAM_IMM8);/*INT imm8*/ CPU80386_INTERNAL_int(theimm,0);/*INT imm8*/ }
void CPU80386_OPCE() {modrm_generateInstructionTEXT("INTO",0,0,PARAM_NONE);/*INTO*/ CPU80386_internal_INTO();/*INTO*/ }
void CPU80386_OPCF() {modrm_generateInstructionTEXT("IRET",0,0,PARAM_NONE);/*IRET*/ CPU80386_IRET();/*IRET : also restore interrupt flag!*/ }
void CPU80386_OPD4() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("AAM",0,theimm,PARAM_IMM8);/*AAM*/ CPU80386_internal_AAM(theimm);/*AAM*/ }
void CPU80386_OPD5() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("AAD",0,theimm,PARAM_IMM8);/*AAD*/ CPU80386_internal_AAD(theimm);/*AAD*/ }
void CPU80386_OPD6(){debugger_setcommand("SALC"); REG_AL=FLAG_CF?0xFF:0x00; CPU[activeCPU].cycles_OP = 2;} //Special case on the 80386: SALC!
void CPU80386_OPD7(){CPU80386_internal_XLAT();}
void CPU80386_OPE0(){INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOPNZ",0, ((REG_EIP+rel8)&0xFFFF),PARAM_IMM32); if ((--REG_ECX) && (!FLAG_ZF)){REG_EIP += rel8; CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 19; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 5; /* Branch not taken */}}
void CPU80386_OPE1(){INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOPZ",0, ((REG_EIP+rel8)&0xFFFF),PARAM_IMM32);if ((--REG_ECX) && (FLAG_ZF)){REG_EIP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 18; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 6; /* Branch not taken */}}
void CPU80386_OPE2(){INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOP", 0,((REG_EIP+rel8)&0xFFFF),PARAM_IMM32);if (--REG_ECX){REG_EIP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 17; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 5; /* Branch not taken */}}
void CPU80386_OPE3(){INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("JCXZ",0,((REG_EIP+rel8)&0xFFFF),PARAM_IMM16); if (!REG_ECX){REG_EIP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/CPU[activeCPU].cycles_OP = 18; didJump = 1; /* Branch taken */}else { CPU[activeCPU].cycles_OP = 6; /* Branch not taken */}}
void CPU80386_OPE5(){INLINEREGISTER byte theimm = imm8();modrm_generateInstructionTEXT("IN EAX,",0,theimm,PARAM_IMM8); CPU_PORT_IN_D(theimm,&REG_EAX); CPU[activeCPU].cycles_OP = 10; /*Timings!*/ }
void CPU80386_OPE7(){INLINEREGISTER byte theimm = imm8(); debugger_setcommand("OUT %02X,EAX",theimm); CPU_PORT_OUT_D(theimm,REG_EAX); CPU[activeCPU].cycles_OP = 10; /*Timings!*/ }
void CPU80386_OPE8(){INLINEREGISTER int_32 reloffset = imm32(); modrm_generateInstructionTEXT("CALL",0,((REG_EIP + reloffset)&0xFFFF),PARAM_IMM32); if (checkStackAccess(1,1,1)) return; CPU_PUSH32(&REG_EIP); REG_EIP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 19; /* Intrasegment direct */}
void CPU80386_OPE9(){INLINEREGISTER sword reloffset = imm32(); modrm_generateInstructionTEXT("JMP",0,((REG_EIP + reloffset)&0xFFFF),PARAM_IMM32); REG_IP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 15; /* Intrasegment direct */}
void CPU80386_OPEA(){INLINEREGISTER uint_64 segmentoffset = imm64; debugger_setcommand("JMP %04X:%04X", (segmentoffset>>32), (segmentoffset&0xFFFF)); destEIP = (segmentoffset&0xFFFF); segmentWritten(CPU_SEGMENT_CS, (word)(segmentoffset>>32), 1); CPU_flushPIQ(); CPU[activeCPU].cycles_OP = 15; /* Intersegment direct */}
void CPU80386_OPEB(){INLINEREGISTER signed char reloffset = imm8(); modrm_generateInstructionTEXT("JMP",0,((REG_EIP + reloffset)&0xFFFF),PARAM_IMM32); REG_EIP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 15; /* Intrasegment direct short */}
void CPU80386_OPED(){modrm_generateInstructionTEXT("IN EAX,DX",0,0,PARAM_NONE); CPU_PORT_IN_D(REG_DX,&REG_EAX); CPU[activeCPU].cycles_OP = 8; /*Timings!*/ }
void CPU80386_OPEF(){modrm_generateInstructionTEXT("OUT DX,EAX",0,0,PARAM_NONE); CPU_PORT_OUT_D(REG_DX,REG_EAX); CPU[activeCPU].cycles_OP = 8; /*Timings!*/ }
void CPU80386_OPF1(){modrm_generateInstructionTEXT("<Undefined and reserved opcode, no error>",0,0,PARAM_NONE);}

/*

NOW COME THE GRP1-5 OPCODES:

*/

//GRP1

/*

DEBUG: REALLY SUPPOSED TO HANDLE OP80-83 HERE?

*/

void CPU80386_OP81() //GRP1 Ev,Iv
{
	INLINEREGISTER uint_32 imm = imm32;
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params,1,0);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDD %s,%04X",&modrm_param1,imm); //ADD Eb, Ib
		}
		CPU80386_internal_ADD32(modrm_addr32(&params,1,0),imm,3); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORD %s,%04X",&modrm_param1,imm); //OR Eb, Ib
		}
		CPU80386_internal_OR32(modrm_addr32(&params,1,0),imm,3); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCD %s,%04X",&modrm_param1,imm); //ADC Eb, Ib
		}
		CPU80386_internal_ADC32(modrm_addr32(&params,1,0),imm,3); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBD %s,%04X",&modrm_param1,imm); //SBB Eb, Ib
		}
		CPU80386_internal_SBB32(modrm_addr32(&params,1,0),imm,3); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDD %s,%04X",&modrm_param1,imm); //AND Eb, Ib
		}
		CPU80386_internal_AND32(modrm_addr32(&params,1,0),imm,3); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBD %s,%04X",&modrm_param1,imm); //SUB Eb, Ib
		}
		CPU80386_internal_SUB32(modrm_addr32(&params,1,0),imm,3); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORD %s,%04X",&modrm_param1,imm); //XOR Eb, Ib
		}
		CPU80386_internal_XOR32(modrm_addr32(&params,1,0),imm,3); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPD %s,%04X",&modrm_param1,imm); //CMP Eb, Ib
		}
		if (modrm_check32(&params,1,1)) return; //Abort when needed!
		CMP_dw(modrm_read32(&params,1),imm,3); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU80386_OP83() //GRP1 Ev,Ib
{
	INLINEREGISTER uint_32 imm;
	imm = immb;
	if (imm&0x80) imm |= 0xFFFFFF00; //Sign extend!
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params,1,0);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDW %s,%04X",&modrm_param1,imm); //ADD Eb, Ib
		}
		CPU80386_internal_ADD32(modrm_addr32(&params,1,0),imm,3); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORW %s,%04X",&modrm_param1,imm); //OR Eb, Ib
		}
		CPU80386_internal_OR32(modrm_addr32(&params,1,0),imm,3); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCW %s,%04X",&modrm_param1,imm); //ADC Eb, Ib
		}
		CPU80386_internal_ADC32(modrm_addr32(&params,1,0),imm,3); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBW %s,%04X",&modrm_param1,imm); //SBB Eb, Ib
		}
		CPU80386_internal_SBB32(modrm_addr32(&params,1,0),imm,3); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDW %s,%04X",&modrm_param1,imm); //AND Eb, Ib
		}
		CPU80386_internal_AND32(modrm_addr32(&params,1,0),imm,3); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBW %s,%04X",&modrm_param1,imm); //SUB Eb, Ib
		}
		CPU80386_internal_SUB32(modrm_addr32(&params,1,0),imm,3); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORW %s,%04X",&modrm_param1,imm); //XOR Eb, Ib
		}
		CPU80386_internal_XOR32(modrm_addr32(&params,1,0),imm,3); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPW %s,%04X",&modrm_param1,imm); //CMP Eb, Ib
		}
		if (modrm_check32(&params,1,1)) return; //Abort when needed!
		CMP_dw(modrm_read32(&params,1),imm,3); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU80386_OP8F() //Undocumented GRP opcode 8F r/m32
{
	if (cpudebugger)
	{
		modrm_debugger32(&params,0,1);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //POP
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT("POPD",32,0,PARAM_MODRM2); //POPW Ew
		}
		if (checkStackAccess(1,0,1)) return; //Abort when needed!
		modrm_write32(&params,1,CPU_POP32()); //POP r/m32
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 17+MODRM_EA(params); /*Pop Mem!*/
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/
		}
		break;
	default: //Unknown opcode or special?
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("Unknown opcode: 8F /%i",MODRM_REG(params.modrm)); //Error!
		}
		CPU_unkOP(); //Execute the unknown opcode exception handler, if any!
		break;
	}
}

void CPU80386_OPD1() //GRP2 Ev,1
{
	thereg = MODRM_REG(params.modrm);
	if (modrm_check32(&params,1,1)) return; //Abort when needed!
	if (modrm_check32(&params,1,0)) return; //Abort when needed!
	oper1d = modrm_read32(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLD %s,1",&modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORD %s,1",&modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLD %s,1",&modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRD %s,1",&modrm_param2);
			break;
		case 4: //SHL
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHLD %s,1",&modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRD %s,1",&modrm_param2);
			break;
		case 7: //SAR
			debugger_setcommand("SARD %s,1",&modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write32(&params,1,op386_grp2_32(1,0));
}

void CPU80386_OPD3() //GRP2 Ev,CL
{
	thereg = MODRM_REG(params.modrm);
	if (modrm_check32(&params,1,1)) return; //Abort when needed!
	if (modrm_check32(&params,1,0)) return; //Abort when needed!
	oper1d = modrm_read32(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLD %s,CL",&modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORD %s,CL",&modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLD %s,CL",&modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRD %s,CL",&modrm_param2);
			break;
		case 4: //SHL
			debugger_setcommand("SHLD %s,CL",&modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRD %s,CL",&modrm_param2);
			break;
		case 6: //--- Unknown Opcode! ---
			debugger_setcommand("<UNKNOWN MODR/M: GRP2(w) /6, CL>");
			break;
		case 7: //SAR
			debugger_setcommand("SARD %s,CL",&modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write32(&params,1,op386_grp2_32(REG_CL,1));
}

void CPU80386_OPF7() //GRP3b Ev
{
	thereg = MODRM_REG(params.modrm);
	if (modrm_check32(&params,1,1)) return; //Abort when needed!
	if ((thereg>1) && (thereg<4)) //NOT/NEG?
	{
		if (modrm_check32(&params,1,0)) return; //Abort when needed!
	}
	oper1d = modrm_read32(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params,0,1); //Get src!
		switch (thereg) //What function?
		{
		case 0: //TEST modrm32, imm32
		case 1: //--- Undocumented opcode, same as above!
			debugger_setcommand("TESTD %s,%02x",&modrm_param2,immw);
			break;
		case 2: //NOT
			modrm_generateInstructionTEXT("NOTD",32,0,PARAM_MODRM2);
			break;
		case 3: //NEG
			modrm_generateInstructionTEXT("NEGD",32,0,PARAM_MODRM2);
			break;
		case 4: //MUL
			modrm_generateInstructionTEXT("MULD",32,0,PARAM_MODRM2);
			break;
		case 5: //IMUL
			modrm_generateInstructionTEXT("IMULD",32,0,PARAM_MODRM2);
			break;
		case 6: //DIV
			modrm_generateInstructionTEXT("DIVD",32,0,PARAM_MODRM2);
			break;
		case 7: //IDIV
			modrm_generateInstructionTEXT("IDIVD",32,0,PARAM_MODRM2);
			break;
		default:
			break;
		}
	}
	op386_grp3_32();
	if ((thereg>1) && (thereg<4)) //NOT/NEG?
	{
		modrm_write32(&params,1,res32);
	}
}
//All OK up till here.

/*

DEBUG: REALLY SUPPOSED TO HANDLE HERE?

*/

void CPU80386_OPFF() //GRP5 Ev
{
	thereg = MODRM_REG(params.modrm);
	if (modrm_check32(&params,1,1)) return; //Abort when needed!
	oper1d = modrm_read32(&params,1);
	ea = modrm_offset32(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger32(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //INC modrm8
			modrm_generateInstructionTEXT("INCD",32,0,PARAM_MODRM2); //INC!
			break;
		case 1: //DEC modrm8
			modrm_generateInstructionTEXT("DECD",32,0,PARAM_MODRM2); //DEC!
			break;
		case 2: //CALL
			modrm_generateInstructionTEXT("CALL",32,0,PARAM_MODRM2); //CALL!
			break;
		case 3: //CALL Mp (Read address word and jump there)
			modrm_generateInstructionTEXT("CALL",32,0,PARAM_MODRM2); //Jump to the address pointed here!
			//debugger_setcommand("CALL %04X:%04X",MMU_rw(CPU_SEGMENT_CS,REG_CS,ea,0),MMU_rw(CPU_SEGMENT_CS,REG_CS,ea+2,0)); //Based on CALL Ap
			break;
		case 4: //JMP
			modrm_generateInstructionTEXT("JMP",32,0,PARAM_MODRM2); //JMP to the register!
			break;
		case 5: //JMP Mp
			modrm_generateInstructionTEXT("JMP",32,0,PARAM_MODRM2); //Jump to the address pointed here!
			//debugger_setcommand("JMP %04X:%04X",MMU_rw(CPU_SEGMENT_CS,REG_CS,ea,0),MMU_rw(CPU_SEGMENT_CS,REG_CS,ea+2,0)); //JMP to destination!
			break;
		case 6: //PUSH
			modrm_generateInstructionTEXT("PUSHD",32,0,PARAM_MODRM2); //PUSH!
			break;
		case 7: //---
			debugger_setcommand("<UNKNOWN Opcode: GRP5(w) /7>");
			break;
		default:
			break;
		}
	}
	op386_grp5_32();
}

/*

Special stuff for NO COprocessor (8087) present/available (default)!

*/

void unkOP_80386() //Unknown opcode on 8086?
{
	//dolog("8086","Unknown opcode on 8086: %02X",CPU[activeCPU].lastopcode); //Last read opcode!
	CPU_unkOP(); //Execute the unknown opcode exception handler, if any!
}

//Gecontroleerd: 100% OK!

//Now, the GRP opcodes!

OPTINLINE void op386_grp2_cycles(byte cnt, byte varshift)
{
	switch (varshift) //What type of shift are we using?
	{
	case 0: //Reg/Mem with 1 shift?
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 2; //Reg
		}
		break;
	case 1: //Reg/Mem with variable shift?
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 20 + MODRM_EA(params) + (cnt << 2); //Mem
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 8 + (cnt << 2); //Reg
		}
		break;
	case 2: //Reg/Mem with immediate variable shift(NEC V20/V30)?
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 20 + MODRM_EA(params) + (cnt << 2); //Mem
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 8 + (cnt << 2); //Reg
		}
		break;
	}
}

uint_32 op386_grp2_32(byte cnt, byte varshift) {
	//uint32_t d,
	INLINEREGISTER uint_32 s, shift, oldCF, msb;
	//if (cnt>0x10) return(oper1d); //NEC V20/V30+ limits shift count
	if (EMULATED_CPU >= CPU_NECV30) cnt &= 0x1F; //Clear the upper 3 bits to become a NEC V20/V30+!
	s = oper1d;
	oldCF = FLAG_CF;
	switch (thereg) {
	case 0: //ROL r/m32
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) FLAGW_CF(1); else FLAGW_CF(0);
			s = s << 1;
			s = s | FLAG_CF;
		}
		if (cnt) FLAGW_OF(FLAG_CF ^ ((s >> 15) & 1));
		break;

	case 1: //ROR r/m32
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			s = (s >> 1) | (FLAG_CF << 15);
		}
		if (cnt) FLAGW_OF((s >> 15) ^ ((s >> 14) & 1));
		break;

	case 2: //RCL r/m32
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			if (s & 0x8000) FLAGW_CF(1); else FLAGW_CF(0);
			s = s << 1;
			s = s | oldCF;
			//oldCF = ((s&0x8000)>>15)&1; //Save FLAG_CF!
			//s = (s<<1)+FLAG_CF;
			//FLAG_CF = oldCF;
		}
		if (cnt) FLAGW_OF(FLAG_CF ^ ((s >> 15) & 1));
		break;

	case 3: //RCR r/m32
		if (cnt) FLAGW_OF(((s >> 15) & 1) ^ FLAG_CF);
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			FLAGW_CF(s & 1);
			s = (s >> 1) | (oldCF << 15);
			//oldCF = s&1;
			//s = (s<<1)+(FLAG_CF<<32);
			//FLAG_CF = oldCF;
		}
		if (cnt) FLAGW_OF((s >> 15) ^ ((s >> 14) & 1));
		break;

	case 4: case 6: //SHL r/m32
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) FLAGW_CF(1); else FLAGW_CF(0);
			s = (s << 1) & 0xFFFF;
		}
		if ((cnt) && (FLAG_CF == (s >> 15))) FLAGW_OF(0); else FLAGW_OF(1);
		flag_szp32(s); break;

	case 5: //SHR r/m32
		if (cnt) FLAGW_OF((s & 0x8000) ? 1 : 0);
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			s = s >> 1;
		}
		flag_szp32(s); break;

	case 7: //SAR r/m32
		if (cnt) FLAGW_OF(0);
		msb = s & 0x8000; //Read the MSB!
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			s = (s >> 1) | msb;
		}
		byte tempSF;
		tempSF = FLAG_SF; //Save the SF!
		flag_szp32(s);
		if (!cnt) //Nothing done?
		{
			FLAGW_SF(tempSF); //We don't update when nothing's done!
		}
		break;
	}
	op386_grp2_cycles(cnt, varshift|4);
	return(s & 0xFFFF);
}

byte tmps,tmpp; //Sign/parity backup!

extern byte CPU_databussize; //Current data bus size!

byte tempAL;
word tempAX;
uint_32 tempDXAX;

OPTINLINE void op_div32(uint_64 valdiv, uint_32 divisor) {
	//word v1, v2;
	if (!divisor) { CPU_exDIV0(); return; }
	if ((valdiv / (uint_64)divisor) > 0xFFFFFFFF) { CPU_exDIV0(); return; }
	REG_EDX = (uint_32)((uint_64)valdiv % (uint_64)divisor);
	REG_EAX = (uint_32)(valdiv / (uint_64)divisor);
}

OPTINLINE void op_idiv32(uint_64 valdiv, uint_32 divisor) {
	//uint32_t v1, v2,

	if (!divisor) { CPU_exDIV0(); return; }
	/*
	uint_32 d1, d2, s1, s2;
	int sign;
	s1 = valdiv;
	s2 = divisor;
	s2 = (s2 & 0x8000) ? (s2 | 0xffff0000) : s2;
	sign = (((s1 ^ s2) & 0x80000000) != 0);
	s1 = (s1 < 0x80000000) ? s1 : ((~s1 + 1) & 0xffffffff);
	s2 = (s2 < 0x80000000) ? s2 : ((~s2 + 1) & 0xffffffff);
	d1 = s1 / s2;
	d2 = s1 % s2;
	if (d1 & 0xFFFF0000) { CPU_exDIV0(); return; }
	if (sign) {
	d1 = (~d1 + 1) & 0xffff;
	d2 = (~d2 + 1) & 0xffff;
	}
	REG_AX = d1;
	REG_DX = d2;
	*/

	//Same, but with normal instructions!
	union
	{
		uint_64 valdivw;
		int_64 valdivs;
	} dataw1, //For loading the signed value of the registers!
		dataw2; //For performing calculations!

	union
	{
		uint_32 divisorb;
		int_32 divisors;
	} datab1, datab2; //For converting the data to signed values!

	dataw1.valdivw = valdiv; //Load word!
	datab1.divisorb = divisor; //Load divisor!

	dataw2.valdivs = dataw1.valdivs; //Set and...
	dataw2.valdivs /= datab1.divisors; //... Divide!

	datab2.divisors = (sword)dataw2.valdivs; //Try to load the signed result!
	if ((int_32)dataw2.valdivw != (int_32)datab2.divisors) { CPU_exDIV0(); return; } //Overflow (data loss)!

	REG_EAX = datab2.divisorb; //Divided!
	dataw2.valdivs = dataw1.valdivs; //Reload and...
	dataw2.valdivs %= datab1.divisors; //... Modulo!
	datab1.divisors = (sword)dataw2.valdivs; //Convert to 8-bit!
	REG_EDX = datab1.divisorb; //Move rest into result!

							  //if (valdiv > 0x7FFFFFFF) v1 = valdiv - 0xFFFFFFFF - 1; else v1 = valdiv;
							  //if (divisor > 32767) v2 = divisor - 65536; else v2 = divisor;
							  //if ((v1/v2) > 65535) { CPU80386_INTERNAL_int(0); return; }
							  //temp3 = (v1/v2) & 65535;
							  //regs.wordregs[regax] = temp3;
							  //temp3 = (v1%v2) & 65535;
							  //regs.wordregs[regdx] = temp3;
}

void op386_grp3_32() {
	//uint32_t d1, d2, s1, s2, sign;
	//word d, s;
	//oper1d = signext(oper1b); oper2d = signext(oper2b);
	//sprintf(msg, "  oper1d: %04X    oper2d: %04X\n", oper1d, oper2d); print(msg);
	switch (thereg) {
	case 0: case 1: //TEST
		CPU80386_internal_TEST32(oper1d, immw, 3);
		break;
	case 2: //NOT
		res32 = ~oper1d;
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 16 + MODRM_EA(params); //Mem!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg!
		}
		break;
	case 3: //NEG
		res32 = (~oper1d) + 1;
		flag_sub32(0, oper1d);
		if (res32) FLAGW_CF(1); else FLAGW_CF(0);
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 16 + MODRM_EA(params); //Mem!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg!
		}
		break;
	case 4: //MULW
		tempAX = REG_AX; //Save a backup for calculating cycles!
		temp1.val64 = (uint32_t)oper1d * (uint32_t)REG_AX;
		REG_AX = temp1.val32;
		REG_DX = temp1.val32high;
		if (REG_DX) { FLAGW_CF(1); FLAGW_OF(1); }
		else { FLAGW_CF(0); FLAGW_OF(0); }
		if ((EMULATED_CPU==CPU_8086) && temp1.val32) FLAGW_ZF(0); //8086/8088 clears the Zero flag when not zero only.
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 124 + MODRM_EA(params); //Mem max!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 118; //Reg!
		}
		if (NumberOfSetBits(tempAX)>1) //More than 1 bit set?
		{
			CPU[activeCPU].cycles_OP += NumberOfSetBits(tempAX) - 1; //1 cycle for all bits more than 1 bit set!
		}
		break;
	case 5: //IMULW
		temp1.val32 = REG_AX;
		temp2.val32 = oper1d;
		//Sign extend!
		if (temp1.val32 & 0x80000000) temp1.val64 |= 0xFFFFFFFF00000000ULL;
		if (temp2.val32 & 0x80000000) temp2.val64 |= 0xFFFFFFFF00000000ULL;
		temp3.val64s = temp1.val64s; //Load and...
		temp3.val64s *= temp2.val64s; //Signed multiplication!
		REG_EAX = temp3.val32; //into register ax
		REG_EDX = temp3.val32high; //into register dx
		FLAGW_OF(((int_32)temp3.val64s != temp3.val64s)?1:0); //Overflow occurred?
		FLAGW_CF(FLAG_OF); //Same!
		FLAGW_SF((REG_DX>>15)&1); //Sign flag is affected!
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 128 + MODRM_EA(params); //Mem max!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 134; //Reg max!
		}
		break;
	case 6: //DIV
		op_div32(((uint_64)REG_EDX << 32) | REG_EAX, oper1d);
		break;
	case 7: //IDIV
		op_idiv32(((uint_64)REG_EDX << 32) | REG_EAX, oper1d); break;
	}
}

void op386_grp5_32() {
	MODRM_PTR info; //To contain the info!
	INLINEREGISTER byte tempCF;
	word destCS;
	switch (thereg) {
	case 0: //INC Ev
		if (modrm_check32(&params,1,0)) return; //Abort when needed!
		oper2d = 1;
		tempCF = FLAG_CF;
		op_add32();
		FLAGW_CF(tempCF);
		modrm_write32(&params, 1, res32);
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 2; //Reg
		}
		break;
	case 1: //DEC Ev
		if (modrm_check32(&params,1,0)) return; //Abort when needed!
		oper2d = 1;
		tempCF = FLAG_CF;
		op_sub32();
		FLAGW_CF(tempCF);
		modrm_write32(&params, 1, res32);
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 2; //Reg
		}
		break;
	case 2: //CALL Ev
		if (checkStackAccess(1,1,1)) return; //Abort when needed!
		CPU_PUSH32(&REG_EIP);
		REG_IP = oper1d;
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 21 + MODRM_EA(params); /* Intrasegment indirect through memory */
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 16; /* Intrasegment indirect through register */
		}
		CPU_flushPIQ(); //We're jumping to another address!
		break;
	case 3: //CALL Mp
		modrm_decode32(&params, &info, 1); //Get data!

		modrm_addoffset = 0; //First IP!
		if (modrm_check32(&params,1,1)) return; //Abort when needed!
		modrm_addoffset = 2; //Then destination CS!
		if (modrm_check16(&params,1,1)) return; //Abort when needed!

		modrm_addoffset = 0; //First IP!
		destEIP = modrm_read32(&params,1); //Get destination IP!
		CPUPROT1
		modrm_addoffset = 2; //Then destination CS!
		destCS = modrm_read16(&params,1); //Get destination CS!
		CPUPROT1
		modrm_addoffset = 0;
		CPU80386_CALLF(destCS,destEIP); //Call the destination address!
		CPUPROT1
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 37 + MODRM_EA(params); /* Intersegment indirect */
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 28; /* Intersegment direct */
		}
		CPUPROT2
		CPUPROT2
		CPUPROT2
		break;
	case 4: //JMP Ev
		REG_IP = oper1d;
		CPU_flushPIQ(); //We're jumping to another address!
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 18 + MODRM_EA(params); /* Intrasegment indirect through memory */
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 11; /* Intrasegment indirect through register */
		}
		break;
	case 5: //JMP Mp
		modrm_decode32(&params, &info, 1); //Get data!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset,1,getCPL())) return; //Abort on fault!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset+1,1,getCPL())) return; //Abort on fault!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset+2,1,getCPL())) return; //Abort on fault!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset+3,1,getCPL())) return; //Abort on fault!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset+4,1,getCPL())) return; //Abort on fault!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset+5,1,getCPL())) return; //Abort on fault!

		destEIP = MMU_rw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset, 0);
		CPUPROT1
		destCS = MMU_rw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset + 2, 0);
		CPUPROT1
		segmentWritten(CPU_SEGMENT_CS, destCS, 1);
		CPU_flushPIQ(); //We're jumping to another address!
		CPUPROT1
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 24 + MODRM_EA(params); /* Intersegment indirect through memory */
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 11; /* Intersegment indirect through register */
		}
		CPUPROT2
		CPUPROT2
		CPUPROT2
		break;
	case 6: //PUSH Ev
		if (checkStackAccess(1,1,1)) return; //Abort on fault!
		CPU_PUSH32(&oper1d); break;
		CPUPROT1
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 16+MODRM_EA(params); /*Push Mem!*/
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/
		}
		CPUPROT2
		break;
	default: //Unknown OPcode?
		CPU_unkOP(); //Execute the unknown opcode exception handler, if any!
		break;
	}
}
