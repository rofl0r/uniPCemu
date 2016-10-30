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
byte oper1b, oper2b; //Byte variants!
word oper1, oper2; //Word variants!
byte res8; //Result 8-bit!
word res16; //Result 16-bit!
byte thereg; //For function number!
uint_32 ea; //From RM OFfset (GRP5 Opcodes only!)
byte tempCF2;

VAL32Splitter temp1, temp2, temp3, temp4, temp5; //All temporary values!
uint_32 temp32, tempaddr32; //Defined in opcodes_8086.c

extern byte debuggerINT; //Interrupt special trigger?

OPTINLINE void INTdebugger8086() //Special INTerrupt debugger!
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
OPTINLINE char *getLEAtext(MODRM_PARAMS *theparams)
{
	modrm_lea16_text(theparams,1,&LEAtext[0]);    //Help function for LEA instruction!
	return &LEAtext[0];
}

/*

Start of help for opcode processing

*/

extern byte CPU_databussize; //0=16/32-bit bus! 1=8-bit bus when possible (8088/80188)!
uint_32 wordaddress; //Word address used during memory access!
OPTINLINE void CPU_addWordMemoryTiming()
{
	if (EMULATED_CPU==CPU_8086) //808(6/8)?
	{
		if (CPU_databussize) //8088?
		{
			CPU[activeCPU].cycles_OP += 4; //Add 4 clocks with all 16-bit cycles on 8086!
		}
		else //8086?
		{
			if (wordaddress&1) //Odd address?
			{
				CPU[activeCPU].cycles_OP += 4; //Add 4 clocks with odd cycles on 8086!
			}
		}
	}
}

OPTINLINE void CPU8086_software_int(byte interrupt, int_64 errorcode) //See int, but for hardware interrupts (IRQs)!
{
	call_soft_inthandler(interrupt,errorcode); //Save adress to stack (We're going soft int!)!
}

OPTINLINE void CPU8086_int(byte interrupt, byte type3) //Software interrupt from us(internal call)!
{
	CPUPROT1
		CPU8086_software_int(interrupt,-1);
	CPUPROT2
	if (type3) //Type-3 interrupt?
		CPU[activeCPU].cycles_OP = 52; /* Type-3 interrupt */
	else //Normal interrupt?
		CPU[activeCPU].cycles_OP = 51; /* Normal interrupt */
	CPU_addWordMemoryTiming(); /*To memory?*/
	CPU_addWordMemoryTiming(); /*To memory?*/
	CPU_addWordMemoryTiming(); /*To memory?*/
	CPU_addWordMemoryTiming(); /*To memory?*/
	CPU_addWordMemoryTiming(); /*To memory?*/
}

void CPU086_int(byte interrupt) //Software interrupt (external call)!
{
	CPU8086_int(interrupt,0); //Direct call!
}

OPTINLINE void CPU8086_IRET()
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
void CPU8086_OPF0() {} //LOCK
void CPU8086_OP2E() {} //CS:
void CPU8086_OP36() {} //SS:
void CPU8086_OP3E() {} //DS:
void CPU8086_OP26() {} //ES:
void CPU8086_OPF2() {} //REPNZ
void CPU8086_OPF3() {} //REPZ
*/

/*

WE START WITH ALL HELP FUNCTIONS

*/

//First CMP instruction (for debugging) and directly related.

//CMP: Substract and set flags according (Z,S,O,C); Help functions

OPTINLINE void op_adc8() {
	res8 = oper1b + oper2b + FLAG_CF;
	flag_adc8 (oper1b, oper2b, FLAG_CF);
}

OPTINLINE void op_adc16() {
	res16 = oper1 + oper2 + FLAG_CF;
	flag_adc16 (oper1, oper2, FLAG_CF);
}

OPTINLINE void op_add8() {
	res8 = oper1b + oper2b;
	flag_add8 (oper1b, oper2b);
}

OPTINLINE void op_add16() {
	res16 = oper1 + oper2;
	flag_add16 (oper1, oper2);
}

OPTINLINE void op_and8() {
	res8 = oper1b & oper2b;
	flag_log8 (res8);
}

OPTINLINE void op_and16() {
	res16 = oper1 & oper2;
	flag_log16 (res16);
}

OPTINLINE void op_or8() {
	res8 = oper1b | oper2b;
	flag_log8 (res8);
}

OPTINLINE void op_or16() {
	res16 = oper1 | oper2;
	flag_log16 (res16);
}

OPTINLINE void op_xor8() {
	res8 = oper1b ^ oper2b;
	flag_log8 (res8);
}

OPTINLINE void op_xor16() {
	res16 = oper1 ^ oper2;
	flag_log16 (res16);
}

OPTINLINE void op_sub8() {
	res8 = oper1b - oper2b;
	flag_sub8 (oper1b, oper2b);
}

OPTINLINE void op_sub16() {
	res16 = oper1 - oper2;
	flag_sub16 (oper1, oper2);
}

OPTINLINE void op_sbb8() {
	res8 = oper1b - (oper2b + FLAG_CF);
	flag_sbb8 (oper1b, oper2b, FLAG_CF);
}

OPTINLINE void op_sbb16() {
	res16 = oper1 - (oper2 + FLAG_CF);
	flag_sbb16 (oper1, oper2, FLAG_CF);
}

OPTINLINE void CMP_w(word a, word b, byte flags) //Compare instruction!
{
	CPUPROT1
	flag_sub16(a,b); //Flags only!
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
			CPU_addWordMemoryTiming();
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
			CPU_addWordMemoryTiming();
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

OPTINLINE void CMP_b(byte a, byte b, byte flags)
{
	CPUPROT1
	flag_sub8(a,b); //Flags only!
	switch (flags&7)
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
byte custommem = 0; //Used in some instructions!
uint_32 customoffset; //Offset to use!

//Help functions:
OPTINLINE void CPU8086_internal_INC16(word *reg)
{
	if (MMU_invaddr() || (reg==NULL))
	{
		return;
	}
	//Check for exceptions first!
	if (!reg) if (modrm_check16(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!reg) if (modrm_check16(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	INLINEREGISTER byte tempcf = FLAG_CF;
	oper1 = reg?*reg:modrm_read16(&params,MODRM_src0);
	oper2 = 1;
	op_add16();
	FLAGW_CF(tempcf);
	if (reg) //Register?
	{
		*reg = res16;
		CPU[activeCPU].cycles_OP = 2; //16-bit reg!
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
		CPU[activeCPU].cycles_OP = 15+MODRM_EA(params); //Mem
		CPU_addWordMemoryTiming();
		CPU_addWordMemoryTiming();
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_DEC16(word *reg)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!reg) if (modrm_check16(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!reg) if (modrm_check16(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	INLINEREGISTER byte tempcf = FLAG_CF;
	oper1 = reg?*reg:modrm_read16(&params,MODRM_src0);
	oper2 = 1;
	op_sub16();
	FLAGW_CF(tempcf);
	if (reg) //Register?
	{
		*reg = res16;
		CPU[activeCPU].cycles_OP = 2; //16-bit reg!
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
		CPU[activeCPU].cycles_OP = 15+MODRM_EA(params); //Mem
		CPU_addWordMemoryTiming();
		CPU_addWordMemoryTiming();
	}
	CPUPROT2
}

OPTINLINE void CPU8086_internal_INC8(byte *reg)
{
	if (MMU_invaddr() || (reg==NULL))
	{
		return;
	}
	if (!reg) if (modrm_check8(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!reg) if (modrm_check8(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1b = reg?*reg:modrm_read8(&params,MODRM_src0);
	oper2b = 1;
	op_add8();
	if (reg) //Register?
	{
		*reg = res8;
		CPU[activeCPU].cycles_OP = 3; //8-bit reg!
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
		CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
	}
	CPUPROT2
}
OPTINLINE void CPU8086_internal_DEC8(byte *reg)
{
	if (MMU_invaddr())
	{
		return;
	}
	CPUPROT1
	if (!reg) if (modrm_check8(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!reg) if (modrm_check8(&params,MODRM_src0,0)) return; //Abort on fault!
	oper1b = reg?*reg:modrm_read8(&params,MODRM_src0);
	oper2b = 1;
	op_sub8();
	if (reg) //Register?
	{
		*reg = res8;
		CPU[activeCPU].cycles_OP = 3; //8-bit reg!
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
		CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
	}
	CPUPROT2
}

OPTINLINE void timing_AND_OR_XOR_ADD_SUB8(byte *dest, byte flags)
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

OPTINLINE void timing_AND_OR_XOR_ADD_SUB16(word *dest, byte flags)
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
			CPU_addWordMemoryTiming();
			if (dest==NULL) CPU_addWordMemoryTiming(); //Second access for writeback!
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
			CPU_addWordMemoryTiming();
			if (dest==NULL) CPU_addWordMemoryTiming(); //Second access for writeback!
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	}
}

//For ADD
OPTINLINE void CPU8086_internal_ADD8(byte *dest, byte addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = addition;
	op_add8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB8(dest, flags);
	CPUPROT2
}
OPTINLINE void CPU8086_internal_ADD16(word *dest, word addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check16(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = addition;
	op_add16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB16(dest, flags);
	CPUPROT2
}

//For ADC
OPTINLINE void CPU8086_internal_ADC8(byte *dest, byte addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = addition;
	op_adc8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB8(dest, flags);
	CPUPROT2
}
OPTINLINE void CPU8086_internal_ADC16(word *dest, word addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check16(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = addition;
	op_adc16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB16(dest, flags);
	CPUPROT2
}


//For OR
OPTINLINE void CPU8086_internal_OR8(byte *dest, byte src, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = src;
	op_or8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB8(dest, flags);
	CPUPROT2
}
OPTINLINE void CPU8086_internal_OR16(word *dest, word src, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check16(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = src;
	op_or16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB16(dest, flags);
	CPUPROT2
}

//For AND
OPTINLINE void CPU8086_internal_AND8(byte *dest, byte src, byte flags)
{
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = src;
	op_and8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB8(dest, flags);
	CPUPROT2
}
OPTINLINE void CPU8086_internal_AND16(word *dest, word src, byte flags)
{
	if (modrm_check16(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return; //Abort on fault on write only!
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = src;
	op_and16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB16(dest, flags);
	CPUPROT2
}


//For SUB
OPTINLINE void CPU8086_internal_SUB8(byte *dest, byte addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (modrm_check8(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return; //Abort on fault on write only!
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = addition;
	op_sub8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB8(dest, flags);
	CPUPROT2
}
OPTINLINE void CPU8086_internal_SUB16(word *dest, word addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (modrm_check16(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return; //Abort on fault on write only!
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = addition;
	op_sub16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB16(dest, flags);
	CPUPROT2
}

//For SBB
OPTINLINE void CPU8086_internal_SBB8(byte *dest, byte addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = addition;
	op_sbb8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB8(dest, flags);
	CPUPROT2
}
OPTINLINE void CPU8086_internal_SBB16(word *dest, word addition, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check16(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = addition;
	op_sbb16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB16(dest, flags);
	CPUPROT2
}

//For XOR
//See AND, but XOR
OPTINLINE void CPU8086_internal_XOR8(byte *dest, byte src, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check8(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check8(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1b = dest?*dest:modrm_read8(&params,MODRM_src0);
	oper2b = src;
	op_xor8();
	if (dest) //Register?
	{
		*dest = res8;
	}
	else //Memory?
	{
		modrm_write8(&params,MODRM_src0,res8); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB8(dest, flags);
	CPUPROT2
}
OPTINLINE void CPU8086_internal_XOR16(word *dest, word src, byte flags)
{
	if (MMU_invaddr())
	{
		return;
	}
	if (!dest) if (modrm_check16(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!dest) if (modrm_check16(&params,MODRM_src0,0)) return; //Abort on fault!
	CPUPROT1
	oper1 = dest?*dest:modrm_read16(&params,MODRM_src0);
	oper2 = src;
	op_xor16();
	if (dest) //Register?
	{
		*dest = res16;
	}
	else //Memory?
	{
		modrm_write16(&params,MODRM_src0,res16,0); //Write the result to memory!
	}
	timing_AND_OR_XOR_ADD_SUB16(dest, flags);
	CPUPROT2
}

//TEST : same as AND, but discarding the result!
OPTINLINE void CPU8086_internal_TEST8(byte dest, byte src, byte flags)
{
	byte tmpdest = dest;
	CPU8086_internal_AND8(&tmpdest,src,0);
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
			CPU_addWordMemoryTiming();
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
				CPU_addWordMemoryTiming();
			}
		}
		else //Reg->Reg?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg->Reg!
		}
		break;
	}
}
OPTINLINE void CPU8086_internal_TEST16(word dest, word src, byte flags)
{
	word tmpdest = dest;
	CPU8086_internal_AND16(&tmpdest,src,0);
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
			CPU_addWordMemoryTiming();
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
				CPU_addWordMemoryTiming();
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
OPTINLINE void CPU8086_internal_MOV8(byte *dest, byte val, byte flags)
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
			CPU_addWordMemoryTiming(); //Second access for writeback!
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
OPTINLINE void CPU8086_internal_MOV16(word *dest, word val, byte flags)
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
			CPU_addWordMemoryTiming(); //To memory?
			break;
		case 2: //ModR/M Memory->Reg?
			if (MODRM_EA(params)) //Memory?
			{
				CPU[activeCPU].cycles_OP = 8 + MODRM_EA(params); //Mem->Reg!
				if (!dest) CPU_addWordMemoryTiming(); //To memory?
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
				CPU_addWordMemoryTiming();
				if (!dest) CPU_addWordMemoryTiming(); //To memory?
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
				if (!dest) CPU_addWordMemoryTiming(); //To memory?
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
			CPU_addWordMemoryTiming();
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
					if (!dest) CPU_addWordMemoryTiming(); //To memory?
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
					if (!dest) CPU_addWordMemoryTiming(); //To memory?
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
					if (!dest) CPU_addWordMemoryTiming(); //To memory?
				}
				break;
			}
		}
	}
	CPUPROT2
}

//LEA for LDS, LES
OPTINLINE word getLEA(MODRM_PARAMS *theparams)
{
	return modrm_lea16(theparams,1);
}


/*

Non-logarithmic opcodes!

*/


OPTINLINE void CPU8086_internal_DAA()
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
OPTINLINE void CPU8086_internal_DAS()
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
OPTINLINE void CPU8086_internal_AAA()
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
OPTINLINE void CPU8086_internal_AAS()
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

OPTINLINE void CPU8086_internal_CBW()
{
	CPUPROT1
	if ((REG_AL&0x80)==0x80)
	{
		REG_AH = 0xFF;
	}
	else
	{
		REG_AH = 0;
	}
	CPU[activeCPU].cycles_OP = 2; //Clock cycles!
	CPUPROT2
}
OPTINLINE void CPU8086_internal_CWD()
{
	CPUPROT1
	if ((REG_AH&0x80)==0x80)
	{
		REG_DX = 0xFFFF;
	}
	else
	{
		REG_DX = 0;
	}
	CPU[activeCPU].cycles_OP = 5; //Clock cycles!
	CPUPROT2
}

//Now the repeatable instructions!

extern byte newREP; //Are we a new repeating instruction (REP issued for a new instruction, not repeating?)

OPTINLINE void CPU8086_internal_MOVSB()
{
	INLINEREGISTER byte data;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),REG_SI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES,REG_ES,REG_DI,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	data = MMU_rb(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the data!
	CPUPROT1
	MMU_wb(CPU_SEGMENT_ES,REG_ES,REG_DI,data);
	CPUPROT1
	if (FLAG_DF)
	{
		--REG_SI;
		--REG_DI;
	}
	else
	{
		++REG_SI;
		++REG_DI;
	}
	CPUPROT2
	CPUPROT2
	if (CPU[activeCPU].repeating) //Are we a repeating instruction?
	{
		if (newREP) //Include the REP?
		{
			CPU[activeCPU].cycles_OP = 9+17; //Clock cycles including REP!
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
OPTINLINE void CPU8086_internal_MOVSW()
{
	INLINEREGISTER word data;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),REG_SI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),REG_SI+1,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES,REG_ES,REG_DI,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES,REG_ES,REG_DI+1,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	data = MMU_rw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the data!
	CPUPROT1
	MMU_ww(CPU_SEGMENT_ES,REG_ES,REG_DI,data); //Try to write the data!
	CPUPROT1
	if (FLAG_DF)
	{
		REG_SI -= 2;
		REG_DI -= 2;
	}
	else
	{
		REG_SI += 2;
		REG_DI += 2;
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
	CPU_addWordMemoryTiming(); //To memory?
	CPU_addWordMemoryTiming(); //To memory?
}
OPTINLINE void CPU8086_internal_CMPSB()
{
	INLINEREGISTER byte data1, data2;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS),REG_SI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_DI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	data1 = MMU_rb(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the first data!
	CPUPROT1
		data2 = MMU_rb(CPU_SEGMENT_ES, REG_ES, REG_DI, 0); //Try to read the second data!
	CPUPROT1
	CMP_b(data1,data2,4);
	if (FLAG_DF)
	{
		--REG_SI;
		--REG_DI;
	}
	else
	{
		++REG_SI;
		++REG_DI;
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
OPTINLINE void CPU8086_internal_CMPSW()
{
	INLINEREGISTER word data1, data2;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS),REG_SI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS),REG_SI+1,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_DI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_DI+1,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	data1 = MMU_rw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the first data!
	CPUPROT1
	data2 = MMU_rw(CPU_SEGMENT_ES, REG_ES, REG_DI, 0); //Try to read the second data!
	CPUPROT1
	CMP_w(data1,data2,4);
	if (FLAG_DF)
	{
		REG_SI -= 2;
		REG_DI -= 2;
	}
	else
	{
		REG_SI += 2;
		REG_DI += 2;
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
	CPU_addWordMemoryTiming(); //To memory?
	CPU_addWordMemoryTiming(); //To memory?
}
OPTINLINE void CPU8086_internal_STOSB()
{
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_DI,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	MMU_wb(CPU_segment_index(CPU_SEGMENT_ES),REG_ES,REG_DI,REG_AL);
	CPUPROT1
	if (FLAG_DF)
	{
		--REG_DI;
	}
	else
	{
		++REG_DI;
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
OPTINLINE void CPU8086_internal_STOSW()
{
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_DI,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_SEGMENT_ES, REG_ES, REG_DI+1,0,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	MMU_ww(CPU_segment_index(CPU_SEGMENT_ES),REG_ES,REG_DI,REG_AX);
	CPUPROT1
	if (FLAG_DF)
	{
		REG_DI -= 2;
	}
	else
	{
		REG_DI += 2;
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
	CPU_addWordMemoryTiming(); //To memory?
}
//OK so far!
OPTINLINE void CPU8086_internal_LODSB()
{
	INLINEREGISTER byte value;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	value = MMU_rb(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the result!
	CPUPROT1
	REG_AL = value;
	if (FLAG_DF)
	{
		--REG_SI;
	}
	else
	{
		++REG_SI;
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
OPTINLINE void CPU8086_internal_LODSW()
{
	INLINEREGISTER word value;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI+1,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}

	value = MMU_rw(CPU_segment_index(CPU_SEGMENT_DS), CPU_segment(CPU_SEGMENT_DS), REG_SI, 0); //Try to read the result!
	CPUPROT1
	REG_AX = value;
	if (FLAG_DF)
	{
		REG_SI -= 2;
	}
	else
	{
		REG_SI += 2;
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
	CPU_addWordMemoryTiming(); //To memory?
}
OPTINLINE void CPU8086_internal_SCASB()
{
	INLINEREGISTER byte cmp1;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_DI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	cmp1 = MMU_rb(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_DI, 0); //Try to read the data to compare!
	CPUPROT1
	CMP_b(REG_AL,cmp1,4);
	if (FLAG_DF)
	{
		--REG_DI;
	}
	else
	{
		++REG_DI;
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
OPTINLINE void CPU8086_internal_SCASW()
{
	INLINEREGISTER word cmp1;
	if (blockREP) return; //Disabled REP!
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_DI,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_DI+1,1,getCPL())) //Error accessing memory?
	{
		return; //Abort on fault!
	}
	cmp1 = MMU_rw(CPU_segment_index(CPU_SEGMENT_ES), REG_ES, REG_DI, 0); //Try to read the data to compare!
	CPUPROT1
	CMP_w(REG_AX,cmp1,4);
	if (FLAG_DF)
	{
		REG_DI -= 2;
	}
	else
	{
		REG_DI += 2;
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
	CPU_addWordMemoryTiming(); //To memory?
}

OPTINLINE void CPU8086_internal_RET(word popbytes, byte isimm)
{
	if (checkStackAccess(1,0,0)) //Error accessing stack?
	{
		return; //Abort on fault!
	}
	INLINEREGISTER word val = CPU_POP16();    //Near return
	CPUPROT1
	REG_IP = val;
	CPU_flushPIQ(); //We're jumping to another address!
	REG_SP += popbytes;
	CPUPROT2
	if (isimm)
		CPU[activeCPU].cycles_OP = 12; /* Intrasegment with constant */
	else
		CPU[activeCPU].cycles_OP = 8; /* Intrasegment */
	CPU_addWordMemoryTiming(); //To memory?
}
OPTINLINE void CPU8086_internal_RETF(word popbytes, byte isimm)
{
	if (checkStackAccess(2,0,0)) //Error accessing stack?
	{
		return; //Abort on fault!
	}
	INLINEREGISTER word val = CPU_POP16(); //Far return
	word destCS;
	CPUPROT1
	destCS = CPU_POP16(); //POP CS!
	CPUPROT1
	destEIP = val; //Load IP!
	segmentWritten(CPU_SEGMENT_CS,destCS,4); //CS changed, we're a RETF instruction!
	CPUPROT1
	REG_SP += popbytes; //Process SP!
	if (isimm)
		CPU[activeCPU].cycles_OP = 17; /* Intersegment with constant */
	else
		CPU[activeCPU].cycles_OP = 18; /* Intersegment */
	CPU_addWordMemoryTiming(); //To memory?
	CPU_addWordMemoryTiming(); //To memory?
	CPUPROT2
	CPUPROT2
	CPUPROT2
}

void external8086RETF(word popbytes)
{
	CPU8086_internal_RETF(popbytes,1); //Return immediate variant!
}

OPTINLINE void CPU8086_internal_INTO()
{
	CPUPROT1
	if (FLAG_OF)
	{
		CPU8086_int(EXCEPTION_OVERFLOW,0);
		CPU[activeCPU].cycles_OP = 53; //Timings!
	}
	else
	{
		CPU[activeCPU].cycles_OP = 4; //Timings!
	}
	CPUPROT2
	CPU_addWordMemoryTiming(); //To memory?
	CPU_addWordMemoryTiming(); //To memory?
	CPU_addWordMemoryTiming(); //To memory?
	CPU_addWordMemoryTiming(); //To memory?
	CPU_addWordMemoryTiming(); //To memory?
}

OPTINLINE void CPU8086_internal_AAM(byte data)
{
	CPUPROT1
	if (!data)
	{
		CPU_exDIV0();    //AAM
		return;
	}
	REG_AH = (((byte)SAFEDIV(REG_AL,data))&0xFF);
	REG_AL = (SAFEMOD(REG_AL,data)&0xFF);
	flag_szp16(REG_AX);
	FLAGW_OF(0); FLAGW_CF(0); FLAGW_AF(0); //Clear these!
	CPUPROT2
	CPU[activeCPU].cycles_OP = 83; //Timings!
}
OPTINLINE void CPU8086_internal_AAD(byte data)
{
	CPUPROT1
	REG_AX = ((REG_AH*data)+REG_AL);    //AAD
	REG_AH = 0;
	flag_szp8(REG_AL); //Update the flags!
	FLAGW_OF(0); FLAGW_CF(0); FLAGW_AF(0); //Clear these!
	CPUPROT2
	CPU[activeCPU].cycles_OP = 60; //Timings!
}

OPTINLINE void CPU8086_internal_XLAT()
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

OPTINLINE void CPU8086_internal_XCHG8(byte *data1, byte *data2, byte flags)
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

OPTINLINE void CPU8086_internal_XCHG16(word *data1, word *data2, byte flags)
{
	if (!data1) if (modrm_check16(&params,MODRM_src0,1)) return; //Abort on fault!
	if (!data1) if (modrm_check16(&params,MODRM_src0,0)) return; //Abort on fault!
	if (!data2) if (modrm_check16(&params,MODRM_src1,1)) return; //Abort on fault!
	if (!data2) if (modrm_check16(&params,MODRM_src1,0)) return; //Abort on fault!
	CPUPROT1
	oper1 = data1?*data1:modrm_read16(&params,MODRM_src0);
	CPUPROT1
	oper2 = data2?*data2:modrm_read16(&params,MODRM_src1);
	CPUPROT1
	//Do a simple swap!
	word temp = oper1; //Copy!
	oper1 = oper2; //We're ...
	oper2 = temp; //Swapping this!
	if (data1)
	{
		*data1 = oper1;
	}
	else
	{
		modrm_write16(&params,MODRM_src0,oper1,0);
	}
	CPUPROT1
	if (data2)
	{
		*data2 = oper2;
	}
	else
	{
		modrm_write16(&params,MODRM_src1,oper2,0);
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
			if (data1) //One memory operand?
			{
				CPU_addWordMemoryTiming(); //To memory?
				CPU_addWordMemoryTiming(); //To memory?
			}
			if (data2) //One/two memory operands?
			{
				CPU_addWordMemoryTiming(); //To memory?
				CPU_addWordMemoryTiming(); //To memory?
			}
		}
		else //Reg<->Reg?
		{
			CPU[activeCPU].cycles_OP = 4; //SegReg->Mem!
		}
		break;
	}
}

extern byte modrm_addoffset; //Add this offset to ModR/M reads!

OPTINLINE void CPU8086_internal_LXS(int segmentregister) //LDS, LES etc.
{
	modrm_addoffset = 0; //Add this to the offset to use!
	if (modrm_check16(&params,1,1)) return; //Abort on fault!
	modrm_addoffset = 2; //Add this to the offset to use!
	if (modrm_check16(&params,1,1)) return; //Abort on fault!
	if (modrm_check16(&params,0,0)) return; //Abort on fault for the used segment itself!

	CPUPROT1
	modrm_addoffset = 0; //Add this to the offset to use!
	word offset = modrm_read16(&params,1);
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
		CPU_addWordMemoryTiming(); //To memory?
		CPU_addWordMemoryTiming(); //To memory?
	}
	else //Register? Should be illegal?
	{
		CPU[activeCPU].cycles_OP = 2; /* LXS based on MOV Mem->SS, DS, ES */
	}
}

void CPU8086_CALLF(word segment, word offset)
{
	destEIP = offset;
	segmentWritten(CPU_SEGMENT_CS, segment, 2); /*CS changed, call version!*/
}

/*

NOW THE REAL OPCODES!

*/

extern byte didJump; //Did we jump this instruction?


void CPU8086_OP00() {modrm_generateInstructionTEXT("ADDB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return;  CPU8086_internal_ADD8(modrm_addr8(&params,1,0),modrm_read8(&params,0),2); }
void CPU8086_OP01() {modrm_generateInstructionTEXT("ADDW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; CPU8086_internal_ADD16(modrm_addr16(&params,1,0),modrm_read16(&params,0),2); }
void CPU8086_OP02() {modrm_generateInstructionTEXT("ADDB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; CPU8086_internal_ADD8(modrm_addr8(&params,0,0),modrm_read8(&params,1),2); }
void CPU8086_OP03() {modrm_generateInstructionTEXT("ADDW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; CPU8086_internal_ADD16(modrm_addr16(&params,0,0),modrm_read16(&params,1),2); }
void CPU8086_OP04() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("ADDB AL,",0,theimm,PARAM_IMM8); CPU8086_internal_ADD8(&REG_AL,theimm,1); }
void CPU8086_OP05() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("ADDW AX,",0,theimm,PARAM_IMM16); CPU8086_internal_ADD16(&REG_AX,theimm,1); }
void CPU8086_OP06() {modrm_generateInstructionTEXT("PUSH ES",0,0,PARAM_NONE); CPU_PUSH16(&REG_ES);/*PUSH ES*/ CPU[activeCPU].cycles_OP = 10; /*Push Segreg!*/}
void CPU8086_OP07() {modrm_generateInstructionTEXT("POP ES",0,0,PARAM_NONE); segmentWritten(CPU_SEGMENT_ES,CPU_POP16(),0); /*CS changed!*/ CPU[activeCPU].cycles_OP = 8; /*Pop Segreg!*/}
void CPU8086_OP08() {modrm_generateInstructionTEXT("ORB",8,0,PARAM_MODRM21);  if (modrm_check8(&params,0,1)) return; CPU8086_internal_OR8(modrm_addr8(&params,1,0),modrm_read8(&params,0),2); }
void CPU8086_OP09() {modrm_generateInstructionTEXT("ORW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; CPU8086_internal_OR16(modrm_addr16(&params,1,0),modrm_read16(&params,0),2); }
void CPU8086_OP0A() {modrm_generateInstructionTEXT("ORB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; CPU8086_internal_OR8(modrm_addr8(&params,0,0),modrm_read8(&params,1),2); }
void CPU8086_OP0B() {modrm_generateInstructionTEXT("ORW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; CPU8086_internal_OR16(modrm_addr16(&params,0,0),modrm_read16(&params,1),2); }
void CPU8086_OP0C() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("ORB AL,",0,theimm,PARAM_IMM8); CPU8086_internal_OR8(&REG_AL,theimm,1); }
void CPU8086_OP0D() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("ORW AX,",0,theimm,PARAM_IMM16); CPU8086_internal_OR16(&REG_AX,theimm,1); }
void CPU8086_OP0E() {modrm_generateInstructionTEXT("PUSH CS",0,0,PARAM_NONE); CPU_PUSH16(&REG_CS);/*PUSH CS*/ CPU[activeCPU].cycles_OP = 10; /*Push Segreg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP0F() /*FLAG_OF: POP CS; shouldn't be used?*/ { modrm_generateInstructionTEXT("POP CS", 0, 0, PARAM_NONE); /*Don't handle: 8086 ignores this opcode, and you won't find it there!*/ destEIP = REG_EIP; segmentWritten(CPU_SEGMENT_CS, CPU_POP16(), 0); /*POP CS!*/ CPU[activeCPU].cycles_OP = 8; /*Pop Segreg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP10() {modrm_generateInstructionTEXT("ADCB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; CPU8086_internal_ADC8(modrm_addr8(&params,1,0),modrm_read8(&params,0),2); }
void CPU8086_OP11() {modrm_generateInstructionTEXT("ADCW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; CPU8086_internal_ADC16(modrm_addr16(&params,1,0),modrm_read16(&params,0),2); }
void CPU8086_OP12() {modrm_generateInstructionTEXT("ADCB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; CPU8086_internal_ADC8(modrm_addr8(&params,0,0),modrm_read8(&params,1),2); }
void CPU8086_OP13() {modrm_generateInstructionTEXT("ADCW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; CPU8086_internal_ADC16(modrm_addr16(&params,0,0),modrm_read16(&params,1),2); }
void CPU8086_OP14() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("ADC AL,",0,theimm,PARAM_IMM8); CPU8086_internal_ADC8(&REG_AL,theimm,1); }
void CPU8086_OP15() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("ADC AX,",0,theimm,PARAM_IMM16); CPU8086_internal_ADC16(&REG_AX,theimm,1); }
void CPU8086_OP16() {modrm_generateInstructionTEXT("PUSH SS",0,0,PARAM_NONE);/*PUSH SS*/ CPU_PUSH16(&REG_SS);/*PUSH SS*/ CPU[activeCPU].cycles_OP = 10; /*Push Segreg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP17() {modrm_generateInstructionTEXT("POP SS",0,0,PARAM_NONE);/*POP SS*/ segmentWritten(CPU_SEGMENT_SS,CPU_POP16(),0); /*CS changed!*/ CPU[activeCPU].cycles_OP = 8; /*Pop Segreg!*/ CPU_addWordMemoryTiming(); /*To memory?*/ CPU[activeCPU].allowInterrupts = 0; /* Inhabit all interrupts up to the next instruction */ }
void CPU8086_OP18() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SBBB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; CPU8086_internal_SBB8(modrm_addr8(&params,1,0),(modrm_read8(&params,0)),2); }
void CPU8086_OP19() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SBBW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; CPU8086_internal_SBB16(modrm_addr16(&params,1,0),(modrm_read16(&params,0)),2); }
void CPU8086_OP1A() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SBBB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; CPU8086_internal_SBB8(modrm_addr8(&params,0,0),(modrm_read8(&params,1)),2); }
void CPU8086_OP1B() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SBBW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; CPU8086_internal_SBB16(modrm_addr16(&params,0,0),(modrm_read16(&params,1)),2); }
void CPU8086_OP1C() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("SBB AL,",0,theimm,PARAM_IMM8); CPU8086_internal_SBB8(&REG_AL,theimm,1); }
void CPU8086_OP1D() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("SBB AX,",0,theimm,PARAM_IMM16); CPU8086_internal_SBB16(&REG_AX,theimm,1); }
void CPU8086_OP1E() {modrm_generateInstructionTEXT("PUSH DS",0,0,PARAM_NONE);/*PUSH DS*/ CPU_PUSH16(&REG_DS);/*PUSH DS*/ CPU[activeCPU].cycles_OP = 10; /*Push Segreg!*/}
void CPU8086_OP1F() {modrm_generateInstructionTEXT("POP DS",0,0,PARAM_NONE);/*POP DS*/ segmentWritten(CPU_SEGMENT_DS,CPU_POP16(),0); /*CS changed!*/ CPU[activeCPU].cycles_OP = 8; /*Pop Segreg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP20() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("ANDB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; CPU8086_internal_AND8(modrm_addr8(&params,1,0),modrm_read8(&params,0),2); }
void CPU8086_OP21() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("ANDW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; CPU8086_internal_AND16(modrm_addr16(&params,1,0),modrm_read16(&params,0),2); }
void CPU8086_OP22() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("ANDB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; CPU8086_internal_AND8(modrm_addr8(&params,0,0),modrm_read8(&params,1),2); }
void CPU8086_OP23() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("ANDW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; CPU8086_internal_AND16(modrm_addr16(&params,0,0),modrm_read16(&params,1),2); }
void CPU8086_OP24() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("AND AL,",0,theimm,PARAM_IMM8); CPU8086_internal_AND8(&REG_AL,theimm,1); }
void CPU8086_OP25() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("AND AX,",0,theimm,PARAM_IMM16); CPU8086_internal_AND16(&REG_AX,theimm,1); }
void CPU8086_OP27() {modrm_generateInstructionTEXT("DAA",0,0,PARAM_NONE);/*DAA?*/ CPU8086_internal_DAA();/*DAA?*/ }
void CPU8086_OP28() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SUBB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; CPU8086_internal_SUB8(modrm_addr8(&params,1,0),(modrm_read8(&params,0)),2); }
void CPU8086_OP29() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SUBW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; CPU8086_internal_SUB16(modrm_addr16(&params,1,0),(modrm_read16(&params,0)),2); }
void CPU8086_OP2A() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("SUBB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; CPU8086_internal_SUB8(modrm_addr8(&params,0,0),(modrm_read8(&params,1)),2); }
void CPU8086_OP2B() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("SUBW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; CPU8086_internal_SUB16(modrm_addr16(&params,0,0),(modrm_read16(&params,1)),2); }
void CPU8086_OP2C() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("SUB AL,",0,theimm,PARAM_IMM8);/*4=AL,imm8*/ CPU8086_internal_SUB8(&REG_AL,theimm,1);/*4=AL,imm8*/ }
void CPU8086_OP2D() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("SUB AX,",0,theimm,PARAM_IMM16);/*5=AX,imm16*/ CPU8086_internal_SUB16(&REG_AX,theimm,1);/*5=AX,imm16*/ }
void CPU8086_OP2F() {modrm_generateInstructionTEXT("DAS",0,0,PARAM_NONE);/*DAS?*/ CPU8086_internal_DAS();/*DAS?*/ }
void CPU8086_OP30() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("XORB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; CPU8086_internal_XOR8(modrm_addr8(&params,1,0),modrm_read8(&params,0),2); }
void CPU8086_OP31() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("XORW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; CPU8086_internal_XOR16(modrm_addr16(&params,1,0),modrm_read16(&params,0),2); }
void CPU8086_OP32() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("XORB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; CPU8086_internal_XOR8(modrm_addr8(&params,0,0),modrm_read8(&params,1),2); }
void CPU8086_OP33() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("XORW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; CPU8086_internal_XOR16(modrm_addr16(&params,0,0),modrm_read16(&params,1),2); }
void CPU8086_OP34() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("XOR AL,",0,theimm,PARAM_IMM8); CPU8086_internal_XOR8(&REG_AL,theimm,1); }
void CPU8086_OP35() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("XOR AX,",0,theimm,PARAM_IMM16); CPU8086_internal_XOR16(&REG_AX,theimm,1); }
void CPU8086_OP37() {modrm_generateInstructionTEXT("AAA",0,0,PARAM_NONE);/*AAA?*/ CPU8086_internal_AAA();/*AAA?*/ }
void CPU8086_OP38() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("CMPB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; if (modrm_check8(&params,1,1)) return; CMP_b(modrm_read8(&params,1),modrm_read8(&params,0),2); }
void CPU8086_OP39() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("CMPW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; if (modrm_check16(&params,1,1)) return; CMP_w(modrm_read16(&params,1),modrm_read16(&params,0),2); }
void CPU8086_OP3A() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("CMPB",8,0,PARAM_MODRM12); if (modrm_check8(&params,0,1)) return; if (modrm_check16(&params,1,1)) return; CMP_b(modrm_read8(&params,0),modrm_read8(&params,1),2); }
void CPU8086_OP3B() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("CMPW",16,0,PARAM_MODRM12); if (modrm_check16(&params,0,1)) return; if (modrm_check16(&params,1,1)) return; CMP_w(modrm_read16(&params,0),modrm_read16(&params,1),2); }
void CPU8086_OP3C() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("CMP AL,",0,theimm,PARAM_IMM8);/*CMP AL, imm8*/ CMP_b(REG_AL,theimm,1);/*CMP AL, imm8*/ }
void CPU8086_OP3D() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("CMP AX,",0,theimm,PARAM_IMM16);/*CMP AX, imm16*/ CMP_w(REG_AX,theimm,1);/*CMP AX, imm16*/ }
void CPU8086_OP3F() {modrm_generateInstructionTEXT("AAS",0,0,PARAM_NONE);/*AAS?*/ CPU8086_internal_AAS();/*AAS?*/ }
void CPU8086_OP40() {modrm_generateInstructionTEXT("INC AX",0,0,PARAM_NONE);/*INC AX*/ CPU8086_internal_INC16(&REG_AX);/*INC AX*/ }
void CPU8086_OP41() {modrm_generateInstructionTEXT("INC CX",0,0,PARAM_NONE);/*INC CX*/ CPU8086_internal_INC16(&REG_CX);/*INC CX*/ }
void CPU8086_OP42() {modrm_generateInstructionTEXT("INC DX",0,0,PARAM_NONE);/*INC DX*/ CPU8086_internal_INC16(&REG_DX);/*INC DX*/ }
void CPU8086_OP43() {modrm_generateInstructionTEXT("INC BX",0,0,PARAM_NONE);/*INC BX*/ CPU8086_internal_INC16(&REG_BX);/*INC BX*/ }
void CPU8086_OP44() {modrm_generateInstructionTEXT("INC SP",0,0,PARAM_NONE);/*INC SP*/ CPU8086_internal_INC16(&REG_SP);/*INC SP*/ }
void CPU8086_OP45() {modrm_generateInstructionTEXT("INC BP",0,0,PARAM_NONE);/*INC BP*/ CPU8086_internal_INC16(&REG_BP);/*INC BP*/ }
void CPU8086_OP46() {modrm_generateInstructionTEXT("INC SI",0,0,PARAM_NONE);/*INC SI*/ CPU8086_internal_INC16(&REG_SI);/*INC SI*/ }
void CPU8086_OP47() {modrm_generateInstructionTEXT("INC DI",0,0,PARAM_NONE);/*INC DI*/ CPU8086_internal_INC16(&REG_DI);/*INC DI*/ }
void CPU8086_OP48() {modrm_generateInstructionTEXT("DEC AX",0,0,PARAM_NONE);/*DEC AX*/ CPU8086_internal_DEC16(&REG_AX);/*DEC AX*/ }
void CPU8086_OP49() {modrm_generateInstructionTEXT("DEC CX",0,0,PARAM_NONE);/*DEC CX*/ CPU8086_internal_DEC16(&REG_CX);/*DEC CX*/ }
void CPU8086_OP4A() {modrm_generateInstructionTEXT("DEC DX",0,0,PARAM_NONE);/*DEC DX*/ CPU8086_internal_DEC16(&REG_DX);/*DEC DX*/ }
void CPU8086_OP4B() {modrm_generateInstructionTEXT("DEC BX",0,0,PARAM_NONE);/*DEC BX*/ CPU8086_internal_DEC16(&REG_BX);/*DEC BX*/ }
void CPU8086_OP4C() {modrm_generateInstructionTEXT("DEC SP",0,0,PARAM_NONE);/*DEC SP*/ CPU8086_internal_DEC16(&REG_SP);/*DEC SP*/ }
void CPU8086_OP4D() {modrm_generateInstructionTEXT("DEC BP",0,0,PARAM_NONE);/*DEC BP*/ CPU8086_internal_DEC16(&REG_BP);/*DEC BP*/ }
void CPU8086_OP4E() {modrm_generateInstructionTEXT("DEC SI",0,0,PARAM_NONE);/*DEC SI*/ CPU8086_internal_DEC16(&REG_SI);/*DEC SI*/ }
void CPU8086_OP4F() {modrm_generateInstructionTEXT("DEC DI",0,0,PARAM_NONE);/*DEC DI*/ CPU8086_internal_DEC16(&REG_DI);/*DEC DI*/ }
void CPU8086_OP50() {modrm_generateInstructionTEXT("PUSH AX",0,0,PARAM_NONE);/*PUSH AX*/ if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_AX);/*PUSH AX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP51() {modrm_generateInstructionTEXT("PUSH CX",0,0,PARAM_NONE);/*PUSH CX*/ if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_CX);/*PUSH CX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP52() {modrm_generateInstructionTEXT("PUSH DX",0,0,PARAM_NONE);/*PUSH DX*/ if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_DX);/*PUSH DX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP53() {modrm_generateInstructionTEXT("PUSH BX",0,0,PARAM_NONE);/*PUSH BX*/ if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_BX);/*PUSH BX*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP54() {modrm_generateInstructionTEXT("PUSH SP",0,0,PARAM_NONE);/*PUSH SP*/ if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_SP);/*PUSH SP*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP55() {modrm_generateInstructionTEXT("PUSH BP",0,0,PARAM_NONE);/*PUSH BP*/ if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_BP);/*PUSH BP*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP56() {modrm_generateInstructionTEXT("PUSH SI",0,0,PARAM_NONE);/*PUSH SI*/ if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_SI);/*PUSH SI*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP57() {modrm_generateInstructionTEXT("PUSH DI",0,0,PARAM_NONE);/*PUSH DI*/ if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_DI);/*PUSH DI*/ CPU[activeCPU].cycles_OP = 11; /*Push Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP58() {modrm_generateInstructionTEXT("POP AX",0,0,PARAM_NONE);/*POP AX*/ if (checkStackAccess(1,0,0)) return; REG_AX = CPU_POP16();/*POP AX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP59() {modrm_generateInstructionTEXT("POP CX",0,0,PARAM_NONE);/*POP CX*/ if (checkStackAccess(1,0,0)) return; REG_CX = CPU_POP16();/*POP CX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP5A() {modrm_generateInstructionTEXT("POP DX",0,0,PARAM_NONE);/*POP DX*/ if (checkStackAccess(1,0,0)) return; REG_DX = CPU_POP16();/*POP DX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP5B() {modrm_generateInstructionTEXT("POP BX",0,0,PARAM_NONE);/*POP BX*/ if (checkStackAccess(1,0,0)) return; REG_BX = CPU_POP16();/*POP BX*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP5C() {modrm_generateInstructionTEXT("POP SP",0,0,PARAM_NONE);/*POP SP*/ if (checkStackAccess(1,0,0)) return; REG_SP = MMU_rw(CPU_SEGMENT_SS,REG_SS,REG_SP,0);/*POP SP*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP5D() {modrm_generateInstructionTEXT("POP BP",0,0,PARAM_NONE);/*POP BP*/ if (checkStackAccess(1,0,0)) return; REG_BP = CPU_POP16();/*POP BP*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP5E() {modrm_generateInstructionTEXT("POP SI",0,0,PARAM_NONE);/*POP SI*/ if (checkStackAccess(1,0,0)) return; REG_SI = CPU_POP16();/*POP SI*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP5F() {modrm_generateInstructionTEXT("POP DI",0,0,PARAM_NONE);/*POP DI*/ if (checkStackAccess(1,0,0)) return; REG_DI = CPU_POP16();/*POP DI*/ CPU[activeCPU].cycles_OP = 8; /*Pop Reg!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OP70() {INLINEREGISTER signed char rel8;/*JO rel8: (FLAG_OF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JO",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_OF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP71() {INLINEREGISTER signed char rel8;/*JNO rel8 : (FLAG_OF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNO",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_OF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP72() {INLINEREGISTER signed char rel8;/*JC rel8: (FLAG_CF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JC",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_CF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP73() {INLINEREGISTER signed char rel8;/*JNC rel8 : (FLAG_CF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNC",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_CF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP74() {INLINEREGISTER signed char rel8;/*JZ rel8: (FLAG_ZF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JZ",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_ZF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP75() {INLINEREGISTER signed char rel8;/*JNZ rel8 : (FLAG_ZF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNZ",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_ZF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP76() {INLINEREGISTER signed char rel8;/*JBE rel8 : (FLAG_CF=1|FLAG_ZF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JBE",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_CF||FLAG_ZF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP77() {INLINEREGISTER signed char rel8;/*JA rel8: (FLAG_CF=0&FLAG_ZF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JA",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_CF && !FLAG_ZF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP78() {INLINEREGISTER signed char rel8;/*JS rel8: (FLAG_SF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JS",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP79() {INLINEREGISTER signed char rel8;/*JNS rel8 : (FLAG_SF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNS",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_SF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP7A() {INLINEREGISTER signed char rel8;/*JP rel8 : (FLAG_PF=1)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JP",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_PF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP7B() {INLINEREGISTER signed char rel8;/*JNP rel8 : (FLAG_PF=0)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JNP",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_PF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP7C() {INLINEREGISTER signed char rel8;/*JL rel8: (FLAG_SF!=FLAG_OF)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JL",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF!=FLAG_OF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP7D() {INLINEREGISTER signed char rel8;/*JGE rel8 : (FLAG_SF=FLAG_OF)*/ rel8 = imm8(); modrm_generateInstructionTEXT("JGE",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF==FLAG_OF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP7E() {INLINEREGISTER signed char rel8;/*JLE rel8 : (FLAG_ZF|(FLAG_SF!=FLAG_OF))*/ rel8 = imm8(); modrm_generateInstructionTEXT("JLE",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if ((FLAG_SF!=FLAG_OF) || FLAG_ZF) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP7F() {INLINEREGISTER signed char rel8;/*JG rel8: ((FLAG_ZF=0)&&(FLAG_SF=FLAG_OF))*/ rel8 = imm8(); modrm_generateInstructionTEXT("JG",0,REG_IP + rel8,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_ZF && (FLAG_SF==FLAG_OF)) {REG_IP += rel8; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU8086_OP84() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("TESTB",8,0,PARAM_MODRM12); if (modrm_check8(&params,0,1)) return; if (modrm_check8(&params,1,1)) return; CPU8086_internal_TEST8(modrm_read8(&params,0),modrm_read8(&params,1),2); }
void CPU8086_OP85() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("TESTW",16,0,PARAM_MODRM12); if (modrm_check16(&params,0,1)) return; if (modrm_check16(&params,1,1)) return; CPU8086_internal_TEST16(modrm_read16(&params,0),modrm_read16(&params,1),2); }
void CPU8086_OP86() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("XCHGB",8,0,PARAM_MODRM12); CPU8086_internal_XCHG8(modrm_addr8(&params,0,0),modrm_addr8(&params,1,1),2); /*XCHG reg8,r/m8*/ }
void CPU8086_OP87() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("XCHGW",16,0,PARAM_MODRM12); CPU8086_internal_XCHG16(modrm_addr16(&params,0,0),modrm_addr16(&params,1,1),2); /*XCHG reg16,r/m16*/ }
void CPU8086_OP88() {modrm_debugger8(&params,1,0); modrm_generateInstructionTEXT("MOVB",8,0,PARAM_MODRM21); if (modrm_check8(&params,0,1)) return; CPU8086_internal_MOV8(modrm_addr8(&params,1,0),modrm_read8(&params,0),2); }
void CPU8086_OP89() {modrm_debugger16(&params,1,0); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; CPU8086_internal_MOV16(modrm_addr16(&params,1,0),modrm_read16(&params,0),2); }
void CPU8086_OP8A() {modrm_debugger8(&params,0,1); modrm_generateInstructionTEXT("MOVB",8,0,PARAM_MODRM12); if (modrm_check8(&params,1,1)) return; CPU8086_internal_MOV8(modrm_addr8(&params,0,0),modrm_read8(&params,1),2); }
void CPU8086_OP8B() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; CPU8086_internal_MOV16(modrm_addr16(&params,0,0),modrm_read16(&params,1),2); }
void CPU8086_OP8C() {modrm_debugger16(&params,1,0); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM21); if (modrm_check16(&params,0,1)) return; CPU8086_internal_MOV16(modrm_addr16(&params,1,0),modrm_read16(&params,0),8); }
void CPU8086_OP8D() {modrm_debugger16(&params,0,1); debugger_setcommand("LEA %s,%s",modrm_param1,getLEAtext(&params)); CPU8086_internal_MOV16(modrm_addr16(&params,0,0),getLEA(&params),0); CPU[activeCPU].cycles_OP = 2+MODRM_EA(params); /* Load effective address */}
void CPU8086_OP8E() {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("MOVW",16,0,PARAM_MODRM12); if (modrm_check16(&params,1,1)) return; CPU8086_internal_MOV16(modrm_addr16(&params,0,0),modrm_read16(&params,1),8); if ((params.info[0].reg16 == &CPU[activeCPU].registers->SS) && (params.info[1].isreg == 1)) { CPU[activeCPU].allowInterrupts = 0; /* Inhabit all interrupts up to the next instruction */ } }
void CPU8086_OP90() /*NOP*/ {modrm_generateInstructionTEXT("NOP",0,0,PARAM_NONE);/*NOP (XCHG AX,AX)*/ CPU8086_internal_XCHG16(&REG_AX,&REG_AX,1); CPU[activeCPU].cycles_OP = 3; /* NOP */}
void CPU8086_OP91() {modrm_generateInstructionTEXT("XCHG CX,AX",0,0,PARAM_NONE);/*XCHG AX,CX*/ CPU8086_internal_XCHG16(&REG_CX,&REG_AX,1); /*XCHG CX,AX*/ }
void CPU8086_OP92() {modrm_generateInstructionTEXT("XCHG DX,AX",0,0,PARAM_NONE);/*XCHG AX,DX*/ CPU8086_internal_XCHG16(&REG_DX,&REG_AX,1); /*XCHG DX,AX*/ }
void CPU8086_OP93() {modrm_generateInstructionTEXT("XCHG BX,AX",0,0,PARAM_NONE);/*XCHG AX,BX*/ CPU8086_internal_XCHG16(&REG_BX,&REG_AX,1); /*XCHG BX,AX*/ }
void CPU8086_OP94() {modrm_generateInstructionTEXT("XCHG SP,AX",0,0,PARAM_NONE);/*XCHG AX,SP*/ CPU8086_internal_XCHG16(&REG_SP,&REG_AX,1); /*XCHG SP,AX*/ }
void CPU8086_OP95() {modrm_generateInstructionTEXT("XCHG BP,AX",0,0,PARAM_NONE);/*XCHG AX,BP*/ CPU8086_internal_XCHG16(&REG_BP,&REG_AX,1); /*XCHG BP,AX*/ }
void CPU8086_OP96() {modrm_generateInstructionTEXT("XCHG SI,AX",0,0,PARAM_NONE);/*XCHG AX,SI*/ CPU8086_internal_XCHG16(&REG_SI,&REG_AX,1); /*XCHG SI,AX*/ }
void CPU8086_OP97() {modrm_generateInstructionTEXT("XCHG DI,AX",0,0,PARAM_NONE);/*XCHG AX,DI*/ CPU8086_internal_XCHG16(&REG_DI,&REG_AX,1); /*XCHG DI,AX*/ }
void CPU8086_OP98() {modrm_generateInstructionTEXT("CBW",0,0,PARAM_NONE);/*CBW : sign extend AL to AX*/ CPU8086_internal_CBW();/*CBW : sign extend AL to AX (8088+)*/ }
void CPU8086_OP99() {modrm_generateInstructionTEXT("CWD",0,0,PARAM_NONE);/*CWD : sign extend AX to DX::AX*/ CPU8086_internal_CWD();/*CWD : sign extend AX to DX::AX (8088+)*/ }
void CPU8086_OP9A() {/*CALL Ap*/ INLINEREGISTER uint_32 segmentoffset = imm32; debugger_setcommand("CALL %04x:%04x", (segmentoffset>>16), (segmentoffset&0xFFFF)); CPU8086_CALLF((segmentoffset>>16)&0xFFFF,segmentoffset&0xFFFF); CPU[activeCPU].cycles_OP = 28; /* Intersegment direct */ CPU_addWordMemoryTiming(); /*To memory?*/ CPU_addWordMemoryTiming(); /*To memory?*/ }
void CPU8086_OP9B() {modrm_generateInstructionTEXT("WAIT",0,0,PARAM_NONE);/*WAIT : wait for TEST pin activity. (UNIMPLEMENTED)*/ CPU[activeCPU].wait = 1;/*9B: WAIT : wait for TEST pin activity. (Edit: continue on interrupts or 8087+!!!)*/ }
void CPU8086_OP9C() {modrm_generateInstructionTEXT("PUSHF",0,0,PARAM_NONE);/*PUSHF*/ if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_FLAGS); CPU[activeCPU].cycles_OP = 10; /*PUSHF timing!*/ CPU_addWordMemoryTiming(); /*To memory?*/ }
void CPU8086_OP9D() {modrm_generateInstructionTEXT("POPF", 0, 0, PARAM_NONE);/*POPF*/ word tempflags; if (checkStackAccess(1,0,0)) return;  tempflags = CPU_POP16(); if (disallowPOPFI()) { tempflags &= ~0x200; tempflags |= REG_FLAGS&0x200; /* Ignore any changes to the Interrupt flag! */ } if (getCPL()) { tempflags &= ~0x3000; tempflags |= REG_FLAGS&0x3000; /* Ignore any changes to the IOPL when not at CPL 0! */ } REG_FLAGS = tempflags; updateCPUmode(); /*POPF*/ CPU[activeCPU].cycles_OP = 8; /*POPF timing!*/ CPU_addWordMemoryTiming(); /*To memory?*/ }
void CPU8086_OP9E() {modrm_generateInstructionTEXT("SAHF", 0, 0, PARAM_NONE);/*SAHF : Save AH to lower half of FLAGS.*/ REG_FLAGS = ((REG_FLAGS & 0xFF00) | REG_AH); updateCPUmode(); /*SAHF : Save AH to lower half of FLAGS.*/ CPU[activeCPU].cycles_OP = 4; /*SAHF timing!*/}
void CPU8086_OP9F() {modrm_generateInstructionTEXT("LAHF",0,0,PARAM_NONE);/*LAHF : Load lower half of FLAGS into AH.*/ REG_AH = (REG_FLAGS&0xFF);/*LAHF : Load lower half of FLAGS into AH.*/  CPU[activeCPU].cycles_OP = 4; /*LAHF timing!*/}
void CPU8086_OPA0() {INLINEREGISTER word theimm = immw; debugger_setcommand("MOVB AL,[%s:%04X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AL,[imm16]*/ if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,1,getCPL())) return; CPU8086_internal_MOV8(&REG_AL,MMU_rb(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,0),1);/*MOV AL,[imm16]*/ }
void CPU8086_OPA1() {INLINEREGISTER word theimm = immw; debugger_setcommand("MOVW AX,[%s:%04X]",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV AX,[imm16]*/  if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,1,getCPL())) return; if (checkMMUaccess(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm+1,1,getCPL())) return; CPU8086_internal_MOV16(&REG_AX,MMU_rw(CPU_segment_index(CPU_SEGMENT_DS),CPU_segment(CPU_SEGMENT_DS),theimm,0),1);/*MOV AX,[imm16]*/ }
void CPU8086_OPA2() {INLINEREGISTER word theimm = immw; debugger_setcommand("MOVB [%s:%04X],AL",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm16],AL*/ custommem = 1; customoffset = theimm; CPU8086_internal_MOV8(NULL,REG_AL,1);/*MOV [imm16],AL*/ custommem = 0; }
void CPU8086_OPA3() {INLINEREGISTER word theimm = immw; debugger_setcommand("MOVW [%s:%04X],AX",CPU_textsegment(CPU_SEGMENT_DS),theimm);/*MOV [imm16], AX*/ custommem = 1; customoffset = theimm; CPU8086_internal_MOV16(NULL,REG_AX,1);/*MOV [imm16], AX*/ custommem = 0; }
void CPU8086_OPA4() {modrm_generateInstructionTEXT("MOVSB",0,0,PARAM_NONE);/*MOVSB*/ CPU8086_internal_MOVSB();/*MOVSB*/ }
void CPU8086_OPA5() {modrm_generateInstructionTEXT("MOVSW",0,0,PARAM_NONE);/*MOVSW*/ CPU8086_internal_MOVSW();/*MOVSW*/ }
void CPU8086_OPA6() {debugger_setcommand("CMPSB [%s:ESI],[ES:EDI]",CPU_textsegment(CPU_SEGMENT_DS));/*CMPSB*/ CPU8086_internal_CMPSB();/*CMPSB*/ }
void CPU8086_OPA7() {debugger_setcommand("CMPSW [%s:ESI],[ES:EDI]",CPU_textsegment(CPU_SEGMENT_DS));/*CMPSW*/ CPU8086_internal_CMPSW();/*CMPSW*/ }
void CPU8086_OPA8() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("TESTB AL,",0,theimm,PARAM_IMM8);/*TEST AL,imm8*/ CPU8086_internal_TEST8(REG_AL,theimm,1);/*TEST AL,imm8*/ }
void CPU8086_OPA9() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("TESTW AX,",0,theimm,PARAM_IMM16);/*TEST AX,imm16*/ CPU8086_internal_TEST16(REG_AX,theimm,1);/*TEST AX,imm16*/ }
void CPU8086_OPAA() {modrm_generateInstructionTEXT("STOSB",0,0,PARAM_NONE);/*STOSB*/ CPU8086_internal_STOSB();/*STOSB*/ }
void CPU8086_OPAB() {modrm_generateInstructionTEXT("STOSW",0,0,PARAM_NONE);/*STOSW*/ CPU8086_internal_STOSW();/*STOSW*/ }
void CPU8086_OPAC() {modrm_generateInstructionTEXT("LODSB",0,0,PARAM_NONE);/*LODSB*/ CPU8086_internal_LODSB();/*LODSB*/ }
void CPU8086_OPAD() {modrm_generateInstructionTEXT("LODSW",0,0,PARAM_NONE);/*LODSW*/ CPU8086_internal_LODSW();/*LODSW*/ }
void CPU8086_OPAE() {modrm_generateInstructionTEXT("SCASB",0,0,PARAM_NONE);/*SCASB*/ CPU8086_internal_SCASB();/*SCASB*/ }
void CPU8086_OPAF() {modrm_generateInstructionTEXT("SCASW",0,0,PARAM_NONE);/*SCASW*/ CPU8086_internal_SCASW();/*SCASW*/ }
void CPU8086_OPB0() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB AL,",0,theimm,PARAM_IMM8);/*MOV AL,imm8*/ CPU8086_internal_MOV8(&REG_AL,theimm,4);/*MOV AL,imm8*/ }
void CPU8086_OPB1() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB CL,",0,theimm,PARAM_IMM8);/*MOV CL,imm8*/ CPU8086_internal_MOV8(&REG_CL,theimm,4);/*MOV CL,imm8*/ }
void CPU8086_OPB2() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB DL,",0,theimm,PARAM_IMM8);/*MOV DL,imm8*/ CPU8086_internal_MOV8(&REG_DL,theimm,4);/*MOV DL,imm8*/ }
void CPU8086_OPB3() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB BL,",0,theimm,PARAM_IMM8);/*MOV BL,imm8*/ CPU8086_internal_MOV8(&REG_BL,theimm,4);/*MOV BL,imm8*/ }
void CPU8086_OPB4() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB AH,",0,theimm,PARAM_IMM8);/*MOV AH,imm8*/ CPU8086_internal_MOV8(&REG_AH,theimm,4);/*MOV AH,imm8*/ }
void CPU8086_OPB5() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB CH,",0,theimm,PARAM_IMM8);/*MOV CH,imm8*/ CPU8086_internal_MOV8(&REG_CH,theimm,4);/*MOV CH,imm8*/ }
void CPU8086_OPB6() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB DH,",0,theimm,PARAM_IMM8);/*MOV DH,imm8*/ CPU8086_internal_MOV8(&REG_DH,theimm,4);/*MOV DH,imm8*/ }
void CPU8086_OPB7() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("MOVB BH,",0,theimm,PARAM_IMM8);/*MOV BH,imm8*/ CPU8086_internal_MOV8(&REG_BH,theimm,4);/*MOV BH,imm8*/ }
void CPU8086_OPB8() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW AX,",0,theimm,PARAM_IMM16);/*MOV AX,imm16*/ CPU8086_internal_MOV16(&REG_AX,theimm,4);/*MOV AX,imm16*/ }
void CPU8086_OPB9() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW CX,",0,theimm,PARAM_IMM16);/*MOV CX,imm16*/ CPU8086_internal_MOV16(&REG_CX,theimm,4);/*MOV CX,imm16*/ }
void CPU8086_OPBA() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW DX,",0,theimm,PARAM_IMM16);/*MOV DX,imm16*/ CPU8086_internal_MOV16(&REG_DX,theimm,4);/*MOV DX,imm16*/ }
void CPU8086_OPBB() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW BX,",0,theimm,PARAM_IMM16);/*MOV BX,imm16*/ CPU8086_internal_MOV16(&REG_BX,theimm,4);/*MOV BX,imm16*/ }
void CPU8086_OPBC() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW SP,",0,theimm,PARAM_IMM16);/*MOV SP,imm16*/ CPU8086_internal_MOV16(&REG_SP,theimm,4);/*MOV SP,imm16*/ }
void CPU8086_OPBD() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW BP,",0,theimm,PARAM_IMM16);/*MOV BP,imm16*/ CPU8086_internal_MOV16(&REG_BP,theimm,4);/*MOV BP,imm16*/ }
void CPU8086_OPBE() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW SI,",0,theimm,PARAM_IMM16);/*MOV SI,imm16*/ CPU8086_internal_MOV16(&REG_SI,theimm,4);/*MOV SI,imm16*/ }
void CPU8086_OPBF() {INLINEREGISTER word theimm = immw; modrm_generateInstructionTEXT("MOVW DI,",0,theimm,PARAM_IMM16);/*MOV DI,imm16*/ CPU8086_internal_MOV16(&REG_DI,theimm,4);/*MOV DI,imm16*/ }
void CPU8086_OPC2() {INLINEREGISTER sword popbytes = imm16();/*RET imm16 (Near return to calling proc and POP imm16 bytes)*/ modrm_generateInstructionTEXT("RET",0,popbytes,PARAM_IMM8); /*RET imm16 (Near return to calling proc and POP imm16 bytes)*/ CPU8086_internal_RET(popbytes,1); }
void CPU8086_OPC3() {modrm_generateInstructionTEXT("RET",0,0,PARAM_NONE);/*RET (Near return to calling proc)*/ /*RET (Near return to calling proc)*/ CPU8086_internal_RET(0,0); }
void CPU8086_OPC4() /*LES modr/m*/ {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("LES",0,0,PARAM_MODRM12); CPU8086_internal_LXS(CPU_SEGMENT_ES); /*Load new ES!*/ }
void CPU8086_OPC5() /*LDS modr/m*/ {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("LDS",0,0,PARAM_MODRM12); CPU8086_internal_LXS(CPU_SEGMENT_DS); /*Load new DS!*/ }
void CPU8086_OPC6() {byte val = immb; modrm_debugger8(&params,0,1); debugger_setcommand("MOVB %s,%02x",modrm_param2,val); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,val); if (MODRM_EA(params)) CPU[activeCPU].cycles_OP = 10+MODRM_EA(params); /* Imm->Mem */ else CPU[activeCPU].cycles_OP = 4; /* Imm->Reg */ }
void CPU8086_OPC7() {word val = immw; modrm_debugger16(&params,0,1); debugger_setcommand("MOVW %s,%04x",modrm_param2,val); if (modrm_check16(&params,1,0)) return; modrm_write16(&params,1,val,0); if (MODRM_EA(params)) { CPU[activeCPU].cycles_OP = 10+MODRM_EA(params); /* Imm->Mem */  CPU_addWordMemoryTiming(); /*To memory?*/ } else CPU[activeCPU].cycles_OP = 4; /* Imm->Reg */ }
void CPU8086_OPCA() {INLINEREGISTER sword popbytes = imm16();/*RETF imm16 (Far return to calling proc and pop imm16 bytes)*/ modrm_generateInstructionTEXT("RETF",0,popbytes,PARAM_IMM16); /*RETF imm16 (Far return to calling proc and pop imm16 bytes)*/ CPU8086_internal_RETF(popbytes,1); }
void CPU8086_OPCB() {modrm_generateInstructionTEXT("RETF",0,0,PARAM_NONE); /*RETF (Far return to calling proc)*/ CPU8086_internal_RETF(0,0); }
void CPU8086_OPCC() {modrm_generateInstructionTEXT("INT 3",0,0,PARAM_NONE); /*INT 3*/ CPU8086_int(EXCEPTION_CPUBREAKPOINT,1);/*INT 3*/ }
void CPU8086_OPCD() {INLINEREGISTER byte theimm = immb; INTdebugger8086(); modrm_generateInstructionTEXT("INT",0,theimm,PARAM_IMM8);/*INT imm8*/ CPU8086_int(theimm,0);/*INT imm8*/ }
void CPU8086_OPCE() {modrm_generateInstructionTEXT("INTO",0,0,PARAM_NONE);/*INTO*/ CPU8086_internal_INTO();/*INTO*/ }
void CPU8086_OPCF() {modrm_generateInstructionTEXT("IRET",0,0,PARAM_NONE);/*IRET*/ CPU8086_IRET();/*IRET : also restore interrupt flag!*/ }
void CPU8086_OPD4() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("AAM",0,theimm,PARAM_IMM8);/*AAM*/ CPU8086_internal_AAM(theimm);/*AAM*/ }
void CPU8086_OPD5() {INLINEREGISTER byte theimm = immb; modrm_generateInstructionTEXT("AAD",0,theimm,PARAM_IMM8);/*AAD*/ CPU8086_internal_AAD(theimm);/*AAD*/ }
void CPU8086_OPD6(){debugger_setcommand("SALC"); REG_AL=FLAG_CF?0xFF:0x00; CPU[activeCPU].cycles_OP = 2;} //Special case on the 8086: SALC!
void CPU8086_OPD7(){CPU8086_internal_XLAT();}
void CPU8086_OPE0(){INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOPNZ",0, ((REG_IP+rel8)&0xFFFF),PARAM_IMM16); if ((--REG_CX) && (!FLAG_ZF)){REG_IP += rel8; CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 19; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 5; /* Branch not taken */}}
void CPU8086_OPE1(){INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOPZ",0, ((REG_IP+rel8)&0xFFFF),PARAM_IMM16);if ((--REG_CX) && (FLAG_ZF)){REG_IP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 18; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 6; /* Branch not taken */}}
void CPU8086_OPE2(){INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("LOOP", 0,((REG_IP+rel8)&0xFFFF),PARAM_IMM16);if (--REG_CX){REG_IP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 17; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 5; /* Branch not taken */}}
void CPU8086_OPE3(){INLINEREGISTER signed char rel8; rel8 = imm8(); modrm_generateInstructionTEXT("JCXZ",0,((REG_IP+rel8)&0xFFFF),PARAM_IMM16); if (!REG_CX){REG_IP += rel8;CPU_flushPIQ(); /*We're jumping to another address*/CPU[activeCPU].cycles_OP = 18; didJump = 1; /* Branch taken */}else { CPU[activeCPU].cycles_OP = 6; /* Branch not taken */}}
void CPU8086_OPE4(){INLINEREGISTER byte theimm = imm8(); modrm_generateInstructionTEXT("IN AL,",0,theimm,PARAM_IMM8); CPU_PORT_IN_B(theimm,&REG_AL); CPU[activeCPU].cycles_OP = 10; /*Timings!*/}
void CPU8086_OPE5(){INLINEREGISTER byte theimm = imm8();modrm_generateInstructionTEXT("IN AX,",0,theimm,PARAM_IMM8); CPU_PORT_IN_W(theimm,&REG_AX); CPU[activeCPU].cycles_OP = 10; /*Timings!*/  CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OPE6(){INLINEREGISTER byte theimm = imm8();debugger_setcommand("OUT %02X,AL",theimm);CPU_PORT_OUT_B(theimm,REG_AL); CPU[activeCPU].cycles_OP = 10; /*Timings!*/}
void CPU8086_OPE7(){INLINEREGISTER byte theimm = imm8(); debugger_setcommand("OUT %02X,AX",theimm); CPU_PORT_OUT_W(theimm,REG_AX); CPU[activeCPU].cycles_OP = 10; /*Timings!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OPE8(){INLINEREGISTER sword reloffset = imm16(); modrm_generateInstructionTEXT("CALL",0,((REG_IP + reloffset)&0xFFFF),PARAM_IMM16); if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_IP); REG_IP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 19; /* Intrasegment direct */}
void CPU8086_OPE9(){INLINEREGISTER sword reloffset = imm16(); modrm_generateInstructionTEXT("JMP",0,((REG_IP + reloffset)&0xFFFF),PARAM_IMM16); REG_IP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 15; /* Intrasegment direct */}
void CPU8086_OPEA(){INLINEREGISTER uint_32 segmentoffset = imm32(); debugger_setcommand("JMP %04X:%04X", (segmentoffset>>16), (segmentoffset&0xFFFF)); destEIP = (segmentoffset&0xFFFF); segmentWritten(CPU_SEGMENT_CS, (segmentoffset>>16), 1); CPU[activeCPU].cycles_OP = 15; /* Intersegment direct */}
void CPU8086_OPEB(){INLINEREGISTER signed char reloffset = imm8(); modrm_generateInstructionTEXT("JMP",0,((REG_IP + reloffset)&0xFFFF),PARAM_IMM16); REG_IP += reloffset;CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 15; /* Intrasegment direct short */}
void CPU8086_OPEC(){modrm_generateInstructionTEXT("IN AL,DX",0,0,PARAM_NONE); CPU_PORT_IN_B(REG_DX,&REG_AL); CPU[activeCPU].cycles_OP = 8; /*Timings!*/}
void CPU8086_OPED(){modrm_generateInstructionTEXT("IN AX,DX",0,0,PARAM_NONE); CPU_PORT_IN_W(REG_DX,&REG_AX); CPU[activeCPU].cycles_OP = 8; /*Timings!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OPEE(){modrm_generateInstructionTEXT("OUT DX,AL",0,0,PARAM_NONE); CPU_PORT_OUT_B(REG_DX,REG_AL); CPU[activeCPU].cycles_OP = 8; /*Timings!*/}
void CPU8086_OPEF(){modrm_generateInstructionTEXT("OUT DX,AX",0,0,PARAM_NONE); CPU_PORT_OUT_W(REG_DX,REG_AX); CPU[activeCPU].cycles_OP = 8; /*Timings!*/ CPU_addWordMemoryTiming(); /*To memory?*/}
void CPU8086_OPF1(){modrm_generateInstructionTEXT("<Undefined and reserved opcode, no error>",0,0,PARAM_NONE);}
void CPU8086_OPF4(){modrm_generateInstructionTEXT("HLT",0,0,PARAM_NONE); CPU[activeCPU].halt = 1; CPU[activeCPU].cycles_OP = 2; /*Special timing!*/}
void CPU8086_OPF5(){modrm_generateInstructionTEXT("CMC",0,0,PARAM_NONE); FLAGW_CF(!FLAG_CF); CPU[activeCPU].cycles_OP = 2; /*Special timing!*/}
void CPU8086_OPF8(){modrm_generateInstructionTEXT("CLC",0,0,PARAM_NONE); FLAGW_CF(0);CPU[activeCPU].cycles_OP = 2; /*Special timing!*/}
void CPU8086_OPF9(){modrm_generateInstructionTEXT("STC",0,0,PARAM_NONE); FLAGW_CF(1);CPU[activeCPU].cycles_OP = 2; /*Special timing!*/}
void CPU8086_OPFA(){modrm_generateInstructionTEXT("CLI",0,0,PARAM_NONE); if (checkSTICLI()) { FLAGW_IF(0); } CPU[activeCPU].cycles_OP = 2; /*Special timing!*/}
void CPU8086_OPFB(){modrm_generateInstructionTEXT("STI",0,0,PARAM_NONE); if (checkSTICLI()) { FLAGW_IF(1); } CPU[activeCPU].cycles_OP = 2; /*Special timing!*/}
void CPU8086_OPFC(){modrm_generateInstructionTEXT("CLD",0,0,PARAM_NONE); FLAGW_DF(0);CPU[activeCPU].cycles_OP = 2; /*Special timing!*/}
void CPU8086_OPFD(){modrm_generateInstructionTEXT("STD",0,0,PARAM_NONE); FLAGW_DF(1);CPU[activeCPU].cycles_OP = 2; /*Special timing!*/}

/*

NOW COME THE GRP1-5 OPCODES:

*/

//GRP1

/*

DEBUG: REALLY SUPPOSED TO HANDLE OP80-83 HERE?

*/

void CPU8086_OP80() //GRP1 Eb,Ib
{
	INLINEREGISTER byte imm = immb;
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger8(&params,1,0);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_ADD8(modrm_addr8(&params,1,0),imm,3); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_OR8(modrm_addr8(&params,1,0),imm,3); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_ADC8(modrm_addr8(&params,1,0),imm,3); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_SBB8(modrm_addr8(&params,1,0),imm,3); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_AND8(modrm_addr8(&params,1,0),imm,3); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_SUB8(modrm_addr8(&params,1,0),imm,3); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		CPU8086_internal_XOR8(modrm_addr8(&params,1,0),imm,3); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPB %s,%02X",&modrm_param1,imm); //CMP Eb, Ib
		}
		if (modrm_check8(&params,1,1)) return; //Abort when needed!
		CMP_b(modrm_read8(&params,1),imm,3); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU8086_OP81() //GRP1 Ev,Iv
{
	INLINEREGISTER word imm = immw;
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,1,0);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDW %s,%04X",&modrm_param1,imm); //ADD Eb, Ib
		}
		CPU8086_internal_ADD16(modrm_addr16(&params,1,0),imm,3); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORW %s,%04X",&modrm_param1,imm); //OR Eb, Ib
		}
		CPU8086_internal_OR16(modrm_addr16(&params,1,0),imm,3); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCW %s,%04X",&modrm_param1,imm); //ADC Eb, Ib
		}
		CPU8086_internal_ADC16(modrm_addr16(&params,1,0),imm,3); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBW %s,%04X",&modrm_param1,imm); //SBB Eb, Ib
		}
		CPU8086_internal_SBB16(modrm_addr16(&params,1,0),imm,3); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDW %s,%04X",&modrm_param1,imm); //AND Eb, Ib
		}
		CPU8086_internal_AND16(modrm_addr16(&params,1,0),imm,3); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBW %s,%04X",&modrm_param1,imm); //SUB Eb, Ib
		}
		CPU8086_internal_SUB16(modrm_addr16(&params,1,0),imm,3); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORW %s,%04X",&modrm_param1,imm); //XOR Eb, Ib
		}
		CPU8086_internal_XOR16(modrm_addr16(&params,1,0),imm,3); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPW %s,%04X",&modrm_param1,imm); //CMP Eb, Ib
		}
		if (modrm_check16(&params,1,1)) return; //Abort when needed!
		CMP_w(modrm_read16(&params,1),imm,3); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU8086_OP82() //GRP1 Eb,Ib (same as OP80)
{
	CPU8086_OP80(); //Same!
}

void CPU8086_OP83() //GRP1 Ev,Ib
{
	INLINEREGISTER word imm;
	imm = immb;
	if (imm&0x80) imm |= 0xFF00; //Sign extend!
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,1,0);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //ADD
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADDW %s,%04X",&modrm_param1,imm); //ADD Eb, Ib
		}
		CPU8086_internal_ADD16(modrm_addr16(&params,1,0),imm,3); //ADD Eb, Ib
		break;
	case 1: //OR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ORW %s,%04X",&modrm_param1,imm); //OR Eb, Ib
		}
		CPU8086_internal_OR16(modrm_addr16(&params,1,0),imm,3); //OR Eb, Ib
		break;
	case 2: //ADC
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ADCW %s,%04X",&modrm_param1,imm); //ADC Eb, Ib
		}
		CPU8086_internal_ADC16(modrm_addr16(&params,1,0),imm,3); //ADC Eb, Ib
		break;
	case 3: //SBB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SBBW %s,%04X",&modrm_param1,imm); //SBB Eb, Ib
		}
		CPU8086_internal_SBB16(modrm_addr16(&params,1,0),imm,3); //SBB Eb, Ib
		break;
	case 4: //AND
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("ANDW %s,%04X",&modrm_param1,imm); //AND Eb, Ib
		}
		CPU8086_internal_AND16(modrm_addr16(&params,1,0),imm,3); //AND Eb, Ib
		break;
	case 5: //SUB
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("SUBW %s,%04X",&modrm_param1,imm); //SUB Eb, Ib
		}
		CPU8086_internal_SUB16(modrm_addr16(&params,1,0),imm,3); //SUB Eb, Ib
		break;
	case 6: //XOR
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("XORW %s,%04X",&modrm_param1,imm); //XOR Eb, Ib
		}
		CPU8086_internal_XOR16(modrm_addr16(&params,1,0),imm,3); //XOR Eb, Ib
		break;
	case 7: //CMP
		if (cpudebugger) //Debugger on?
		{
			debugger_setcommand("CMPW %s,%04X",&modrm_param1,imm); //CMP Eb, Ib
		}
		if (modrm_check16(&params,1,1)) return; //Abort when needed!
		CMP_w(modrm_read16(&params,1),imm,3); //CMP Eb, Ib
		break;
	default:
		break;
	}
}

void CPU8086_OP8F() //Undocumented GRP opcode 8F r/m16
{
	if (cpudebugger)
	{
		modrm_debugger16(&params,0,1);
	}
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //POP
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT("POPW",16,0,PARAM_MODRM2); //POPW Ew
		}
		if (checkStackAccess(1,0,0)) return; //Abort when needed!
		modrm_write16(&params,1,CPU_POP16(),0); //POP r/m16
		if (params.info[1].reg16 == &CPU[activeCPU].registers->SS) //Popping into SS?
		{
			CPU[activeCPU].allowInterrupts = 0; /* Inhabit all interrupts up to the next instruction */
		}
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 17+MODRM_EA(params); /*Pop Mem!*/
			CPU_addWordMemoryTiming(); /*To memory?*/
			CPU_addWordMemoryTiming(); /*To memory?*/
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

void CPU8086_OPD0() //GRP2 Eb,1
{
	thereg = MODRM_REG(params.modrm);
	if (modrm_check8(&params,1,1)) return; //Abort when needed!
	if (modrm_check8(&params,1,0)) return; //Abort when needed!
	oper1b = modrm_read8(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger8(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLB %s,1",&modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORB %s,1",&modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLB %s,1",&modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRB %s,1",&modrm_param2);
			break;
		case 4: //SHL
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHLB %s,1",&modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRB %s,1",&modrm_param2);
			break;
		case 7: //SAR
			debugger_setcommand("SARB %s,1",&modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write8(&params,1,op_grp2_8(1,0));
}
void CPU8086_OPD1() //GRP2 Ev,1
{
	thereg = MODRM_REG(params.modrm);
	if (modrm_check16(&params,1,1)) return; //Abort when needed!
	if (modrm_check16(&params,1,0)) return; //Abort when needed!
	oper1 = modrm_read16(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLW %s,1",&modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORW %s,1",&modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLW %s,1",&modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRW %s,1",&modrm_param2);
			break;
		case 4: //SHL
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHLW %s,1",&modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRW %s,1",&modrm_param2);
			break;
		case 7: //SAR
			debugger_setcommand("SAR %s,1",&modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write16(&params,1,op_grp2_16(1,0),0);
}
void CPU8086_OPD2() //GRP2 Eb,CL
{
	thereg = MODRM_REG(params.modrm);
	if (modrm_check8(&params,1,1)) return; //Abort when needed!
	if (modrm_check8(&params,1,0)) return; //Abort when needed!
	oper1b = modrm_read8(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger8(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLB %s,CL",&modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORB %s,CL",&modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLB %s,CL",&modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRB %s,CL",&modrm_param2);
			break;
		case 4: //SHL
		case 6: //--- Unknown Opcode! --- Undocumented opcode!
			debugger_setcommand("SHLB %s,CL",&modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRB %s,CL",&modrm_param2);
			break;
		case 7: //SAR
			debugger_setcommand("SARB %s,CL",&modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write8(&params,1,op_grp2_8(REG_CL,1));
}
void CPU8086_OPD3() //GRP2 Ev,CL
{
	thereg = MODRM_REG(params.modrm);
	if (modrm_check16(&params,1,1)) return; //Abort when needed!
	if (modrm_check16(&params,1,0)) return; //Abort when needed!
	oper1 = modrm_read16(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //ROL
			debugger_setcommand("ROLW %s,CL",&modrm_param2);
			break;
		case 1: //ROR
			debugger_setcommand("RORW %s,CL",&modrm_param2);
			break;
		case 2: //RCL
			debugger_setcommand("RCLW %s,CL",&modrm_param2);
			break;
		case 3: //RCR
			debugger_setcommand("RCRW %s,CL",&modrm_param2);
			break;
		case 4: //SHL
			debugger_setcommand("SHLW %s,CL",&modrm_param2);
			break;
		case 5: //SHR
			debugger_setcommand("SHRW %s,CL",&modrm_param2);
			break;
		case 6: //--- Unknown Opcode! ---
			debugger_setcommand("<UNKNOWN MODR/M: GRP2(w) /6, CL>");
			break;
		case 7: //SAR
			debugger_setcommand("SARW %s,CL",&modrm_param2);
			break;
		default:
			break;
		}
	}
	modrm_write16(&params,1,op_grp2_16(REG_CL,1),0);
}


void CPU8086_OPF6() //GRP3a Eb
{
	thereg = MODRM_REG(params.modrm);
	if (modrm_check8(&params,1,1)) return; //Abort when needed!
	if ((MODRM_REG(params.modrm)>1) && (MODRM_REG(params.modrm)<4))
	{
		if (modrm_check8(&params,1,0)) return; //Abort when needed!
	}
	oper1b = modrm_read8(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger8(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //TEST modrm8, imm8
		case 1: //--- Undocumented opcode, same as above!
			debugger_setcommand("TESTB %s,%02x",&modrm_param2,immb);
			break;
		case 2: //NOT
			debugger_setcommand("NOTB %s",&modrm_param2);
			break;
		case 3: //NEG
			debugger_setcommand("NEGB %s",&modrm_param2);
			break;
		case 4: //MUL
			debugger_setcommand("MULB %s",&modrm_param2);
			break;
		case 5: //IMUL
			debugger_setcommand("IMULB %s",&modrm_param2);
			break;
		case 6: //DIV
			modrm_generateInstructionTEXT("DIVB",8,0,PARAM_MODRM2);
			break;
		case 7: //IDIV
			modrm_generateInstructionTEXT("IDIVB",8,0,PARAM_MODRM2);
			break;
		default:
			break;
		}
	}
	op_grp3_8();
	if ((MODRM_REG(params.modrm)>1) && (MODRM_REG(params.modrm)<4))
	{
		modrm_write8(&params,1,res8);
	}
}
void CPU8086_OPF7() //GRP3b Ev
{
	thereg = MODRM_REG(params.modrm);
	if (modrm_check16(&params,1,1)) return; //Abort when needed!
	if ((thereg>1) && (thereg<4)) //NOT/NEG?
	{
		if (modrm_check16(&params,1,0)) return; //Abort when needed!
	}
	oper1 = modrm_read16(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,0,1); //Get src!
		switch (thereg) //What function?
		{
		case 0: //TEST modrm16, imm16
		case 1: //--- Undocumented opcode, same as above!
			debugger_setcommand("TESTW %s,%02x",&modrm_param2,immw);
			break;
		case 2: //NOT
			modrm_generateInstructionTEXT("NOTW",16,0,PARAM_MODRM2);
			break;
		case 3: //NEG
			modrm_generateInstructionTEXT("NEGW",16,0,PARAM_MODRM2);
			break;
		case 4: //MUL
			modrm_generateInstructionTEXT("MULW",16,0,PARAM_MODRM2);
			break;
		case 5: //IMUL
			modrm_generateInstructionTEXT("IMULW",16,0,PARAM_MODRM2);
			break;
		case 6: //DIV
			modrm_generateInstructionTEXT("DIVW",16,0,PARAM_MODRM2);
			break;
		case 7: //IDIV
			modrm_generateInstructionTEXT("IDIVW",16,0,PARAM_MODRM2);
			break;
		default:
			break;
		}
	}
	op_grp3_16();
	if ((thereg>1) && (thereg<4)) //NOT/NEG?
	{
		modrm_write16(&params,1,res16,0);
	}
}
//All OK up till here.

/*

DEBUG: REALLY SUPPOSED TO HANDLE HERE?

*/

void CPU8086_OPFE() //GRP4 Eb
{
	INLINEREGISTER byte tempcf;
	modrm_debugger8(&params,0,1);
	switch (MODRM_REG(params.modrm)) //What function?
	{
	case 0: //INC
		if (modrm_check8(&params,1,1)) return; //Abort when needed!
		if (modrm_check8(&params,1,0)) return; //Abort when needed!
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT("INCB",8,0,PARAM_MODRM2); //INC!
		}
		tempcf = FLAG_CF;
		res8 = modrm_read8(&params,1)+1;
		flag_add8(modrm_read8(&params,1),1);
		FLAGW_CF(tempcf);
		modrm_write8(&params,1,res8);
		break;
	case 1: //DEC
		if (modrm_check8(&params,1,1)) return; //Abort when needed!
		if (modrm_check8(&params,1,0)) return; //Abort when needed!
		if (cpudebugger) //Debugger on?
		{
			modrm_generateInstructionTEXT("DECB",8,0,PARAM_MODRM2); //DEC!
		}
		tempcf = FLAG_CF;
		res8 = modrm_read8(&params,1)-1;
		flag_sub8(modrm_read8(&params,1),1);
		FLAGW_CF(tempcf);
		modrm_write8(&params,1,res8);
		break;
	default: //Unknown opcode or special?
		CPU_unkOP(); //Execute the unknown opcode exception handler, if any!
		break;
	}
}

void CPU8086_OPFF() //GRP5 Ev
{
	thereg = MODRM_REG(params.modrm);
	if (modrm_check16(&params,1,1)) return; //Abort when needed!
	oper1 = modrm_read16(&params,1);
	ea = modrm_offset16(&params,1);
	if (cpudebugger) //Debugger on?
	{
		modrm_debugger16(&params,0,1); //Get src!
		switch (MODRM_REG(params.modrm)) //What function?
		{
		case 0: //INC modrm8
			modrm_generateInstructionTEXT("INCW",16,0,PARAM_MODRM2); //INC!
			break;
		case 1: //DEC modrm8
			modrm_generateInstructionTEXT("DECW",16,0,PARAM_MODRM2); //DEC!
			break;
		case 2: //CALL
			modrm_generateInstructionTEXT("CALL",16,0,PARAM_MODRM2); //CALL!
			break;
		case 3: //CALL Mp (Read address word and jump there)
			modrm_generateInstructionTEXT("CALL",16,0,PARAM_MODRM2); //Jump to the address pointed here!
			//debugger_setcommand("CALL %04X:%04X",MMU_rw(CPU_SEGMENT_CS,REG_CS,ea,0),MMU_rw(CPU_SEGMENT_CS,REG_CS,ea+2,0)); //Based on CALL Ap
			break;
		case 4: //JMP
			modrm_generateInstructionTEXT("JMP",16,0,PARAM_MODRM2); //JMP to the register!
			break;
		case 5: //JMP Mp
			modrm_generateInstructionTEXT("JMP",16,0,PARAM_MODRM2); //Jump to the address pointed here!
			//debugger_setcommand("JMP %04X:%04X",MMU_rw(CPU_SEGMENT_CS,REG_CS,ea,0),MMU_rw(CPU_SEGMENT_CS,REG_CS,ea+2,0)); //JMP to destination!
			break;
		case 6: //PUSH
			modrm_generateInstructionTEXT("PUSH",16,0,PARAM_MODRM2); //PUSH!
			break;
		case 7: //---
			debugger_setcommand("<UNKNOWN Opcode: GRP5(w) /7>");
			break;
		default:
			break;
		}
	}
	op_grp5();
}

/*

Special stuff for NO COprocessor (8087) present/available (default)!

*/


void FPU8087_noCOOP(){
	debugger_setcommand("<No COprocessor OPcodes implemented!>");
	CPU[activeCPU].cycles_OP = MODRM_EA(params)?8+MODRM_EA(params):2; //No hardware interrupt to use anymore!
}

void unkOP_8086() //Unknown opcode on 8086?
{
	//dolog("8086","Unknown opcode on 8086: %02X",CPU[activeCPU].lastopcode); //Last read opcode!
	CPU_unkOP(); //Execute the unknown opcode exception handler, if any!
}

//Gecontroleerd: 100% OK!

//Now, the GRP opcodes!

OPTINLINE void op_grp2_cycles(byte cnt, byte varshift)
{
	switch (varshift) //What type of shift are we using?
	{
	case 0: //Reg/Mem with 1 shift?
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
			if (varshift&4) CPU_addWordMemoryTiming(); /*To memory?*/
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
			if (varshift&4) CPU_addWordMemoryTiming(); /*To memory?*/
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
			if (varshift&4) CPU_addWordMemoryTiming(); /*To memory?*/
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 8 + (cnt << 2); //Reg
		}
		break;
	}
}

byte op_grp2_8(byte cnt, byte varshift) {
	//word d,
	INLINEREGISTER word s, shift, oldCF, msb;
	//if (cnt>0x8) return(oper1b); //NEC V20/V30+ limits shift count
	s = oper1b;
	oldCF = FLAG_CF;
	if (EMULATED_CPU >= CPU_NECV30) cnt &= 0x1F; //Clear the upper 3 bits to become a NEC V20/V30+!
	switch (thereg) {
	case 0: //ROL r/m8
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) FLAGW_CF(1); else FLAGW_CF(0);
			s = s << 1;
			s = s | FLAG_CF;
		}
		if (cnt) FLAGW_OF(FLAG_CF ^ ((s >> 7) & 1));
		break;

	case 1: //ROR r/m8
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			s = (s >> 1) | (FLAG_CF << 7);
		}
		if (cnt) FLAGW_OF((s >> 7) ^ ((s >> 6) & 1));
		break;

	case 2: //RCL r/m8
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			if (s & 0x80) FLAGW_CF(1); else FLAGW_CF(0);
			s = s << 1;
			s = s | oldCF;
		}
		if (cnt) FLAGW_OF(FLAG_CF ^ ((s >> 7) & 1));
		break;

	case 3: //RCR r/m8
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			FLAGW_CF(s & 1);
			s = (s >> 1) | (oldCF << 7);
		}
		if (cnt) FLAGW_OF((s >> 7) ^ ((s >> 6) & 1));
		break;

	case 4: case 6: //SHL r/m8
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) FLAGW_CF(1); else FLAGW_CF(0);
			s = (s << 1) & 0xFF;
		}
		if (cnt) FLAGW_OF((FLAG_CF ^ (s >> 7)));
		flag_szp8((uint8_t)(s&0xFF)); break;

	case 5: //SHR r/m8
		if (cnt == 1) FLAGW_OF((s & 0x80) ? 1 : 0); else FLAGW_OF(0);
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			s = s >> 1;
		}
		flag_szp8((uint8_t)(s & 0xFF)); break;

	case 7: //SAR r/m8
		if (cnt) FLAGW_OF(0);
		msb = s & 0x80;
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			s = (s >> 1) | msb;
		}
		byte tempSF;
		tempSF = FLAG_SF; //Save the SF!
		flag_szp8((uint8_t)(s & 0xFF)); break;
		if (!cnt) //Nothing done?
		{
			FLAGW_SF(tempSF); //We don't update when nothing's done!
		}
	}
	op_grp2_cycles(cnt, varshift);
	return(s & 0xFF);
}

word op_grp2_16(byte cnt, byte varshift) {
	//uint32_t d,
	INLINEREGISTER uint_32 s, shift, oldCF, msb;
	//if (cnt>0x10) return(oper1); //NEC V20/V30+ limits shift count
	if (EMULATED_CPU >= CPU_NECV30) cnt &= 0x1F; //Clear the upper 3 bits to become a NEC V20/V30+!
	s = oper1;
	oldCF = FLAG_CF;
	switch (thereg) {
	case 0: //ROL r/m16
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) FLAGW_CF(1); else FLAGW_CF(0);
			s = s << 1;
			s = s | FLAG_CF;
		}
		if (cnt) FLAGW_OF(FLAG_CF ^ ((s >> 15) & 1));
		break;

	case 1: //ROR r/m16
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			s = (s >> 1) | (FLAG_CF << 15);
		}
		if (cnt) FLAGW_OF((s >> 15) ^ ((s >> 14) & 1));
		break;

	case 2: //RCL r/m16
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

	case 3: //RCR r/m16
		if (cnt) FLAGW_OF(((s >> 15) & 1) ^ FLAG_CF);
		for (shift = 1; shift <= cnt; shift++) {
			oldCF = FLAG_CF;
			FLAGW_CF(s & 1);
			s = (s >> 1) | (oldCF << 15);
			//oldCF = s&1;
			//s = (s<<1)+(FLAG_CF<<16);
			//FLAG_CF = oldCF;
		}
		if (cnt) FLAGW_OF((s >> 15) ^ ((s >> 14) & 1));
		break;

	case 4: case 6: //SHL r/m16
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) FLAGW_CF(1); else FLAGW_CF(0);
			s = (s << 1) & 0xFFFF;
		}
		if ((cnt) && (FLAG_CF == (s >> 15))) FLAGW_OF(0); else FLAGW_OF(1);
		flag_szp16(s); break;

	case 5: //SHR r/m16
		if (cnt) FLAGW_OF((s & 0x8000) ? 1 : 0);
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			s = s >> 1;
		}
		flag_szp16(s); break;

	case 7: //SAR r/m16
		if (cnt) FLAGW_OF(0);
		msb = s & 0x8000; //Read the MSB!
		for (shift = 1; shift <= cnt; shift++) {
			FLAGW_CF(s & 1);
			s = (s >> 1) | msb;
		}
		byte tempSF;
		tempSF = FLAG_SF; //Save the SF!
		flag_szp16(s);
		if (!cnt) //Nothing done?
		{
			FLAGW_SF(tempSF); //We don't update when nothing's done!
		}
		break;
	}
	op_grp2_cycles(cnt, varshift|4);
	return(s & 0xFFFF);
}

OPTINLINE void op_div8(word valdiv, byte divisor) {
	if (!divisor) { CPU_exDIV0(); return; }
	if ((valdiv / (word)divisor) > 0xFF) { CPU_exDIV0(); return; }
	REG_AH = valdiv % (word)divisor;
	REG_AL = valdiv / (word)divisor;
}

OPTINLINE void op_idiv8(word valdiv, byte divisor) {
	//word v1, v2,
	if (divisor == 0) { CPU_exDIV0(); return; }
	/*
	word s1, s2, d1, d2;
	int sign;
	s1 = valdiv;
	s2 = divisor;
	sign = (((s1 ^ s2) & 0x8000) != 0);
	s1 = (s1 < 0x8000) ? s1 : ((~s1 + 1) & 0xffff);
	s2 = (s2 < 0x8000) ? s2 : ((~s2 + 1) & 0xffff);
	d1 = s1 / s2;
	d2 = s1 % s2;
	if (d1 & 0xFF00) { CPU_exDIV0(); return; }
	if (sign) {
	d1 = (~d1 + 1) & 0xff;
	d2 = (~d2 + 1) & 0xff;
	}
	REG_AH = d2;
	REG_AL = d1;
	*/

	//Same, but with normal instructions!
	union
	{
		word valdivw;
		sword valdivs;
	} dataw1, //For loading the signed value of the registers!
		dataw2; //For performing calculations!

	union
	{
		byte divisorb;
		sbyte divisors;
	} datab1, //For loading the data
		datab2; //For loading the result and test it against overflow!
				//For converting the data to signed values!

	dataw1.valdivw = valdiv; //Load word!
	datab1.divisorb = divisor; //Load divisor!

	dataw2.valdivs = dataw1.valdivs; //Set and...
	dataw2.valdivs /= datab1.divisors; //... Divide!

	datab2.divisors = (sbyte)dataw2.valdivs; //Try to load the signed result!
	if (datab2.divisors != dataw2.valdivs) { CPU_exDIV0(); return; } //Overflow (data loss)!

	REG_AL = datab2.divisors; //Divided!
	dataw2.valdivs = dataw1.valdivs; //Reload and...
	dataw2.valdivs %= datab1.divisors; //... Modulo!
	datab1.divisors = (sbyte)dataw2.valdivs; //Convert to 8-bit!
	REG_AH = datab1.divisorb; //Move rest into result!

							  //if (valdiv > 32767) v1 = valdiv - 65536; else v1 = valdiv;
							  //if (divisor > 127) v2 = divisor - 256; else v2 = divisor;
							  //v1 = valdiv;
							  //v2 = signext(divisor);
							  //if ((v1/v2) > 255) { CPU8086_int(0); return; }
							  //regs.byteregs[regal] = (v1/v2) & 255;
							  //regs.byteregs[regah] = (v1 % v2) & 255;
}

byte tmps,tmpp; //Sign/parity backup!

extern byte CPU_databussize; //Current data bus size!

byte tempAL;
word tempAX;
uint_32 tempDXAX;

void op_grp3_8() {
	//uint32_t d1, d2, s1, s2, sign;
	//word d, s;
	oper1 = signext(oper1b); oper2 = signext(oper2b);
	switch (thereg) {
	case 0: case 1: //TEST
		CPU8086_internal_TEST8(oper1b,immb,3);
		break;

	case 2: //NOT
		res8 = ~oper1b;
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
		res8 = (~oper1b) + 1;
		flag_sub8(0, oper1b);
		if (res8 == 0) FLAGW_CF(0); else FLAGW_CF(1);
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 16 + MODRM_EA(params); //Mem!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 3; //Reg!
		}
		break;

	case 4: //MULB
		tempAL = REG_AL; //Save a backup for calculating cycles!
		temp1.val32 = (uint32_t)oper1b * (uint32_t)REG_AL;
		REG_AX = temp1.val16 & 0xFFFF;
		FLAGW_OF(((word)REG_AL != REG_AX)?1:0);
		FLAGW_CF(FLAG_OF); //Same!
		if ((EMULATED_CPU==CPU_8086) && REG_AX) FLAGW_ZF(0); //8086/8088 clears the Zero flag when not zero only.
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 76+MODRM_EA(params); //Mem max!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 70; //Reg!
		}
		if (NumberOfSetBits(tempAL)>1) //More than 1 bit set?
		{
			CPU[activeCPU].cycles_OP += NumberOfSetBits(tempAL) - 1; //1 cycle for all bits more than 1 bit set!
		}
		break;

	case 5: //IMULB
		oper1 = oper1b;
		temp1.val32 = REG_AL;
		temp2.val32 = oper1b;
		//Sign extend!
		if ((temp1.val8 & 0x80) == 0x80) temp1.val32 |= 0xFFFFFF00;
		if ((temp2.val8 & 0x80) == 0x80) temp2.val32 |= 0xFFFFFF00;
		//Multiply and convert to 16-bit!
		temp3.val32s = temp1.val32s; //Load and...
		temp3.val32s *= temp2.val32s; //Multiply!
		REG_AX = temp3.val16; //Load into AX!
		FLAGW_OF(((sword)(unsigned2signed8(REG_AL) != unsigned2signed16(REG_AX)) ? 1 : 0));
		FLAGW_CF(FLAG_OF); //Same!
		FLAGW_SF((REG_AX>>15)&1); //Sign flag is affected!
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 86 + MODRM_EA(params); //Mem max!
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 80; //Reg!
		}
		break;

	case 6: //DIV
		op_div8(REG_AX, oper1b);
		break;

	case 7: //IDIV
		op_idiv8(REG_AX, oper1b);
		break;
	}
}

OPTINLINE void op_div16(uint32_t valdiv, word divisor) {
	//word v1, v2;
	if (!divisor) { CPU_exDIV0(); return; }
	if ((valdiv / (uint32_t)divisor) > 0xFFFF) { CPU_exDIV0(); return; }
	REG_DX = valdiv % (uint32_t)divisor;
	REG_AX = valdiv / (uint32_t)divisor;
}

OPTINLINE void op_idiv16(uint32_t valdiv, word divisor) {
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
		uint_32 valdivw;
		int_32 valdivs;
	} dataw1, //For loading the signed value of the registers!
		dataw2; //For performing calculations!

	union
	{
		word divisorb;
		sword divisors;
	} datab1, datab2; //For converting the data to signed values!

	dataw1.valdivw = valdiv; //Load word!
	datab1.divisorb = divisor; //Load divisor!

	dataw2.valdivs = dataw1.valdivs; //Set and...
	dataw2.valdivs /= datab1.divisors; //... Divide!

	datab2.divisors = (sword)dataw2.valdivs; //Try to load the signed result!
	if ((int_32)dataw2.valdivw != (int_32)datab2.divisors) { CPU_exDIV0(); return; } //Overflow (data loss)!

	REG_AX = datab2.divisorb; //Divided!
	dataw2.valdivs = dataw1.valdivs; //Reload and...
	dataw2.valdivs %= datab1.divisors; //... Modulo!
	datab1.divisors = (sword)dataw2.valdivs; //Convert to 8-bit!
	REG_DX = datab1.divisorb; //Move rest into result!

							  //if (valdiv > 0x7FFFFFFF) v1 = valdiv - 0xFFFFFFFF - 1; else v1 = valdiv;
							  //if (divisor > 32767) v2 = divisor - 65536; else v2 = divisor;
							  //if ((v1/v2) > 65535) { CPU8086_int(0); return; }
							  //temp3 = (v1/v2) & 65535;
							  //regs.wordregs[regax] = temp3;
							  //temp3 = (v1%v2) & 65535;
							  //regs.wordregs[regdx] = temp3;
}

void op_grp3_16() {
	//uint32_t d1, d2, s1, s2, sign;
	//word d, s;
	//oper1 = signext(oper1b); oper2 = signext(oper2b);
	//sprintf(msg, "  Oper1: %04X    Oper2: %04X\n", oper1, oper2); print(msg);
	switch (thereg) {
	case 0: case 1: //TEST
		CPU8086_internal_TEST16(oper1, immw, 3);
		break;
	case 2: //NOT
		res16 = ~oper1;
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
		res16 = (~oper1) + 1;
		flag_sub16(0, oper1);
		if (res16) FLAGW_CF(1); else FLAGW_CF(0);
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
		temp1.val32 = (uint32_t)oper1 * (uint32_t)REG_AX;
		REG_AX = temp1.val16;
		REG_DX = temp1.val16high;
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
		temp2.val32 = oper1;
		//Sign extend!
		if (temp1.val16 & 0x8000) temp1.val32 |= 0xFFFF0000;
		if (temp2.val16 & 0x8000) temp2.val32 |= 0xFFFF0000;
		temp3.val32s = temp1.val32s; //Load and...
		temp3.val32s *= temp2.val32s; //Signed multiplication!
		REG_AX = temp3.val16; //into register ax
		REG_DX = temp3.val16high; //into register dx
		FLAGW_OF(((int_32)temp3.val16s != temp3.val32s)?1:0); //Overflow occurred?
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
		op_div16(((uint32_t)REG_DX << 16) | REG_AX, oper1);
		if (MODRM_EA(params)) CPU_addWordMemoryTiming(); /*To memory?*/
		break;
	case 7: //IDIV
		op_idiv16(((uint32_t)REG_DX << 16) | REG_AX, oper1); break;
	}
}

void op_grp5() {
	MODRM_PTR info; //To contain the info!
	INLINEREGISTER byte tempCF;
	word destCS;
	switch (thereg) {
	case 0: //INC Ev
		if (modrm_check16(&params,1,0)) return; //Abort when needed!
		oper2 = 1;
		tempCF = FLAG_CF;
		op_add16();
		FLAGW_CF(tempCF);
		modrm_write16(&params, 1, res16, 0);
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
			CPU_addWordMemoryTiming(); /*To memory?*/
			CPU_addWordMemoryTiming(); /*To memory?*/
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 2; //Reg
		}
		break;
	case 1: //DEC Ev
		if (modrm_check16(&params,1,0)) return; //Abort when needed!
		oper2 = 1;
		tempCF = FLAG_CF;
		op_sub16();
		FLAGW_CF(tempCF);
		modrm_write16(&params, 1, res16, 0);
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 15 + MODRM_EA(params); //Mem
			CPU_addWordMemoryTiming(); /*To memory?*/
			CPU_addWordMemoryTiming(); /*To memory?*/
		}
		else //Reg?
		{
			CPU[activeCPU].cycles_OP = 2; //Reg
		}
		break;
	case 2: //CALL Ev
		if (checkStackAccess(1,1,0)) return; //Abort when needed!
		CPU_PUSH16(&REG_IP);
		REG_IP = oper1;
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 21 + MODRM_EA(params); /* Intrasegment indirect through memory */
			CPU_addWordMemoryTiming(); /*To memory?*/
			CPU_addWordMemoryTiming(); /*To memory?*/
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 16; /* Intrasegment indirect through register */
		}
		CPU_flushPIQ(); //We're jumping to another address!
		break;
	case 3: //CALL Mp
		modrm_decode16(&params, &info, 1); //Get data!

		modrm_addoffset = 0; //First IP!
		if (modrm_check16(&params,1,1)) return; //Abort when needed!
		modrm_addoffset = 2; //Then destination CS!
		if (modrm_check16(&params,1,1)) return; //Abort when needed!

		modrm_addoffset = 0; //First IP!
		destEIP = modrm_read16(&params,1); //Get destination IP!
		CPUPROT1
		modrm_addoffset = 2; //Then destination CS!
		destCS = modrm_read16(&params,1); //Get destination CS!
		CPUPROT1
		modrm_addoffset = 0;
		CPU8086_CALLF(destCS,destEIP); //Call the destination address!
		CPUPROT1
		if (MODRM_EA(params)) //Mem?
		{
			CPU[activeCPU].cycles_OP = 37 + MODRM_EA(params); /* Intersegment indirect */
			CPU_addWordMemoryTiming(); /*To memory?*/
			CPU_addWordMemoryTiming(); /*To memory?*/
			CPU_addWordMemoryTiming(); /*To memory?*/
			CPU_addWordMemoryTiming(); /*To memory?*/
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
		REG_IP = oper1;
		CPU_flushPIQ(); //We're jumping to another address!
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 18 + MODRM_EA(params); /* Intrasegment indirect through memory */
			CPU_addWordMemoryTiming(); /*To memory?*/
		}
		else //Register?
		{
			CPU[activeCPU].cycles_OP = 11; /* Intrasegment indirect through register */
		}
		break;
	case 5: //JMP Mp
		modrm_decode16(&params, &info, 1); //Get data!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset,1,getCPL())) return; //Abort on fault!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset+1,1,getCPL())) return; //Abort on fault!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset+2,1,getCPL())) return; //Abort on fault!
		if (checkMMUaccess(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset+3,1,getCPL())) return; //Abort on fault!

		destEIP = MMU_rw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset, 0);
		CPUPROT1
		destCS = MMU_rw(get_segment_index(info.segmentregister), info.mem_segment, info.mem_offset + 2, 0);
		CPUPROT1
		segmentWritten(CPU_SEGMENT_CS, destCS, 1);
		CPUPROT1
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 24 + MODRM_EA(params); /* Intersegment indirect through memory */
			CPU_addWordMemoryTiming(); /*To memory?*/
			CPU_addWordMemoryTiming(); /*To memory?*/
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
		if (checkStackAccess(1,1,0)) return; //Abort on fault!
		CPU_PUSH16(&oper1); break;
		CPUPROT1
		if (MODRM_EA(params)) //Memory?
		{
			CPU[activeCPU].cycles_OP = 16+MODRM_EA(params); /*Push Mem!*/
			CPU_addWordMemoryTiming(); /*To memory?*/
			CPU_addWordMemoryTiming(); /*To memory?*/
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
