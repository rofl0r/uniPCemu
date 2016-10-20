#define IS_CPU
#include "headers/cpu/cpu.h"
#include "headers/cpu/interrupts.h"
#include "headers/cpu/mmu.h"
#include "headers/support/signedness.h" //CPU support!
#include "headers/cpu/cpu_OP8086.h" //8086 comp.
#include "headers/cpu/cpu_OPNECV30.h" //unkOP comp.
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/emu/gpu/gpu.h" //Start&StopVideo!
#include "headers/cpu/cb_manager.h" //CB support!
#include "headers/cpu/protection.h"
#include "headers/cpu/cpu_OP80286.h" //0F opcode support!
#include "headers/support/zalloc.h" //For allocating registers etc.
#include "headers/support/locks.h" //Locking support!
#include "headers/cpu/modrm.h" //MODR/M support!
#include "headers/emu/emucore.h" //Needed for CPU reset handler!
#include "headers/mmu/mmuhandler.h" //bufferMMU, MMU_resetaddr and flushMMU support!
#include "headers/cpu/cpu_pmtimings.h" //80286+ timings lookup table support!

//ALL INTERRUPTS

#include "headers/interrupts/interrupt05.h"
#include "headers/interrupts/interrupt10.h"
#include "headers/interrupts/interrupt11.h"
#include "headers/interrupts/interrupt13.h"
#include "headers/interrupts/interrupt16.h"
#include "headers/interrupts/interrupt18.h"
#include "headers/interrupts/interrupt19.h"

//Enable this define to use cycle-accurate emulation for supported CPUs!
#define CPU_USECYCLES

//Save the last instruction address and opcode in a backup?
#define CPU_SAVELAST

byte activeCPU = 0; //What CPU is currently active?

byte cpudebugger; //To debug the CPU?

CPU_type CPU[MAXCPUS]; //The CPU data itself!

//CPU timings information
extern CPU_Timings CPUTimings[CPU_MODES][0x100]; //All normal CPU timings, which are used, for all modes available!
extern CPU_Timings CPUTimings0F[CPU_MODES][0x100]; //All normal 0F CPU timings, which are used, for all modes available!

//ModR/M information!
MODRM_PARAMS params; //For getting all params for the CPU exection ModR/M data!
byte MODRM_src0 = 0; //What source is our modr/m? (1/2)
byte MODRM_src1 = 0; //What source is our modr/m? (1/2)

//Immediate data read for execution!
byte immb; //For CPU_readOP result!
word immw; //For CPU_readOPw result!
uint_32 imm32; //For CPU_readOPdw result!
uint_64 imm64; //For CPU_readOPdw x2 result!

//Opcode&Stack sizes: 0=16-bits, 1=32-bits!
byte CPU_Operand_size[2] = { 0 , 0 }; //Operand size for this opcode!
byte CPU_Address_size[2] = { 0 , 0 }; //Address size for this opcode!
byte CPU_StackAddress_size[2] = { 0 , 0 }; //Stack Address size for this opcode (determines whether SP or ESP is used)!

//Internal prefix table for below functions!
byte CPU_prefixes[2][32]; //All prefixes, packed in a bitfield!

//More info about interrupts: http://www.bioscentral.com/misc/interrupts.htm#
//More info about interrupts: http://www.bioscentral.com/misc/interrupts.htm#

uint_32 makeupticks; //From PIC?

extern byte PIQSizes[2][NUMCPUS]; //The PIQ buffer sizes!

#ifdef CPU_USECYCLES
byte CPU_useCycles = 0; //Enable normal cycles for supported CPUs when uncommented?
#endif

//Now the code!

byte calledinterruptnumber = 0; //Called interrupt number for unkint funcs!

void CPU_PORT_OUT_B(word port, byte data)
{
	//Check rights!
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
	}
	//Execute it!
	PORT_OUT_B(port,data); //Port out!
}

void CPU_PORT_OUT_W(word port, word data)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
		if (checkPortRights(port+1)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
	}
	//Execute it!
	PORT_OUT_W(port, data); //Port out!
}

void CPU_PORT_OUT_D(word port, uint_32 data)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
		if (checkPortRights(port + 1)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
		if (checkPortRights(port + 2)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
		if (checkPortRights(port + 3)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
	}
	//Execute it!
	PORT_OUT_D(port, data); //Port out!
}

void CPU_PORT_IN_B(word port, byte *result)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
	}
	//Execute it!
	*result = PORT_IN_B(port); //Port in!
}

void CPU_PORT_IN_W(word port, word *result)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
		if (checkPortRights(port + 1)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
	}
	//Execute it!
	*result = PORT_IN_W(port); //Port in!
}

void CPU_PORT_IN_D(word port, uint_32 *result)
{
	if (getcpumode() != CPU_MODE_REAL) //Protected mode?
	{
		if (checkPortRights(port)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
		if (checkPortRights(port + 1)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
		if (checkPortRights(port + 2)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
		if (checkPortRights(port + 3)) //Not allowed?
		{
			THROWDESCGP(CPU[activeCPU].registers->TR); //#GP!
			return; //Abort!
		}
	}
	//Execute it!
	*result = PORT_IN_D(port); //Port in!
}

void call_soft_inthandler(byte intnr)
{
	//Now call handler!
	CPU[activeCPU].cycles_HWOP += 61; /* Normal interrupt as hardware interrupt */
	calledinterruptnumber = intnr; //Save called interrupt number!
	CPU_INT(intnr,0); //Call interrupt!
}

void call_hard_inthandler(byte intnr) //Hardware interrupt handler (FROM hardware only, or int>=0x20 for software call)!
{
//Now call handler!
	CPU[activeCPU].cycles_HWOP += 61; /* Normal interrupt as hardware interrupt */
	calledinterruptnumber = intnr; //Save called interrupt number!
	CPU_INT(intnr,1); //Call interrupt!
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

byte CPU_databussize = 0; //0=16/32-bit bus! 1=8-bit bus when possible (8088/80188)!

OPTINLINE void CPU_resetPrefixes() //Resets all prefixes we use!
{
	memset(&CPU_prefixes[activeCPU], 0, sizeof(CPU_prefixes[activeCPU])); //Reset prefixes!
}

OPTINLINE void CPU_initPrefixes()
{
	CPU_resetPrefixes(); //This is the same: just reset all prefixes to zero!
}

OPTINLINE void alloc_CPUregisters()
{
	CPU[activeCPU].registers = (CPU_registers *)zalloc(sizeof(*CPU[activeCPU].registers), "CPU_REGISTERS", getLock(LOCK_CPU)); //Allocate the registers!
	if (!CPU[activeCPU].registers)
	{
		raiseError("CPU", "Failed to allocate the required registers!");
	}
}

OPTINLINE void free_CPUregisters()
{
	if (CPU[activeCPU].registers) //Still allocated?
	{
		freez((void **)&CPU[activeCPU].registers, sizeof(*CPU[activeCPU].registers), "CPU_REGISTERS"); //Release the registers if needed!
	}
}

OPTINLINE void CPU_initRegisters() //Init the registers!
{
	byte CSAccessRights; //Default CS access rights, overwritten during first software reset!
	if (CPU[activeCPU].registers) //Already allocated?
	{
		CSAccessRights = CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].AccessRights; //Save old CS acccess rights to use now (after first reset)!
		free_CPUregisters(); //Free the CPU registers!
	}
	else
	{
		CSAccessRights = 0x9D; //Initialise the CS access rights!
	}

	alloc_CPUregisters(); //Allocate the CPU registers!

	if (!CPU[activeCPU].registers) return; //We can't work!
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
	CPU[activeCPU].registers->ESP = 0; //Init offset of stack (top-1)
	CPU[activeCPU].registers->SS = 0; //Stack segment!


									  //Code location
	if (EMULATED_CPU >= CPU_NECV30) //186+?
	{
		CPU[activeCPU].registers->CS = 0xF000; //We're this selector!
		CPU[activeCPU].registers->EIP = 0xFFF0; //We're starting at this offset!
	}
	else //8086?
	{
		CPU[activeCPU].registers->CS = 0xFFFF; //Code segment: default to segment 0xFFFF to start at 0xFFFF0 (bios boot jump)!
		CPU[activeCPU].registers->EIP = 0; //Start of executable code!
	}
	CPU_flushPIQ(); //We're jumping to another address!
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
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_TR] = &CPU[activeCPU].registers->TR; //Link!
	CPU[activeCPU].SEGMENT_REGISTERS[CPU_SEGMENT_LDTR] = &CPU[activeCPU].registers->LDTR; //Link!

	memset(CPU[activeCPU].SEG_DESCRIPTOR, 0, sizeof(CPU[activeCPU].SEG_DESCRIPTOR)); //Clear the descriptor cache!
	 //Now, load the default descriptors!

	//IDTR
	CPU[activeCPU].registers->IDTR.base = 0;
	CPU[activeCPU].registers->IDTR.limit = 0x3FF;

	//GDTR
	CPU[activeCPU].registers->GDTR.base = 0;
	CPU[activeCPU].registers->GDTR.limit = 0xFFFF; //From bochs!

	//LDTR (invalid)
	CPU[activeCPU].registers->LDTR = 0; //No LDTR (also invalid)!

	//TR (invalid)
	CPU[activeCPU].registers->TR = 0; //No TR (also invalid)!

	if (EMULATED_CPU == CPU_80286) //80286 CPU?
	{
		CPU[activeCPU].registers->CR0_full &= 0x7FFF0000; //Clear bit 32 and 4-0!
		CPU[activeCPU].registers->CR0_full |= 0xFFF0; //The MSW is initialized to FFF0!
	}
	else //Default or 80386?
	{
		CPU[activeCPU].registers->CR0_full &= 0x7FFFFFE0; //Clear bit 32 and 4-0!
		if (EMULATED_CPU >= CPU_80386) //Diffent initialization?
		{
			CPU[activeCPU].registers->CR0_full &= ~0xFFFF; //The MSW is initialized to 0000!
		}
	}

	byte reg = 0;
	for (reg = 0; reg<NUMITEMS(CPU[activeCPU].SEG_DESCRIPTOR); reg++) //Process all segment registers!
	{
		//Load Real mode compatible values for all registers!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].base_high = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].base_mid = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].base_low = 0;
		CPU[activeCPU].SEG_DESCRIPTOR[reg].limit_low = 0xFFFF; //64k limit!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].limit_high = 0; //No high limit!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].AVL = 0; //Byte granularity!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].unk_0 = 0; //Byte granularity!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].D_B = 0; //Normal size!
		CPU[activeCPU].SEG_DESCRIPTOR[reg].G = 0; //Byte granularity!
		if ((reg != CPU_SEGMENT_LDTR) && (reg != CPU_SEGMENT_TR)) //Special protected-mode segments? We're undefined!
		{
			CPU[activeCPU].SEG_DESCRIPTOR[reg].AccessRights = 0x93; //Data segment!
		}
	}

	//CS specific!
	CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].AccessRights = CSAccessRights; //Load CS default access rights!
	if (EMULATED_CPU>CPU_NECV30) //286+?
	{
		//Pulled low on first load, pulled high on reset:
		if (EMULATED_CPU>CPU_80286) //32-bit CPU?
		{
			CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].base_high = 0xFF; //More than 24 bits are pulled high as well!
		}
		CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_CS].base_mid = 0xFF; //We're starting at the end of our address space, final block! (segment F000=>high 8 bits set)
	}
}

void CPU_initLookupTables(); //Initialize the CPU timing lookup tables! Prototype!

void resetCPU() //Initialises the currently selected CPU!
{
	byte i;
	for (i = 0;i < NUMITEMS(CPU);++i) //Process all CPUs!
	{
		CPU[i].allowInterrupts = 1; //Default to allowing all interrupts to run!
	}
	CPU_initRegisters(); //Initialise the registers!
	CPU_initPrefixes(); //Initialise all prefixes!
	CPU_resetMode(); //Reset the mode to the default mode!
	//Default: not waiting for interrupt to occur on startup!
	//Not waiting for TEST pin to occur!
	//Default: not blocked!
	//Continue interrupt call (hardware)?
	CPU[activeCPU].running = 1; //We're running!
	
	CPU[activeCPU].lastopcode = CPU[activeCPU].previousopcode = 0; //Last opcode, default to 0 and unknown?
	generate_opcode_jmptbl(); //Generate the opcode jmptbl for the current CPU!
	generate_opcode0F_jmptbl(); //Generate the opcode 0F jmptbl for the current CPU!
	generate_timings_tbl(); //Generate the timings tables for all CPU's!
	CPU_initLookupTables(); //Initialize our timing lookup tables!
	if (PIQSizes[CPU_databussize][EMULATED_CPU]) //Gotten any PIQ installed with the CPU?
	{
		CPU[activeCPU].PIQ = allocfifobuffer(PIQSizes[CPU_databussize][EMULATED_CPU],0); //Our PIQ we use!
	}
	CPU[activeCPU].CallGateStack = allocfifobuffer(32<<2,0); //Non-lockable 32-bit 32 arguments FIFO buffer for call gate stack parameter copy!
	#ifdef CPU_USECYCLES
	CPU_useCycles = (EMULATED_CPU<=CPU_80286); //Are we using cycle-accurate emulation?
	#endif
	EMU_onCPUReset(); //Make sure all hardware, like CPU A20 is updated for the reset!
	CPU[activeCPU].D_B_Mask = (EMULATED_CPU>=CPU_80386)?1:0; //D_B mask when applyable!
	CPU[activeCPU].G_Mask = (EMULATED_CPU >= CPU_80386) ? 1 : 0; //G mask when applyable!
	CPU[activeCPU].is_reset = 1; //We're reset!
}

void initCPU() //Initialize CPU for full system reset into known state!
{
	memset(&CPU, 0, sizeof(CPU)); //Reset the CPU fully!
	resetCPU(); //Reset normally!
}

//data order is low-high, e.g. word 1234h is stored as 34h, 12h

byte CPU_readOP() //Reads the operation (byte) at CS:EIP
{
	byte result; //Buffer from the PIQ and actual memory data!
	uint_32 instructionEIP = CPU[activeCPU].registers->EIP++; //Our current instruction position is increased always!
	if (CPU[activeCPU].PIQ) //PIQ present?
	{
		PIQ_retry: //Retry after refilling PIQ!
		if (readfifobuffer(CPU[activeCPU].PIQ,&result)) //Read from PIQ?
		{
			if (cpudebugger) //We're an OPcode retrieval and debugging?
			{
				MMU_addOP(result); //Add to the opcode cache!
			}
			return result; //Give the prefetched data!
		}
		//Not enough data in the PIQ? Refill for the next data!
		CPU_fillPIQ(); //Fill instruction cache with next data!
		goto PIQ_retry; //Read again!
	}
	result = MMU_rb(CPU_SEGMENT_CS, CPU[activeCPU].registers->CS, instructionEIP, 1); //Read OPcode directly from memory!
	if (cpudebugger) //We're an OPcode retrieval and debugging?
	{
		MMU_addOP(result); //Add to the opcode cache!
	}
	return result; //Give the result!
}

word CPU_readOPw() //Reads the operation (word) at CS:EIP
{
	INLINEREGISTER byte temp, temp2;
	temp = CPU_readOP(); //Read OPcode!
	temp2 = CPU_readOP(); //Read OPcode!
	return temp|(temp2<<8); //Give result!
}

uint_32 CPU_readOPdw() //Reads the operation (32-bit unsigned integer) at CS:EIP
{
	INLINEREGISTER uint_32 result;
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

OPTINLINE void CPU_setprefix(byte prefix) //Sets a prefix on!
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

OPTINLINE byte CPU_isPrefix(byte prefix)
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
			return 1; //Always a prefix!
		case 0x66: //Operand-size override
		case 0x67: //Address-size override
			return (EMULATED_CPU>=CPU_80286); //We're a prefix when 286+!
		case 0x64: //FS segment override prefix
		case 0x65: //GS segment override prefix
			return (EMULATED_CPU >= CPU_80386); //We're a prefix when 386+!
		default: //It's a normal OPcode?
			return 0; //No prefix!
			break; //Not use others!
	}

	return 0; //No prefix!
}

byte DATA_SEGMENT_DESCRIPTOR_B_BIT() //80286+: Gives the B-Bit of the DATA DESCRIPTOR TABLE FOR SS-register!
{
	if (EMULATED_CPU<=CPU_NECV30) //8086-NEC V20/V30?
	{
		return 0; //Always 16-bit descriptor!
	}

	return CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_SS].D_B&CPU[activeCPU].D_B_Mask; //Give the B-BIT of the SS-register!
}

uint_32 CPU_InterruptReturn = 0;

OPTINLINE byte CPU_readOP_prefix() //Reads OPCode with prefix(es)!
{
	INLINEREGISTER byte OP; //The current opcode!
	INLINEREGISTER uint_32 last_eip;
	INLINEREGISTER byte ismultiprefix = 0; //Are we multi-prefix?
	CPU_Timings *timing; //The timing used!
	byte result = 0;
	CPU_resetPrefixes(); //Reset all prefixes for this opcode!
	reset_modrm(); //Reset modr/m for the current opcode, for detecting it!

	CPU_InterruptReturn = last_eip = CPU[activeCPU].registers->EIP; //Interrupt return point by default!
	OP = CPU_readOP(); //Read opcode or prefix?
	CPU[activeCPU].cycles_Prefix = 0; //No cycles for the prefix by default!
	for (;CPU_isPrefix(OP);) //We're a prefix?
	{
		CPU[activeCPU].cycles_Prefix += 2; //Add timing for the prefix!
		if (ismultiprefix && (EMULATED_CPU <= CPU_80286)) //This CPU has the bug and multiple prefixes are added?
		{
			CPU_InterruptReturn = last_eip; //Return to the last prefix only!
		}
		CPU_setprefix(OP); //Set the prefix ON!
		last_eip = CPU[activeCPU].registers->EIP; //Save the current EIP of the last prefix possibility!
		ismultiprefix = 1; //We're multi-prefix now when triggered again!
		OP = CPU_readOP(); //Next opcode/prefix!
	}
	//Now we have the opcode and prefixes set or reset!

	if ((OP == 0x0F) && (EMULATED_CPU >= CPU_80286)) //0F instruction extensions used?
	{
		OP = CPU_readOP(); //Read the actual opcode to use!
		CPU[activeCPU].is0Fopcode = 1; //We're a 0F opcode!
	}
	else //Normal instruction?
	{
		CPU[activeCPU].is0Fopcode = 0; //We're a normal opcode!
	}

//Determine the stack&attribute sizes(286+)!
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

	//Now, check for the ModR/M byte, if present, and read the parameters if needed!
	result = OP; //Save the OPcode for later result!

	if (CPU[activeCPU].is0Fopcode) //0F opcode?
	{
		timing = &CPUTimings0F[CPU_Address_size[activeCPU]][OP]; //Only 2 modes implemented so far, 32-bit or 16-bit mode!
	}
	else //Normal opcode?
	{
		timing = &CPUTimings[CPU_Address_size[activeCPU]][OP]; //Only 2 modes implemented so far, 32-bit or 16-bit mode!
	}

	if (timing->used==0) goto skiptimings; //Are we not used?
	if (timing->has_modrm) //Do we have ModR/M data?
	{
		modrm_readparams(&params,timing->modrm_readparams_0,timing->modrm_readparams_1); //Read the params!
		MODRM_src0 = timing->modrm_src0; //First source!
		MODRM_src1 = timing->modrm_src1; //Second source!
	}

	if (timing->parameters) //Gotten parameters?
	{
		switch (timing->parameters&~4) //What parameters?
		{
			case 1: //imm8?
				if (timing->parameters&4) //Only when ModR/M REG<2?
				{
					if (MODRM_REG(params.modrm)<2) //8-bit immediate?
					{
						immb = CPU_readOP(); //Read 8-bit immediate!
					}
				}
				else //Normal imm8?
				{
					immb = CPU_readOP(); //Read 8-bit immediate!
				}
				break;
			case 2: //imm16?
				if (timing->parameters&4) //Only when ModR/M REG<2?
				{
					if (MODRM_REG(params.modrm)<2) //16-bit immediate?
					{
						immw = CPU_readOPw(); //Read 16-bit immediate!
					}
				}
				else //Normal imm16?
				{
					immw = CPU_readOPw(); //Read 16-bit immediate!
				}
				break;
			case 3: //imm32?
				if (timing->parameters&4) //Only when ModR/M REG<2?
				{
					if (MODRM_REG(params.modrm)<2) //32-bit immediate?
					{
						imm32 = CPU_readOPdw(); //Read 32-bit immediate!
					}
				}
				else //Normal imm32?
				{
					imm32 = CPU_readOPdw(); //Read 32-bit immediate!
				}
				break;
			case 8: //imm16 + imm8
				immw = CPU_readOPw(); //Read 16-bit immediate!
				immb = CPU_readOP(); //Read 8-bit immediate!
				break;
			case 9: //imm64?
				if (timing->parameters & 4) //Only when ModR/M REG<2?
				{
					if (MODRM_REG(params.modrm)<2) //32-bit immediate?
					{
						imm64 = CPU_readOPdw(); //Read 32-bit immediate!
						imm64 |= ((uint_64)CPU_readOPdw() << 32); //Read another 32-bit immediate!
					}
				}
				else //Normal imm32?
				{
					imm64 = CPU_readOPdw(); //Read 32-bit immediate!
					imm64 |= ((uint_64)CPU_readOPdw() << 32); //Read another 32-bit immediate!
				}
				break;
			default: //Unknown?
				//Ignore the parameters!
				break;
		}
	}


skiptimings: //Skip all timings and parameters(invalid instruction)!
	return result; //Give the OPCode to execute!
}

void doneCPU() //Finish the CPU!
{
	free_CPUregisters(); //Finish the allocated registers!
	free_fifobuffer(&CPU[activeCPU].PIQ); //Release our PIQ!
	free_fifobuffer(&CPU[activeCPU].CallGateStack); //Release our Call Gate Stack space!
}

CPU_registers dummyregisters; //Dummy registers!

//Specs for 80386 says we start in REAL mode!
//STDMODE: 0=protected; 1=real; 2=Virtual 8086.

void CPU_resetMode() //Resets the mode!
{
	if (!CPU[activeCPU].registers) CPU_initRegisters(); //Make sure we have registers!
	//Always start in REAL mode!
	if (!CPU[activeCPU].registers) return; //We can't work now!
	CPU[activeCPU].registers->SFLAGS.V8 = 0; //Disable Virtual 8086 mode!
	CPU[activeCPU].registers->CR0.PE = 0; //Real mode!
	updateCPUmode(); //Update the CPU mode!
}

byte CPUmode = CPU_MODE_REAL; //The current CPU mode!

void updateCPUmode() //Update the CPU mode!
{
	static const byte modes[4] = { CPU_MODE_REAL, CPU_MODE_PROTECTED, CPU_MODE_REAL, CPU_MODE_8086 }; //All possible modes (VM86 mode can't exist without Protected Mode!)
	byte mode = 0;
	if (!CPU[activeCPU].registers)
	{
		CPU_initRegisters(); //Make sure we have registers!
		if (!CPU[activeCPU].registers) CPU[activeCPU].registers = &dummyregisters; //Dummy registers!
	}
	mode = CPU[activeCPU].registers->SFLAGS.V8; //VM86 mode?
	mode <<= 1;
	mode |= CPU[activeCPU].registers->CR0.PE; //Protected mode?
	CPUmode = modes[mode]; //Mode levels: Real mode > Protected Mode > VM86 Mode!
}

byte getcpumode() //Retrieves the current mode!
{
	return CPUmode; //Give the current CPU mode!
}

byte isPM()
{
	return (CPUmode!=CPU_MODE_REAL)?1:0; //Are we in protected mode?
}

byte isV86()
{
	return (CPUmode==CPU_MODE_8086)?1:0; //Are we in virtual 8086 mode?
}

//PUSH and POP values!

byte topdown_stack() //Top-down stack?
{
	//We're a 286+, so detect it! Older processors are always in real mode!
	if (getcpumode()==CPU_MODE_REAL)
	{
		return 1; //Real mode!
	}
	return !(CPU[activeCPU].SEG_DESCRIPTOR[CPU_SEGMENT_SS].Type & 4); //Real mode=8086; Other=SS segment, bit 4 (off=Topdown stack!)
}

OPTINLINE uint_32 getstackaddrsizelimiter()
{
	return CPU_StackAddress_size[activeCPU]? 0xFFFFFFFF : 0xFFFF; //Stack address size!
}

//Memory is the same as PSP: 1234h is 34h 12h, in stack terms reversed, because of top-down stack!

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

//Changes in stack during PUSH and POP operations!
sbyte stack_pushchange(byte dword)
{
	if (topdown_stack()) //--?
	{
		return -(2 << dword); //Decrease!
	}
	else //++?
	{
		return (2 << dword); //Increase!
	}
}

sbyte stack_popchange(byte dword)
{
	if (topdown_stack()) //--?
	{
		return (2 << dword); //Decrease!
	}
	else //++?
	{
		return -(2 << dword); //Increase!
	}
}


OPTINLINE void stack_push(byte dword) //Push 16/32-bits to stack!
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

OPTINLINE void stack_pop(byte dword) //Push 16/32-bits to stack!
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
		stack_push(CPU_Operand_size[activeCPU]); //We're pushing a 16-bit or 32-bit value!
		if (CPU_Operand_size[activeCPU]) //32-bit?
		{
			MMU_wdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), (uint_32)oldval); //Put value!
		}
		else
		{
			MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval); //Put value!
		}
	}
}

word CPU_POP16() //Pop Word!
{
	word result;
	if (CPU_Operand_size[activeCPU]) //32-bit?
	{
		result = (word)MMU_rdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), 0); //Get value!
	}
	else //16-bit?
	{
		result = MMU_rw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), 0); //Get value!
	}
	stack_pop(CPU_Operand_size[activeCPU]); //We're popping a 16-bit value!
	return result; //Give the result!
}

void CPU_PUSH32(uint_32 *val) //Push DWord!
{
	if (EMULATED_CPU<CPU_80386) //286-?
	{
		stack_push(0); //We're pushing a 32-bit value!
		MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), *val); //Put value!
	}
	else //386+?
	{
		uint_32 oldval = *val; //Old value!
		stack_push(CPU_Operand_size[activeCPU]); //We're pushing a 32-bit value!
		if (CPU_Operand_size[activeCPU]) //32-bit?
		{
			MMU_wdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), oldval); //Put value!
		}
		else //16-bit?
		{
			MMU_ww(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, (CPU[activeCPU].registers->ESP&getstackaddrsizelimiter()), (word)oldval); //Put value!
		}
	}
}

uint_32 CPU_POP32() //Full stack used!
{
	uint_32 result;
	if (CPU_Operand_size[activeCPU]) //32-bit?
	{
		result = MMU_rdw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), 0); //Get value!
	}
	else //16-bit?
	{
		result = (uint_32)MMU_rw(CPU_SEGMENT_SS, CPU[activeCPU].registers->SS, CPU[activeCPU].registers->ESP&getstackaddrsizelimiter(), 0); //Get value!
	}
	stack_pop(CPU_Operand_size[activeCPU]); //We're popping a 32-bit value!
	return result; //Give the result!
}

//Final stuff:

char textsegments[][5] =   //Comply to CPU_REGISTER_XX order!
{
	"CS",
	"SS",
	"DS",
	"ES",
	"FS",
	"GS",
	"TR",
	"LDTR"
};

char *CPU_textsegment(byte defaultsegment) //Plain segment to use!
{
	if (CPU[activeCPU].segment_register==CPU_SEGMENT_DEFAULT) //Default segment?
	{
		return &textsegments[defaultsegment][0]; //Default segment!
	}
	return &textsegments[CPU[activeCPU].segment_register][0]; //Use Data Segment (or different in case) for data!
}

void CPU_afterexec(); //Prototype for below!

word CPU_exec_CS; //OPCode CS
uint_32 CPU_exec_EIP; //OPCode EIP

#ifdef CPU_SAVELAST
word CPU_exec_lastCS; //OPCode CS
uint_32 CPU_exec_lastEIP; //OPCode EIP
#endif

extern Handler CurrentCPU_opcode_jmptbl[512]; //Our standard internal standard opcode jmptbl!
extern Handler CurrentCPU_opcode0F_jmptbl[512]; //Our standard internal standard opcode jmptbl!

void CPU_OP(byte OP) //Normal CPU opcode execution!
{
	protection_nextOP(); //Tell the protection exception handlers that we can give faults again!
	CPU[activeCPU].lastopcode = OP; //Last OPcode!
	if (CPU[activeCPU].is0Fopcode) //0F opcode?
	{
		CurrentCPU_opcode0F_jmptbl[(OP << 1) | CPU_Operand_size[activeCPU]](); //Now go execute the OPcode once in the runtime!
	}
	else
	{
		CurrentCPU_opcode_jmptbl[(OP<<1)|CPU_Operand_size[activeCPU]](); //Now go execute the OPcode once in the runtime!
	}
	//Don't handle unknown opcodes here: handled by native CPU parser, defined in the jmptbl.
}

void CPU_beforeexec()
{
	//This applies to all processors:
	CPU[activeCPU].trapped = CPU[activeCPU].registers->SFLAGS.TF; //Are we to be trapped this instruction?
	INLINEREGISTER uint_32 tempflags;
	tempflags = CPU[activeCPU].registers->EFLAGS; //Load the flags to set/clear!
	tempflags &= ~(8|32); //Clear bits 3&5!

	switch (EMULATED_CPU) //What CPU flags to emulate?
	{
	case CPU_8086:
	case CPU_NECV30:
		tempflags |= 0xF000; //High bits are stuck to 1!
		break;
	case CPU_80286:
		if (getcpumode() == CPU_MODE_REAL) //Real mode?
		{
			tempflags &= 0xFFFF0FFF; //Always set the high flags in real mode only!
		}
		else //Protected mode?
		{
			tempflags &= 0xFFFF7FFF; //Bit 15 is always cleared!
		}
		break;
	case CPU_80386:
		tempflags &= 0xFFFB7FFF; //Bit 15 is always cleared! AC is stuck to 0!
		break;
	case CPU_80486:
		CPU[activeCPU].registers->FLAGS &= 0xFFDF7FFF; //Bit 15 is always cleared! Don't allow setting of the CPUID flag!
		break;
	case CPU_PENTIUM:
		//Allow all bits to be set!
		break;
	default: //Unknown CPU?
		break;
	}
	tempflags |= 2; //Clear bit values 8&32(unmapped bits 3&5) and set bit value 2!
	CPU[activeCPU].registers->EFLAGS = tempflags; //Update the flags!
}

byte blockREP = 0; //Block the instruction from executing (REP with (E)CX=0
byte gotREP = 0; //Default: no REP-prefix used!
byte REPPending = 0; //Pending REP reset?

void CPU_8086REPPending() //Execute this before CPU_exec!
{
	if (REPPending) //Pending REP?
	{
		REPPending = 0; //Disable pending REP!
		CPU_resetOP(); //Rerun the last instruction!
	}
}

byte CPU_segmentOverridden(byte TheActiveCPU)
{
	return (CPU[TheActiveCPU].segment_register != CPU_SEGMENT_DEFAULT); //Is the segment register overridden?
}

extern byte DosboxClock; //Dosbox clocking?

byte newREP = 1; //Are we a new repeating instruction (REP issued?)

//Stuff for CPU timing processing!
byte didJump = 0; //Did we jump this instruction?
extern CPUPM_Timings CPUPMTimings[215]; //The PM timings full table!

void CPU_exec() //Processes the opcode at CS:EIP (386) or CS:IP (8086).
{
	byte didNewREP = 0, didRepeating=0; //Did we do a REP?
	word *currentinstructiontiming; //Current timing we're processing!
	byte instructiontiming, ismemory, modrm_threevariablesused; //Timing loop used on 286+ CPUs!
	MemoryTimingInfo *currenttimingcheck; //Current timing check!
	CPU[activeCPU].allowInterrupts = 1; //Allow interrupts again after this instruction!
	bufferMMU(); //Buffer the MMU writes for us!
	byte cycles_counted = 0; //Cycles have been counted?
	MMU_clearOP(); //Clear the OPcode buffer in the MMU (equal to our instruction cache)!
	debugger_beforeCPU(); //Everything that needs to be done before the CPU executes!
	MMU_resetaddr(); //Reset invalid address for our usage!
	CPU_8086REPPending(); //Process pending REP!

	if (CPU[activeCPU].permanentreset) //We've entered a permanent reset?
	{
		CPU[activeCPU].cycles = 4; //Small cycle dummy! Must be greater than zero!
		return; //Don't run the CPU: we're in a permanent reset state!
	}

	//Initialize stuff needed for local CPU timing!
	didJump = 0; //Default: we didn't jump!

	//Now, starting the instruction preprocessing!
	CPU[activeCPU].is_reset = 0; //We're not reset anymore from now on!
	CPU[activeCPU].segment_register = CPU_SEGMENT_DEFAULT; //Default data segment register (default: auto)!
	if (!CPU[activeCPU].repeating) //Not repeating instructions?
	{
		#ifdef CPU_SAVELAST
		//Save the last coordinates!
		CPU_exec_lastCS = CPU_exec_CS;
		CPU_exec_lastEIP = CPU_exec_lastEIP;
		#endif
		CPU_exec_CS = CPU[activeCPU].registers->CS; //CS of command!
		CPU_exec_EIP = CPU[activeCPU].registers->EIP; //EIP of command!
	}

	char debugtext[256]; //Debug text!
	bzero(debugtext,sizeof(debugtext)); //Init debugger!	

	INLINEREGISTER byte OP; //The opcode!
	if (CPU[activeCPU].repeating) //REPeating instruction?
	{
		OP = CPU[activeCPU].lastopcode; //Execute the last opcode again!
		newREP = 0; //Not a new repeating instruction!
	}
	else //Not a repeating instruction?
	{
		OP = CPU_readOP_prefix(); //Process prefix(es) and read OPCode!
		newREP = 1; //We're a new repeating instruction!
	}
	CPU[activeCPU].cycles_OP = 0; //Reset cycles (used by CPU to check for presets (see below))!
	if (cpudebugger) debugger_setprefix(""); //Reset prefix for the debugger!
	gotREP = 0; //Default: no REP-prefix used!
	byte REPZ = 0; //Default to REP!
	if (CPU_getprefix(0xF2)) //REPNE Opcode set?
	{
		gotREP = REPZ = 1; //Allow and we're REPZ!
		switch (OP) //Which special adjustment cycles Opcode?
		{
		//80186+ REP opcodes!
		//REPNZ INSB/INSW and REPNZ OUTSB/OUTSW doesn't exist!
		//8086 REPable opcodes!	
		//New:
		case 0xA4: //A4: REPNZ MOVSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
 			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;
		case 0xA5: //A5: REPNZ MOVSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;

		//Old:
		case 0xA6: //A6: REPNZ CMPSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;
		case 0xA7: //A7: REPNZ CMPSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;

		//New:
		case 0xAA: //AA: REPNZ STOSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;
		case 0xAB: //AB: REPNZ STOSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;
		case 0xAC: //AC: REPNZ LODSB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;
		case 0xAD: //AD: REPNZ LODSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;

		//Old:
		case 0xAE: //AE: REPNZ SCASB
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;
		case 0xAF: //AF: REPNZ SCASW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			break;
		default: //Unknown yet?
		noREPNE0Fand8086: //0F/8086 #UD exception!
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
		//80186+ REP opcodes!
		case 0x6C: //A4: REP INSB
		case 0x6D: //A4: REP INSW
		case 0x6E: //A4: REP OUTSB
		case 0x6F: //A4: REP OUTSW
			if (CPU[activeCPU].is0Fopcode) goto noREPNE0Fand8086; //0F opcode?
			if (EMULATED_CPU < CPU_NECV30) goto noREPNE0Fand8086; //Not existant on 8086!
			REPZ = 0; //Don't check the zero flag: it maybe so in assembly, but not in execution!
			break;
			//8086 REP opcodes!
		case 0xA4: //A4: REP MOVSB
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xA5: //A5: REP MOVSW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xA6: //A6: REPE CMPSB
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			REPZ = 1; //REPE/REPZ!
			break;
		case 0xA7: //A7: REPE CMPSW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			REPZ = 1; //REPE/REPZ!
			break;
		case 0xAA: //AA: REP STOSB
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xAB: //AB: REP STOSW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xAC: //AC: REP LODSB
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xAD: //AD: REP LODSW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			break;
		case 0xAE: //AE: REPE SCASB
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			REPZ = 1; //REPE/REPZ!
			break;
		case 0xAF: //AF: REPE SCASW
			if (CPU[activeCPU].is0Fopcode) goto noREPE0Fand8086; //0F opcode?
			REPZ = 1; //REPE/REPZ!
			break;
		default: //Unknown yet?
			noREPE0Fand8086: //0F exception!
			gotREP = 0; //Don't allow after all!
			break; //Not supported yet!
		}
	}

	if (gotREP) //Gotten REP?
	{
		if (cpudebugger) //Need to set any debugger info?
		{
			if (CPU_getprefix(0xF0)) //LOCK?
			{
				debugger_setprefix("LOCK"); //LOCK!
			}
			if (CPU_getprefix(0xF2)) //REPNZ?
			{
				debugger_setprefix("REPNZ"); //Set prefix!
			}
			else if (CPU_getprefix(0xF3)) //REP/REPZ?
			{
				if (REPZ) //REPZ?
				{
					debugger_setprefix("REPZ"); //Set prefix!
				}
				else //REP?
				{
					debugger_setprefix("REP"); //Set prefix!
				}
			}
		}
		if (!CPU[activeCPU].registers->CX) //REP and finished?
		{
			blockREP = 1; //Block the CPU instruction from executing!
		}
	}
	didRepeating = CPU[activeCPU].repeating; //Were we doing REP?
	didNewREP = newREP; //Were we doing a REP for the first time?
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
		if (CPU[activeCPU].registers->CX-- && gotREP) //Still looping and allowed? Decrease CX after checking for the final item!
		{
			REPPending = CPU[activeCPU].repeating = 1; //Run the current instruction again and flag repeat!
		}
		else
		{
			CPU[activeCPU].repeating = 0; //Not repeating anymore!
		}
	}
	else
	{
		REPPending = CPU[activeCPU].repeating = 0; //Not repeating anymore!
	}
	blockREP = 0; //Don't block REP anymore!
	if (DosboxClock)
	{
		CPU[activeCPU].cycles = 1; //Instead of actually using cycles per second(CPS) , we use instructions per second for this setting(IPS)!
		if (CPU[activeCPU].PIQ) //Prefetching?
		{
			for (;fifobuffer_freesize(CPU[activeCPU].PIQ);)
			{
				CPU_fillPIQ(); //Keep the FIFO fully filled!
			}
		}
	}
	else //Use normal cycles dependent on the CPU (Cycle accuracy w/ documented speed)?
	{
		switch (EMULATED_CPU) //What CPU to use?
		{
		case CPU_80286: //Special 286 case for easy 8086-compatibility!
			//80286 uses other timings than the other chips!
			ismemory = modrm_ismemory(params)?1:0; //Are we accessing memory?
			if (ismemory)
			{
				modrm_threevariablesused = MODRM_threevariables(params); //Three variables used?
			}
			else
			{
				modrm_threevariablesused = 0; //Only 2 or less variables used in calculating the ModR/M.
			}
			currentinstructiontiming = &CPU[activeCPU].timing286lookup[isPM()][ismemory][CPU[activeCPU].is0Fopcode][CPU[activeCPU].lastopcode][MODRM_REG(params.modrm)][0]; //Start by pointing to our records to process!
			//Try to use the lookup table!
			for (instructiontiming=0;((instructiontiming<8)&&*currentinstructiontiming);++instructiontiming, ++currentinstructiontiming) //Process all timing candidates!
			{
				if (*currentinstructiontiming) //Valid timing?
				{
					if (CPUPMTimings[*currentinstructiontiming].CPUmode[isPM()].ismemory[ismemory].basetiming) //Do we have valid timing to use?
					{
						currenttimingcheck = &CPUPMTimings[*currentinstructiontiming].CPUmode[isPM()].ismemory[ismemory]; //Our current info to check!
						if (currenttimingcheck->addclock&0x20) //L of instruction doesn't fit in 1 bit?
						{
							//TODO
						}
						else if (currenttimingcheck->addclock&0x10) //L of instruction fits in 1 bit and matches?
						{
							//TODO
						}
						else if (currenttimingcheck->addclock&0x08) //Only when jump taken?
						{
							if (didJump) //Did we jump?
							{
								CPU[activeCPU].cycles = currenttimingcheck->basetiming; //Use base timing specified only!								
								goto apply286cycles; //Apply the cycles!
							}
						}
						else if (currenttimingcheck->addclock&0x04) //Gate type has to match in order to be processed?
						{
							//TODO
						}
						else if (currenttimingcheck->addclock&0x02) //REP((N)Z) instruction prefix only?
						{
							if (didRepeating) //Are we executing a repeat?
							{
								if (didNewREP) //Including the REP, first instruction?
								{
									CPU[activeCPU].cycles = currenttimingcheck->basetiming; //Use base timing specified only!
								}
								else //Already repeating instruction continued?
								{
									CPU[activeCPU].cycles = currenttimingcheck->n; //Simply cycle count added each REPeated instruction!
								}
								goto apply286cycles; //Apply the cycles!
							}
						}
						else //Normal/default behaviour? Always matches!
						{
							CPU[activeCPU].cycles = currenttimingcheck->basetiming; //Use base timing specified only!
							goto apply286cycles; //Apply the cycles!
						}
					}
				}
			}
			//Fall back to the default handler on 80(1)86 systems!
		case CPU_8086: //8086/8088?
		case CPU_NECV30: //NEC V20/V30/80188?
			//Placeholder until 8086/8088 cycles are fully implemented. Originally 8. 9 works better with 8088 MPH(better sound). 10 works worse than 9(sound disappears into the background)?
			#ifdef CPU_USECYCLES
			if ((CPU[activeCPU].cycles_OP|CPU[activeCPU].cycles_HWOP|CPU[activeCPU].cycles_Exception) && CPU_useCycles) //cycles entered by the instruction?
			{
				CPU[activeCPU].cycles = CPU[activeCPU].cycles_OP+CPU[activeCPU].cycles_HWOP+CPU[activeCPU].cycles_Prefix + CPU[activeCPU].cycles_Exception + CPU[activeCPU].cycles_Prefetch + CPU[activeCPU].cycles_MMUR + CPU[activeCPU].cycles_MMUW; //Use the cycles as specified by the instruction!
			}
			else //Automatic cycles placeholder?
			{
			#endif
				CPU[activeCPU].cycles = (CPU_databussize>=1)?9:8; //Use 9 with 8088MPH CPU(8088 CPU), normal 8 with 8086.
			#ifdef CPU_USECYCLES
			}
			apply286cycles: //Apply the 286+ cycles used!
			cycles_counted = 1; //Cycles have been counted!
			#endif
			break;
		default: //Not implemented yet?
			CPU[activeCPU].cycles = 4; //Take 4 cycles per instruction for now(1 PIT tick at 8086 speed)!
			cycles_counted = 1; //Cycles have been counted!
			break;
		}
	}
	CPU_afterexec(); //After executing OPCode stuff!
	CPU_tickPrefetch(); //Tick the prefetch as required!
	if (cycles_counted) //Finished with our cycle countings? Clear that information for the next instruction!
	{
		CPU[activeCPU].cycles_HWOP = 0; //No hardware interrupt to use anymore!
		CPU[activeCPU].cycles_Prefix = 0; //No cycles prefix to use anymore!
		CPU[activeCPU].cycles_Exception = 0; //No cycles Exception to use anymore!
		CPU[activeCPU].cycles_MMUR = CPU[activeCPU].cycles_MMUW = 0; //No cycles MMU to use anymore!
		CPU[activeCPU].cycles_Prefetch = 0; //No cycles prefetch to use anymore!
	}
	flushMMU(); //Flush MMU writes!
	CPU[activeCPU].previousopcode = CPU[activeCPU].lastopcode; //Last executed OPcode for reference purposes!
}

byte haslower286timingpriority(byte CPUmode,byte ismemory,word lowerindex, word higherindex)
{
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&2)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&2)) return 1; //REP over non-REP
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&4)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&4)) return 1; //Gate over non-Gate!
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&8)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&8)) return 1; //JMP taken over JMP not taken!
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&16)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&16)) return 1; //L value of BOUND fits having higher priority!
	if ((CPUPMTimings[higherindex].CPUmode[CPUmode].ismemory[ismemory].addclock&32)>(CPUPMTimings[lowerindex].CPUmode[CPUmode].ismemory[ismemory].addclock&32)) return 1; //L value of BOUND counts having higher priority!
	return 0; //We're equal priority or having higher priority! Don't swap!
}

void CPU_initLookupTables() //Initialize the CPU timing lookup tables!
{
	word index; //The index into the main table!
	word sublistindex; //The index in the sublist!
	byte CPUmode; //The used CPU mode!
	byte ismemory; //Memory used in the CPU mode!
	byte is0Fopcode; //0F opcode bit!
	word instruction; //Instruction itself!
	word modrm_register; //The modr/m register used, if any(modr/m specified only)!
	word sublistsize; //Sub-list size!
	word sublist[8]; //All instructions matching this!
	word tempsublist; //Temporary value for swapping items!
	byte latestCPU; //Last supported CPU for this instruction timing!
	byte currentCPU; //The CPU we're emulating, relative to the 80286!
	byte notfound = 0; //Not found CPU timings?
	currentCPU = EMULATED_CPU-CPU_80286; //The CPU to find in the table!

	memset(&CPU[activeCPU].timing286lookup,0,sizeof(CPU[activeCPU].timing286lookup)); //Clear the entire list!

	for (CPUmode=0;CPUmode<2;++CPUmode) //All CPU modes!
	{
		for (ismemory=0;ismemory<2;++ismemory) //All memory modes!
		{
			for (is0Fopcode=0;is0Fopcode<2;++is0Fopcode) //All 0F opcode possibilities!
			{
				for (instruction=0;instruction<0x100;++instruction) //All instruction opcodes!
				{
					for (modrm_register=0;modrm_register<8;++modrm_register) //All modr/m variants!
					{
						sublistsize = 0; //Initialize our size to none!
						latestCPU = currentCPU; //Start off with the current CPU that's supported!
						notfound = 1; //Default to not found!
						for (;;) //Find the top CPU supported!
						{
							//First, detect the latest supported CPU!
							for (index=0;index<NUMITEMS(CPUPMTimings);++index) //Process all timings available!
							{
								if ((CPUPMTimings[index].CPU==latestCPU) && (CPUPMTimings[index].is0F==is0Fopcode) && (CPUPMTimings[index].OPcode==(instruction&CPUPMTimings[index].OPcodemask))) //Basic opcode matches?
								{
									if ((CPUPMTimings[index].modrm_reg==0) || (CPUPMTimings[index].modrm_reg==modrm_register)) //MODR/M filter matches to full opcode?
									{
										notfound = 0; //We're found!
										goto topCPUTimingsdetected; //We're detected!
									}
								}
							}
							if (latestCPU==0) goto topCPUTimingsdetected; //Abort when finished!
							--latestCPU; //Check the next CPU!
						}
						topCPUTimingsdetected: //TOP CPU timings detected?
						memset(&sublist,0,sizeof(sublist)); //Clear our sublist!
						if (notfound) //No CPU found matching this instruction?
						{
							memset(&CPU[activeCPU].timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register],0,sizeof(&CPU[activeCPU].timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register])); //Unused timings!
						}
						else //Valid CPU found for this instruction?
						{
							//Now, find all items that apply to this instruction!
							for (index=0;index<NUMITEMS(CPUPMTimings);++index) //Process all timings available!
							{
								if ((CPUPMTimings[index].CPU==latestCPU) && (CPUPMTimings[index].is0F==is0Fopcode) && (CPUPMTimings[index].OPcode==(instruction&CPUPMTimings[index].OPcodemask))) //Basic opcode matches?
								{
									if ((CPUPMTimings[index].modrm_reg==0) || (CPUPMTimings[index].modrm_reg==modrm_register)) //MODR/M filter matches to full opcode?
									{
										if (sublistsize<NUMITEMS(sublist)) //Can we even add this item?
										{
											sublist[sublistsize++] = (index+1); //Add the index to the sublist, when possible!
										}
									}
								}
							}
					
							//Now, sort the items in their apropriate order!
							for (index=0;index<sublistsize;++index) //Process all items to sort!
							{
								for (sublistindex=index+1;sublistindex<sublistsize;++sublistindex) //The items to compare!
								{
									if (haslower286timingpriority(CPUmode,ismemory,sublist[index],sublist[sublistindex])) //Do we have lower timing priority (item must be after the item specified)?
									{
										tempsublist = sublist[index]; //Lower priority index saved!
										sublist[index] = sublist[sublistindex]; //Higher priority index to higher priority position!
										sublist[sublistindex] = tempsublist; //Lower priority index to lower priority position!
									}
								}
							}

							//Now, the sublist is filled with items needed for the entry!
							memcpy(&CPU[activeCPU].timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register],&sublist,MIN(sizeof(sublist),sizeof(CPU[activeCPU].timing286lookup[CPUmode][ismemory][is0Fopcode][instruction][modrm_register]))); //Copy the sublist to the active items!
						}
					}
				}
			}
		}
	}
	//The list is now ready for use!
}

void CPU_afterexec() //Stuff to do after execution of the OPCode (cycular tasks etc.)
{
	CPU[activeCPU].faultraised = 0; //We don't have a fault anymore! Continue on!

	if (CPU[activeCPU].registers->SFLAGS.TF) //Trapped and to be trapped this instruction?
	{
		if (CPU[activeCPU].trapped) //Are we trapped?
		{
			CPU_exSingleStep(); //Type-1 interrupt: Single step interrupt!
			CPU_afterexec(); //All after execution fixing!
		}
	}
}

extern uint_32 destEIP;

void CPU_resetOP() //Rerun current Opcode? (From interrupt calls this recalls the interrupts, handling external calls in between)
{
	CPU[activeCPU].registers->EIP = CPU_exec_EIP; //Destination address is reset!
	CPU_flushPIQ(); //Flush the PIQ!
	CPU[activeCPU].PIQ_EIP = CPU_exec_EIP; //Destination address of the PIQ is reset too!
}

//Exceptions!

//8086+ exceptions (real mode)

byte tempcycles;

void CPU_exDIV0() //Division by 0!
{
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	if (EMULATED_CPU == CPU_8086) //We point to the instruction following the division?
	{
		//Points to next opcode!
		call_soft_inthandler(EXCEPTION_DIVIDEERROR); //Execute INT0 normally using current CS:(E)IP!
	}
	else
	{
		//Points to next opcode!
		CPU_customint(EXCEPTION_DIVIDEERROR,CPU_exec_CS,CPU_exec_EIP,0); //Return to opcode!
	}
	CPU[activeCPU].cycles_Exception += CPU[activeCPU].cycles_OP; //Our cycles are counted as a hardware interrupt's cycles instead!
	CPU[activeCPU].cycles_OP = tempcycles; //Restore cycles!
}

extern byte HWINT_nr, HWINT_saved; //HW interrupt saved?

void CPU_exSingleStep() //Single step (after the opcode only)
{
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	HWINT_nr = 1; //Trapped INT NR!
	HWINT_saved = 1; //We're trapped!
	//Points to next opcode!
	call_soft_inthandler(EXCEPTION_DEBUG); //Execute INT1 normally using current CS:(E)IP!
	CPU[activeCPU].cycles_Exception += 50; //Our cycles!
	CPU[activeCPU].cycles_OP = tempcycles; //Restore cycles!
}

void CPU_BoundException() //Bound exception!
{
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	//Point to opcode origins!
	CPU_customint(EXCEPTION_BOUNDSCHECK,CPU_exec_CS,CPU_exec_EIP,0); //Return to opcode!
	CPU[activeCPU].cycles_Exception += CPU[activeCPU].cycles_OP; //Our cycles are counted as a hardware interrupt's cycles instead!
	CPU[activeCPU].cycles_OP = tempcycles; //Restore cycles!
}

void CPU_COOP_notavailable() //COProcessor not available!
{
	tempcycles = CPU[activeCPU].cycles_OP; //Save old cycles!
	//Point to opcode origins!
	CPU_customint(EXCEPTION_NOCOPROCESSOR,CPU_exec_CS,CPU_exec_EIP,0); //Return to opcode!
	CPU[activeCPU].cycles_Exception += CPU[activeCPU].cycles_OP; //Our cycles are counted as a hardware interrupt's cycles instead!
	CPU[activeCPU].cycles_OP = tempcycles; //Restore cycles!
}

void CPU_flushPIQ()
{
	if (CPU[activeCPU].PIQ) fifobuffer_clear(CPU[activeCPU].PIQ); //Clear the Prefetch Input Queue!
	CPU[activeCPU].PIQ_EIP = CPU[activeCPU].registers->EIP; //Save the PIQ EIP to the current address!
	CPU[activeCPU].repeating = 0; //We're not repeating anymore!
}

void CPU_fillPIQ() //Fill the PIQ until it's full!
{
	if (CPU[activeCPU].PIQ==0) return; //Not gotten a PIQ? Abort!
	byte oldMMUCycles;
	oldMMUCycles = CPU[activeCPU].cycles_MMUR; //Save the MMU cycles!
	CPU[activeCPU].cycles_MMUR = 0; //Counting raw time spent retrieving memory!
	writefifobuffer(CPU[activeCPU].PIQ, MMU_rb(CPU_SEGMENT_CS, CPU[activeCPU].registers->CS, CPU[activeCPU].PIQ_EIP++, 1)); //Add the next byte from memory into the buffer!
	CPU[activeCPU].cycles_Prefetch += CPU[activeCPU].cycles_MMUR; //Apply the memory cycles to prefetching!
	//Next data! Take 4 cycles on 8088, 2 on 8086 when loading words/4 on 8086 when loading a single byte.
	CPU[activeCPU].cycles_MMUR = oldMMUCycles; //Restore the MMU cycles!
}

void CPU_tickPrefetch()
{
	if (!CPU[activeCPU].PIQ) return; //Disable invalid PIQ!
	byte cycles;
	cycles = CPU[activeCPU].cycles; //How many cycles have been spent on the instruction?
	cycles -= CPU[activeCPU].cycles_MMUR; //Don't count memory access cycles!
	cycles -= CPU[activeCPU].cycles_MMUW; //Don't count memory access cycles!
	cycles -= CPU[activeCPU].cycles_Prefetch; //Don't count memory access cycles by prefetching required data!
	//Now we have the amount of cycles we're idling.
	if (EMULATED_CPU<CPU_80286) //Old CPU?
	{
		for (;(cycles >= 4) && fifobuffer_freesize(CPU[activeCPU].PIQ);) //Prefetch left to fill?
		{
			CPU_fillPIQ(); //Add a byte to the prefetch!
			cycles -= 4; //This takes four cycles to transfer!
		}
	}
	else //286+
	{
		for (;(cycles >= 3) && fifobuffer_freesize(CPU[activeCPU].PIQ);) //Prefetch left to fill?
		{
			CPU_fillPIQ(); //Add a byte to the prefetch!
			cycles -= 3; //This takes four cycles to transfer!
		}
	}
}

void CPU_unkOP() //General unknown OPcode handler!
{
	if (EMULATED_CPU>=CPU_NECV30) //Invalid opcode exception? 8086 just ignores the instruction and continues running!
	{
		unkOP_186(); //Execute the unknown opcode exception handler for the 186+!
	}
}
