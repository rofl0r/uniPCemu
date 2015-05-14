#define IS_CPU
#include "headers/cpu/cpu.h"
#include "headers/cpu/interrupts.h"
#include "headers/mmu/mmu.h"
#include "headers/support/signedness.h" //CPU support!
#include "headers/cpu/8086/cpu_OP8086.h" //8086 comp.
#include "headers/debugger/debugger.h" //Debugger support!
#include "headers/emu/gpu/gpu.h" //Start&StopVideo!
#include "headers/cpu/callback.h" //CB support!
#include "headers/cpu/80286/protection.h"
#include "headers/support/zalloc.h" //For allocating registers etc.

//ALL INTERRUPTS

#include "headers/interrupts/interrupt05.h"
#include "headers/interrupts/interrupt10.h"
#include "headers/interrupts/interrupt11.h"
#include "headers/interrupts/interrupt13.h"
#include "headers/interrupts/interrupt16.h"
#include "headers/interrupts/interrupt18.h"
#include "headers/interrupts/interrupt19.h"

byte activeCPU = 0; //What CPU is currently active?

byte cpudebugger; //To debug the CPU?

CPU_type CPU[MAXCPUS]; //The CPU data itself!
extern Handler opcode_jmptbl[NUMCPUS][0x100][2]; //x86 opcode table
//extern Handler debug_jmptbl[NUMCPUS][0x100][2]; //x86 debug opcode table
extern Handler soft_interrupt_jmptbl[]; //Interrupt call table (software INT instructions)

//Opcode&Stack sizes: 0=16-bits, 1=32-bits!
byte CPU_Operand_size[2] = { 0 , 0 }; //Operand size for this opcode!
byte CPU_Address_size[2] = { 0 , 0 }; //Address size for this opcode!
byte CPU_StackAddress_size[2] = { 0 , 0 }; //Stack Address size for this opcode (determines whether SP or ESP is used)!

//Internal prefix table for below functions!
byte CPU_prefixes[2][32]; //All prefixes, packed in a bitfield!

//More info about interrupts: http://www.bioscentral.com/misc/interrupts.htm#
//More info about interrupts: http://www.bioscentral.com/misc/interrupts.htm#

uint_32 makeupticks; //From PIC?

//Now the code!

byte calledinterruptnumber = 0; //Called interrupt number for unkint funcs!

void call_hard_inthandler(byte intnr) //Hardware interrupt handler (FROM hardware only, or int>=0x20 for software call)!
{
//Now call handler!
	calledinterruptnumber = intnr; //Save called interrupt number!
	CPU_INT(intnr); //Call interrupt!
}

void CPU_8086_RETI() //Not from CPU!
{
	CPU_IRET(); //RETURN FROM INTERRUPT!
}

extern byte reset; //Reset?

void CPU_ErrorCallback_RESET() //Error callback with error code!
{
	debugrow("Resetting emulator: Error callback called!");
	reset = 1; //Reset the emulator!
}








void copyint(byte src, byte dest) //Copy interrupt handler pointer to different interrupt!
{
	MMU_ww(-1,0x0000,(dest<<2),MMU_rw(-1,0x0000,(src<<2),0)); //Copy segment!
	MMU_ww(-1,0x0000,(dest<<2)|2,MMU_rw(-1,0x0000,((src<<2)|2),0)); //Copy offset!
}



int STACK_SIZE = 2; //Stack item in bytes! (4 official for 32-bit, 2 for 16-bit?)

void resetCPU() //Initialises CPU!
{
	memset(&CPU,0,sizeof(CPU)); //Reset the CPU fully!
	CPU_initRegisters(); //Initialise the registers!
	CPU_initPrefixes(); //Initialise all prefixes!
	CPU_resetMode(); //Reset the mode to the default mode!
	if (EMULATED_CPU==CPU_8086 || EMULATED_CPU==CPU_80186) //Emulating 80(1)86?
	{
		STACK_SIZE = 2; //2-byte stack!
	}
	else //80286+?
	{
		STACK_SIZE = 4; //4-byte stack!
	}
	//Default: not waiting for interrupt to occur on startup!
	//Not waiting for TEST pin to occur!
	//Default: not blocked!
	//Continue interrupt call (hardware)?
	CPU[activeCPU].running = 1; //We're running!
	
	CPU[activeCPU].lastopcode = 0; //Last opcode, default to 0 and unknown?
}

//data order is low-high, e.g. word 1234h is stored as 34h, 12h

byte CPU_readOP() //Reads the operation (byte) at CS:EIP
{
	return MMU_rb(CPU_SEGMENT_CS,CPU[activeCPU].registers->CS,CPU[activeCPU].registers->EIP++,1); //Read OPcode!
}

word CPU_readOPw() //Reads the operation (word) at CS:EIP
{
	word result;
	result = CPU_readOP(); //Read OPcode!
	result |= CPU_readOP()<<8; //Read OPcode!
	return result; //Give result!
}

uint_32 CPU_readOPdw() //Reads the operation (32-bit unsigned integer) at CS:EIP
{
	uint_32 result;
	result = CPU_readOPw(); //Read OPcode!
	result |= CPU_readOPw()<<16; //Read OPcode!
	return result; //Give result!
}





/*
0xF3 Used with string REP, REPE/REPZ
0xF2 REPNE/REPNZ prefix
0xF0 LOCK prefix
0x2E CS segment override prefix
0x36 SS segment override prefix
0x3E DS segment override prefix
0x26 ES segment override prefix
0x64 FS segment override prefix
0x65 GS segment override prefix
0x66 Operand-size override
0x67 Address-size override
*/

void CPU_setprefix(byte prefix) //Sets a prefix on!
{
	CPU_prefixes[activeCPU][(prefix>>3)] |= (128>>(prefix&7)); //Have prefix!
	switch (prefix) //Which prefix?
	{
	case 0x2E: //CS segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_CS; //Override DS to CS!
		break;
	case 0x36: //SS segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_SS; //Override DS to SS!
		break;
	case 0x3E: //DS segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_DS; //Override SS to DS!
		break;
	case 0x26: //ES segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_ES; //Override DS to ES!
		break;
	case 0x64: //FS segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_FS; //Override DS to FS!
		break;
	case 0x65: //GS segment override prefix
		CPU[activeCPU].segment_register = CPU_SEGMENT_GS; //Override DS to GS!
		break;
	default: //Unknown special prefix action?
		break; //Do nothing!
	}
}

byte CPU_getprefix(byte prefix) //Prefix set?
{
	return (CPU_prefixes[activeCPU][prefix>>3]&(128>>(prefix&7)))>0; //Get prefix set or reset!
}

void CPU_initPrefixes()
{
	byte c;
	for (c=0; c<sizeof(CPU_prefixes[0]); c++)
	{
		CPU_prefixes[activeCPU][c] = 0; //Reset!
	}
}

void CPU_resetPrefixes() //Resets all prefixes we use!
{
	memset(CPU_prefixes[activeCPU],0,sizeof(CPU_prefixes[activeCPU])); //Reset prefixes!
}

int CPU_isPrefix(byte prefix)
{
	switch (prefix) //What prefix/opcode?
	{
	//First, normal instruction prefix codes:
		case 0xF2: //REPNE/REPNZ prefix
		case 0xF3: //REPZ
		case 0xF0: //LOCK prefix
		case 0x2E: //CS segment override prefix
		case 0x36: //SS segment override prefix
		case 0x3E: //DS segment override prefix
		case 0x26: //ES segment override prefix
		case 0x64: //FS segment override prefix
		case 0x65: //GS segment override prefix
		case 0x66: //Operand-size override
		case 0x67: //Address-size override
			return 1; //Use!
		default: //It's a normal OPcode?
			return 0; //No prefix!
			break; //Not use others!
	}

	return 0; //No prefix!
}

int DATA_SEGMENT_DESCRIPTOR_B_BIT() //80286+: Gives the B-Bit of the DATA DESCRIPTOR TABLE FOR SS-register!
{
	if (EMULATED_CPU<=CPU_80186) //8086-80186?
	{
		return 0; //Always 16-bit descriptor!
	}

	return CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_SS].D_B; //Give the B-BIT of the SS-register!
}



byte CPU_readOP_prefix() //Reads OPCode with prefix(es)!
{
	byte OP; //The current opcode!
	CPU_resetPrefixes(); //Reset all prefixes for this opcode!

	OP = CPU_readOP(); //Read opcode or prefix?
	for (;CPU_isPrefix(OP);) //We're a prefix?
	{
		CPU_setprefix(OP); //Set the prefix ON!
		OP = CPU_readOP(); //Next opcode/prefix!
	}
	//Now we have the opcode and prefixes set or reset!

//Determine the stack&attribute sizes!

	CPU_StackAddress_size[activeCPU] = DATA_SEGMENT_DESCRIPTOR_B_BIT(); //16 or 32-bits stack!
	if (CPU_StackAddress_size[activeCPU]) //32-bits stack? We're a 32-bit Operand&Address size!
	{
		CPU_Operand_size[activeCPU] = 1; //Set!
		CPU_Address_size[activeCPU] = 1; //Set!
	}
	if (CPU_getprefix(0x66)) //Invert operand size?
	{
		CPU_Operand_size[activeCPU] = !CPU_Operand_size[activeCPU]; //Invert!
	}
	if (CPU_getprefix(0x67)) //Invert address size?
	{
		CPU_Address_size[activeCPU] = !CPU_Address_size[activeCPU]; //Invert!
	}
	return OP; //Give the OPCode!
}

void alloc_CPUregisters()
{
	CPU[activeCPU].registers = (CPU_registers *)zalloc(sizeof(*CPU[activeCPU].registers), "CPU_REGISTERS", NULL); //Allocate the registers!
	if (!CPU[activeCPU].registers)
	{
		raiseError("CPU","Failed to allocate the required registers!");
	}
}

void free_CPUregisters()
{
	if (CPU[activeCPU].registers) //Still allocated?
	{
		freez((void **)&CPU[activeCPU].registers,sizeof(*CPU[activeCPU].registers),"CPU_REGISTERS"); //Release the registers if needed!
	}
}

void CPU_initRegisters() //Init the registers!
{
	static byte CSAccessRights = 0x93; //Default CS access rights, overwritten during first software reset!
	if (CPU[activeCPU].registers) //Already allocated?
	{
		CSAccessRights = CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].AccessRights; //Save old CS acccess rights to use now (after first reset)!
		free_CPUregisters(); //Free the CPU registers!
	}
	alloc_CPUregisters(); //Allocate the CPU registers!

	//Calculation registers
	CPU[activeCPU].registers->EAX = 0;
	CPU[activeCPU].registers->EBX = 0;
	CPU[activeCPU].registers->ECX = 0;
	CPU[activeCPU].registers->EDX = 0;
	
	//Index registers
	CPU[activeCPU].registers->EBP = 0; //Init offset of BP?
	CPU[activeCPU].registers->ESI = 0; //Source index!
	CPU[activeCPU].registers->EDI = 0; //Destination index!

	//Stack registers
	CPU[activeCPU].registers->ESP = 0x0000FFFE; //Init offset of stack (top-1)
	if (EMULATED_CPU>=CPU_80286) //32-bits?
	{
		CPU[activeCPU].registers->ESP = 0xFFFFFFFE; //Start at highest offset!
	}
	CPU[activeCPU].registers->SS = 0; //Stack segment!

	
	//Code location
	CPU[activeCPU].registers->EIP = 0; //Start of executable code!
	CPU[activeCPU].registers->CS = 0xFFFF; //Code segment: default to segment 0xFFFF to start at 0xFFFF0 (bios boot jump)!
	//if (EMULATED_CPU>CPU_80186) //286+?
	{
		CPU[activeCPU].registers->CS = 0xF000; //We're this selector!
		CPU[activeCPU].registers->EIP = 0xFFF0; //We're starting at this offset!
	}
	
	//Data registers!
	CPU[activeCPU].registers->DS = 0; //Data segment!
	CPU[activeCPU].registers->ES = 0; //Extra segment!
	CPU[activeCPU].registers->FS = 0; //Far segment (extra segment)
	CPU[activeCPU].registers->GS = 0; //??? segment (extra segment like FS)
	CPU[activeCPU].registers->EFLAGS = 0x2; //Flags!

//Now the handling of solid state segments (might change, use index for that!)
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_CS] = &CPU[activeCPU].registers->CS; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_SS] = &CPU[activeCPU].registers->SS; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_DS] = &CPU[activeCPU].registers->DS; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_ES] = &CPU[activeCPU].registers->ES; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_FS] = &CPU[activeCPU].registers->FS; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_GS] = &CPU[activeCPU].registers->GS; //Link!
	
	memset(CPU[activeCPU].SEG_DESCRIPTOR, 0, sizeof(CPU[activeCPU].SEG_DESCRIPTOR)); //Clear the descriptor cache!
	//Now, load the default descriptors!
	
	//IDTR
	CPU[activeCPU].registers->IDTR.base = 0;
	CPU[activeCPU].registers->IDTR.limit = 0x3FF;
	
	//GDTR
	CPU[activeCPU].registers->GDTR.base = 0;
	CPU[activeCPU].registers->GDTR.limit = 0xFFFF; //From bochs!
	
	//LDTR (invalid)
	CPU[activeCPU].registers->LDTR.base = CPU[activeCPU].registers->LDTR.limit = 0; //None and invalid!
	
	//TR (also invalid)
	CPU[activeCPU].registers->TR = 0; //No TR!
	
	CPU[activeCPU].registers->CR0_full &= 0x7FFFFFE0; //Clear bit 32 and 4-0!
	
	byte reg=0;
	for (reg = 0; reg<NUMITEMS(CPU[activeCPU].SEG_DESCRIPTOR); reg++) //Process all segment registers!
	{
		CPU[activeCPU].SEG_DESCRIPTOR[reg].base_high = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].base_mid = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].base_low = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].limit_low = 0xFFFF; //64k limit!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].limit_high = 0; //No high limit!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].G = 0; //Byte granularity!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].DATASEGMENT.E = 0; //Expand up!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].DATASEGMENT.W = 1; //Writable!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].P = 1; //Present!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].DPL = 0;
	}
	
	//CS specific!
	CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].AccessRights = CSAccessRights; //Load CS default access rights!
	if (EMULATED_CPU>CPU_80186) //286+?
	{
		CPU[activeCPU].registers->CS = 0xF000; //We're this selector!
		//Pulled low on first load, pulled high on reset:
		CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].base_high = 0xFF;
		CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].base_mid = 0xFF;
	}
}

void doneCPU() //Finish the CPU!
{
	free_CPUregisters(); //Finish the allocated registers!
}

CPU_registers dummyregisters; //Dummy registers!

//Specs for 80386 says we start in REAL mode!
//STDMODE: 0=protected; 1=real; 2=Virtual 8086.

void CPU_resetMode() //Resets the mode!
{
	if (!CPU[activeCPU].registers) CPU_initRegisters(); //Make sure we have registers!
	//Always start in REAL mode!
	CPU[activeCPU].registers->SFLAGS.V8 = 0; //Disable Virtual 8086 mode!
	CPU[activeCPU].registers->CR0.PE = 0; //Real mode!
}

byte getcpumode() //Retrieves the current mode!
{
	static const byte modes[4] = { CPU_MODE_REAL, CPU_MODE_PROTECTED, CPU_MODE_REAL, CPU_MODE_8086 }; //All possible modes (VM86 mode can't exist without Protected Mode!)
	byte mode = 0;
	if (!CPU[activeCPU].registers) CPU_initRegisters(); //Make sure we have registers!
	if (!CPU[activeCPU].registers) CPU[activeCPU].registers = &dummyregisters; //Dummy registers!
	mode = CPU[activeCPU].registers->SFLAGS.V8; //VM86 mode?
	mode <<= 1;
	mode |= CPU[activeCPU].registers->CR0.PE; //Protected mode?
	return modes[mode]; //Mode levels: Real mode > Protected Mode > VM86 Mode!
}












//PUSH and POP values!

int topdown_stack() //Top-down stack?
{
	if (EMULATED_CPU<=CPU_80186) //8086-80186?
	{
		return 1; //Always to-down!
	}
	//We're a 286+, so detect it!
	if (getcpumode()==CPU_MODE_REAL)
	{
		return 1; //Real mode!
	}
	return !((CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_SS].Type & 4)>0); //Real mode=8086; Other=SS segment, bit 4 (off=Topdown stack!)
}

uint_32 getstackaddrsizelimiter()
{
	return CPU_StackAddress_size[activeCPU]? 0xFFFFFFFF : 0xFFFF; //Stack address size!
}

//Memory is the same as PSP: 1234h is 34h 12h, in stack terms reversed, because of back-to-start (top-down) stack!

//Use below functions for the STACK!

void CPU_PUSH8(byte val) //Push Byte!
{
	word v=val; //Convert!
	CPU_PUSH16(&v); //Push 16!
}

byte CPU_POP8()
{
	return (CPU_POP16()&0xFF); //Give the result!
}

void stack_push(byte dword) //Push 16/32-bits to stack!
{
	if (topdown_stack()) //--?
	{
		if (CPU_StackAddress_size[activeCPU]) //32-bits?
		{
			CPU[activeCPU].registers->ESP -= (2 << dword); //Decrease!
		}
		else //16-bits?
		{
			CPU[activeCPU].registers->SP -= (2 << dword); //Decrease!
		}
	}
	else //++?
	{
		if (CPU_StackAddress_size[activeCPU]) //32-bits?
		{
			CPU[activeCPU].registers->ESP += (2 << dword); //Increase!
		}
		else //16-bits?
		{
			CPU[activeCPU].registers->SP += (2 << dword); //Increase!
		}
	}
}

void stack_pop(byte dword) //Push 16/32-bits to stack!
{
	if (topdown_stack()) //++?
	{
		if (CPU_StackAddress_size[activeCPU]) //32-bits?
		{
			CPU[activeCPU].registers->ESP += (2 << dword); //Increase!
		}
		else //16-bits?
		{
			CPU[activeCPU].registers->SP += (2 << dword); //Increase!
		}
	}
	else //--?
	{

		if (CPU_StackAddress_size[activeCPU]) //32-bits?
		{
			CPU[activeCPU].registers->ESP -= (2 << dword); //Decrease!
		}
		else //16-bits?
		{
			CPU[activeCPU].registers->SP -= (2 << dword); //Decrease!
		}
	}
}

void CPU_PUSH16(word *val) //Push Word!
{
	if (EMULATED_CPU<CPU_80386) //286-?
	{
		stack_push(0); //We're pushing a 16-bit value!
		MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), *val); //Put value!
	}
	else //386+?
	{
		word oldval = *val; //Old value!
		stack_push(0); //We're pushing a 16-bit value!
		MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval); //Put value!
	}
}

word CPU_POP16() //Pop Word!
{
	word result;
	result = MMU_rw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), 0); //Get value!
	stack_pop(0); //We're popping a 16-bit value!
	return result; //Give the result!
}

void CPU_PUSH32(uint_32 *val) //Push DWord!
{
	if (EMULATED_CPU<CPU_80386) //286-?
	{
		stack_push(1); //We're pushing a 32-bit value!
		MMU_wdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), *val); //Put value!
	}
	else //386+?
	{
		uint_32 oldval = *val; //Old value!
		stack_push(1); //We're pushing a 32-bit value!
		MMU_wdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval); //Put value!
	}
}

uint_32 CPU_POP32() //Full stack used!
{
	word result;
	result = MMU_rdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), 0); //Get value!
	stack_pop(1); //We're popping a 32-bit value!
	return result; //Give the result!
}











//Final stuff:









char textsegments[][3] =   //Comply to CPU_REGISTER_XX order!
{
	"CS",
	"SS",
	"DS",
	"ES",
	"FS",
	"GS"
};

char *CPU_textsegment(byte defaultsegment) //Plain segment to use!
{
	if (CPU[activeCPU].segment_register==CPU_SEGMENT_DEFAULT) //Default segment?
	{
		return (char *)&textsegments[defaultsegment]; //Default segment!
	}
	return (char *)&textsegments[CPU[activeCPU].segment_register]; //Use Data Segment (or different in case) for data!
}























void CPU_afterexec(); //Prototype for below!

word CPU_exec_CS; //OPCode CS
uint_32 CPU_exec_EIP; //OPCode EIP

void CPU_OP(byte OP) //Normal CPU opcode execution!
{
	int cpu = EMULATED_CPU; //Init cpu!
	byte operandsize = CPU_Operand_size[activeCPU]; //Operand size to use!
	while (!opcode_jmptbl[cpu][OP][operandsize]) //No opcode to handle at current CPU&operand size?
	{
		if (operandsize) //We have an operand size: switch to standard if possible!
		{
			operandsize = 0; //Not anymore!
			continue; //Try again!
		}
		else //No operand size: we're a standard, so go up one cpu and retry!
		{
			operandsize = CPU_Operand_size[activeCPU]; //Reset operand size!
			if (cpu) //We've got CPUs left?
			{
				--cpu; //Go up one CPU!
				operandsize = CPU_Operand_size[activeCPU]; //Reset operand size to search!
			}
			else //No CPUs left!
			{
				raiseError("CPU","Opcode not defined in jmptbl: %02X",OP); //OPCode not defined!
				return; //Can't execute!
			}
		}
	}
	protection_nextOP(); //Tell the protection exception handlers that we can give faults again!
	reset_modrm(); //Reset modr/m for the current opcode, for detecting it!
	CPU[activeCPU].lastopcode = OP; //Last OPcode!
	opcode_jmptbl[cpu][OP][operandsize](); //Now go execute the OPcode once in the runtime!
	//Don't handle unknown opcodes here: handled by native CPU parser, defined in the jmptbl.
}

extern byte primaryinterrupt; //Have we gotten a primary interrupt (first PIC)?

void CPU_beforeexec()
{
	if (CPU[activeCPU].registers->SFLAGS.TF) //Trapped?
	{
		CPU_exSingleStep(); //Type-1 interrupt: Single step interrupt!
		CPU_afterexec(); //All after execution fixing!
		CPU[activeCPU].trapped = 1; //We're trapped! Don't allow hardware interrupts to intervene!
	}
	else
	{
		CPU[activeCPU].trapped = 0; //We're not trapped (allow hardware interrupts)!
	}
	switch (EMULATED_CPU)
	{
	case CPU_8086:
	case CPU_80186:
		CPU[activeCPU].registers->FLAGS |= 0xF000; //High bits are stuck to 1!
		break;
	case CPU_80286:
		if (getcpumode() == CPU_MODE_REAL) //Real mode?
		{
			CPU[activeCPU].registers->FLAGS &= 0xFFF; //Always set the high flags in real mode only!
		}
		else //Protected mode?
		{
			CPU[activeCPU].registers->FLAGS &= 0x7FFF; //Bit 15 is always cleared!
		}
		break;
	case CPU_80386:
		CPU[activeCPU].registers->FLAGS &= 0x7FFF; //Bit 15 is always cleared!
		CPU[activeCPU].registers->SFLAGS.AC = 0; //Stuck to 0!
		break;
	case CPU_80486:
		break;
	}
}

byte blockREP = 0; //Block the instruction from executing (REP with (E)CX=0

void CPU_exec() //Processes the opcode at CS:EIP (386) or CS:IP (8086).
{
	MMU_clearOP(); //Clear the OPcode buffer in the MMU (equal to our instruction cache)!
	debugger_beforeCPU(); //Everything that needs to be deone before the CPU executes!
	MMU_resetaddr(); //Reset invalid address for our usage!
	CPU[activeCPU].segment_register = CPU_SEGMENT_DEFAULT; //Default data segment register (default: auto)!
	CPU_exec_CS = CPU[activeCPU].registers->CS; //CS of command!
	CPU_exec_EIP = CPU[activeCPU].registers->EIP; //EIP of command!

	char debugtext[256]; //Debug text!
	bzero(debugtext,sizeof(debugtext)); //Init debugger!	

	byte OP; //The opcode!
	OP = CPU_readOP_prefix(); //Process prefix(es) and read OPCode!
	CPU[activeCPU].cycles_OP = 0; //Reset cycles (used by CPU to check for presets (see below))!
	debugger_setprefix(""); //Reset prefix for the debugger!
	if (EMULATED_CPU>=0 && EMULATED_CPU<NUMITEMS(opcode_jmptbl)) //Emulating valid?
	{
		byte gotREP = 0; //Default: no REP-prefix used!
		byte REPZ = 0; //Default to REP!
		if (CPU_getprefix(0xF2)) //REPNE Opcode set?
		{
			gotREP = REPZ = 1; //Allow and we're REPZ!
			switch (OP) //Which special adjustment cycles Opcode?
			{
			//New:
			case 0xA4: //A4: REPNZ MOVSB
				REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
				break;
			case 0xA5: //A5: REPNZ MOVSW
				REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
				break;

			//Old:
			case 0xA6: //A6: REPNZ CMPSB
				break;
			case 0xA7: //A7: REPNZ CMPSW
				break;

			//New:
			case 0xAA: //AA: REPNZ STOSB
				REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
				break;
			case 0xAB: //AB: REPNZ STOSW
				REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
				break;
			case 0xAC: //AC: REPNZ LODSB
				REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
				break;
			case 0xAD: //AD: REPNZ LODSW
				REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
				break;

			//Old:
			case 0xAE: //AE: REPNZ SCASB
				break;
			case 0xAF: //AF: REPNZ SCASW
				break;
			default: //Unknown yet?
				gotREP = 0; //Dont allow after all!
				CPU[activeCPU].cycles_OP = 0; //Unknown!
				break; //Not supported yet!
			}
		}
		else if (CPU_getprefix(0xF3)) //REP/REPE Opcode set?
		{
			gotREP = 1; //Allow!
			switch (OP) //Which special adjustment cycles Opcode?
			{
			case 0xA4: //A4: REP MOVSB
				break;
			case 0xA5: //A5: REP MOVSW
				break;
			case 0xA6: //A6: REPE CMPSB
				REPZ = 1; //REPE/REPZ!
				break;
			case 0xA7: //A7: REPE CMPSW
				REPZ = 1; //REPE/REPZ!
				break;
			case 0xAA: //AA: REP STOSB
				break;
			case 0xAB: //AB: REP STOSW
				break;
			case 0xAC: //AC: REP LODSB
				break;
			case 0xAD: //AD: REP LODSW
				break;
			case 0xAE: //AE: REPE SCASB
				REPZ = 1; //REPE/REPZ!
				break;
			case 0xAF: //AF: REPE SCASW
				REPZ = 1; //REPE/REPZ!
				break;
			default: //Unknown yet?
				gotREP = 0; //Don't allow after all!
				break; //Not supported yet!
			}
		}

		word oldCS;
		oldCS = CPU[activeCPU].registers->CS;
		word oldIP;
		oldIP = CPU[activeCPU].registers->IP; //Save location!

		if (gotREP) //Gotten REP?
		{
			if (CPU_getprefix(0xF2)) //REPNZ?
			{
				debugger_setprefix("REPNZ "); //Set prefix!
			}
			else if (CPU_getprefix(0xF3)) //REP/REPZ?
			{
				if (REPZ) //REPZ?
				{
					debugger_setprefix("REPZ "); //Set prefix!
				}
				else //REP?
				{
					debugger_setprefix("REP "); //Set prefix!
				}
			}
			if (!CPU[activeCPU].registers->CX) //REP and finished?
			{
				blockREP = 1; //Block the CPU instruction from executing!
			}
		}
		CPU_OP(OP); //Now go execute the OPcode once!
		if (gotREP && !CPU[activeCPU].faultraised && !blockREP) //Gotten REP, no fault has been raised and we're executing?
		{
			if (CPU_getprefix(0xF2)) //REPNZ?
			{
				if (REPZ) //Check for zero flag?
				{
					gotREP &= (CPU[activeCPU].registers->SFLAGS.ZF ^ 1); //To reset the opcode (ZF needs to be cleared to loop)?
				}
			}
			else if (CPU_getprefix(0xF3) && REPZ) //REPZ?
			{
				gotREP &= CPU[activeCPU].registers->SFLAGS.ZF; //To reset the opcode (ZF needs to be set to loop)?
			}
			if (--CPU[activeCPU].registers->CX && gotREP) //Still looping and allowed?
			{
				CPU_resetOP(); //Run the current instruction again!
			}
		}
	}
	blockREP = 0; //Don't block REP anymore!
	CPU[activeCPU].cycles += CPU[activeCPU].cycles_OP; //Add cycles executed to total ammount of cycles!
	CPU_afterexec(); //After executing OPCode stuff!
}

void CPU_exec_blocked(uint_32 minEIP, uint_32 maxEIP)
{
	CPU[activeCPU].blocked = 1; //Block: we're running till this is gone or over the limits.
	while (CPU[activeCPU].blocked) //Still running?
	{
		CPU_exec(); //Run one OpCode!
		if ((CPU[activeCPU].registers->EIP>maxEIP) || (CPU[activeCPU].registers->EIP<minEIP)) //Out of range?
		{
			break; //Terminate: out of range: we're done!
		}
	}
}

void CPU_hard_RETI() //Hardware RETI!
{
	if (EMULATED_CPU==CPU_8086) //8086?
	{
		CPU_OP(0xCF); //Execute!
	}
	else //80386?
	{
//Not implemented yet!
	}
}

//have interrupt must be improven disable overrides for now!!!


int have_interrupt(byte nr) //We have this interrupt in the IVT?
{
	if (EMULATED_CPU<=CPU_80186) //80(1)86?
	{
		word offset = MMU_rw(-1,0x0000,nr<<2,1);
		word segment = MMU_rw(-1,0x0000,(nr<<2)|2,1);
		if (!(!segment && !offset)) //Got interrupt?
		{
			return 1; //Assigned!
		}
		else
		{
			return 0; //Not assigned!
		}
	}
	else //386?
	{
		return 0; //Not implemented yet!
	}
}

void CPU_afterexec() //Stuff to do after execution of the OPCode (cycular tasks etc.)
{
	if (MMU_invaddr()) //Invalid adress called?
	{
		//Do something on invalid adress?
	}
	if (EMULATED_CPU <= CPU_80186) //16-bits mode (protect too high data)?
	{
		CPU[activeCPU].registers->EAX &= 0xFFFF; //Convert to 16-bits!
		CPU[activeCPU].registers->EBX &= 0xFFFF; //Convert to 16-bits!
		CPU[activeCPU].registers->ECX &= 0xFFFF; //Convert to 16-bits!
		CPU[activeCPU].registers->EDX &= 0xFFFF; //Convert to 16-bits!
		CPU[activeCPU].registers->ESP &= 0xFFFF; //Convert to 16-bits!
		CPU[activeCPU].registers->EBP &= 0xFFFF; //Convert to 16-bits!
		CPU[activeCPU].registers->ESI &= 0xFFFF; //Convert to 16-bits!
		CPU[activeCPU].registers->EDI &= 0xFFFF; //Convert to 16-bits!
		CPU[activeCPU].registers->EIP &= 0xFFFF; //Convert to 16-bits!
		CPU[activeCPU].registers->EFLAGS &= 0xFFFF; //Convert to 16-bits: we only have 16-bits flags!
	}
	CPU[activeCPU].faultraised = 0; //We don't have a fault anymore! Continue on!
}

void CPU_debugger_STOP() //Stops on debugging!
{
	//We do nothing!
}

int getcpuwraparround() //Wrap arround 1MB limit?
{
	return (getcpumode()!=CPU_MODE_PROTECTED); //Wrap arround when not in protected mode!
}

extern uint_32 destEIP;

void CPU_resetOP() //Rerun current Opcode? (From interrupt calls this recalls the interrupts, handling external calls in between)
{
	destEIP = CPU_exec_EIP; //Destination address!
	segmentWritten(CPU_SEGMENT_CS,CPU_exec_CS,0); //CS changed and rerun ...
}

//Read signed numbers from CS:(E)IP!

sbyte imm8()
{
	return unsigned2signed8(CPU_readOP());
}
sword imm16()
{
	return unsigned2signed16(CPU_readOPw());
}
int_32 imm32()
{
	return unsigned2signed32(CPU_readOPdw());
}

//Exceptions!

//8086+ exceptions (real mode)

void CPU_exDIV0() //Division by 0!
{
	//Points to next opcode!
	CPU_customint(0,CPU_exec_CS,CPU_exec_EIP); //Return to opcode!
}

void CPU_exSingleStep() //Single step (after the opcode only)
{
	//Points to next opcode!
	CPU_INT(1); //Execute INT1 normally using current CS:(E)IP!
}

void CPU_BoundException() //Bound exception!
{
	//Point to opcode origins!
	CPU_customint(5,CPU_exec_CS,CPU_exec_EIP); //Return to opcode!
}

void CPU_COOP_notavailable() //COProcessor not available!
{
	//Point to opcode origins!
	CPU_customint(7,CPU_exec_CS,CPU_exec_EIP); //Return to opcode!
}