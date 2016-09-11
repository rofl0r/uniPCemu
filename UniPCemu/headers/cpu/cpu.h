#ifndef CPU_H
#define CPU_H

#include "headers/types.h"
#include "headers/mmu/mmu.h"
#include "headers/bios/bios.h" //Basic BIOS!
#include "headers/support/fifobuffer.h" //Prefetch Input Queue support!

//CPU?
extern BIOS_Settings_TYPE BIOS_Settings; //BIOS Settings (required for determining emulating CPU)

//How many CPU instances are used?
#define MAXCPUS 2

//Number of currently supported CPUs & opcode 0F extensions.
#define NUMCPUS 6
#define NUM0FEXTS 4
//What CPU is emulated?
#define CPU_8086 0
#define CPU_NECV30 1
#define CPU_80286 2
#define CPU_80386 3
#define CPU_80486 4
#define CPU_PENTIUM 5

//How many modes are there in the CPU? Currently 2: 16-bit and 32-bit modes!
#define CPU_MODES 2

//Currently emulating CPU (values see above, formula later)?
#define EMULATED_CPU BIOS_Settings.emulated_CPU
//Since we're comparing to Bochs, emulate a Pentium PC!
//#define EMULATED_CPU CPU_PENTIUM

//For easygoing solid state segments (not changeable) in CPU[activeCPU].registers.SEGMENT_REGISTERS[]
#define CPU_SEGMENT_CS 0
#define CPU_SEGMENT_SS 1
#define CPU_SEGMENT_DS 2
#define CPU_SEGMENT_ES 3
#define CPU_SEGMENT_FS 4
#define CPU_SEGMENT_GS 5
#define CPU_SEGMENT_TR 6
#define CPU_SEGMENT_LDTR 7
//Default specified segment!
#define CPU_SEGMENT_DEFAULT 0xFF

//Descriptors used by the CPU

//Data segment descriptor
//based on: http://www.c-jump.com/CIS77/ASM/Protection/W77_0050_segment_descriptor_structure.htm


//New struct based on: http://lkml.indiana.edu/hypermail/linux/kernel/0804.0/1447.html

#define u16 word
#define u8 byte
#define u32 uint_32

//NULL pointer definition
#define NULLPTR(x) ((x.segment==0) && (x.offset==0))
//Same, but for pointer dereference
#define NULLPTR_PTR(x,location) (ANTINULL(x,location)?((x->segment==0) && (x->offset==0)):1)

typedef struct
{
	byte used; //Valid instruction? If zero, passthrough to earlier CPU timings.
	byte has_modrm; //Do we have ModR/M parameters?
	byte modrm_readparams_0; //First parameter of ModR/M setting
	byte modrm_readparams_1; //Second parameter of ModR/M setting
	byte modrm_src0; //ModR/M first parameter!
	byte modrm_src1; //ModR/M second parameter!
	byte parameters; //The type of parameters to follow the ModR/M! 0=No parameters, 1=imm8, 2=imm16, 3=imm32, bit 2=Immediate is enabled on the REG of the RM byte(only when <2).
	byte readwritebackinformation; //The type of writing back/reading data to memory if needed! Bits 0-1: 0=None, 1=Read, Write back operation, 2=Write operation only, 3=Read operation only, Bit 4: Operates on AL/AX/EAX when set. Bit 5: Push operation. Bit 6: Pop operation.
} CPU_Timings; //The CPU timing information!

typedef struct
{
	union
	{
		union
		{
			struct
			{
				u16 limit_low;
				u16 base_low;
				u8 base_mid;
				union
				{
					struct
					{
						u8 Type : 4; //System segment type! Used when nonS==0
						u8 nonS : 1; //Not system segment?
						u8 DPL : 2; //Descriptor Privilege level.
						u8 P : 1; //Present in memory?
					}; //General segment information!
					struct
					{
						u8 A : 1; //Accessed by user (has been loaded by OS)?
						u8 W : 1; //Writable?
						u8 E : 1; //Expand-down?
						u8 OTHERSTRUCT : 1; //Executable segment? When set we don't use this struct!
						byte upperdummy1 : 4; //Upper dummy!
					} DATASEGMENT;
					struct
					{
						u8 A_notused : 1; //Not used, use above A!
						u8 R : 1; //Readable?
						u8 C : 1; //Conforming?
						u8 ISEXEC : 1; //Must be 1 to use this struct!
						byte upperdummy2 : 4; //Upper dummy!
					} EXECSEGMENT;
					byte AccessRights; //Access rights!
				};
				u8 limit_high : 4; //High part of limit (if used).
				u8 AVL : 1; //Available for programmers use.
				u8 unk_0 : 1; //0!
				u8 D_B : 1; //D(default) or B(big) (or unknown(X) in a system segment)
				u8 G : 1; //Granularity: defines Segment limit and base address.
				u8 base_high;
			};
			byte bytes[8]; //The data as bytes!
		};
		uint_32 dwords[2]; //2 32-bit values for easy access!
		uint_64 DATA64; //Full data for simple set!
	};
} SEGMENT_DESCRIPTOR;

typedef struct
{
	word BackLink; //Back Link to Previous TSS
	word Unused0;
	uint_32 ESP0;
	word SS0;
	word Unused8;
	uint_32 ESP1;
	word SS1;
	word Unused10;
	uint_32 ESP2;
	word SS2;
	word Unused20;
	uint_32 CR3; //CR3 (PDPR)
	uint_32 EIP;
	uint_32 EFLAGS;
	uint_32 EAX;
	uint_32 ECX;
	uint_32 EDX;
	uint_32 EBX;
	uint_32 ESP;
	uint_32 EBP;
	uint_32 ESI;
	uint_32 EDI;
	word ES;
	word Unused48;
	word CS;
	word Unused4C;
	word SS;
	word Unused50;
	word DS;
	word Unused54;
	word FS;
	word Unused58;
	word GS;
	word Unused5C;
	word LDT;
	word Unused60;
	struct
	{
		byte T: 1;
		word Unused64: 15;
	};
	word IOMapBase;
} TSS386; //80386 32-Bit Task State Segment

typedef struct
{
	word BackLink; //Back Link to Previous TSS
	word SP0;
	word SS0;
	word SP1;
	word SS1;
	word SP2;
	word SS2;
	word IP;
	word FLAGS;
	word AX;
	word CX;
	word DX;
	word BX;
	word SP;
	word BP;
	word SI;
	word DI;
	word ES;
	word CS;
	word SS;
	word DS;
	word LDT;
} TSS286; //80286 32-Bit Task State Segment

typedef union
{
	struct
	{
		word offsethigh; //Higer part of the offset
		byte P : 1; //Present
		byte DPL : 2; //Descriptor Privilege Level
		byte S : 1; //Storage Segment. Set to 0 for interrupt gates
		word Type : 4; //One of the supported types!
		byte zero; //Must be zero!
		word selector; //The selector of the interrupt function. It's DPL field has be be 0.
		word offsetlow; //Lower part of the interrupt function's offset address (a.k.a. pointer)
	};
	byte descdata[8]; //The full entry data!
} IDTENTRY; //80286/80386 Interrupt descriptor table entry
		  
//A segment descriptor

//Full gate value
#define IDTENTRY_32BIT_TASKGATE 0x5

//Partial value for interrupt/trap gates!
#define IDTENTRY_16BIT_INTERRUPTGATE 0x6
#define IDTENTRY_16BIT_TRAPGATE 0x7
//32-bit variants (gate extensino set with interrupt&trap gates)
#define IDTENTRY_32BIT_GATEEXTENSIONFLAG 0x8

/*

Information:

A - ACCESSED
AVL - AVAILABLE FOR PROGRAMMERS USE
B - BIG
C - CONFORMING
D - DEFAULT
DPL - DESCRIPTOR PRIVILEGE LEVEL
E - EXPAND-DOWN
G - GRANUARITY
P - SEGMENT PRESENT (available for use?)
R - READABLE
W - WRITABLE


G: Determines Segment Limit & Segment size:
	G:0 = Segment Limit 1byte-1MB; 1B segment size; limit is ~1MB)
	G:1 = Segment Limit 4KB-4GB  ; 4KB (limit<<12 for limit of 4GB)

D_B: depends on type of access:
	code segment (see AVL):
		0=Operand size 16-bit
		1=Operand size 32-bit

	data segment (see AVL):
		0=SP is used with an upper bound of 0FFFFh (cleared for real mode)
		1=ESP is used with and upper bound of 0FFFFFFFh (set for protected mode)

S determines AVL type!

AVL: available to the programmer:
	see below for values.



*/

//AVL Also contains the type of segment (CODE DATA or SYSTEM)

//DATA descriptors:

#define AVL_DATA_READONLY 0
#define AVL_DATA_READONLY_ACCESSED 1
#define AVL_DATA_READWRITE 2
#define AVL_DATA_READWRITE_ACCESSED 3
#define AVL_DATA_READONLY_EXPANDDOWN 4
#define AVL_DATA_READONLY_EXPANDDOWN_ACCESSED 5
#define AVL_DATA_READWRITE_EXPANDDOWN 6
#define AVL_DATA_READWRITE_EXPANDDOWN_ACCESSED 7

//CODE descriptors:

#define AVL_CODE_EXECUTEONLY 8
#define AVL_CODE_EXECUTEONLY_ACCESSED 9
#define AVL_CODE_EXECUTE_READ 0xA
#define AVL_CODE_EXECUTE_READ_ACCESSED 0xB
#define AVL_CODE_EXECUTEONLY_CONFORMING 0xC
#define AVL_CODE_EXECUTEONLY_CONFORMING_ACCESSED 0xD
#define AVL_CODE_EXECUTE_READONLY_CONFORMING 0xE
#define AVL_CODE_EXECUTE_READONLY_CONFORMING_ACCESSED 0xF


//Type3 values (alternative for above):
#define AVL_TYPE3_READONLY 0
#define AVL_TYPE3_READWRITE 1
#define AVL_TYPE3_READONLY_EXPANDDOWN 2
#define AVL_TYPE3_READWRITE_EXPANDDOWN 3

//Or the bits:

#define AVL_TYPE3_ALLOWWRITEBIT 1
#define AVL_TYPE3_EXPANDDOWNBIT 2







//Extra data for above (bits on):

//Executable segment and values:
#define TYPE_EXEC_SEGBIT 4
#define EXEC_SEGBIT_DATASEG 0
#define EXEC_SEGBIT_CODESEG 1

//Expansion direction.
#define TYPE_EXPANDBIT 2
#define EXPAND_SEGBIT_UP 0
#define EXPAND_SEGBIT_DOWN 1

//Read/write
#define TYPE_READWRITEBIT 1






//System segment descriptor types:

#define AVL_SYSTEM_RESERVED_0 0
#define AVL_SYSTEM_TSS16BIT 1
#define AVL_SYSTEM_LDT 2
#define AVL_SYSTEM_BUSY_TSS16BIT 3
#define AVL_SYSTEM_CALLGATE16BIT 4
#define AVL_SYSTEM_TASKGATE 5
#define AVL_SYSTEM_INTERRUPTGATE16BIT 6
#define AVL_SYSTEM_TRAPGATE16BIT 7
#define AVL_SYSTEM_RESERVED_1 8
#define AVL_SYSTEM_TSS32BIT 9
#define AVL_SYSTEM_RESERVED_2 0xA
#define AVL_SYSTEM_BUSY_TSS32BIT 0xB
#define AVL_SYSTEM_CALLGATE32BIT 0xC
#define AVL_SYSTEM_RESERVED_3 0xD
#define AVL_SYSTEM_INTERRUPTGATE32BIT 0xE
#define AVL_SYSTEM_TRAPGATE32BIT 0xF







typedef struct //Seperate bits of above EFlags
{
	byte CF:1;
	byte unmapped2:1; //Unused. Value 1
	byte PF:1;
	byte unmapped8:1; //Unused. Value 0
	byte AF:1;
	byte unmapped32:1; //Unused. Value 0
	byte ZF:1;
	byte SF:1;
	byte TF:1;
	byte IF:1;
	byte DF:1;
	byte OF :1;
	byte IOPL : 2; //Always 1 on 186-
	byte NT : 1; //Always 1 on 186-
	byte unmapped32768 : 1; //Unused. Value 1 on 186-, 0 on later models.
	//High nibble
	byte RF : 1; //Resume flag. 386+
	byte V8 : 1; //Virtual 8086 mode flag (386+ only)
	byte AC : 1; //Alignment check (486SX+ only)
	byte VIF : 1; //Virtual interrupt flag (Pentium+)
	byte VIP : 1; //Virtual interrupt pending (Pentium+)
	byte ID : 1; //Able to use CPUID function (Pentium+)
	word unmappedhi : 10; //Rest data (unused)
} SEPERATEFLAGS; //Seperate flags!

typedef struct
{
	union
	{
		struct
		{
			word limit; //Limit
			uint_32 base; //Base
			word unused1; //Unused 1!
		};
		struct
		{
			uint_64 data : 48; //48 bits long!
			word unused2; //Unused data!
		};
	};
} TR_PTR;

//Different kinds of pointers!
typedef struct
{
	union
	{
		struct
		{
			uint_32 offset; //Default: 0
			unsigned int segment; //Default: 0
		};
		unsigned char data[6]; //46-bits as bytes!
	};
} ptr48; //48-bit pointer Adress (32-bit mode)!

typedef struct
{
	union
	{
		struct
		{
			unsigned int offset; //Default: 0
			unsigned int segment; //Default: 0
		};
		unsigned char data[4]; //32-bits as bytes!
	};
} ptr32; //32-bit pointer Adress (16-bit mode)!

typedef struct
{
	union
	{
		struct
		{
			unsigned short limit; //Limit!
			unsigned int base; //Base!
		};
		unsigned char data[6]; //46-bit adress!
	};
} GDTR_PTR;

typedef struct
{
	union
	{
		struct
		{
			unsigned int limit;
			uint_32 base;
		};
		unsigned char data[6]; //46-bit adress!
	};
} IDTR_PTR;

typedef struct //The registers!
{

//Info: with union, first low data then high data!
	union
	{
		struct
		{
			struct
			{
				union
				{
					word AX;
					struct
					{
						byte AL;
						byte AH;
					};
				};
			};
			word EAXDUMMY;
		};
		uint_32 EAX;
	};

	union
	{
		struct
		{
			struct
			{
				union
				{
					word BX;
					struct
					{
						byte BL;
						byte BH;
					};
				};
			};
			word EBXDUMMY;
		};
		uint_32 EBX;
	};

	union
	{
		struct
		{
			struct
			{
				union
				{
					word CX;
					struct
					{
						byte CL;
						byte CH;
					};
				};
			};
			word ECXDUMMY;
		};
		uint_32 ECX;
	};

	union
	{
		struct
		{
			struct
			{
				union
				{
					word DX;
					struct
					{
						byte DL;
						byte DH;
					};
				};
			};
			word EDXDUMMY;
		};
		uint_32 EDX;
	};











	union
	{
		uint_32 ESP; //ESP!
		struct
		{
			word SP; //Stack pointer
			word ESPDUMMY; //Dummy!
		};
	};
	union
	{
		uint_32 EBP; //EBP!
		struct
		{
			word BP; //Base pointer
			word EBPDUMMY; //Dummy!
		};
	};
	union
	{
		uint_32 ESI; //ESI
		struct
		{
			word SI; //Source index
			word ESIDUMMY; //Dummy!
		};
	};

	union
	{
		uint_32 EDI; //EDI
		struct
		{
			word DI; //Destination index
			word EDIDUMMY; //Dummy!
		};
	};

	union
	{
		uint_32 EIP; //EIP
		struct
		{
			word IP; //Instruction pointer; CS:IP=Current instruction; Reset at load of program
			word EIPDUMMY; //Dummy!
		};
	};

	word CS; //Code segment: start of program code; used by instructions and jumps.
	word DS; //Data segment: beginning of data storage section of memory; used by data manipulation
	word ES; //Extra segment: used for operations where data is transferred from one segment to another
	word SS; //Stack segment: stack segment
	word FS; //???
	word GS; //???

	union
	{
		struct
		{
			word FLAGS; //8086 Flags!
			word EFLAGSDUMMY; //Dummy!
		};
		uint_32 EFLAGS;
		SEPERATEFLAGS SFLAGS;
	};

	struct
	{
//Tables:
		TR_PTR GDTR; //GDTR pointer (48-bits) Global Descriptor Table Register
		TR_PTR IDTR; //IDTR pointer (48-bits) Interrupt Descriptor Table Register
		word LDTR; //LDTR pointer (16-bits) Local Descriptor Table Register (points to an index in the GDT)
		word TR; //TR (16-bits) Talk Register: currently executing task (points to an index in the GDT)

		union
		{
			uint_32 CR[4];
			struct
			{
				union
				{
					struct
					{
						byte PE : 1; //Protected mode enable
						byte MP : 1; //Math coprocessor present
						byte TS : 2; //Task Switched
						byte ET : 1; //Emulation Type
						uint_32 CR0unk : 26; //Not used!
						byte PG : 1; //Paging enable
					} CR0;
					uint_32 CR0_full;
				};
				uint_32 CR1; //Unused!
				uint_32 CR2; //Page Fault Linear Address
				union
				{
					struct
					{
						byte unused : 6; //Unused!
						uint_32 PageDirectoryBase : 26; //Page directory base register!
					} CR3;
					uint_32 CR3_full;
				};
			}; //CR0-3!
			uint_32 unusedCR[2]; //2 unused CRs!
		}; //CR0-3!
		//DR0-7; 4=6&5=7!
	}; //Special registers!

} CPU_registers; //Registers


typedef struct
{
	CPU_registers *registers; //The registers of the CPU!

	//Everything containing and buffering segment registers!
	SEGMENT_DESCRIPTOR SEG_DESCRIPTOR[8]; //Segment descriptor for all segment registers, currently cached, loaded when it's used!
	word *SEGMENT_REGISTERS[8]; //Segment registers pointers container (CS, SS, DS, ES, FS, GS, TR; in that order)!
	byte CPL; //The current privilege level, registered on descriptor load!

	uint_32 cycles; //Total cycles number (adjusted after operation)
	byte cycles_OP; //Total number of cycles for an operation!
	byte cycles_HWOP; //Total number of cycles for an hardware interrupt!
	byte cycles_Prefix; //Total number of cycles for the prefix!
	byte cycles_Exception; //Total number of cycles for an exception!
	byte cycles_MMUR, cycles_MMUW; //Total number of cycles for memory access!
	byte cycles_IO; //Total number of cycles for I/O access!
	byte cycles_Prefetch; //Total number of cycles for prefetching from memory!

	//PE in .registers.CR0.PE: In real mode or V86 mode (V86 flag&PE=V86; !PE=protected; else real)?

	byte segment_register; //Current segment register of the above!
	int halt; //Halted: waiting for interrupt to occur!
	int wait; //Wait: wait for TEST pin to occur (8087)
	int blocked; //Blocked=1: int 21 function 9C00h stops CPU. Reset blocked to 0 when occurs.
	int continue_int; //Continue interrupt call=1 or (POP CS:IP)=0?
	int calllayer; //What CALL layer are we (Starts with 0 for none, 1+=CALL)
	int running; //We're running?
	byte lastopcode; //Currently/last running opcode!
	byte faultraised; //Has a fault been raised by the protection module?
	byte faultlevel; //The level of the raised fault!
	byte trapped; //Have we been trapped? Don't execute hardware interrupts!

	//PIQ support!
	FIFOBUFFER *PIQ; //Our Prefetch Input Queue!
	uint_32 PIQ_EIP; //EIP of the current PIQ data!

	//REP support (ignore re-reading instruction bytes from memory)
	byte repeating; //We're executing a REP* instruction?

	//POP SS inhabits interrupts!
	byte allowInterrupts; //Do we allow interrupts to run?
	byte is0Fopcode; //Are we a 0F opcode to be executed?
} CPU_type;


#define F_CARRY 0x01
//Carry flag (CF): Math instr: high-order bit carried or borrowed, else cleared.
//0x02 unmapped
#define F_PARITY 0x04
//Parity flag (PF): Lower 8-bits of a result contains an even number of bits set to 1 (set), else not set (cleared).
//0x08 unmapped.
#define F_AUXILIARY_CARRY 0x10
//Auxiliary Carry flag (AF): (=Adjust flag?): Math instr: low order 4-bits of AL were carried or borrowed, else not set (cleared).
//0x20 unmapped.
#define F_ZERO 0x40
//Zero flag (ZF): Math instr: Result=0: Set; Else Cleared.
#define F_SIGN 0x80
//Sign flag (SF): Set equal to high-order bit of results of math instr. (Result<0)=>Set; (Result>=0)=>Cleared.
#define F_TRAP 0x100
//Trap flag (TF)
#define F_INTERRUPT 0x200
//Interrupt flag (IF)
#define F_DIRECTION 0x400
//Direction flag (DF): Used by string instr. to determine to process strings from end (DECR) or start (INCR).
#define F_OVERFLOW 0x800
//Overflow flag (OF): Indicates if the number placed in the detination operand overflowed, either too large or small. No overflow=Cleared.
#define F_IOPREV 0x3000
//I/O Privilege Level (two bits long) (PL)
#define F_NESTEDTASK 0x4000
//Nested Task Flag (NT)
//0x8000 unmapped.

//New flags in 80386:
#define F_RESUME 0x10000
//Resume flag
#define F_V8 0x20000
//Virtual 8086 MODE flag


//Same, but shorter:

#define F_CF F_CARRY
#define F_PF F_PARITY
#define F_AF F_AUXILIARY_CARRY
#define F_ZF F_ZERO
#define F_SF F_SIGN
#define F_TF F_TRAP
#define F_IF F_INTERRUPT
#define F_DF F_DIRECTION
#define F_PL F_IOPREV
#define F_NT F_NESTEDTASK
#define F_RF F_RESUME













//Now other structs we need:





typedef struct
{
	union
	{
		struct
		{
			byte low; //Low nibble
			byte high; //High nibble
		};
		word w; //The word value!
	};
} wordsplitter; //Splits word in two bytes!





















typedef struct
{
	union
	{
		struct
		{
			word wordlow; //Low nibble
			word wordhigh; //High nibble
		};
		uint_32 dword; //The word value!
	};
} dwordsplitter; //Splits dword (32 bits) in two words!


typedef struct
{
	union
	{
		struct //Contains int vars!
		{
			union //High 16-bits
			{
				struct //Contains byte vars!
				{
					byte low16_low;
					byte low16_high;
				};
				word low16;
			};
			union //High 16-bits
			{
				struct //Contains byte vars!
				{
					byte high16_low;
					byte high16_high;
				};
				word high16;
			};
		};
		uint_32 dword; //Dword var!
	};
} dwordsplitterb; //Splits dword (32 bits) in four bytes and subs (high/low16_high/low)!

typedef struct
{
	union
	{
		byte SIB;
		struct
		{
			byte base:3;
			byte index:3;
			byte scale:2;
		};
	};
} SIBType; //SIB byte!

#ifndef IS_CPU
extern byte activeCPU; //That currently active CPU!
extern CPU_type CPU[MAXCPUS]; //All CPUs itself!
extern byte CPU_Operand_size[2]; //Operand size for this opcode!
extern byte CPU_Address_size[2]; //Address size for this opcode!
extern byte CPU_StackAddress_size[2]; //Address size for this opcode!
#endif

#include "headers/cpu/cpu_ops.h"

//Overrides:

//Lock prefix
#define CPU_PREFIX_LOCK 0xF0
//REPNE, REPNZ prefix
#define CPU_PREFIX_REPNEZ 0xF2
//REP, REPZ, REPE prefix
#define CPU_PREFIX_REPZPE 0xF3
//Segment overrides:
#define CPU_PREFIX_CS 0x2E
#define CPU_PREFIX_SS 0x36
#define CPU_PREFIX_DS 0x3E
#define CPU_PREFIX_ES 0x26
#define CPU_PREFIX_FS 0x64
#define CPU_PREFIX_GS 0x65
//Operand override:
#define CPU_PREFIX_OP 0x66
//Address size override:
#define CPU_PREFIX_ADDR 0x67

//CPU Modes:
#define CPU_MODE_PROTECTED 1
#define CPU_MODE_REAL 0
#define CPU_MODE_8086 2
#define CPU_MODE_UNKNOWN 3

//Exception interrupt numbers!
#define EXCEPTION_DIVIDEERROR 0
#define EXCEPTION_DEBUG 1
#define EXCEPTION_NMI 2
#define EXCEPTION_CPUBREAKPOINT 3
#define EXCEPTION_OVERFLOW 4
#define EXCEPTION_BOUNDSCHECK 5
#define EXCEPTION_BADOPCODE 6
#define EXCEPTION_NOCOPROCESSOR 7
#define EXCEPTION_DOUBLEFAULT 8
#define EXCEPTION_COPROCESSOROVERRUN 9
#define EXCEPTION_INVALIDTSSSEGMENT 0xA
#define EXCEPTION_SEGMENTNOTPRESENT 0xB
#define EXCEPTION_STACKFAULT 0xC
#define EXCEPTION_GENERALPROTECTIONFAULT 0xD
#define EXCEPTION_PAGEFAULT 0xE
#define EXCEPTION_COPROCESSORERROR 0x10

void resetCPU(); //Initialises CPU!
void doneCPU(); //Finish the CPU!
byte CPU_readOP(); //Reads the operation (byte) at CS:EIP
word CPU_readOPw(); //Reads the operation (word) at CS:EIP
uint_32 CPU_readOPdw(); //Reads the operation (32-bit unsigned integer) at CS:EIP
void CPU_resetMode(); //Reset the mode to the default mode! (see above)
byte CPU_getprefix(byte prefix); //Prefix set? (might be used by OPcodes!)
byte getcpumode(); //Get current CPU mode (see CPU modes above!)

void CPU_resetOP(); //Rerun current Opcode? (From interrupt calls this recalls the interrupts, handling external calls in between)

//CPU executing functions:

void CPU_beforeexec(); //Everything before the execution of the current CPU OPcode!
void CPU_exec(); //Run one CPU OPCode!
//void CPU_exec_DEBUGGER(); //Processes the opcode at CS:EIP (386) or CS:IP (8086) for debugging.

//Sign extension!
#define SIGNEXTEND_16(x) (sword)x
#define SIGNEXTEND_32(x) (int_32)x

//Adress for booting: default is physical adress 0x7C00!
#define BOOT_SEGMENT 0x0000
#define BOOT_OFFSET 0x7C00


word CPU_segment(byte defaultsegment); //Plain segment to use (Plain and overrides)!
char * CPU_textsegment(byte defaultsegment); //Plain segment to use (text)!

//PUSH and POP for CPU STACK!

void CPU_PUSH8(byte val); //Push Byte!
byte CPU_POP8();

void CPU_PUSH16(word *val); //Push Word!
word CPU_POP16();

void CPU_PUSH32(uint_32 *val); //Push DWord!
uint_32 CPU_POP32(); //Full stack used!

void call_hard_inthandler(byte intnr); //Software interrupt handler (FROM hardware or EMULATOR INTERRUPTS only (int>=0x20 for software call from Handler))!


word *CPU_segment_ptr(byte defaultsegment); //Plain segment to use, direct access!

void copyint(byte src, byte dest); //Copy interrupt handler pointer to different interrupt!

#define signext(value) ((((word)value&0x80)*0x1FE)|(word)value)
#define signext32(value) ((((uint_32)value&0x8000)*0x1FFFE)|(uint_32)value)

//Software access with protection!
#define CPUPROT1 if(!CPU[activeCPU].faultraised){
#define CPUPROT2 }

#include "headers/cpu/interrupts.h" //Real interrupts!

//Extra:

#include "headers/cpu/modrm.h" //MODR/M comp!

//Read signed numbers from CS:(E)IP!
#define imm8() unsigned2signed8(immb)
#define imm16() unsigned2signed16(immw)
#define imm32() unsigned2signed32(imm32)

//Exceptions!

//8086+ CPU triggered exceptions (real mode)

void CPU_exDIV0(); //Division by 0!
void CPU_exSingleStep(); //Single step (after the opcode only)
void CPU_BoundException(); //Bound exception!
void CPU_COOP_notavailable(); //COProcessor not available!

void CPU_getint(byte intnr, word *segment, word *offset); //Set real mode IVT entry!

void generate_opcode_jmptbl(); //Generate the current CPU opcode jmptbl!
void generate_timings_tbl(); //Generate the timings table!
void updateCPUmode(); //Update the CPU mode!

byte CPU_segmentOverridden(byte activeCPU);
void CPU_8086REPPending(); //Execute this before CPU_exec!

byte execNMI(byte causeisMemory); //Execute an NMI!

void CPU_flushPIQ(); //Flush the PIQ!
void CPU_fillPIQ(); //Fill the PIQ until it's full!
void CPU_tickPrefetch(); //Ticks the prefetch cycles!
void CPU_unkOP(); //General unknown OPcode handler!


//Port I/O by the emulated CPU itself!
void CPU_PORT_OUT_B(word port, byte data);
void CPU_PORT_OUT_W(word port, word data);
void CPU_PORT_OUT_D(word port, uint_32 data);
void CPU_PORT_IN_B(word port, byte *result);
void CPU_PORT_IN_W(word port, word *result);
void CPU_PORT_IN_D(word port, uint_32 *result);

#endif
