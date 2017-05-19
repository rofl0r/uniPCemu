#ifndef CPU_H
#define CPU_H

#include "headers/types.h"
#include "headers/cpu/mmu.h"
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

//The types of parameters used in the instruction for the instruction text debugger!
#define PARAM_NONE 0
#define PARAM_MODRM1 1
#define PARAM_MODRM2 2
#define PARAM_MODRM12 3
#define PARAM_MODRM21 4
#define PARAM_IMM8 5
#define PARAM_IMM16 6
#define PARAM_IMM32 7
#define PARAM_MODRM12_IMM8 8
#define PARAM_MODRM21_IMM8 9
#define PARAM_MODRM12_CL 10
#define PARAM_MODRM21_CL 11
//Descriptors used by the CPU

//Data segment descriptor
//based on: http://www.c-jump.com/CIS77/ASM/Protection/W77_0050_segment_descriptor_structure.htm


//New struct based on: http://lkml.indiana.edu/hypermail/linux/kernel/0804.0/1447.html

#define u16 word
#define u8 byte
#define u32 uint_32

#include "headers/packed.h" //Packed type!
typedef struct PACKED
{
	byte used; //Valid instruction? If zero, passthrough to earlier CPU timings.
	byte has_modrm; //Do we have ModR/M parameters?
	byte modrm_size; //First parameter of ModR/M setting
	byte modrm_specialflags; //Second parameter of ModR/M setting
	byte modrm_src0; //ModR/M first parameter!
	byte modrm_src1; //ModR/M second parameter!
	byte parameters; //The type of parameters to follow the ModR/M! 0=No parameters, 1=imm8, 2=imm16, 3=imm32, bit 2=Immediate is enabled on the REG of the RM byte(only when <2).
	byte readwritebackinformation; //The type of writing back/reading data to memory if needed! Bits 0-1: 0=None, 1=Read, Write back operation, 2=Write operation only, 3=Read operation only, Bit 4: Operates on AL/AX/EAX when set. Bit 5: Push operation. Bit 6: Pop operation.
} CPU_Timings; //The CPU timing information!
#include "headers/endpacked.h" //End of packed type!

#include "headers/packed.h" //Packed type!
typedef struct PACKED
{
	union
	{
		union
		{
			struct
			{
				union
				{
					u16 limit_low;
					u16 callgate_base_low; //Call Gate base low!
				};
				union
				{
					u16 base_low; //Low base for non-Gate Descriptors!
					u16 selector; //Selector field of a Gate Descriptor!
				};
				union
				{
					u8 base_mid; //Mid base for non-Gate Descriptors!
					u8 ParamCnt; //Number of (uint_32) stack arguments to copy on stack switch(Call Gate Descriptor). Bits 4-7 have to be 0 for gate descriptors.
				};
				byte AccessRights; //Access rights!
				union
				{
					u8 noncallgate_info; //Non-callgate access!
					u8 callgate_base_mid;
				};
				union
				{
					u8 base_high;
					u8 callgate_base_high;
				};
			};
			byte bytes[8]; //The data as bytes!
		};
		uint_32 dwords[2]; //2 32-bit values for easy access!
		uint_64 DATA64; //Full data for simple set!
	};
} SEGMENT_DESCRIPTOR;
#include "headers/endpacked.h" //End of packed type!

//Code/data descriptor information(General/Data/Exec segment)
#define GENERALSEGMENT_TYPE(desc) (desc.AccessRights&0xF)
#define GENERALSEGMENT_S(desc) ((desc.AccessRights>>4)&1)
#define GENERALSEGMENT_DPL(desc) ((desc.AccessRights>>5)&3)
#define GENERALSEGMENT_P(desc) ((desc.AccessRights>>7)&1)
#define DATASEGMENT_A(desc) (desc.AccessRights&1)
#define DATASEGMENT_W(desc) ((desc.AccessRights>>1)&1)
#define DATASEGMENT_E(desc) ((desc.AccessRights>>2)&1)
#define DATASEGMENT_OTHERSTRUCT(desc) ((desc.AccessRights>>3)&1)
#define EXECSEGMENT_R(desc) ((desc.AccessRights>>1)&1)
#define EXECSEGMENT_C(desc) ((desc.AccessRights>>2)&1)
#define EXECSEGMENT_ISEXEC(desc) ((desc.AccessRights>>3)&1)

#define GENERALSEGMENTPTR_TYPE(desc) (desc->AccessRights&0xF)
#define GENERALSEGMENTPTR_S(desc) ((desc->AccessRights>>4)&1)
#define GENERALSEGMENTPTR_DPL(desc) ((desc->AccessRights>>5)&3)
#define GENERALSEGMENTPTR_P(desc) ((desc->AccessRights>>7)&1)
#define DATASEGMENTPTR_A(desc) (desc->AccessRights&1)
#define DATASEGMENTPTR_W(desc) ((desc->AccessRights>>1)&1)
#define DATASEGMENTPTR_E(desc) ((desc->AccessRights>>2)&1)
#define DATASEGMENTPTR_OTHERSTRUCT(desc) ((desc->AccessRights>>3)&1)
#define EXECSEGMENTPTR_R(desc) ((desc->AccessRights>>1)&1)
#define EXECSEGMENTPTR_C(desc) ((desc->AccessRights>>2)&1)
#define EXECSEGMENTPTR_ISEXEC(desc) ((desc->AccessRights>>3)&1)


//Pointer versions!

//Rest information in descriptors!
#define SEGDESC_NONCALLGATE_LIMIT_HIGH(desc) (desc.noncallgate_info&0xF)
#define SEGDESC_NONCALLGATE_AVL(desc) ((desc.noncallgate_info>>4)&1)
#define SEGDESC_NONCALLGATE_D_B(desc) ((desc.noncallgate_info>>6)&1)
#define SEGDESC_NONCALLGATE_G(desc) ((desc.noncallgate_info>>7)&1)

//Pointer versions!
#define SEGDESCPTR_NONCALLGATE_LIMIT_HIGH(desc) (desc->noncallgate_info&0xF)
#define SEGDESCPTR_NONCALLGATE_AVL(desc) ((desc->noncallgate_info>>4)&1)
#define SEGDESCPTR_NONCALLGATE_D_B(desc) ((desc->noncallgate_info>>6)&1)
#define SEGDESCPTR_NONCALLGATE_G(desc) ((desc->noncallgate_info>>7)&1)

#include "headers/packed.h" //Packed type!
typedef union PACKED
{
	struct
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
		word T; //1-bit, upper 15 bits unused!
		word IOMapBase;
	};
	byte data[108]; //All our data!
} TSS386; //80386 32-Bit Task State Segment
#include "headers/endpacked.h" //End of packed type!

#include "headers/packed.h" //Packed type!
typedef union PACKED
{
	struct
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
	};
	byte data[44]; //All our data!
} TSS286; //80286 32-Bit Task State Segment
#include "headers/endpacked.h" //End of packed type!

#include "headers/packed.h" //Packed type!
typedef union PACKED
{
	struct
	{
		word offsetlow; //Lower part of the interrupt function's offset address (a.k.a. pointer)
		word selector; //The selector of the interrupt function. It's DPL field has be be 0.
		byte zero; //Must be zero!
		byte AccessRights; //Access rights byte
		word offsethigh; //Higer part of the offset
	};
	byte descdata[8]; //The full entry data!
} IDTENTRY; //80286/80386 Interrupt descriptor table entry
#include "headers/endpacked.h" //End of packed type!

#define IDTENTRY_TYPE(ENTRY) (ENTRY.AccessRights&0xF)
#define IDTENTRY_S(ENTRY) ((ENTRY.AccessRights>>4)&1)
#define IDTENTRY_DPL(ENTRY) ((ENTRY.AccessRights>>5)&3)
#define IDTENTRY_P(ENTRY) ((ENTRY.AccessRights>>7)&1)

//A segment descriptor

//Full gate value
#define IDTENTRY_TASKGATE 0x5

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

//All flags, seperated!

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

//Read versions
#define FLAGREGR_CF(registers) (registers->FLAGS&1)
//Unused. Value 1
#define FLAGREGR_UNMAPPED2(registers) ((registers->FLAGS>>1)&1)
#define FLAGREGR_PF(registers) ((registers->FLAGS>>2)&1)
//Unused. Value 0
#define FLAGREGR_UNMAPPED8(registers) ((registers->FLAGS>>3)&1)
#define FLAGREGR_AF(registers) ((registers->FLAGS>>4)&1)
//Unused. Value 1 on 186-, 0 on later models.
#define FLAGREGR_UNMAPPED32768(registers) ((registers->FLAGS>>15)&1)
//Unused. Value 0
#define FLAGREGR_UNMAPPED32(registers) ((registers->FLAGS>>5)&1)
#define FLAGREGR_ZF(registers) ((registers->FLAGS>>6)&1)
#define FLAGREGR_SF(registers) ((registers->FLAGS>>7)&1)
#define FLAGREGR_TF(registers) ((registers->FLAGS>>8)&1)
#define FLAGREGR_IF(registers) ((registers->FLAGS>>9)&1)
#define FLAGREGR_DF(registers) ((registers->FLAGS>>10)&1)
#define FLAGREGR_OF(registers) ((registers->FLAGS>>11)&1)
//Always 1 on 186-
#define FLAGREGR_IOPL(registers) ((registers->FLAGS>>12)&3)
//Always 1 on 186-
#define FLAGREGR_NT(registers) ((registers->FLAGS>>14)&1)
//High nibble
//Resume flag. 386+
#define FLAGREGR_RF(registers) ((registers->EFLAGS>>16)&1)
//Virtual 8086 mode flag (386+ only)
#define FLAGREGR_V8(registers) ((registers->EFLAGS>>17)&1)
//Alignment check (486SX+ only)
#define FLAGREGR_AC(registers) ((registers->EFLAGS>>18)&1)
//Virtual interrupt flag (Pentium+)
#define FLAGREGR_VIF(registers) ((registers->EFLAGS>>19)&1)
//Virtual interrupt pending (Pentium+)
#define FLAGREGR_VIP(registers) ((registers->EFLAGS>>20)&1)
//Able to use CPUID function (Pentium+)
#define FLAGREGR_ID(registers) ((registers->EFLAGS>>21)&1)
#define FLAGREGR_UNMAPPEDHI(registers) (registers->EFLAGS>>22)&0x3FF

//Write versions
#define FLAGREGW_CF(registers,val) registers->FLAGS=((registers->FLAGS&~F_CF)|((val)&1))
//Unused. Value 1
#define FLAGREGW_UNMAPPED2(registers,val) registers->FLAGS=(((registers->FLAGS&~2)|(((val)&1)<<1)))
#define FLAGREGW_PF(registers,val) registers->FLAGS=(((registers->FLAGS&~4)|(((val)&1)<<2)))
//Unused. Value 0
#define FLAGREGW_UNMAPPED8(registers,val) registers->FLAGS=(((registers->FLAGS&~8)|(((val)&1)<<3)))
#define FLAGREGW_AF(registers,val) registers->FLAGS=(((registers->FLAGS&~0x10)|(((val)&1)<<4)))
//Unused. Value 1 on 186-, 0 on later models.
#define FLAGREGW_UNMAPPED32768(registers,val) registers->FLAGS=(((registers->FLAGS&~0x8000)|(((val)&1)<<15)))
//Unused. Value 0
#define FLAGREGW_UNMAPPED32(registers,val) registers->FLAGS=(((registers->FLAGS&~0x20)|(((val)&1)<<5)))
#define FLAGREGW_ZF(registers,val) registers->FLAGS=(((registers->FLAGS&~0x40)|(((val)&1)<<6)))
#define FLAGREGW_SF(registers,val) registers->FLAGS=(((registers->FLAGS&~0x80)|(((val)&1)<<7)))
#define FLAGREGW_TF(registers,val) registers->FLAGS=(((registers->FLAGS&~0x100)|(((val)&1)<<8)))
#define FLAGREGW_IF(registers,val) registers->FLAGS=(((registers->FLAGS&~0x200)|(((val)&1)<<9)))
#define FLAGREGW_DF(registers,val) registers->FLAGS=(((registers->FLAGS&~0x400)|(((val)&1)<<10)))
#define FLAGREGW_OF(registers,val) registers->FLAGS=(((registers->FLAGS&~0x800)|(((val)&1)<<11)))
//Always 1 on 186-
#define FLAGREGW_IOPL(registers,val) registers->FLAGS=(((registers->FLAGS&~0x3000)|(((val)&3)<<12)))
//Always 1 on 186-
#define FLAGREGW_NT(registers,val) registers->FLAGS=(((registers->FLAGS&~0x4000)|(((val)&1)<<14)))
//High nibble
//Resume flag. 386+
#define FLAGREGW_RF(registers,val) registers->EFLAGS=(((registers->EFLAGS&~0x10000)|(((val)&1)<<16)))
//Virtual 8086 mode flag (386+ only)
#define FLAGREGW_V8(registers,val) registers->EFLAGS=(((registers->EFLAGS&~0x20000)|(((val)&1)<<17)))
//Alignment check (486SX+ only)
#define FLAGREGW_AC(registers,val) registers->EFLAGS=(((registers->EFLAGS&~0x40000)|(((val)&1)<<18)))
//Virtual interrupt flag (Pentium+)
#define FLAGREGW_VIF(registers,val) registers->EFLAGS=(((registers->EFLAGS&~0x80000)|(((val)&1)<<19)))
//Virtual interrupt pending (Pentium+)
#define FLAGREGW_VIP(registers,val) registers->EFLAGS=(((registers->EFLAGS&~0x100000)|(((val)&1)<<20)))
//Able to use CPUID function (Pentium+)
#define FLAGREGW_ID(registers,val) registers->EFLAGS=(((registers->EFLAGS&~0x200000)|(((val)&1)<<21)))
#define FLAGREGW_UNMAPPEDHI(registers,val) registers->EFLAGS=((registers->EFLAGS&0x3FFFFF)|(((val)&0x3FF)<<22))

#include "headers/packed.h" //Packed type!
typedef struct PACKED
{
	union
	{
		struct
		{
			word limit; //Limit
			uint_32 base; //Base
			word unused1; //Unused 1!
		};
		uint_64 data; //48 bits long!
	};
} TR_PTR;
#include "headers/endpacked.h" //End of packed type!

#include "headers/packed.h" //Packed type!
typedef struct PACKED //The registers!
{

//Info: with union, first low data then high data!
	union
	{
		struct
		{
			#ifdef IS_BIG_ENDIAN
			word EAXDUMMY;
			#endif
			struct
			{
				union
				{
					word AX;
					struct
					{
						#ifndef IS_BIG_ENDIAN
						byte AL;
						byte AH;
						#else
						byte AH;
						byte AL;
						#endif
					};
				};
			};
			#ifndef IS_BIG_ENDIAN
			word EAXDUMMY;
			#endif
		};
		uint_32 EAX;
	};

	union
	{
		struct
		{
			#ifdef IS_BIG_ENDIAN
			word EBXDUMMY;
			#endif
			struct
			{
				union
				{
					word BX;
					struct
					{
						#ifndef IS_BIG_ENDIAN
						byte BL;
						byte BH;
						#else
						byte BH;
						byte BL;
						#endif
					};
				};
			};
			#ifndef IS_BIG_ENDIAN
			word EBXDUMMY;
			#endif
		};
		uint_32 EBX;
	};

	union
	{
		struct
		{
			#ifdef IS_BIG_ENDIAN
			word ECXDUMMY;
			#endif
			struct
			{
				union
				{
					word CX;
					struct
					{
						#ifndef IS_BIG_ENDIAN
						byte CL;
						byte CH;
						#else
						byte CH;
						byte CL;
						#endif
					};
				};
			};
			#ifndef IS_BIG_ENDIAN
			word ECXDUMMY;
			#endif
		};
		uint_32 ECX;
	};

	union
	{
		struct
		{
			#ifdef IS_BIG_ENDIAN
			word EDXDUMMY;
			#endif
			struct
			{
				union
				{
					word DX;
					struct
					{
						#ifndef IS_BIG_ENDIAN
						byte DL;
						byte DH;
						#else
						byte DH;
						byte DL;
						#endif
					};
				};
			};
			#ifndef IS_BIG_ENDIAN
			word EDXDUMMY;
			#endif
		};
		uint_32 EDX;
	};

	union
	{
		uint_32 ESP; //ESP!
		struct
		{
			#ifdef IS_BIG_ENDIAN
			word ESPDUMMY;
			#endif
			word SP; //Stack pointer
			#ifndef IS_BIG_ENDIAN
			word ESPDUMMY; //Dummy!
			#endif
		};
	};
	union
	{
		uint_32 EBP; //EBP!
		struct
		{
			#ifdef IS_BIG_ENDIAN
			word EBPDUMMY;
			#endif
			word BP; //Base pointer
			#ifndef IS_BIG_ENDIAN
			word EBPDUMMY; //Dummy!
			#endif
		};
	};
	union
	{
		uint_32 ESI; //ESI
		struct
		{
			#ifdef IS_BIG_ENDIAN
			word ESIDUMMY;
			#endif
			word SI; //Source index
			#ifndef IS_BIG_ENDIAN
			word ESIDUMMY; //Dummy!
			#endif
		};
	};

	union
	{
		uint_32 EDI; //EDI
		struct
		{
			#ifdef IS_BIG_ENDIAN
			word EDIDUMMY;
			#endif
			word DI; //Destination index
			#ifndef IS_BIG_ENDIAN
			word EDIDUMMY; //Dummy!
			#endif
		};
	};

	union
	{
		uint_32 EIP; //EIP
		struct
		{
			#ifdef IS_BIG_ENDIAN
			word EIPDUMMY;
			#endif
			word IP; //Instruction pointer; CS:IP=Current instruction; Reset at load of program
			#ifndef IS_BIG_ENDIAN
			word EIPDUMMY; //Dummy!
			#endif
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
			#ifdef IS_BIG_ENDIAN
			word EFLAGSDUMMY;
			#endif
			word FLAGS; //8086 Flags!
			#ifndef IS_BIG_ENDIAN
			word EFLAGSDUMMY; //Dummy!
			#endif
		};
		uint_32 EFLAGS;
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
			uint_32 CR[8];
			struct
			{
				uint_32 CR0;
				uint_32 CR1; //Unused!
				uint_32 CR2; //Page Fault Linear Address
				uint_32 CR3;
				uint_32 CR4; //4 unused CRs until Pentium!
				uint_32 CR5;
				uint_32 CR6;
				uint_32 CR7;
			}; //CR0-3!
		}; //CR0-3!
		union
		{
			uint_32 DR[8]; //All debugger registers! index 4=>6 and index 5=>7!
			struct
			{
				uint_32 DR0;
				uint_32 DR1;
				uint_32 DR2;
				uint_32 DR3;
				uint_32 DR4; //Existant on Pentium+ only! Redirected to DR6 on 386+, except when enabled using CR4.
				uint_32 noDRregister; //Not defined: DR5 on Pentium! Redirected to DR7 on 386+(when not disabled using CR4).
				uint_32 DR6; //DR4->6 on 386+, DR6 on Pentium+!
				uint_32 DR7; //DR5->7 on 386+, DR7 on Pentium+!
			};
		}; //DR0-7; 4=6&5=7!
		union
		{
			uint_32 TRX[8]; //All debugger registers! index 4=>6 and index 5=>7!
			struct
			{
				uint_32 TR0;
				uint_32 TR1;
				uint_32 TR2;
				uint_32 TR3;
				uint_32 TR4;
				uint_32 TR5;
				uint_32 TR6;
				uint_32 TR7;
			};
		}; //DR0-7; 4=6&5=7!
	}; //Special registers!
} CPU_registers; //Registers
#include "headers/endpacked.h" //End of packed type!

//Protected mode enable
#define CR0_PE 0x00000001
//Math coprocessor present
#define CR0_MP 0x00000002 
//Emulation: math instructions are to be emulated?
#define CR0_EM 0x00000004
//Task Switched
#define CR0_TS 0x00000008 
//Extension Type: type of coprocessor present, 80286 or 80387
#define CR0_ET 0x00000010
//26 unknown/unspecified bits
//Bit 31
//Paging enable
 #define CR0_PG 0x80000000

#include "headers/packed.h" //Packed type!
typedef struct PACKED
{
	byte CPU_isFetching; //1=Fetching/decoding new instruction to execute(CPU_readOP_prefix), 0=Executing instruction(decoded)
	byte CPU_fetchphase; //Fetching phase: 1=Reading new opcode, 2=Reading prefixes or opcode, 3=Reading 0F instruction, 0=Main Opcode fetched
	byte CPU_fetchingRM; //1=Fetching modR/M parameters, 0=ModR/M loaded when used.
	byte CPU_fetchparameters; //1+=Fetching parameter #X, 0=Parameters fetched.
	byte CPU_fetchparameterPos; //Parameter position we're fetching. 0=First byte, 1=Second byte etc.
} CPU_InstructionFetchingStatus;
#include "headers/endpacked.h" //End of packed type!

#include "headers/packed.h" //Packed type!
typedef struct PACKED
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
	byte cycles_EA; //ModR/M decode cycles!
	byte cycles_Exception; //Total number of cycles for an exception!
	byte cycles_Prefetch; //Total number of cycles for prefetching from memory!
	byte cycles_stallBIU; //How many cycles to stall the BIU this step?
	byte cycles_stallBUS; //How many cycles to stall the BUS(all CPU hardware) this step?
	byte cycles_Prefetch_BIU; //BIU cycles actually spent on prefetching during the remaining idle BUS time!
	byte cycles_Prefetch_DMA; //DMA cycles actually spent on prefetching during the remaining idle BUS time!

	//PE in .registers.CR0.PE: In real mode or V86 mode (V86 flag&PE=V86; !PE=protected; else real)?

	byte segment_register; //Current segment register of the above!
	int halt; //Halted: waiting for interrupt to occur!
	int wait; //Wait: wait for TEST pin to occur (8087)
	int blocked; //Blocked=1: int 21 function 9C00h stops CPU. Reset blocked to 0 when occurs.
	int continue_int; //Continue interrupt call=1 or (POP CS:IP)=0?
	int calllayer; //What CALL layer are we (Starts with 0 for none, 1+=CALL)
	int running; //We're running?
	byte lastopcode; //Currently/last running opcode!
	byte previousopcode; //Previous opcode for diagnostic purposes!
	byte previousopcode0F; //Previous opcode 0F state!
	uint_32 previousCSstart; //Previous CS starting address!
	byte faultraised; //Has a fault been raised by the protection module?
	byte faultlevel; //The level of the raised fault!
	byte faultraised_lasttype; //Last type of fault raised!
	byte trapped; //Have we been trapped? Don't execute hardware interrupts!

	//REP support (ignore re-reading instruction bytes from memory)
	byte repeating; //We're executing a REP* instruction?

	//POP SS inhabits interrupts!
	byte allowInterrupts; //Do we allow interrupts to run?
	byte allowTF; //Allow trapping now?
	byte is0Fopcode; //Are we a 0F opcode to be executed?
	byte D_B_Mask; //D_B bit mask when used for 16 vs 32-bits!
	byte G_Mask; //G bit mask when used for 16 vs 32-bits!

	//For stack argument copying of call gates!
	FIFOBUFFER *CallGateStack; //Arguments to copy!
	byte is_reset; //Are we a reset CPU?
	byte permanentreset; //Are we in a permanent reset lock?

	//80286 timing support for lookup tables!
	byte have_oldESP; //oldESP is set to use?
	uint_32 oldESP; //Back-up of ESP during stack faults to use!
	byte have_oldSS; //oldSS is set to use?
	word oldSS;
	byte have_oldSegments;
	word oldSegmentFS, oldSegmentGS, oldSegmentDS, oldSegmentES; //Back-up of the segment registers to restore during faults!
	byte have_oldEFLAGS;
	uint_32 oldEFLAGS;
	byte debuggerFaultRaised; //Debugger faults raised after execution flags?
	CPU_InstructionFetchingStatus instructionfetch; //Information about fetching the current instruction. This contains the status we're in!
	byte executed; //Has the current instruction finished executing?
	byte instructionstep, internalinstructionstep, internalmodrmstep, internalinterruptstep, stackchecked; //Step we're at, executing the instruction that's fetched and loaded to execute.
	byte pushbusy; //Is a push operation busy?
	byte BUSactive; //Is the BUS currently active? Determines who's owning the BUS: 0=No control, 1=CPU, 2=DMA
} CPU_type;
#include "headers/endpacked.h" //End of packed type!

//Now other structs we need:

#include "headers/packed.h" //Packed type!
typedef struct PACKED
{
	union
	{
		struct
		{
			#ifndef IS_BIG_ENDIAN
			byte low; //Low nibble
			byte high; //High nibble
			#else
			byte high;
			byte low;
			#endif
		};
		word w; //The word value!
	};
} wordsplitter; //Splits word in two bytes!
#include "headers/endpacked.h" //End of packed type!

#include "headers/packed.h" //Packed type!
typedef struct PACKED
{
	union
	{
		struct
		{
			#ifndef IS_BIG_ENDIAN
			word wordlow; //Low nibble
			word wordhigh; //High nibble
			#else
			word wordhigh;
			word wordlow;
			#endif
		};
		uint_32 dword; //The word value!
	};
} dwordsplitter; //Splits dword (32 bits) in two words!
#include "headers/endpacked.h" //End of packed type!

#include "headers/packed.h" //Packed type!
typedef struct PACKED
{
	union
	{
		struct //Contains int vars!
		{
			#ifdef IS_BIG_ENDIAN
			union //High 16-bits
			{
				struct //Contains byte vars!
				{
					#ifndef IS_BIG_ENDIAN
					byte high16_low;
					byte high16_high;
					#else
					byte high16_high;
					byte high16_low;
					#endif
				};
				word high16;
			};
			#endif
			union //Low 16-bits
			{
				struct //Contains byte vars!
				{
					#ifndef IS_BIG_ENDIAN
					byte low16_low;
					byte low16_high;
					#else
					byte low16_high;
					byte low16_low;
					#endif
				};
				word low16;
			};
			#ifndef IS_BIG_ENDIAN
			union //High 16-bits
			{
				struct //Contains byte vars!
				{
					#ifndef IS_BIG_ENDIAN
					byte high16_low;
					byte high16_high;
					#else
					byte high16_high;
					byte high16_low;
					#endif
				};
				word high16;
			};
			#endif
		};
		uint_32 dword; //Dword var!
	};
} dwordsplitterb; //Splits dword (32 bits) in four bytes and subs (high/low16_high/low)!
#include "headers/endpacked.h" //End of packed type!

#include "headers/packed.h" //Packed type!
typedef union PACKED
{
	struct
	{
		#ifdef IS_BIG_ENDIAN
		word val16high; //Filler
		#endif
		union
		{
			struct
			{
				#ifdef IS_BIG_ENDIAN
				byte val8high;
				#endif
				union
				{
					byte val8;
					sbyte val8s;
				};
				#ifndef IS_BIG_ENDIAN
				byte val8high;
				#endif
			};
			word val16; //Normal
			sword val16s; //Signed
		};
		#ifndef IS_BIG_ENDIAN
		word val16high; //Filler
		#endif
	};
	uint_32 val32; //Normal
	int_32 val32s; //Signed
} VAL32Splitter; //Our 32-bit value splitter!
#include "headers/endpacked.h" //End of packed type!

#include "headers/packed.h" //Packed type!
typedef union PACKED
{
	struct
	{
		#ifdef IS_BIG_ENDIAN
		uint_32 val32high; //Filler
		#endif
		union
		{
			struct
			{
				union
				{
					uint_32 val32;
					struct
					{
						#ifdef IS_BIG_ENDIAN
						word val16_high;
						#endif
						word val16;
						#ifndef IS_BIG_ENDIAN
						word val16_high;
						#endif
					};
				};
			};
			int_32 val32s;
		};
		#ifndef IS_BIG_ENDIAN
		uint_32 val32high; //Filler
		#endif
	};
	uint_32 val64; //Normal
	int_32 val64s; //Signed
} VAL64Splitter; //Our 32-bit value splitter!
#include "headers/endpacked.h" //End of packed type!

#define SIB_BASE(SIB) (SIB&7)
#define SIB_INDEX(SIB) ((SIB>>3)&7)
#define SIB_SCALE(SIB) ((SIB>>6)&3)
typedef byte SIBType; //SIB byte!

#ifndef IS_CPU
extern byte activeCPU; //That currently active CPU!
extern CPU_type CPU[MAXCPUS]; //All CPUs itself!
extern byte CPU_Operand_size[2]; //Operand size for this opcode!
extern byte CPU_Address_size[2]; //Address size for this opcode!
#endif

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
//Real mode has no special stuff
#define CPU_MODE_REAL 0
//Protected mode has bit 1 set
#define CPU_MODE_PROTECTED 1
//Virtual 8086 mode has bit 1(Protected mode) and bit 2(Virtual override) set
#define CPU_MODE_8086 3
//Virtual override without Protected mode has no effect: it's real mode after all!
#define CPU_MODE_UNKNOWN 2

//Exception interrupt numbers!
#define EXCEPTION_DIVIDEERROR 0
#define EXCEPTION_DEBUG 1
#define EXCEPTION_NMI 2
#define EXCEPTION_CPUBREAKPOINT 3
#define EXCEPTION_OVERFLOW 4
#define EXCEPTION_BOUNDSCHECK 5
#define EXCEPTION_INVALIDOPCODE 6
#define EXCEPTION_COPROCESSORNOTAVAILABLE 7
#define EXCEPTION_DOUBLEFAULT 8
#define EXCEPTION_COPROCESSOROVERRUN 9
#define EXCEPTION_INVALIDTSSSEGMENT 0xA
#define EXCEPTION_SEGMENTNOTPRESENT 0xB
#define EXCEPTION_STACKFAULT 0xC
#define EXCEPTION_GENERALPROTECTIONFAULT 0xD
#define EXCEPTION_PAGEFAULT 0xE
#define EXCEPTION_COPROCESSORERROR 0x10

#define EXCEPTION_TABLE_GDT 0x00
#define EXCEPTION_TABLE_IDT 0x01
#define EXCEPTION_TABLE_LDT 0x02
//0x03 seems to be an alias of 0x01?

void initCPU(); //Initialize CPU for full system reset into known state!
void resetCPU(); //Initialises CPU!
void doneCPU(); //Finish the CPU!
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
char *CPU_textsegment(byte defaultsegment); //Plain segment to use (text)!

//PUSH and POP for CPU STACK! The _BIU suffixes place a request for the BIU to handle it (and requires a response to be read, which is either the result of the operation or 1 for writes).

void CPU_PUSH8(byte val); //Push Byte!
byte CPU_PUSH8_BIU(byte val); //Push Byte!
byte CPU_POP8();
byte CPU_POP8_BIU(); //Request an 8-bit POP from the BIU!

void CPU_PUSH16(word *val); //Push Word!
byte CPU_PUSH16_BIU(word *val); //Push Word!
word CPU_POP16();
byte CPU_POP16_BIU(); //Pop Word!

void CPU_PUSH32(uint_32 *val); //Push DWord!
byte CPU_PUSH32_BIU(uint_32 *val); //Push DWord!
uint_32 CPU_POP32(); //Full stack used!
byte CPU_POP32_BIU(); //Full stack used!

byte call_soft_inthandler(byte intnr, int_64 errorcode); //Software interrupt handler (FROM software interrupts only (int>=0x20 for software call from Handler))!
byte call_hard_inthandler(byte intnr); //Software interrupt handler (FROM hardware only)!


word *CPU_segment_ptr(byte defaultsegment); //Plain segment to use, direct access!

void copyint(byte src, byte dest); //Copy interrupt handler pointer to different interrupt!

#define signext(value) ((((word)value&0x80)*0x1FE)|(word)value)
#define signext32(value) ((((uint_32)value&0x8000)*0x1FFFE)|(uint_32)value)

//Software access with protection!
#define CPUPROT1 if(CPU[activeCPU].faultraised==0){
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

void CPU_unkOP(); //General unknown OPcode handler!


//Port I/O by the emulated CPU itself!
byte CPU_PORT_OUT_B(word port, byte data);
byte CPU_PORT_OUT_W(word port, word data);
byte CPU_PORT_OUT_D(word port, uint_32 data);
byte CPU_PORT_IN_B(word port, byte *result);
byte CPU_PORT_IN_W(word port, word *result);
byte CPU_PORT_IN_D(word port, uint_32 *result);

//Information for simulating PUSH and POP on the stack!
sbyte stack_pushchange(byte dword);
sbyte stack_popchange(byte dword);

byte isPM(); //Are we in protected mode?
byte isV86(); //Are we in Virtual 8086 mode?

//ModR/M debugger support!
byte NumberOfSetBits(uint_32 i); //Number of bits set in this variable!

byte checkStackAccess(uint_32 poptimes, byte isPUSH, byte isdword); //How much do we need to POP from the stack?
byte checkENTERStackAccess(uint_32 poptimes, byte isdword); //How much do we need to POP from the stack?

void CPU_resetTimings(); //Reset timings before processing the next CPU state!

void CPU_JMPrel(int_32 reladdr);
void CPU_JMPabs(uint_32 addr);
uint_32 CPU_EIPmask();
byte CPU_EIPSize();

byte CPU_apply286cycles(); //Apply the 80286+ cycles method. Result: 0 when to apply normal cycles. 1 when 80286+ cycles are applied!
#endif
