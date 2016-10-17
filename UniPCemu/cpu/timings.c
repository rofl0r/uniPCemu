#include "headers/cpu/cpu.h" //Basic types!

typedef struct
{
	byte CPU; //For what CPU(286 relative)? 0=286, 1=386, 2=486, 3=586(Pentium) etc
	byte is0F; //Are we an extended instruction(0F instruction)?
	byte OPcode; //The opcode to be applied to!
	byte OPcodemask; //The mask to be applied to the original opcode to match this opcode in order to be applied!
	byte modrm_reg; //>0: Substract 1 for the modr/m reg requirement. Else no modr/m is looked at!
	struct
	{
		struct
		{
			word basetiming;
			word n; //With RO*/SH*/SAR is the amount of bytes actually shifted; With String instructions, added to base ount with multiplier(number of repeats after first instruction)
			byte addclock; //bit 0=Add one clock if we're using 3 memory operands! bit 1=n is count to add for string instructions (every repeat).  This variant is only used with string instructions., bit 2=We depend on the gate used. The gate type we're for is specified in the low 4 bits of n. The upper 2(bits 4-5) bits of n specify: 1=Same privilege level Call gate, 2=Different privilege level Call gate, no parameters, 3=Different privilege level, X parameters, 0=Ignore privilege level/parameters in the cycle calculation.
			//Setting addclock bit 2, n lower bits to call gate and n higher bits to 2 adds 4 cycles for each parameter on a 80286.
		} ismemory[2]; //First entry is register value(modr/m register-register), Second entry is memory value(modr/m register-memory)
	} CPUmode[2]; //0=Real mode, 1=Protected mode
} CPUPM_Timings;

//Lower 4 bits of the n information
#define GATECOMPARISON_CALLGATE 1
#define GATECOMPARISON_TSS 2
#define GATECOMPARISON_TASKGATE 3
//Special RET case for returning to different privilege levels!
#define GATECOMPARISON_RET 4

//High 2 bits of the n information
#define CALLGATETIMING_SAMEPRIVILEGELEVEL 1
#define CALLGATETIMING_DIFFERENTPRIVILEGELEVEL_NOPARAMETERS 2
#define CALLGATETIMING_DIFFERENTPRIVILEGELEVEL_XPARAMETERS 3
#define GATETIMING_ANYPRIVILEGELEVEL 0

//Simplified stuff for 286 gate descriptors(combination of the above flags used, which are used in the lookup table multiple times)!
#define CALLGATE_SAMELEVEL ((GATECOMPARISON_CALLGATE)|(CALLGATETIMING_SAMEPRIVILEGELEVEL<<4))
#define CALLGATE_DIFFERENTLEVEL_NOPARAMETERS ((GATECOMPARISON_CALLGATE)|(CALLGATETIMING_DIFFERENTPRIVILEGELEVEL_NOPARAMETERS<<4))
#define CALLGATE_DIFFERENTLEVEL_XPARAMETERS ((GATECOMPARISON_CALLGATE)|(CALLGATETIMING_DIFFERENTPRIVILEGELEVEL_XPARAMETERS<<4))
#define OTHERGATE_NORMALTSS ((GATECOMPARISON_TSS)|(GATETIMING_ANYPRIVILEGELEVEL<<4))
#define OTHERGATE_NORMALTASKGATE ((GATECOMPARISON_TASKGATE)|(GATETIMING_ANYPRIVILEGELEVEL<<4))
#define RET_DIFFERENTLEVEL ((GATECOMPARISON_RET)|(CALLGATETIMING_DIFFERENTPRIVILEGELEVEL_NOPARAMETERS<<4))

//Compressed protected&real mode timing table. This will need to be uncompressed for usage to be usable(long lookup times otherwise)
CPUPM_Timings CPUPMTimings[] = {
	//286 CPU timings
	//MOV
	{0,0,0x88,0xFE,0x00,{{{{2,0,0},{3,0,1}}},{{{2,0,0},{3,0,1}}}}} //MOV Register to Register/Memory
	,{0,0,0x8A,0xFE,0x00,{{{{2,0,0},{5,0,1}}},{{{2,0,0},{5,0,1}}}}} //MOV Register/memory to Register
	,{0,0,0xC6,0xFE,0x01,{{{{2,0,0},{3,0,1}}},{{{2,0,0},{3,0,1}}}}} //MOV Immediate to register/memory
	,{0,0,0xB0,0xF0,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //MOV Immediate to register
	,{0,0,0xA0,0xFE,0x00,{{{{5,0,0},{5,0,0}}},{{{5,0,0},{5,0,0}}}}} //MOV Memory to accumulator
	,{0,0,0xA2,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //MOV Accumulator to memory
	,{0,0,0x8E,0xFF,0x00,{{{{2,0,0},{5,0,1}}},{{{17,0,0},{19,0,1}}}}} //MOV Register/memory to segment register
	,{0,0,0x8C,0xFF,0x00,{{{{2,0,0},{3,0,1}}},{{{2,0,0},{3,0,1}}}}} //MOV Segment register to register/memory
	//PUSH
	,{0,0,0xFF,0xFF,0x07,{{{{5,0,1},{5,0,1}}},{{{5,0,1},{5,0,1}}}}} //PUSH Memory
	,{0,0,0x50,0xF8,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //PUSH Register
	,{0,0,0x06,0xE7,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //PUSH Segment register
	,{0,0,0x68,0xFD,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //PUSH immediate
	,{0,0,0x60,0xFF,0x00,{{{{17,0,0},{17,0,0}}},{{{17,0,0},{17,0,0}}}}} //PUSHA
	//POP
	,{0,0,0x8F,0xFF,0x01,{{{{5,0,1},{5,0,1}}},{{{5,0,1},{5,0,1}}}}} //POP Memory
	,{0,0,0x58,0xF8,0x00,{{{{5,0,0},{5,0,0}}},{{{5,0,0},{5,0,0}}}}} //POP Register
	,{0,0,0x07,0xFF,0x00,{{{{5,0,0},{5,0,0}}},{{{20,0,0},{20,0,0}}}}} //POP Segment register
	,{0,0,0x17,0xFF,0x00,{{{{5,0,0},{5,0,0}}},{{{20,0,0},{20,0,0}}}}}  //POP Segment register
	,{0,0,0x1F,0xFF,0x00,{{{{5,0,0},{5,0,0}}},{{{20,0,0},{20,0,0}}}}} //POP Segment register
	,{0,0,0x61,0xFF,0x00,{{{{19,0,0},{19,0,0}}},{{{19,0,0},{19,0,0}}}}} //POPA
	//XCHG
	,{0,0,0x86,0xFE,0x00,{{{{3,0,0},{5,0,1}}},{{{3,0,0},{5,0,1}}}}} //XCHG Register/memory with register
	,{0,0,0x90,0xF8,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //XCHG Register with accumulator
	//IN
	,{0,0,0xE4,0xFE,0x00,{{{{5,0,0},{5,0,0}}},{{{5,0,0},{5,0,0}}}}} //IN Fixed port
	,{0,0,0xEC,0xFE,0x00,{{{{5,0,0},{5,0,0}}},{{{5,0,0},{5,0,0}}}}} //IN Variable port
	//OUT
	,{0,0,0xE6,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //OUT Fixed port
	,{0,0,0xEE,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //OUT Variable port
	//XLAT
	,{0,0,0xD7,0xFF,0x00,{{{{5,0,0},{5,0,0}}},{{{5,0,0},{5,0,0}}}}} //XLAT
	//LEA
	,{0,0,0x8D,0xFF,0x00,{{{{3,0,1},{3,0,1}}},{{{3,0,1},{3,0,1}}}}} //LEA
	//LDS
	,{0,0,0xC5,0xFF,0x00,{{{{7,0,1},{7,0,1}}},{{{21,0,1},{21,0,1}}}}} //LDS
	//LES
	,{0,0,0xC4,0xFF,0x00,{{{{7,0,1},{7,0,1}}},{{{21,0,1},{21,0,1}}}}} //LES

	//Page 3-48
	//LAHF
	,{0,0,0x9F,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //LAHF
	//SAHF
	,{0,0,0x9E,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //SAHF
	//PUSHF
	,{0,0,0x9C,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //PUSHF
	//POPF
	,{0,0,0x9D,0xFF,0x00,{{{{5,0,0},{5,0,0}}},{{{5,0,0},{5,0,0}}}}} //POPF
	//ADD
	,{0,0,0x00,0xFC,0x00,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //ADD Reg/memory with register to either
	,{0,0,0x80,0xFC,0x01,{{{{3,0,0},{7,0,1}}},{{{3,0,0},{7,0,1}}}}} //ADD Immediate to register/memory
	,{0,0,0x04,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //ADD Immediate to accumulator
	//ADC
	,{0,0,0x10,0xFC,0x00,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //ADC Reg/memory with register to either
	,{0,0,0x80,0xFC,0x03,{{{{3,0,0},{7,0,1}}},{{{3,0,0},{7,0,1}}}}} //ADC Immediate to register/memory
	,{0,0,0x14,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //ADC Immediate to accumulator
	//INC
	,{0,0,0xFE,0xFE,0x01,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //INC Register/memory
	,{0,0,0x40,0xF8,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //INC Register
	//SUB
	,{0,0,0x28,0xFC,0x00,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //SUB Reg/memory and register to either
	,{0,0,0x80,0xFC,0x06,{{{{3,0,0},{7,0,1}}},{{{3,0,0},{7,0,1}}}}} //SUB Immediate from register/memory
	,{0,0,0x2C,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //SUB Immediate from accumulator
	//SBB
	,{0,0,0x18,0xFC,0x00,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //SBB Reg/memory and register to either
	,{0,0,0x80,0xFC,0x04,{{{{3,0,0},{7,0,1}}},{{{3,0,0},{7,0,1}}}}} //SBB Immediate from register/memory
	,{0,0,0x1C,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //SBB Immediate from accumulator
	//DEC
	,{0,0,0xFE,0xFE,0x02,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //DEC Register/memory
	,{0,0,0x48,0xF8,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //DEC Register
	//CMP
	,{0,0,0x3A,0xFE,0x00,{{{{2,0,0},{6,0,1}}},{{{2,0,0},{6,0,1}}}}} //CMP Register/memory with register
	,{0,0,0x38,0xFE,0x00,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //CMP Register with register/memory
	,{0,0,0x80,0xFC,0x08,{{{{3,0,0},{6,0,1}}},{{{3,0,0},{6,0,1}}}}} //CMP Immediate with register/memory
	,{0,0,0x3C,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //CMP Immediate with accumulator
	//NEG
	,{0,0,0xF6,0xFE,0x04,{{{{2,0,0},{2,0,0}}},{{{7,0,1},{7,0,1}}}}} //NEG
	//AAA
	,{0,0,0x37,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //AAA
	//DAA
	,{0,0,0x27,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //DAA

	//Page 3-49
	//AAS
	,{0,0,0x3F,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //AAS
	//DAS
	,{0,0,0x2F,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //DAS
	//MUL
	,{0,0,0xF6,0xFF,0x05,{{{{13,0,0},{16,0,1}}},{{{13,0,0},{16,0,1}}}}} //MULB Register/Memory-Byte
	,{0,0,0xF7,0xFF,0x05,{{{{21,0,0},{24,0,1}}},{{{21,0,0},{24,0,1}}}}} //MULW Register/Memory-Word
	//IMUL
	,{0,0,0xF6,0xFF,0x06,{{{{13,0,0},{16,0,1}}},{{{13,0,0},{16,0,1}}}}} //IMULB Register/Memory-Byte
	,{0,0,0xF7,0xFF,0x06,{{{{21,0,0},{24,0,1}}},{{{21,0,0},{24,0,1}}}}} //IMULW Register/Memory-Word
	//IMUL (186+ instruction)
	,{0,0,0x69,0xFD,0x00,{{{{21,0,0},{24,0,1}}},{{{21,0,0},{24,0,1}}}}} //IMUL
	//DIV
	,{0,0,0xF6,0xFF,0x07,{{{{14,0,0},{17,0,1}}},{{{14,0,0},{17,0,1}}}}} //DIV Register/Memory-Byte
	,{0,0,0xF7,0xFF,0x07,{{{{22,0,0},{25,0,1}}},{{{22,0,0},{25,0,1}}}}} //DIV Register/Memory-Word
	//IDIV
	,{0,0,0xF6,0xFF,0x08,{{{{17,0,0},{20,0,1}}},{{{17,0,0},{20,0,1}}}}} //IDIV Register/Memory-Byte
	,{0,0,0xF7,0xFF,0x08,{{{{25,0,0},{28,0,1}}},{{{25,0,0},{28,0,1}}}}} //IDIV Register/Memory-Word
	//AAM
	,{0,0,0xD4,0xFF,0x00,{{{{16,0,0},{16,0,0}}},{{{16,0,0},{16,0,0}}}}} //AAM
	//AAD
	,{0,0,0xD5,0xFF,0x00,{{{{14,0,0},{14,0,0}}},{{{14,0,0},{14,0,0}}}}} //AAD
	//CBW
	,{0,0,0x98,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //CBW
	//CWD
	,{0,0,0x99,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //CWD
	//Shift/Rotate Instructions
	,{0,0,0xD0,0xFE,0x00,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //Shift/Rotate Register/Memory by 1
	,{0,0,0xD2,0xFE,0x00,{{{{5,1,0},{8,1,1}}},{{{5,1,0},{8,1,1}}}}} //Shift/Rotate Register/Memory by CL
	,{0,0,0xC0,0xFE,0x00,{{{{5,1,0},{8,1,1}}},{{{5,1,0},{8,1,1}}}}} //Shift/Rotate Register/Memory by Count

	//Page 3-50
	//AND
	,{0,0,0x20,0xFC,0x00,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //AND Reg/memory and register to either
	,{0,0,0x80,0xFE,0x05,{{{{3,0,0},{7,0,1}}},{{{3,0,0},{7,0,1}}}}} //AND Immediate to register/memory
	,{0,0,0x24,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //AND Immediate to accumulator
	//TEST
	,{0,0,0x84,0xFE,0x00,{{{{2,0,0},{6,0,1}}},{{{2,0,0},{6,0,1}}}}} //TEST Register/memory and register
	,{0,0,0xF6,0xFE,0x01,{{{{3,0,0},{6,0,1}}},{{{3,0,0},{6,0,1}}}}} //TEST Immediate data and register/memory
	,{0,0,0xA8,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //TEST Immediate data and accumulator
	//OR
	,{0,0,0x08,0xFC,0x00,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //OR Reg/memory and register to either
	,{0,0,0x80,0xFE,0x02,{{{{3,0,0},{7,0,1}}},{{{3,0,0},{7,0,1}}}}} //OR Immediate to register/memory
	,{0,0,0x0C,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //OR Immediate to accumulator
	//XOR
	,{0,0,0x30,0xFC,0x00,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //XOR Reg/memory and register to either
	,{0,0,0x80,0xFE,0x07,{{{{3,0,0},{7,0,1}}},{{{3,0,0},{7,0,1}}}}} //XOR Immediate to register/memory
	,{0,0,0x34,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //XOR Immediate to accumulator
	//NOT
	,{0,0,0xF6,0xFE,0x03,{{{{2,0,0},{7,0,1}}},{{{2,0,0},{7,0,1}}}}} //NOT

	//String instructions without REP((N)Z)
	//MOVS
	,{0,0,0xA4,0xFE,0x00,{{{{5,0,0},{5,0,0}}},{{{5,0,0},{5,0,0}}}}} //MOVS
	//CMPS
	,{0,0,0xA6,0xFE,0x00,{{{{8,0,0},{8,0,0}}},{{{8,0,0},{8,0,0}}}}} //CMPS
	//SCAS
	,{0,0,0xAE,0xFE,0x00,{{{{7,0,0},{7,0,0}}},{{{7,0,0},{7,0,0}}}}} //SCAS
	//LODS
	,{0,0,0xAC,0xFE,0x00,{{{{5,0,0},{5,0,0}}},{{{5,0,0},{5,0,0}}}}} //LODS
	//STOS
	,{0,0,0xAA,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //STOS
	//INS
	,{0,0,0x6C,0xFE,0x00,{{{{5,0,0},{5,0,0}}},{{{5,0,0},{5,0,0}}}}} //INS
	//OUTS
	,{0,0,0x6E,0xFE,0x00,{{{{5,0,0},{5,0,0}}},{{{5,0,0},{5,0,0}}}}} //OUTS

	//String instructions with REP(N)(Z)
	//MOVS
	,{0,0,0xA4,0xFE,0x00,{{{{5,4,2},{5,4,2}}},{{{5,4,2},{5,4,2}}}}} //MOVS
	//CMPS
	,{0,0,0xA6,0xFE,0x00,{{{{5,9,2},{5,9,2}}},{{{5,9,2},{5,9,2}}}}} //CMPS
	//SCAS
	,{0,0,0xAE,0xFE,0x00,{{{{5,8,2},{5,8,2}}},{{{5,8,2},{5,8,2}}}}} //SCAS
	//LODS
	,{0,0,0xAC,0xFE,0x00,{{{{5,4,2},{5,4,2}}},{{{5,4,2},{5,4,2}}}}} //LODS
	//STOS
	,{0,0,0xAA,0xFE,0x00,{{{{4,3,2},{4,3,2}}},{{{4,3,2},{4,3,2}}}}} //STOS
	//INS
	,{0,0,0x6C,0xFE,0x00,{{{{5,4,2},{5,4,2}}},{{{5,4,2},{5,4,2}}}}} //INS
	//OUTS
	,{0,0,0x6E,0xFE,0x00,{{{{5,4,2},{5,4,2}}},{{{5,4,2},{5,4,2}}}}} //OUTS

	//Page 3-51
	//We don't use the m value: this is done by the prefetch unit itself.
	//CALL Direct Intersegment
	,{0,0,0xE8,0xFF,0x00,{{{{7,0,0},{7,0,0}}},{{{7,0,0},{7,0,0}}}}} //CALL Direct within segment
	,{0,0,0xFF,0xFF,0x03,{{{{7,0,0},{11,0,1}}},{{{7,0,0},{11,0,1}}}}} //CALL Register/memory indirect within segment
	,{0,0,0x9A,0xFF,0x00,{{{{13,0,0},{26,0,0}}},{{{13,0,0},{26,0,0}}}}} //CALL Direct Intersegment

	//Protected mode variants
	,{0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{41,CALLGATE_SAMELEVEL,4},{41,CALLGATE_SAMELEVEL,4}}}}} //CALL Via call gate to same privilege level
	,{0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{82,CALLGATE_DIFFERENTLEVEL_NOPARAMETERS,4},{82,CALLGATE_DIFFERENTLEVEL_NOPARAMETERS,4}}}}} //CALL VIa call gate to different privilege level, no parameters
	,{0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{86,CALLGATE_DIFFERENTLEVEL_XPARAMETERS,4},{86,CALLGATE_DIFFERENTLEVEL_XPARAMETERS,4}}}}} //CALL VIa call gate to different privilege level, X parameters
	,{0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{177,OTHERGATE_NORMALTSS,4},{177,OTHERGATE_NORMALTSS,4}}}}} //CALL Via TSS
	,{0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{182,OTHERGATE_NORMALTASKGATE,4},{182,OTHERGATE_NORMALTASKGATE,4}}}}} //CALL Via task gate

	//CALL Indirect Intersegment
	,{0,0,0xFF,0xFF,0x04,{{{{16,0,0},{16,0,0}}},{{{29,0,1},{29,0,1}}}}} //CALL Register/memory indirect within segment

	//Protected mode variants
	,{0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{44,CALLGATE_SAMELEVEL,5},{44,CALLGATE_SAMELEVEL,5}}}}} //CALL Via call gate to same privilege level
	,{0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{83,CALLGATE_DIFFERENTLEVEL_NOPARAMETERS,5},{83,CALLGATE_DIFFERENTLEVEL_NOPARAMETERS,5}}}}} //CALL VIa call gate to different privilege level, no parameters
	,{0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{90,CALLGATE_DIFFERENTLEVEL_XPARAMETERS,5},{90,CALLGATE_DIFFERENTLEVEL_XPARAMETERS,5}}}}} //CALL VIa call gate to different privilege level, X parameters
	,{0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{180,OTHERGATE_NORMALTSS,5},{180,OTHERGATE_NORMALTSS,5}}}}} //CALL Via TSS
	,{0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{185,OTHERGATE_NORMALTASKGATE,5},{185,OTHERGATE_NORMALTASKGATE,5}}}}} //CALL Via task gate

	//JMP
	,{0,0,0xEB,0xFF,0x00,{{{{7,0,0},{7,0,0}}},{{{7,0,0},{7,0,0}}}}} //JMP Short/long
	,{0,0,0xED,0xFF,0x00,{{{{7,0,0},{7,0,0}}},{{{7,0,0},{7,0,0}}}}} //JMP Direct within segment
	,{0,0,0xFF,0xFF,0x05,{{{{7,0,0},{11,0,0}}},{{{7,0,0},{11,0,0}}}}} //JMP Register/memory indirect within segment
	,{0,0,0xEA,0xFF,0x00,{{{{11,0,0},{11,0,0}}},{{{23,0,0},{23,0,0}}}}} //JMP Direct intersegment
	
	//Protected mode variants(Direct Intersegment)
	,{0,0,0xEA,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{38,CALLGATE_SAMELEVEL,5},{38,CALLGATE_SAMELEVEL,5}}}}} //JMP Via call gate to same privilege level
	,{0,0,0xEA,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{175,OTHERGATE_NORMALTSS,5},{175,OTHERGATE_NORMALTSS,5}}}}} //JMP Via TSS
	,{0,0,0xEA,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{180,OTHERGATE_NORMALTASKGATE,5},{180,OTHERGATE_NORMALTASKGATE,5}}}}} //JMP Via task gate

	//JMP Indirect Intersegment
	,{0,0,0xFF,0xFF,0x06,{{{{15,0,1},{15,0,1}}},{{{26,0,1},{26,0,1}}}}} //JMP Indirect intersegment

	//Protected mode variants (Indirect Intersegment)
	,{0,0,0xFF,0xFF,0x06,{{{{0,0,0},{0,0,0}}},{{{41,CALLGATE_SAMELEVEL,5},{41,CALLGATE_SAMELEVEL,5}}}}} //JMP Via call gate to same privilege level
	,{0,0,0xFF,0xFF,0x06,{{{{0,0,0},{0,0,0}}},{{{178,OTHERGATE_NORMALTSS,5},{178,OTHERGATE_NORMALTSS,5}}}}} //JMP Via TSS
	,{0,0,0xFF,0xFF,0x06,{{{{0,0,0},{0,0,0}}},{{{183,OTHERGATE_NORMALTASKGATE,5},{183,OTHERGATE_NORMALTASKGATE,5}}}}} //JMP Via task gate

	//RET
	,{0,0,0xC3,0xFF,0x00,{{{{11,0,0},{11,0,0}}},{{{11,0,0},{11,0,0}}}}} //RET Within segment
	,{0,0,0xC2,0xFF,0x00,{{{{11,0,0},{11,0,0}}},{{{11,0,0},{11,0,0}}}}} //RET Within seg adding immed to SP
	,{0,0,0xCB,0xFF,0x00,{{{{15,0,0},{15,0,0}}},{{{25,0,0},{25,0,0}}}}} //RET Intersegment
	,{0,0,0xCA,0xFF,0x00,{{{{15,0,0},{15,0,0}}},{{{15,0,0},{15,0,0}}}}} //RET Intersegment adding immediate to SP

	//Protected mode variants (Intersegment)
	,{0,0,0xCB,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{55,RET_DIFFERENTLEVEL,4},{55,RET_DIFFERENTLEVEL,4}}}}} //RET Intersegment
	,{0,0,0xCA,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{55,RET_DIFFERENTLEVEL,4},{55,RET_DIFFERENTLEVEL,4}}}}} //RET Intersegment adding immediate to SP

	//Page 3-52


};

CPU_Timings CPUInformation[NUMCPUS][2][0x100] = {
	{
		{ //16-bit!
			{ 1,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 1,1,1,0,1,0,0,0x01 }, //01
			{ 1,1,0,0,0,1,0,0x01 }, //02
			{ 1,1,1,0,0,1,0,0x01 }, //03
			{ 1,0,0,0,0,0,1,0x04 }, //04
			{ 1,0,0,0,0,0,2,0x04 }, //05
			{ 1,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 1,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 1,1,0,0,1,0,0,0x01 }, //08 OR
			{ 1,1,1,0,1,0,0,0x01 }, //09
			{ 1,1,0,0,0,1,0,0x01 }, //0A
			{ 1,1,1,0,0,1,0,0x01 }, //0B
			{ 1,0,0,0,0,0,1,0x04 }, //0C
			{ 1,0,0,0,0,0,2,0x04 }, //0D
			{ 1,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 1,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 1,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 1,1,1,0,1,0,0,0x01 }, //11
			{ 1,1,0,0,0,1,0,0x01 }, //12
			{ 1,1,1,0,0,1,0,0x01 }, //13
			{ 1,0,0,0,0,0,1,0x04 }, //14
			{ 1,0,0,0,0,0,2,0x04 }, //15
			{ 1,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 1,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 1,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 1,1,1,0,1,0,0,0x01 }, //19
			{ 1,1,0,0,0,1,0,0x01 }, //1A
			{ 1,1,1,0,0,1,0,0x01 }, //1B
			{ 1,0,0,0,0,0,1,0x04 }, //1C
			{ 1,0,0,0,0,0,2,0x04 }, //1D
			{ 1,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 1,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 1,1,0,0,1,0,0,0x01 }, //20 AND
			{ 1,1,1,0,1,0,0,0x01 }, //21
			{ 1,1,0,0,0,1,0,0x01 }, //22
			{ 1,1,1,0,0,1,0,0x01 }, //23
			{ 1,0,0,0,0,0,1,0x04 }, //24
			{ 1,0,0,0,0,0,2,0x04 }, //25
			{ 1,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 1,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 1,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 1,1,1,0,1,0,0,0x01 }, //29
			{ 1,1,0,0,0,1,0,0x01 }, //2A
			{ 1,1,1,0,0,1,0,0x01 }, //2B
			{ 1,0,0,0,0,0,1,0x04 }, //2C
			{ 1,0,0,0,0,0,2,0x04 }, //2D
			{ 1,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 1,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 1,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 1,1,1,0,1,0,0,0x01 }, //31
			{ 1,1,0,0,0,1,0,0x01 }, //32
			{ 1,1,1,0,0,1,0,0x01 }, //33
			{ 1,0,0,0,0,0,1,0x04 }, //34
			{ 1,0,0,0,0,0,2,0x04 }, //35
			{ 1,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 1,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 1,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 1,1,1,0,1,0,0,0x01 }, //39
			{ 1,1,0,0,0,1,0,0x01 }, //3A
			{ 1,1,1,0,0,1,0,0x01 }, //3B
			{ 1,0,0,0,0,0,1,0x04 }, //3C
			{ 1,0,0,0,0,0,2,0x04 }, //3D
			{ 1,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 1,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 1,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 1,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 1,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 1,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 1,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 1,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 1,1,1,0,1,0,2,0x00 }, //81 GRP1
			{ 1,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 1,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 1,1,0,0,0,1,0,0x00 }, //84 TEST
			{ 1,1,1,0,0,1,0,0x00 }, //85 TEST
			{ 1,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 1,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 1,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 1,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 1,1,0,0,0,1,0,0x01 }, //8A MOV
			{ 1,1,1,0,0,1,0,0x01 }, //8B MOV
			{ 1,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 1,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 1,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 1,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 1,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 1,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 1,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 1,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 1,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 1,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 1,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 1,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 1,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 1,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 1,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 1,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 1,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 1,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 1,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 1,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 1,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 1,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 1,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 1,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 1,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 1,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 1,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 1,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 1,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 1,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 1,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 1,0,0,0,0,0,0,0x00 }, //C0 186+ opcode
			{ 1,0,0,0,0,0,0,0x00 }, //C1 186+ opcode
			{ 1,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 1,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 1,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 1,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 1,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 1,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //C8 RETF imm16
			{ 1,0,0,0,0,0,0,0x00 }, //C9 RETF
			{ 1,0,0,0,0,0,2,0x00 }, //CA RET imm16
			{ 1,0,0,0,0,0,0,0x00 }, //CB RET
			{ 1,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 1,0,0,0,0,0,1,0x00 }, //CD INT
			{ 1,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 1,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 1,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 1,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 1,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 1,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 1,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 1,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 1,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 1,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 1,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 1,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 1,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 1,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 1,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 1,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 1,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 1,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 1,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 1,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 1,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 1,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 1,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 1,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 1,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 1,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 1,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 1,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 1,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 1,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 1,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 1,1,0,0,1,0,5,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 1,1,1,0,1,0,6,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 1,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 1,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 1,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 1,0,0,0,0,0,0,0x00 }, //FB STI
			{ 1,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 1,0,0,0,0,0,0,0x00 }, //FD STD
			{ 1,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 1,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}, //16-bit
		{ //32-bit
			{ 1,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 1,1,1,0,1,0,0,0x01 }, //01
			{ 1,1,0,0,0,1,0,0x01 }, //02
			{ 1,1,1,0,0,1,0,0x01 }, //03
			{ 1,0,0,0,0,0,1,0x04 }, //04
			{ 1,0,0,0,0,0,2,0x04 }, //05
			{ 1,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 1,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 1,1,0,0,1,0,0,0x01 }, //08 OR
			{ 1,1,1,0,1,0,0,0x01 }, //09
			{ 1,1,0,0,0,1,0,0x01 }, //0A
			{ 1,1,1,0,0,1,0,0x01 }, //0B
			{ 1,0,0,0,0,0,1,0x04 }, //0C
			{ 1,0,0,0,0,0,2,0x04 }, //0D
			{ 1,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 1,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 1,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 1,1,1,0,1,0,0,0x01 }, //11
			{ 1,1,0,0,0,1,0,0x01 }, //12
			{ 1,1,1,0,0,1,0,0x01 }, //13
			{ 1,0,0,0,0,0,1,0x04 }, //14
			{ 1,0,0,0,0,0,2,0x04 }, //15
			{ 1,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 1,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 1,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 1,1,1,0,1,0,0,0x01 }, //19
			{ 1,1,0,0,0,1,0,0x01 }, //1A
			{ 1,1,1,0,0,1,0,0x01 }, //1B
			{ 1,0,0,0,0,0,1,0x04 }, //1C
			{ 1,0,0,0,0,0,2,0x04 }, //1D
			{ 1,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 1,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 1,1,0,0,1,0,0,0x01 }, //20 AND
			{ 1,1,1,0,1,0,0,0x01 }, //21
			{ 1,1,0,0,0,1,0,0x01 }, //22
			{ 1,1,1,0,0,1,0,0x01 }, //23
			{ 1,0,0,0,0,0,1,0x04 }, //24
			{ 1,0,0,0,0,0,2,0x04 }, //25
			{ 1,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 1,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 1,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 1,1,1,0,1,0,0,0x01 }, //29
			{ 1,1,0,0,0,1,0,0x01 }, //2A
			{ 1,1,1,0,0,1,0,0x01 }, //2B
			{ 1,0,0,0,0,0,1,0x04 }, //2C
			{ 1,0,0,0,0,0,2,0x04 }, //2D
			{ 1,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 1,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 1,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 1,1,1,0,1,0,0,0x01 }, //31
			{ 1,1,0,0,0,1,0,0x01 }, //32
			{ 1,1,1,0,0,1,0,0x01 }, //33
			{ 1,0,0,0,0,0,1,0x04 }, //34
			{ 1,0,0,0,0,0,2,0x04 }, //35
			{ 1,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 1,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 1,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 1,1,1,0,1,0,0,0x01 }, //39
			{ 1,1,0,0,0,1,0,0x01 }, //3A
			{ 1,1,1,0,0,1,0,0x01 }, //3B
			{ 1,0,0,0,0,0,1,0x04 }, //3C
			{ 1,0,0,0,0,0,2,0x04 }, //3D
			{ 1,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 1,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 1,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 1,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 1,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 1,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 1,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 1,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 1,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 1,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 1,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 1,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 1,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 1,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 1,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 1,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 1,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 1,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 1,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 1,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 1,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 1,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 1,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 1,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 1,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 1,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 1,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 1,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 1,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 1,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 1,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 1,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 1,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 1,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 1,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 1,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 1,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 1,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 1,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 1,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 1,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 1,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 1,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 1,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 1,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 1,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 1,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 1,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 1,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 1,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 1,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 1,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 1,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 1,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 1,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 1,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 1,0,0,0,0,0,0,0x00 }, //C0 186+ opcode
			{ 1,0,0,0,0,0,0,0x00 }, //C1 186+ opcode
			{ 1,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 1,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 1,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 1,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 1,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 1,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //C8 RETF imm16
			{ 1,0,0,0,0,0,0,0x00 }, //C9 RETF
			{ 1,0,0,0,0,0,2,0x00 }, //CA RET imm16
			{ 1,0,0,0,0,0,0,0x00 }, //CB RET
			{ 1,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 1,0,0,0,0,0,1,0x00 }, //CD INT
			{ 1,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 1,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 1,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 1,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 1,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 1,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 1,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 1,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 1,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 1,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 1,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 1,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 1,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 1,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 1,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 1,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 1,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 1,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 1,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 1,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 1,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 1,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 1,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 1,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 1,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 1,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 1,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 1,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 1,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 1,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 1,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 1,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 1,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 1,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 1,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 1,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 1,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 1,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 1,0,0,0,0,0,0,0x00 }, //FB STI
			{ 1,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 1,0,0,0,0,0,0,0x00 }, //FD STD
			{ 1,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 1,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}
	}, //8086+
	{ //NEV V30+
		{ //16-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 1,0,0,0,0,0,0,0x00 }, //0F --- Now undocumented! ---
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 1,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 1,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 1,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,0,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //67/77 JXX
			{ 1,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 1,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 1,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 1,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 1,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 1,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 1,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 1,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,0,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,0,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,0,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,0,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,0,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,0,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 1,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 1,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 1,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 1,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}, //16-bit
		{ //32-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 1,0,0,0,0,0,0,0x00 }, //0F --- Now undocumented! ---
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}
	}, //NEC V30+
	{ //80286+
		{ //16-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x00 }, //0F Double opcode extension! This is handled by the CPU core itself!
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 1,1,1,0,0,1,0,0x00 }, //63 ARPL r/m16,r16 (286+)
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 1,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode. Doesn't generate exceptions!
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}, //16-bit
		{ //32-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x00 }, //0F Two-opcode instruction!
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}
	}, //80286+
	{ //80386+
		{ //16-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}, //16-bit
		{ //32-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 1,1,2,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 1,1,2,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 1,1,2,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 1,1,2,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 1,1,2,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 1,1,2,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 1,1,2,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 1,1,2,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 1,1,2,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 1,1,2,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 1,1,2,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 1,1,2,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 1,1,2,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 1,1,2,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 1,1,2,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 1,1,2,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 1,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 1,1,2,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 1,1,2,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 1,1,2,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 1,1,2,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 1,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 1,1,2,2,1,0,0,0x01 }, //8C MOV
			{ 1,1,2,0,0,0,0,0x03 }, //8D LEA
			{ 1,1,2,2,0,0,0,0x01 }, //8E MOV
			{ 1,1,2,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 1,0,0,0,0,0,9,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 1,0,0,0,0,0,3,0x00 }, //A0 MOV AL,[imm32]
			{ 1,0,0,0,0,0,3,0x00 }, //A1 MOV AX,[imm32]
			{ 1,0,0,0,0,0,3,0x00 }, //A2 MOV [imm32],AL
			{ 1,0,0,0,0,0,3,0x00 }, //A3 MOV [imm32],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 1,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 1,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 1,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 1,1,2,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 1,0,0,0,0,0,3,0x00 }, //C2 RET imm32
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 1,1,2,0,0,0,0,0x00 }, //C4 LES
			{ 1,1,2,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 1,1,2,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 1,0,0,0,0,0,3,0x00 }, //CA RETF imm32
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 1,1,2,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 1,1,2,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 1,0,0,0,0,0,3,0x00 }, //E8 CALL imm32
			{ 1,0,0,0,0,0,3,0x00 }, //E9 JMP imm32
			{ 1,0,0,0,0,0,9,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 1,1,2,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 1,1,2,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}
	}, //80386+
	{ //80486+
		{ //16-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}, //16-bit
		{ //32-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}
	}, //80486+
	{ //Pentium+
		{ //16-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}, //16-bit
		{ //32-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}
	} //Pentium+
};

CPU_Timings CPUInformation0F[NUMCPUS-CPU_80286][2][0x100] = { //0F information, from 80286 upwards!
	{ //80286+
		{ //16-bit
			{ 1,1,1,0,0,1,0,0x00 }, //00 Various extended 286+ instructions GRP opcode.
			{ 1,1,1,0,0,1,0,0x00 }, //01 Various extended 286+ instructions GRP opcode.
			{ 1,1,1,0,0,1,0,0x00 }, //02 LAR /r
			{ 1,1,1,0,0,1,0,0x00 }, //03 LSL /r
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 1,0,0,0,0,0,0,0x00 }, //06 CLTS
			{ 0,0,0,0,0,0,0,0x10 }, //07
			{ 0,1,0,0,1,0,0,0x01 }, //08
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B Delibarate #UD! 286+
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E
			{ 0,0,0,0,0,0,0,0x00 }, //0F
			{ 0,1,0,0,1,0,0,0x01 }, //10
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 286+ Deliberate #UD!
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}, //16-bit
		{ //32-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x00 }, //0F Two-opcode instruction!
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}
	}, //80286+
	{ //80386+
		{ //16-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}, //16-bit
		{ //32-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,2,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,2,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,2,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,2,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,2,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,2,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,2,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,2,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,2,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,2,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,2,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,2,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,2,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,2,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,2,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,2,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,2,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,2,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,2,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,2,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,2,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,2,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,2,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,2,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,9,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,3,0x00 }, //A0 MOV AL,[imm32]
			{ 0,0,0,0,0,0,3,0x00 }, //A1 MOV AX,[imm32]
			{ 0,0,0,0,0,0,3,0x00 }, //A2 MOV [imm32],AL
			{ 0,0,0,0,0,0,3,0x00 }, //A3 MOV [imm32],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,2,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,3,0x00 }, //C2 RET imm32
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,2,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,2,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,2,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,3,0x00 }, //CA RETF imm32
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,2,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,2,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,3,0x00 }, //E8 CALL imm32
			{ 0,0,0,0,0,0,3,0x00 }, //E9 JMP imm32
			{ 0,0,0,0,0,0,9,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,2,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,2,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}
	}, //80386+
	{ //80486+
		{ //16-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}, //16-bit
		{ //32-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}
	}, //80486+
	{ //Pentium+
		{ //16-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}, //16-bit
		{ //32-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,0,0,0,0x01 }, //01
			{ 0,1,0,0,0,0,0,0x01 }, //02
			{ 0,1,1,0,0,0,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 0,1,1,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 0,1,1,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 0,1,1,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 0,1,1,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 0,1,1,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 0,1,1,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 0,1,1,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 0,0,0,0,0,0,2,0x04 }, //3D
			{ 0,0,0,0,0,0,0,0x00 }, //3E DS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //3F AAS
			{ 0,0,0,0,0,0,0,0x00 }, //40 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //41 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //42 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //43 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //44 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //45 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //46 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //47 INC REG
			{ 0,0,0,0,0,0,0,0x00 }, //48 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //49 DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4A DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4B DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4C DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4D DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4E DEC REG
			{ 0,0,0,0,0,0,0,0x00 }, //4F DEC REG
			{ 0,0,0,0,0,0,0,0x10 }, //50 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //51 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //52 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //53 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //54 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //55 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //56 PUSH REG
			{ 0,0,0,0,0,0,0,0x10 }, //57 PUSH REG
			{ 0,0,0,0,0,0,0,0x20 }, //58 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //59 POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5A POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5B POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5C POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5D POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5E POP REG
			{ 0,0,0,0,0,0,0,0x20 }, //5F POP REG
			{ 0,0,0,0,0,0,0,0x00 }, //60 186+ PUSHA
			{ 0,0,0,0,0,0,0,0x00 }, //61 186+ POPA
			{ 0,1,1,0,0,0,1,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 0,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 0,1,1,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //62/72 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //68/78 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //69/79 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6A/7A JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6B/7B JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6C/7C JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6D/7D JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6E/7E JXX
			{ 0,0,0,0,0,0,1,0x00 }, //6F/7F JXX
			{ 0,1,0,0,1,0,1,0x00 }, //80 GRP1
			{ 0,1,0,0,1,0,2,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 0,1,1,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 0,1,1,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 0,1,1,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 0,1,1,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 0,1,1,0,0,0,0,0x01 }, //8B MOV
			{ 0,1,1,2,1,0,0,0x01 }, //8C MOV
			{ 0,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 0,1,1,2,0,0,0,0x01 }, //8E MOV
			{ 0,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP
			{ 0,0,0,0,0,0,0,0x00 }, //90 NOP
			{ 0,0,0,0,0,0,0,0x00 }, //91 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //92 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //93 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //94 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //95 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //96 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //97 XCHG REG,AX
			{ 0,0,0,0,0,0,0,0x00 }, //98 CBW
			{ 0,0,0,0,0,0,0,0x00 }, //99 CWD
			{ 0,0,0,0,0,0,3,0x00 }, //9A Call Ap
			{ 0,0,0,0,0,0,0,0x00 }, //9B WAIT
			{ 0,0,0,0,0,0,0,0x10 }, //9C PUSHF
			{ 0,0,0,0,0,0,0,0x20 }, //9D POPF
			{ 0,0,0,0,0,0,0,0x00 }, //9E SAHF
			{ 0,0,0,0,0,0,0,0x00 }, //9F LAHF
			{ 0,0,0,0,0,0,2,0x00 }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,2,0x00 }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,2,0x00 }, //A3 MOV [imm16],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 0,0,0,0,0,0,0,0x00 }, //AB STOSW
			{ 0,0,0,0,0,0,0,0x00 }, //AC LODSB
			{ 0,0,0,0,0,0,0,0x00 }, //AD LODSW
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 0,0,0,0,0,0,0,0x00 }, //AF SCASW
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B2 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B3 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B4 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B5 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B6 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B7 MOV REG,imm8
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BA MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BB MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BC MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BD MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16 CS forbidden on 186+
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 0,1,1,0,0,0,1,0x00 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 0,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 0,0,0,0,0,0,1,0x00 }, //CD INT
			{ 0,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 0,0,0,0,0,0,0,0x00 }, //CF IRET
			{ 0,1,0,0,1,0,0,0x00 }, //D0 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D1 GRP2
			{ 0,1,0,0,1,0,0,0x00 }, //D2 GRP2
			{ 0,1,1,0,1,0,0,0x00 }, //D3 GRP2
			{ 0,0,0,0,0,0,1,0x00 }, //D4 AAM
			{ 0,0,0,0,0,0,1,0x00 }, //D5 AAD
			{ 0,0,0,0,0,0,0,0x00 }, //D6 SALC
			{ 0,0,0,0,0,0,0,0x00 }, //D7 XLAT
			{ 0,1,0,0,0,0,0,0x00 }, //D8 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //D9 <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DA <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DB <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DC <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DD <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DE <COOP ESC>
			{ 0,1,0,0,0,0,0,0x00 }, //DF <COOP ESC>
			{ 0,0,0,0,0,0,1,0x00 }, //E0 LOOPNZ
			{ 0,0,0,0,0,0,1,0x00 }, //E1 LOOPZ
			{ 0,0,0,0,0,0,1,0x00 }, //E2 LOOP
			{ 0,0,0,0,0,0,1,0x00 }, //E3 JCXZ
			{ 0,0,0,0,0,0,1,0x00 }, //E4 IN AL,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN AX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,AX
			{ 0,0,0,0,0,0,2,0x00 }, //E8 CALL imm16
			{ 0,0,0,0,0,0,2,0x00 }, //E9 JMP imm16
			{ 0,0,0,0,0,0,3,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,0,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,0,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Case 7 takes immediate operands(Special callback instruction in this emulation only).
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}
	} //Pentium+
};

CPU_Timings CPUTimings[CPU_MODES][0x100]; //All normal CPU timings, which are used, for all modes available!
CPU_Timings CPUTimings0F[CPU_MODES][0x100]; //All normal 0F CPU timings, which are used, for all modes available!

void generate_timings_tbl() //Generate the timings table!
{
	int opcode;
	byte mode, curmode;
	byte theCPU;

	//Normal instruction timings!
	memset(CPUTimings,0,sizeof(CPUTimings)); //Clear the timing table!
	for (mode = 0;mode<NUMITEMS(CPUTimings);++mode) //All processor modes?
	{
		for (opcode = 0;opcode < 0x100;++opcode) //Process all opcodes!
		{
			curmode = mode; //The current mode we're processing!
			tryopcodes: //Retry with the other mode!
			theCPU = (byte)EMULATED_CPU; //Start with the emulated CPU!
			for (;(CPUInformation[theCPU][curmode][opcode].used==0) && theCPU;) --theCPU; //Goto parent while not used!
			if ((CPUInformation[theCPU][curmode][opcode].used == 0) && curmode) //Unused instruction and higher bit mode?
			{
				--curmode; //Try lower-bit mode!
				goto tryopcodes; //Try the next mode!
			}
			memcpy(&CPUTimings[mode][opcode],&CPUInformation[theCPU][curmode][opcode],sizeof(CPUTimings[mode][opcode])); //Set the mode to the highest mode detected that's available!
		}
	}

	//0F instruction timings!
	memset(CPUTimings0F, 0, sizeof(CPUTimings0F)); //Clear the timing table!
	if (EMULATED_CPU>=CPU_80286) //We need to be a 80286 or higher to use 0F extensions!
	{
		for (mode = 0;mode<NUMITEMS(CPUTimings0F);++mode) //All processor modes?
		{
			for (opcode = 0;opcode < 0x100;++opcode) //Process all opcodes!
			{
				curmode = mode; //The current mode we're processing!
			tryopcodes0F: //Retry with the other mode!
				theCPU = (byte)(EMULATED_CPU-CPU_80286); //Start with the emulated CPU! The first generation to support this is the 286!
				for (;(CPUInformation0F[theCPU][curmode][opcode].used == 0) && theCPU;) --theCPU; //Goto parent while not used!
				if ((CPUInformation0F[theCPU][curmode][opcode].used == 0) && curmode) //Unused instruction and higher bit mode?
				{
					--curmode; //Try lower-bit mode!
					goto tryopcodes0F; //Try the next mode!
				}
				memcpy(&CPUTimings0F[mode][opcode], &CPUInformation0F[theCPU][curmode][opcode], sizeof(CPUTimings0F[mode][opcode])); //Set the mode to the highest mode detected that's available!
			}
		}
	}
}
