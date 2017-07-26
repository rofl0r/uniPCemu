#include "headers/cpu/cpu.h" //Basic types!
#include "headers/cpu/cpu_pmtimings.h" //Protected-mode timings header!

#define EU_CYCLES_SUBSTRACT_ACCESSREAD 2
#define EU_CYCLES_SUBSTRACT_ACCESSWRITE 2
#define EU_CYCLES_SUBSTRACT_ACCESSRW 4

//Compressed protected&real mode timing table. This will need to be uncompressed for usage to be usable(long lookup times otherwise)
CPUPM_Timings CPUPMTimings[216] = {
	//286 CPU timings
	//MOV
	{0,0,0,0x88,0xFE,0x00,{{{{2,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,1}}},{{{2,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,1}}}}} //MOV Register to Register/Memory
	,{0,0,0,0x8A,0xFE,0x00,{{{{2,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{2,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //MOV Register/memory to Register
	,{0,0,0,0xC6,0xFE,0x01,{{{{2,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,1}}},{{{2,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,1}}}}} //MOV Immediate to register/memory
	,{0,0,0,0xB0,0xF0,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //MOV Immediate to register
	,{0,0,0,0xA0,0xFE,0x00,{{{{5,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{5,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //MOV Memory to accumulator
	,{0,0,0,0xA2,0xFE,0x00,{{{{3,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}},{{{3,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}}}} //MOV Accumulator to memory
	,{0,0,0,0x8E,0xFF,0x00,{{{{2,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{17,0,0},{19-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //MOV Register/memory to segment register
	,{0,0,0,0x8C,0xFF,0x00,{{{{2,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,1}}},{{{2,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,1}}}}} //MOV Segment register to register/memory
	//PUSH
	,{0,0,0,0xFF,0xFF,0x07,{{{{5,0,1},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{5,0,1},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //PUSH Memory
	,{0,0,0,0x50,0xF8,0x00,{{{{3,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}},{{{3,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}}}} //PUSH Register
	,{0,0,0,0x06,0xE7,0x00,{{{{3,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}},{{{3,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}}}} //PUSH Segment register
	,{0,0,0,0x68,0xFD,0x00,{{{{3,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}},{{{3,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}}}} //PUSH immediate
	,{0,0,0,0x60,0xFF,0x00,{{{{17-(EU_CYCLES_SUBSTRACT_ACCESSWRITE*8),0,0},{17-(EU_CYCLES_SUBSTRACT_ACCESSWRITE*8),0,0}}},{{{17-(EU_CYCLES_SUBSTRACT_ACCESSWRITE*8),0,0},{17-(EU_CYCLES_SUBSTRACT_ACCESSWRITE*8),0,0}}}}} //PUSHA
	//POP
	,{0,0,0,0x8F,0xFF,0x01,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //POP Memory
	,{0,0,0,0x58,0xF8,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //POP Register
	,{0,0,0,0x07,0xFF,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{20-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{20-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //POP Segment register
	,{0,0,0,0x17,0xFF,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{20-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{20-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}}  //POP Segment register
	,{0,0,0,0x1F,0xFF,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{20-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{20-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //POP Segment register
	,{0,0,0,0x61,0xFF,0x00,{{{{19-(EU_CYCLES_SUBSTRACT_ACCESSREAD*8),0,0},{19-(EU_CYCLES_SUBSTRACT_ACCESSREAD*8),0,0}}},{{{19-(EU_CYCLES_SUBSTRACT_ACCESSREAD*8),0,0},{19-(EU_CYCLES_SUBSTRACT_ACCESSREAD*8),0,0}}}}} //POPA
	//XCHG
	,{0,0,0,0x86,0xFE,0x00,{{{{3,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{3,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //XCHG Register/memory with register
	,{0,0,0,0x90,0xF8,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //XCHG Register with accumulator
	//IN
	,{0,0,0,0xE4,0xFE,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //IN Fixed port
	,{0,0,0,0xEC,0xFE,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //IN Variable port
	//OUT
	,{0,0,0,0xE6,0xFE,0x00,{{{{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}},{{{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}}}} //OUT Fixed port
	,{0,0,0,0xEE,0xFE,0x00,{{{{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}},{{{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}}}} //OUT Variable port
	//XLAT
	,{0,0,0,0xD7,0xFF,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //XLAT
	//LEA
	,{0,0,0,0x8D,0xFF,0x00,{{{{3,0,1},{3,0,1}}},{{{3,0,1},{3,0,1}}}}} //LEA
	//LDS
	,{0,0,0,0xC5,0xFF,0x00,{{{{7,0,1},{7-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}},{{{21,0,1},{21-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}}}} //LDS
	//LES
	,{0,0,0,0xC4,0xFF,0x00,{{{{7,0,1},{7-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}},{{{21,0,1},{21-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}}}} //LES

	//Page 3-48
	//LAHF
	,{0,0,0,0x9F,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //LAHF
	//SAHF
	,{0,0,0,0x9E,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //SAHF
	//PUSHF
	,{0,0,0,0x9C,0xFF,0x00,{{{{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}},{{{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}}}} //PUSHF
	//POPF
	,{0,0,0,0x9D,0xFF,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //POPF
	//ADD
	,{0,0,0,0x00,0xFC,0x00,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //ADD Reg/memory with register to either
	,{0,0,0,0x80,0xFC,0x01,{{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //ADD Immediate to register/memory
	,{0,0,0,0x04,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //ADD Immediate to accumulator
	//ADC
	,{0,0,0,0x10,0xFC,0x00,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //ADC Reg/memory with register to either
	,{0,0,0,0x80,0xFC,0x03,{{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //ADC Immediate to register/memory
	,{0,0,0,0x14,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //ADC Immediate to accumulator
	//INC
	,{0,0,0,0xFE,0xFE,0x01,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //INC Register/memory
	,{0,0,0,0x40,0xF8,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //INC Register
	//SUB
	,{0,0,0,0x28,0xFC,0x00,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //SUB Reg/memory and register to either
	,{0,0,0,0x80,0xFC,0x06,{{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //SUB Immediate from register/memory
	,{0,0,0,0x2C,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //SUB Immediate from accumulator
	//SBB
	,{0,0,0,0x18,0xFC,0x00,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //SBB Reg/memory and register to either
	,{0,0,0,0x80,0xFC,0x04,{{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //SBB Immediate from register/memory
	,{0,0,0,0x1C,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //SBB Immediate from accumulator
	//DEC
	,{0,0,0,0xFE,0xFE,0x02,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //DEC Register/memory
	,{0,0,0,0x48,0xF8,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //DEC Register
	//CMP
	,{0,0,0,0x3A,0xFE,0x00,{{{{2,0,0},{6-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{2,0,0},{6-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //CMP Register/memory with register
	,{0,0,0,0x38,0xFE,0x00,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //CMP Register with register/memory
	,{0,0,0,0x80,0xFC,0x08,{{{{3,0,0},{6-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{3,0,0},{6-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //CMP Immediate with register/memory
	,{0,0,0,0x3C,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //CMP Immediate with accumulator
	//NEG
	,{0,0,0,0xF6,0xFE,0x04,{{{{2,0,0},{2/*-EU_CYCLES_SUBSTRACT_ACCESSRW*/,0,0}}},{{{7,0,1},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //NEG: Can't read/write to memory: negative cycles(4>=2)?
	//AAA
	,{0,0,0,0x37,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //AAA
	//DAA
	,{0,0,0,0x27,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //DAA

	//Page 3-49
	//AAS
	,{0,0,0,0x3F,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //AAS
	//DAS
	,{0,0,0,0x2F,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //DAS
	//MUL
	,{0,0,0,0xF6,0xFF,0x05,{{{{13,0,0},{16-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{13,0,0},{16-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //MULB Register/Memory-Byte
	,{0,0,0,0xF7,0xFF,0x05,{{{{21,0,0},{24-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{21,0,0},{24-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //MULW Register/Memory-Word
	//IMUL
	,{0,0,0,0xF6,0xFF,0x06,{{{{13,0,0},{16-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{13,0,0},{16-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //IMULB Register/Memory-Byte
	,{0,0,0,0xF7,0xFF,0x06,{{{{21,0,0},{24-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{21,0,0},{24-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //IMULW Register/Memory-Word
	//IMUL (186+ instruction)
	,{0,0,0,0x69,0xFD,0x00,{{{{21,0,0},{24-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{21,0,0},{24-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //IMUL
	//DIV
	,{0,0,0,0xF6,0xFF,0x07,{{{{14,0,0},{17-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{14,0,0},{17-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //DIV Register/Memory-Byte
	,{0,0,0,0xF7,0xFF,0x07,{{{{22,0,0},{25-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{22,0,0},{25-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //DIV Register/Memory-Word
	//IDIV
	,{0,0,0,0xF6,0xFF,0x08,{{{{17,0,0},{20-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{17,0,0},{20-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //IDIV Register/Memory-Byte
	,{0,0,0,0xF7,0xFF,0x08,{{{{25,0,0},{28-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{25,0,0},{28-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //IDIV Register/Memory-Word
	//AAM
	,{0,0,0,0xD4,0xFF,0x00,{{{{16,0,0},{16,0,0}}},{{{16,0,0},{16,0,0}}}}} //AAM
	//AAD
	,{0,0,0,0xD5,0xFF,0x00,{{{{14,0,0},{14,0,0}}},{{{14,0,0},{14,0,0}}}}} //AAD
	//CBW
	,{0,0,0,0x98,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //CBW
	//CWD
	,{0,0,0,0x99,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //CWD
	//Shift/Rotate Instructions
	,{0,0,0,0xD0,0xFE,0x00,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //Shift/Rotate Register/Memory by 1
	,{0,0,0,0xD2,0xFE,0x00,{{{{5,1,0},{8-EU_CYCLES_SUBSTRACT_ACCESSRW,1,1}}},{{{5,1,0},{8-EU_CYCLES_SUBSTRACT_ACCESSRW,1,1}}}}} //Shift/Rotate Register/Memory by CL
	,{0,0,0,0xC0,0xFE,0x00,{{{{5,1,0},{8-EU_CYCLES_SUBSTRACT_ACCESSRW,1,1}}},{{{5,1,0},{8-EU_CYCLES_SUBSTRACT_ACCESSRW,1,1}}}}} //Shift/Rotate Register/Memory by Count

	//Page 3-50
	//AND
	,{0,0,0,0x20,0xFC,0x00,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //AND Reg/memory and register to either
	,{0,0,0,0x80,0xFE,0x05,{{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //AND Immediate to register/memory
	,{0,0,0,0x24,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //AND Immediate to accumulator
	//TEST
	,{0,0,0,0x84,0xFE,0x00,{{{{2,0,0},{6-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{2,0,0},{6-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //TEST Register/memory and register
	,{0,0,0,0xF6,0xFE,0x01,{{{{3,0,0},{6-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{3,0,0},{6-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //TEST Immediate data and register/memory
	,{0,0,0,0xA8,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //TEST Immediate data and accumulator
	//OR
	,{0,0,0,0x08,0xFC,0x00,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //OR Reg/memory and register to either
	,{0,0,0,0x80,0xFE,0x02,{{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //OR Immediate to register/memory
	,{0,0,0,0x0C,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //OR Immediate to accumulator
	//XOR
	,{0,0,0,0x30,0xFC,0x00,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //XOR Reg/memory and register to either
	,{0,0,0,0x80,0xFE,0x07,{{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{3,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //XOR Immediate to register/memory
	,{0,0,0,0x34,0xFE,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //XOR Immediate to accumulator
	//NOT
	,{0,0,0,0xF6,0xFE,0x03,{{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}},{{{2,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSRW,0,1}}}}} //NOT

	//String instructions without REP((N)Z)
	//MOVS
	,{0,0,0,0xA4,0xFE,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0}}}}} //MOVS
	//CMPS
	,{0,0,0,0xA6,0xFE,0x00,{{{{8-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0},{8-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0}}},{{{8-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0},{8-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0}}}}} //CMPS
	//SCAS
	,{0,0,0,0xAE,0xFE,0x00,{{{{7-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{7-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{7-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //SCAS
	//LODS
	,{0,0,0,0xAC,0xFE,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //LODS
	//STOS
	,{0,0,0,0xAA,0xFE,0x00,{{{{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}},{{{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,0}}}}} //STOS
	//INS
	,{0,0,0,0x6C,0xFE,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0}}}}} //INS
	//OUTS
	,{0,0,0,0x6E,0xFE,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,0,0}}}}} //OUTS

	//String instructions with REP(N)(Z)
	//MOVS
	,{0,0,0,0xA4,0xFE,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2}}}}} //MOVS
	//CMPS
	,{0,0,0,0xA6,0xFE,0x00,{{{{5-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),9,2},{5-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),9,2}}},{{{5-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),9,2},{5-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),9,2}}}}} //CMPS
	//SCAS
	,{0,0,0,0xAE,0xFE,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,8,2},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,8,2}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,8,2},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,8,2}}}}} //SCAS
	//LODS
	,{0,0,0,0xAC,0xFE,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,4,2},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,4,2}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,4,2},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,4,2}}}}} //LODS
	//STOS
	,{0,0,0,0xAA,0xFE,0x00,{{{{4-EU_CYCLES_SUBSTRACT_ACCESSWRITE,3,2},{4-EU_CYCLES_SUBSTRACT_ACCESSWRITE,3,2}}},{{{4-EU_CYCLES_SUBSTRACT_ACCESSWRITE,3,2},{4-EU_CYCLES_SUBSTRACT_ACCESSWRITE,3,2}}}}} //STOS
	//INS
	,{0,0,0,0x6C,0xFE,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2}}}}} //INS
	//OUTS
	,{0,0,0,0x6E,0xFE,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2},{5-EU_CYCLES_SUBSTRACT_ACCESSRW,4,2}}}}} //OUTS

	//Page 3-51
	//We don't use the m value: this is done by the prefetch unit itself.
	//CALL Direct Intersegment
	,{0,0,0,0xE8,0xFF,0x00,{{{{7,0,0},{7,0,0}}},{{{7,0,0},{7,0,0}}}}} //CALL Direct within segment
	,{0,0,0,0xFF,0xFF,0x03,{{{{7,0,0},{11-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{7,0,0},{11-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //CALL Register/memory indirect within segment
	,{0,0,0,0x9A,0xFF,0x00,{{{{13,0,0},{26,0,0}}},{{{13,0,0},{26,0,0}}}}} //CALL Direct Intersegment

	//Protected mode variants
	,{0,0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{41,CALLGATE_SAMELEVEL,4},{41,CALLGATE_SAMELEVEL,4}}}}} //CALL Via call gate to same privilege level
	,{0,0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{82,CALLGATE_DIFFERENTLEVEL_NOPARAMETERS,4},{82,CALLGATE_DIFFERENTLEVEL_NOPARAMETERS,4}}}}} //CALL VIa call gate to different privilege level, no parameters
	,{0,0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{86,CALLGATE_DIFFERENTLEVEL_XPARAMETERS,4},{86,CALLGATE_DIFFERENTLEVEL_XPARAMETERS,4}}}}} //CALL VIa call gate to different privilege level, X parameters
	,{0,0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{177,OTHERGATE_NORMALTSS,4},{177,OTHERGATE_NORMALTSS,4}}}}} //CALL Via TSS
	,{0,0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{182,OTHERGATE_NORMALTASKGATE,4},{182,OTHERGATE_NORMALTASKGATE,4}}}}} //CALL Via task gate

	//CALL Indirect Intersegment
	,{0,0,0,0xFF,0xFF,0x04,{{{{16,0,0},{16-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{29,0,1},{29-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //CALL Register/memory indirect within segment

	//Protected mode variants
	,{0,0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{44,CALLGATE_SAMELEVEL,5},{44,CALLGATE_SAMELEVEL,5}}}}} //CALL Via call gate to same privilege level
	,{0,0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{83,CALLGATE_DIFFERENTLEVEL_NOPARAMETERS,5},{83,CALLGATE_DIFFERENTLEVEL_NOPARAMETERS,5}}}}} //CALL VIa call gate to different privilege level, no parameters
	,{0,0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{90,CALLGATE_DIFFERENTLEVEL_XPARAMETERS,5},{90,CALLGATE_DIFFERENTLEVEL_XPARAMETERS,5}}}}} //CALL VIa call gate to different privilege level, X parameters
	,{0,0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{180,OTHERGATE_NORMALTSS,5},{180,OTHERGATE_NORMALTSS,5}}}}} //CALL Via TSS
	,{0,0,0,0x9A,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{185,OTHERGATE_NORMALTASKGATE,5},{185,OTHERGATE_NORMALTASKGATE,5}}}}} //CALL Via task gate

	//JMP
	,{0,0,0,0xEB,0xFF,0x00,{{{{7,0,0},{7,0,0}}},{{{7,0,0},{7,0,0}}}}} //JMP Short/long
	,{0,0,0,0xE9,0xFF,0x00,{{{{7,0,0},{7,0,0}}},{{{7,0,0},{7,0,0}}}}} //JMP Direct within segment
	,{0,0,0,0xFF,0xFF,0x05,{{{{7,0,0},{11-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0}}},{{{7,0,0},{11-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0}}}}} //JMP Register/memory indirect within segment
	,{0,0,0,0xEA,0xFF,0x00,{{{{11,0,0},{11,0,0}}},{{{23,0,0},{23,0,0}}}}} //JMP Direct intersegment
	
	//Protected mode variants(Direct Intersegment)
	,{0,0,0,0xEA,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{38,CALLGATE_SAMELEVEL,5},{38,CALLGATE_SAMELEVEL,5}}}}} //JMP Via call gate to same privilege level
	,{0,0,0,0xEA,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{175,OTHERGATE_NORMALTSS,5},{175,OTHERGATE_NORMALTSS,5}}}}} //JMP Via TSS
	,{0,0,0,0xEA,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{180,OTHERGATE_NORMALTASKGATE,5},{180,OTHERGATE_NORMALTASKGATE,5}}}}} //JMP Via task gate

	//JMP Indirect Intersegment
	,{0,0,0,0xFF,0xFF,0x06,{{{{15,0,1},{15-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}},{{{26,0,1},{26-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}}}} //JMP Indirect intersegment

	//Protected mode variants (Indirect Intersegment)
	,{0,0,0,0xFF,0xFF,0x06,{{{{0,0,0},{0,0,0}}},{{{41,CALLGATE_SAMELEVEL,5},{41,CALLGATE_SAMELEVEL,5}}}}} //JMP Via call gate to same privilege level
	,{0,0,0,0xFF,0xFF,0x06,{{{{0,0,0},{0,0,0}}},{{{178,OTHERGATE_NORMALTSS,5},{178,OTHERGATE_NORMALTSS,5}}}}} //JMP Via TSS
	,{0,0,0,0xFF,0xFF,0x06,{{{{0,0,0},{0,0,0}}},{{{183,OTHERGATE_NORMALTASKGATE,5},{183,OTHERGATE_NORMALTASKGATE,5}}}}} //JMP Via task gate

	//RET
	,{0,0,0,0xC3,0xFF,0x00,{{{{11-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{11-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{11-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{11-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //RET Within segment
	,{0,0,0,0xC2,0xFF,0x00,{{{{11-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{11-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{11-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{11-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //RET Within seg adding immed to SP
	,{0,0,0,0xCB,0xFF,0x00,{{{{15-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0},{15-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0}}},{{{25-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0},{25-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0}}}}} //RET Intersegment
	,{0,0,0,0xCA,0xFF,0x00,{{{{15-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0},{15-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0}}},{{{15-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0},{15-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0}}}}} //RET Intersegment adding immediate to SP

	//Protected mode variants (Intersegment)
	,{0,0,0,0xCB,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{55-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),RET_DIFFERENTLEVEL,4},{55-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),RET_DIFFERENTLEVEL,4}}}}} //RET Intersegment
	,{0,0,0,0xCA,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{55-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),RET_DIFFERENTLEVEL,4},{55-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),RET_DIFFERENTLEVEL,4}}}}} //RET Intersegment adding immediate to SP

	//Page 3-52

	//JE/JZ
	,{0,0,0,0x74,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JZ Not taken!
	,{0,0,0,0x74,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JZ taken!
	//JL/JNGE
	,{0,0,0,0x7C,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JL Not taken!
	,{0,0,0,0x7C,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JL taken!
	//JLE/JNG
	,{0,0,0,0x7E,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JLE Not taken!
	,{0,0,0,0x7E,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JLE taken!
	//JB/JNAE
	,{0,0,0,0x72,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JB Not taken!
	,{0,0,0,0x72,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JB taken!
	//JBE/JNA
	,{0,0,0,0x76,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JBE Not taken!
	,{0,0,0,0x76,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JBE taken!
	//JP/JPE
	,{0,0,0,0x7A,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JP Not taken!
	,{0,0,0,0x7A,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JP taken!
	//JO
	,{0,0,0,0x70,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JO Not taken!
	,{0,0,0,0x70,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JO taken!
	//JB
	,{0,0,0,0x78,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JB Not taken!
	,{0,0,0,0x78,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JB taken!
	//JNE/JNZ
	,{0,0,0,0x75,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JNE Not taken!
	,{0,0,0,0x75,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JNE taken!
	//JNL/JGE
	,{0,0,0,0x7D,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JNL Not taken!
	,{0,0,0,0x7D,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JNL taken!
	//JNLE/JG
	,{0,0,0,0x7F,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JNLE Not taken!
	,{0,0,0,0x7F,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JNLE taken!
	////JNB/JAE
	,{0,0,0,0x73,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JNB Not taken!
	,{0,0,0,0x73,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JNB taken!
	//JNBE/JA
	,{0,0,0,0x77,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JNBE Not taken!
	,{0,0,0,0x77,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JNBE taken!
	//JNP/JPO
	,{0,0,0,0x7B,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JNP Not taken!
	,{0,0,0,0x7B,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JNP taken!
	//JNO
	,{0,0,0,0x71,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JNO Not taken!
	,{0,0,0,0x71,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JNO taken!
	//JNS
	,{0,0,0,0x79,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //JNS Not taken!
	,{0,0,0,0x79,0xFF,0x00,{{{{7,0,8},{7,0,8}}},{{{7,0,8},{7,0,8}}}}} //JNS taken!
	//LOOP
	,{0,0,0,0xE2,0xFF,0x00,{{{{4,0,0},{4,0,0}}},{{{4,0,0},{4,0,0}}}}} //LOOP Not taken!
	,{0,0,0,0xE2,0xFF,0x00,{{{{8,0,8},{8,0,8}}},{{{8,0,8},{8,0,8}}}}} //LOOP taken!
	//LOOPZ/LOOPE
	,{0,0,0,0xE1,0xFF,0x00,{{{{4,0,0},{4,0,0}}},{{{4,0,0},{4,0,0}}}}} //LOOPZ Not taken!
	,{0,0,0,0xE1,0xFF,0x00,{{{{8,0,8},{8,0,8}}},{{{8,0,8},{8,0,8}}}}} //LOOPZ taken!
	//LOOPNZ/LOOPNE
	,{0,0,0,0xE0,0xFF,0x00,{{{{4,0,0},{4,0,0}}},{{{4,0,0},{4,0,0}}}}} //LOOPNZ Not taken!
	,{0,0,0,0xE0,0xFF,0x00,{{{{8,0,8},{8,0,8}}},{{{8,0,8},{8,0,8}}}}} //LOOPNZ taken!
	//JCXZ
	,{0,0,0,0xE3,0xFF,0x00,{{{{4,0,0},{4,0,0}}},{{{4,0,0},{4,0,0}}}}} //JCXZ Not taken!
	,{0,0,0,0xE3,0xFF,0x00,{{{{8,0,8},{8,0,8}}},{{{8,0,8},{8,0,8}}}}} //JCXZ taken!
	//ENTER
	,{0,0,0,0xC8,0xFF,0x00,{{{{11,0,16},{11,0,16}}},{{{11,0,16},{11,0,16}}}}} //ENTER L=0
	,{0,0,0,0xC8,0xFF,0x00,{{{{15,1,16},{15,1,16}}},{{{15,1,16},{15,1,16}}}}} //ENTER L=1
	,{0,0,0,0xC8,0xFF,0x00,{{{{16,4,32},{16,4,32}}},{{{16,4,32},{16,4,32}}}}} //ENTER L>1
	//LEAVE
	,{0,0,0,0xC9,0xFF,0x00,{{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}},{{{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0},{5-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,0}}}}} //LEAVE
	//INT (Real mode only)
	,{0,0,0,0xCD,0xFF,0x00,{{{{23-((EU_CYCLES_SUBSTRACT_ACCESSREAD*2)+(EU_CYCLES_SUBSTRACT_ACCESSWRITE*3)),0,0},{23-((EU_CYCLES_SUBSTRACT_ACCESSREAD*2)+(EU_CYCLES_SUBSTRACT_ACCESSWRITE*3)),0,0}}},{{{0,0,0},{0,0,0}}}}} //INT Type specified
	,{0,0,0,0xCC,0xFF,0x00,{{{{23-((EU_CYCLES_SUBSTRACT_ACCESSREAD*2)+(EU_CYCLES_SUBSTRACT_ACCESSWRITE*3)),0,0},{23-((EU_CYCLES_SUBSTRACT_ACCESSREAD*2)+(EU_CYCLES_SUBSTRACT_ACCESSWRITE*3)),0,0}}},{{{0,0,0},{0,0,0}}}}} //INT 3
	//INTO (Real mode only)
	,{0,0,0,0xCE,0xFF,0x00,{{{{24-((EU_CYCLES_SUBSTRACT_ACCESSREAD*2)+(EU_CYCLES_SUBSTRACT_ACCESSWRITE*3)),0,8},{24-((EU_CYCLES_SUBSTRACT_ACCESSREAD*2)+(EU_CYCLES_SUBSTRACT_ACCESSWRITE*3)),0,0}}},{{{0,0,0},{0,0,0}}}}} //INTO Taken
	,{0,0,0,0xCE,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //INTO Not taken

	//Page 3-53
	//INT&INTO Protected mode variants
	,{0,0,0,0xCD,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{40,INTERRUPTGATETIMING_SAMELEVEL,4},{40,INTERRUPTGATETIMING_SAMELEVEL,4}}}}} //INT Via Interrupt or Trap Gate to same privilege level
	,{0,0,0,0xCD,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{78,INTERRUPTGATETIMING_DIFFERENTLEVEL,4},{78,INTERRUPTGATETIMING_DIFFERENTLEVEL,4}}}}} //INT Via INterrupt or Trap Gate to different privilege level
	,{0,0,0,0xCD,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{167,INTERRUPTGATETIMING_TASKGATE,4},{167,INTERRUPTGATETIMING_TASKGATE,4}}}}} //INT Via Task Gate
	,{0,0,0,0xCE,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{40,INTERRUPTGATETIMING_SAMELEVEL,4},{40,INTERRUPTGATETIMING_SAMELEVEL,4}}}}} //INT Via Interrupt or Trap Gate to same privilege level
	,{0,0,0,0xCE,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{78,INTERRUPTGATETIMING_DIFFERENTLEVEL,4},{78,INTERRUPTGATETIMING_DIFFERENTLEVEL,4}}}}} //INT Via INterrupt or Trap Gate to different privilege level
	,{0,0,0,0xCE,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{167,INTERRUPTGATE_TASKGATE,4},{167,INTERRUPTGATE_TASKGATE,4}}}}} //INT Via Task Gate

	//BOUND
	,{0,0,0,0x62,0xFF,0x00,{{{{13-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0},{13-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}},{{{13-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,0},{13-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}}}} //BOUND

	//Processor Control
	//CLC
	,{0,0,0,0xF8,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //CLC
	//CMC
	,{0,0,0,0xF5,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //CMC
	//STC
	,{0,0,0,0xF9,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //STC
	//CLD
	,{0,0,0,0xFC,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //CLD
	//STD
	,{0,0,0,0xFD,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //STD
	//CLI
	,{0,0,0,0xFA,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //CLI
	//STI
	,{0,0,0,0xFB,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //CLC
	//HLT
	,{0,0,0,0xF4,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //HLT
	//WAIT
	,{0,0,0,0x9B,0xFF,0x00,{{{{3,0,0},{3,0,0}}},{{{3,0,0},{3,0,0}}}}} //WAIT
	//LOCK
	,{0,0,0,0xF0,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{0,0,0},{0,0,0}}}}} //LOCK
	//CTS
	,{0,0,1,0x06,0xFF,0x00,{{{{2,0,0},{2,0,0}}},{{{2,0,0},{2,0,0}}}}} //CTS
	//ESC (Coprocessor Instruction Escape)
	,{0,0,0,0xD8,0xF8,0x00,{{{{9,0,0},{9,0,0}}},{{{9,0,0},{9,0,0}}}}} //ESC
	//Segment override prefix
	,{0,0,0,0x26,0xE7,0x00,{{{{0,0,0},{0,0,0}}},{{{0,0,0},{0,0,0}}}}} //Any Segment override prefix (CS,SS,DS,ES,FS,GS)
	
	//Protection control
	//LGDT
	,{0,0,1,0x01,0xFF,0x03,{{{{11,0,1},{11-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}},{{{11,0,1},{11-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}}}} //LGDT
	//SGDT
	,{0,0,1,0x01,0xFF,0x01,{{{{11,0,1},{11-(EU_CYCLES_SUBSTRACT_ACCESSWRITE*2),0,1}}},{{{11,0,1},{11-(EU_CYCLES_SUBSTRACT_ACCESSWRITE*2),0,1}}}}} //SGDT
	//LIDT
	,{0,0,1,0x01,0xFF,0x04,{{{{12,0,1},{12-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}},{{{12,0,1},{12-(EU_CYCLES_SUBSTRACT_ACCESSREAD*2),0,1}}}}} //LIDT
	//SIDT
	,{0,0,1,0x01,0xFF,0x02,{{{{12,0,1},{12-(EU_CYCLES_SUBSTRACT_ACCESSWRITE*2),0,1}}},{{{12,0,1},{12-(EU_CYCLES_SUBSTRACT_ACCESSWRITE*2),0,1}}}}} //SIDT
	//LLDT
	,{0,0,1,0x00,0xFF,0x03,{{{{0,0,0},{0,0,0}}},{{{17,0,1},{19-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //LLDT
	//SLDT
	,{0,0,1,0x00,0xFF,0x01,{{{{0,0,0},{0,0,0}}},{{{2,0,1},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,1}}}}} //SLDT

	//Page 3-54

	//LTR
	,{0,0,1,0x00,0xFF,0x04,{{{{0,0,0},{0,0,0}}},{{{17,0,1},{19-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //LTR
	//STR
	,{0,0,1,0x00,0xFF,0x02,{{{{0,0,0},{0,0,0}}},{{{2,0,1},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,1}}}}} //STR
	//LMSW
	,{0,0,1,0x01,0xFF,0x07,{{{{3,0,0},{6-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}},{{{3,0,0},{6-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //LMSW
	//SMSW
	,{0,0,1,0x01,0xFF,0x05,{{{{2,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,1}}},{{{2,0,0},{3-EU_CYCLES_SUBSTRACT_ACCESSWRITE,0,1}}}}} //SMSW
	//LAR
	,{0,0,1,0x02,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{14,0,0},{16-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //LAR
	//LSL
	,{0,0,1,0x03,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{14,0,0},{16-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //LSL
	//ARPL
	,{0,0,0,0x63,0xFF,0x00,{{{{0,0,0},{0,0,0}}},{{{14-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1},{11-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //ARPL
	//VERR
	,{0,0,1,0x00,0xFF,0x05,{{{{0,0,0},{0,0,0}}},{{{14,0,0},{16-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //VERR
	//VERW
	,{0,0,1,0x00,0xFF,0x06,{{{{0,0,0},{0,0,0}}},{{{14,0,0},{16-EU_CYCLES_SUBSTRACT_ACCESSREAD,0,1}}}}} //VERR
	//215 items at this point!
	//Undocumented instruction: LOADALL
	,{0,0,1,0x05,0xFF,0x00,{{{{195-51,0,0},{195-51,0,0}}},{{{195-51,0,0},{195-51,0,0}}}}} //LOADALL uses 195 clocks and performs 51 bus cycles.
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
			{ 1,1,1,2,1,0,0,0x01 }, //8C MOV Ew,Sw
			{ 1,1,1,0,0,0,0,0x03 }, //8D LEA
			{ 1,1,1,2,0,0,0,0x01 }, //8E MOV Sw,Ew
			{ 1,1,1,0,1,0,0,0x00 }, //8F Undocumented GRP opcode POP Ev
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
			{ 1,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 1,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 1,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 1,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 1,0,0,0,0,0,0,0x00 }, //C8 ENTER 186+
			{ 1,0,0,0,0,0,0,0x00 }, //C9 LEAVE 186+
			{ 1,0,0,0,0,0,2,0x00 }, //CA RETF imm16
			{ 1,0,0,0,0,0,0,0x00 }, //CB RETF
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
			{ 1,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back. Others are invalid.
			{ 1,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
		}, //16-bit
		{ //32-bit
			{ 0,1,0,0,1,0,0,0x01 }, //00 ADD
			{ 0,1,1,0,1,0,0,0x01 }, //01
			{ 0,1,0,0,0,1,0,0x01 }, //02
			{ 0,1,1,0,0,1,0,0x01 }, //03
			{ 0,0,0,0,0,0,1,0x04 }, //04
			{ 0,0,0,0,0,0,2,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 0,1,1,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,1,0,0x01 }, //0A
			{ 0,1,1,0,0,1,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 0,0,0,0,0,0,2,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 0,1,1,0,1,0,0,0x01 }, //11
			{ 0,1,0,0,0,1,0,0x01 }, //12
			{ 0,1,1,0,0,1,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 0,0,0,0,0,0,2,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 0,1,1,0,1,0,0,0x01 }, //19
			{ 0,1,0,0,0,1,0,0x01 }, //1A
			{ 0,1,1,0,0,1,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 0,0,0,0,0,0,2,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 0,1,1,0,1,0,0,0x01 }, //21
			{ 0,1,0,0,0,1,0,0x01 }, //22
			{ 0,1,1,0,0,1,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 0,1,1,0,1,0,0,0x01 }, //29
			{ 0,1,0,0,0,1,0,0x01 }, //2A
			{ 0,1,1,0,0,1,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 0,0,0,0,0,0,2,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 0,1,1,0,1,0,0,0x01 }, //31
			{ 0,1,0,0,0,1,0,0x01 }, //32
			{ 0,1,1,0,0,1,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 0,0,0,0,0,0,2,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 0,1,1,0,1,0,0,0x01 }, //39
			{ 0,1,0,0,0,1,0,0x01 }, //3A
			{ 0,1,1,0,0,1,0,0x01 }, //3B
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 0,0,0,0,0,0,2,0x00 }, //BE MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //BF MOV REG,imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C0 186+ opcode
			{ 0,0,0,0,0,0,0,0x00 }, //C1 186+ opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C8 ENTER 186+ 8086 skips
			{ 0,0,0,0,0,0,0,0x00 }, //C9 LEAVE 186+ 8086 skips
			{ 0,0,0,0,0,0,2,0x00 }, //CA RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //CB RET
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
			{ 0,1,0,0,1,0,5,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 0,1,1,0,1,0,6,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
			{ 0,0,0,0,0,0,0,0x00 }, //F8 CLC
			{ 0,0,0,0,0,0,0,0x00 }, //F9 STC
			{ 0,0,0,0,0,0,0,0x00 }, //FA CLI
			{ 0,0,0,0,0,0,0,0x00 }, //FB STI
			{ 0,0,0,0,0,0,0,0x00 }, //FC CLD
			{ 0,0,0,0,0,0,0,0x00 }, //FD STD
			{ 0,1,0,0,1,0,0,0x00 }, //FE GRP4 Eb Case 0&1 read and write back.
			{ 0,1,1,0,1,0,0,0x00 } //FF GRP5 Various operations depending on REG.
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
			{ 1,1,1,0,0,1,0,0x00 }, //62 186+ BOUND
			{ 0,0,0,0,0,0,0,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //67/77 JXX
			{ 1,0,0,0,0,0,2,0x00 }, //68 186+ PUSH immw
			{ 1,1,1,0,0,0,2,0x00 }, //69 186+ IMUL ModR/M imm16
			{ 1,0,0,0,0,0,1,0x08 }, //6A 186+ PUSH immb
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 0,0,0,0,0,0,0,0x00 }, //0F --- Now undocumented! ---
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 0,1,1,0,0,1,0,0x00 }, //62 186+ BOUND
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 1,0,0,0,0,0,0,0x00 }, //D6 SALC
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
			{ 1,0,0,0,0,0,0,0x00 }, //F1: ICEBP!
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
			{ 0,1,1,0,0,0,0,0x00 }, //62 186+ BOUND
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
			{ 0,0,0,0,0,0,0,0x00 }, //62/72 JXX
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 1,0,0,0,0,0,3,0x04 }, //05
			{ 0,0,0,0,0,0,0,0x08 }, //06 PUSH ES
			{ 0,0,0,0,0,0,0,0x10 }, //07 POP ES
			{ 0,1,0,0,1,0,0,0x01 }, //08 OR
			{ 1,1,2,0,1,0,0,0x01 }, //09
			{ 0,1,0,0,0,0,0,0x01 }, //0A
			{ 1,1,2,0,0,0,0,0x01 }, //0B
			{ 0,0,0,0,0,0,1,0x04 }, //0C
			{ 1,0,0,0,0,0,3,0x04 }, //0D
			{ 0,0,0,0,0,0,0,0x08 }, //0E PUSH CS
			{ 0,0,0,0,0,0,0,0x10 }, //0F POP CS
			{ 0,1,0,0,1,0,0,0x01 }, //10 ADC
			{ 1,1,2,0,0,0,0,0x01 }, //11
			{ 0,1,0,0,0,0,0,0x01 }, //12
			{ 1,1,2,0,0,0,0,0x01 }, //13
			{ 0,0,0,0,0,0,1,0x04 }, //14
			{ 1,0,0,0,0,0,3,0x04 }, //15
			{ 0,0,0,0,0,0,0,0x08 }, //16 PUSH SS
			{ 0,0,0,0,0,0,0,0x10 }, //17 POP SS
			{ 0,1,0,0,1,0,0,0x01 }, //18 SBB
			{ 1,1,2,0,0,0,0,0x01 }, //19
			{ 0,1,0,0,0,0,0,0x01 }, //1A
			{ 1,1,2,0,0,0,0,0x01 }, //1B
			{ 0,0,0,0,0,0,1,0x04 }, //1C
			{ 1,0,0,0,0,0,3,0x04 }, //1D
			{ 0,0,0,0,0,0,0,0x08 }, //1E PUSH DS
			{ 0,0,0,0,0,0,0,0x10 }, //1F POP DS
			{ 0,1,0,0,1,0,0,0x01 }, //20 AND
			{ 1,1,2,0,0,0,0,0x01 }, //21
			{ 0,1,0,0,0,0,0,0x01 }, //22
			{ 1,1,2,0,0,0,0,0x01 }, //23
			{ 0,0,0,0,0,0,1,0x04 }, //24
			{ 1,0,0,0,0,0,3,0x04 }, //25
			{ 0,0,0,0,0,0,0,0x00 }, //26 ES prefix
			{ 0,0,0,0,0,0,0,0x00 }, //27 DAA
			{ 0,1,0,0,1,0,0,0x01 }, //28 SUB
			{ 1,1,2,0,0,0,0,0x01 }, //29
			{ 0,1,0,0,0,0,0,0x01 }, //2A
			{ 1,1,2,0,0,0,0,0x01 }, //2B
			{ 0,0,0,0,0,0,1,0x04 }, //2C
			{ 1,0,0,0,0,0,3,0x04 }, //2D
			{ 0,0,0,0,0,0,0,0x00 }, //2E CS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //2F DAS
			{ 0,1,0,0,1,0,0,0x01 }, //30 XOR
			{ 1,1,2,0,0,0,0,0x01 }, //31
			{ 0,1,0,0,0,0,0,0x01 }, //32
			{ 1,1,2,0,0,0,0,0x01 }, //33
			{ 0,0,0,0,0,0,1,0x04 }, //34
			{ 1,0,0,0,0,0,3,0x04 }, //35
			{ 0,0,0,0,0,0,0,0x00 }, //36 SS prefix
			{ 0,0,0,0,0,0,0,0x00 }, //37 AAA
			{ 0,1,0,0,1,0,0,0x01 }, //38 CMP
			{ 1,1,2,0,0,0,0,0x01 }, //39
			{ 0,1,0,0,0,0,0,0x01 }, //3A
			{ 1,1,2,0,0,0,0,0x01 }, //3B
			{ 0,0,0,0,0,0,1,0x04 }, //3C
			{ 1,0,0,0,0,0,3,0x04 }, //3D
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
			{ 1,1,2,0,0,0,3,0x00 }, //62 386+ BOUND 32-bit
			{ 0,0,0,0,0,0,1,0x00 }, //63/73 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //64/74 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //65/75 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //66/76 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //67/77 JXX
			{ 1,0,0,0,0,0,3,0x00 }, //68 186+ PUSH immd
			{ 1,1,2,0,0,0,3,0x00 }, //69 186+ IMUL ModR/M imm32
			{ 0,0,0,0,0,0,1,0x00 }, //6A 186+ PUSH immb
			{ 1,1,2,0,0,0,1,0x00 }, //6B 186+ IMUL ModR/M imm8
			{ 0,0,0,0,0,0,0,0x00 }, //6C 186+ INSB
			{ 0,0,0,0,0,0,0,0x00 }, //6D 186+ INSW
			{ 0,0,0,0,0,0,0,0x00 }, //6E 186+ OUTSB
			{ 0,0,0,0,0,0,0,0x00 }, //6F 186+ OUWSW
			{ 0,0,0,0,0,0,1,0x00 }, //60/70 JXX
			{ 0,0,0,0,0,0,1,0x00 }, //61/71 JXX
			{ 0,0,0,0,0,0,0,0x00 }, //62/72 JXX
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
			{ 1,1,2,0,1,0,3,0x00 }, //81 GRP1
			{ 0,1,0,0,1,0,1,0x00 }, //82 GRP1=80
			{ 1,1,2,0,1,0,1,0x00 }, //83 GRP1
			{ 0,1,0,0,0,0,0,0x00 }, //84 TEST
			{ 1,1,2,0,0,0,0,0x00 }, //85 TEST
			{ 0,1,0,0,0,1,0,0x01 }, //86 XCHG
			{ 1,1,2,0,0,1,0,0x01 }, //87 XCHG
			{ 0,1,0,0,1,0,0,0x01 }, //88 MOV
			{ 1,1,2,0,1,0,0,0x01 }, //89 MOV
			{ 0,1,0,1,0,0,0,0x01 }, //8A MOV
			{ 1,1,2,0,0,0,0,0x01 }, //8B MOV
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
			{ 1,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm32]
			{ 1,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm32]
			{ 1,0,0,0,0,0,0xA,0  }, //A2 MOV [imm32],AL
			{ 1,0,0,0,0,0,0xA,0  }, //A3 MOV [imm32],AX
			{ 0,0,0,0,0,0,0,0x00 }, //A4 MOVSB
			{ 0,0,0,0,0,0,0,0x00 }, //A5 MOVSW
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 1,0,0,0,0,0,3,0x00 }, //A9 TESTW AX
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
			{ 1,0,0,0,0,0,3,0x00 }, //B8 MOV REG,imm32
			{ 1,0,0,0,0,0,3,0x00 }, //B9 MOV REG,imm32
			{ 1,0,0,0,0,0,3,0x00 }, //BA MOV REG,imm32
			{ 1,0,0,0,0,0,3,0x00 }, //BB MOV REG,imm32
			{ 1,0,0,0,0,0,3,0x00 }, //BC MOV REG,imm32
			{ 1,0,0,0,0,0,3,0x00 }, //BD MOV REG,imm32
			{ 1,0,0,0,0,0,3,0x00 }, //BE MOV REG,imm32
			{ 1,0,0,0,0,0,3,0x00 }, //BF MOV REG,imm32
			{ 0,1,0,0,0,0,1,0x00 }, //C0 186+ GRP opcode
			{ 1,1,2,0,0,0,1,0x00 }, //C1 386+ GRP opcode 32-bit address
			{ 1,0,0,0,0,0,3,0x00 }, //C2 RET imm32
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 1,1,2,0,0,0,0,0x00 }, //C4 LES
			{ 1,1,2,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 1,1,2,0,0,0,3,0x00 }, //C7 MOV Mem/reg,imm16
			{ 0,0,0,0,0,0,8,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 0,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 0,0,0,0,0,0,3,0x00 }, //CA RETF imm16
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
			{ 0,0,0,0,0,0,1,0x00 }, //E5 IN EAX,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //E6 OUT imm8,AL
			{ 0,0,0,0,0,0,1,0x00 }, //E7 OUT imm8,EAX
			{ 1,0,0,0,0,0,3,0x00 }, //E8 CALL imm32
			{ 1,0,0,0,0,0,3,0x00 }, //E9 JMP imm32
			{ 1,0,0,0,0,0,9,0x00 }, //EA JMP Ap
			{ 0,0,0,0,0,0,1,0x00 }, //EB JMP imm8
			{ 0,0,0,0,0,0,0,0x00 }, //EC IN AL,DX
			{ 0,0,0,0,0,0,0,0x00 }, //ED IN AX,DX
			{ 0,0,0,0,0,0,0,0x00 }, //EE OUT DX,AL
			{ 0,0,0,0,0,0,0,0x00 }, //EF OUT DX,AX
			{ 0,0,0,0,0,0,0,0x00 }, //F0: LOCK prefix
			{ 1,0,0,0,0,0,0,0x00 }, //F1: Undefined and reserved opcode
			{ 0,0,0,0,0,0,0,0x00 }, //F2 REPNZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F3 REPZ prefix
			{ 0,0,0,0,0,0,0,0x00 }, //F4 HLT
			{ 0,0,0,0,0,0,0,0x00 }, //F5 CMC
			{ 0,1,0,0,1,0,5,0x00 }, //F6 Grp3a Eb Uses writeback with REG 2&3 only! REG 0&1 also have an immediate byte parameter!
			{ 1,1,2,0,1,0,7,0x00 }, //F7 Grp3b Ev See opcode F6(Grp3a Eb), but with word values for all cases!
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 1,0,0,0,0,0,0,0x00 }, //05 Undocumented LOADALL (286 only)!
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 1,0,0,0,0,0,0,0x00 }, //05 #UD
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
			{ 1,1,2,3,1,0,0,0x01 }, //20 MOV /r r32,CRn
			{ 1,1,2,3,1,0,0,0x01 }, //21 MOV /r r32,DRn
			{ 1,1,2,3,0,1,0,0x01 }, //22 MOV /r CRn,r32
			{ 1,1,2,3,0,1,0,0x01 }, //23 MOV /r DRn,r32
			{ 1,1,2,7,1,0,0,0x01 }, //24 MOV /r r32,TRn
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 1,1,2,7,0,1,0,0x01 }, //26 MOV /r TRn,r32
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
			{ 1,0,0,0,0,0,2,0x00 }, //80 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //81 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //82 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //83 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //84 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //85 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //86 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //87 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //88 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //89 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //8A 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //8B 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //8C 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //8D 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //8E 386+ JCC
			{ 1,0,0,0,0,0,2,0x00 }, //8F 386+ JCC
			{ 1,1,0,0,0,0,0,0x02 }, //90 SETO r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //91 SETNO r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //92 SETC r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //93 SETAE r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //94 SETE r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //95 SETNE r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //96 SETNA r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //97 SETA r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //98 SETS r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //99 SETNS r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //9A SETP r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //9B SETNP r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //9C SETL r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //9D SETGE r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //9E SETLE r/m8
			{ 1,1,0,0,0,0,0,0x02 }, //9F SETG r/m8
			{ 1,0,0,0,0,0,0,0x08 }, //A0 PUSH FS
			{ 1,0,0,0,0,0,0,0x10 }, //A1 POP FS
			{ 0,0,0,0,0,0,0,0x00 }, //A2 MOV [imm16],AL
			{ 1,1,1,1,1,0,0,0x01 }, //A3 BT /r r/m16,r16
			{ 1,1,1,1,1,0,1,0x01 }, //A4 SHLD /r r/m16,r16,imm8
			{ 1,1,1,1,1,0,0,0x01 }, //A5 SHLD /r r/m16,r16,CL
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 1,0,0,0,0,0,0,0x08 }, //A8 PUSH GS
			{ 1,0,0,0,0,0,0,0x10 }, //A9 POP GS
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 1,1,1,1,1,0,0,0x01 }, //AB BTS /r r/m16,r16
			{ 1,1,1,1,1,0,1,0x01 }, //AC SHRD /r r/m16,r16,imm8
			{ 1,1,1,1,1,0,0,0x01 }, //AD SHRD /r r/m16,r16,CL
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 1,1,1,1,0,1,0,0x01 }, //AF IMUL /r r16,r/m16
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 1,1,1,0,0,0,0,0x00 }, //B2 LSS r16,m16:16
			{ 1,1,1,1,1,0,0,0x01 }, //B3 BTR /r r/m16,r16
			{ 1,1,1,0,0,0,0,0x00 }, //B4 LFS r16,m16:16
			{ 1,1,1,0,0,0,0,0x00 }, //B5 LGS r16,m16:16
			{ 1,1,0,5,0,1,0,0x01 }, //B6 MOVZX /r r16,r/m8
			{ 1,1,1,5,0,1,0,0x01 }, //B7 MOVZX /r r16,r/m16
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 1,1,1,0,1,0,1,0x01 }, //BA BT* r/m16,imm8(GRP opcode)
			{ 1,1,1,1,1,0,0,0x01 }, //BB BTC /r r/m16,r16
			{ 1,1,1,1,0,1,0,0x01 }, //BC BSF /r r16,r/m16
			{ 1,1,1,1,0,1,0,0x01 }, //BD BSR /r r16,r/m16
			{ 1,1,0,5,0,1,0,0x01 }, //BE MOVSX /r r16,r/m8
			{ 1,1,1,5,0,1,0,0x01 }, //BF MOVSX /r r16,r/m16
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
			{ 1,1,2,0,0,1,0,0x00 }, //01 various instructions, 32-bit operand size.
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
			{ 1,0,0,0,0,0,3,0x00 }, //80 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //81 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //82 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //83 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //84 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //85 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //86 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //87 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //88 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //89 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //8A 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //8B 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //8C 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //8D 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //8E 386+ JCC
			{ 1,0,0,0,0,0,3,0x00 }, //8F 386+ JCC
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm32]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm32]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm32],AL
			{ 1,1,2,1,1,0,0,0x01 }, //A3 BT /r r/m32,r32
			{ 1,1,2,1,1,0,1,0x01 }, //A4 SHLD /r r/m32,r32,imm8
			{ 1,1,2,1,1,0,0,0x01 }, //A5 SHLD /r r/m32,r32,CL
			{ 0,0,0,0,0,0,0,0x00 }, //A6 CMPSB
			{ 0,0,0,0,0,0,0,0x00 }, //A7 CMPSW
			{ 0,0,0,0,0,0,1,0x00 }, //A8 TESTB AL
			{ 0,0,0,0,0,0,2,0x00 }, //A9 TESTW AX
			{ 0,0,0,0,0,0,0,0x00 }, //AA STOSB
			{ 1,1,2,1,1,0,0,0x01 }, //AB BTS /r r/m32,r32
			{ 1,1,2,1,1,0,1,0x01 }, //AC SHRD /r r/m32,r32,imm8
			{ 1,1,2,1,1,0,0,0x01 }, //AD SHRD /r r/m32,r32,CL
			{ 0,0,0,0,0,0,0,0x00 }, //AE SCASB
			{ 1,1,2,1,0,1,0,0x01 }, //AF IMUL /r r32,r/m32
			{ 0,0,0,0,0,0,1,0x00 }, //B0 MOV REG,imm8
			{ 0,0,0,0,0,0,1,0x00 }, //B1 MOV REG,imm8
			{ 1,1,2,0,0,0,0,0x00 }, //B2 LSS r32,m16:32
			{ 1,1,2,1,1,0,0,0x01 }, //B3 BTR /r r/m32,r32
			{ 1,1,2,0,0,0,0,0x00 }, //B4 LFS r32,m16:32
			{ 1,1,2,0,0,0,0,0x00 }, //B5 LGS r32,m16:32
			{ 1,1,0,6,0,1,0,0x01 }, //B6 MOVZX /r r32,r/m8
			{ 1,1,1,6,0,1,0,0x01 }, //B7 MOVZX /r r32,r/m16
			{ 0,0,0,0,0,0,2,0x00 }, //B8 MOV REG,imm16
			{ 0,0,0,0,0,0,2,0x00 }, //B9 MOV REG,imm16
			{ 1,1,2,0,1,0,1,0x01 }, //BA BT* r/m32,imm8(GRP opcode)
			{ 1,1,2,1,1,0,0,0x01 }, //BB BTC /r r/m32,r32
			{ 1,1,2,1,0,1,0,0x01 }, //BC BSF /r r32,r/m32
			{ 1,1,2,1,0,1,0,0x01 }, //BD BSR /r r32,r/m32
			{ 1,1,0,6,0,1,0,0x01 }, //BE MOVSX /r r32,r/m8
			{ 1,1,1,6,0,1,0,0x01 }, //BF MOVSX /r r32,r/m16
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
			{ 1,0,0,0,0,0,0,0x00 }, //08 OR
			{ 1,0,0,0,0,0,0,0x00 }, //09
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 1,1,0,0,0,1,0,0x01 }, //B0 CMPXCHG r/m8,AL,r8
			{ 1,1,1,0,0,1,0,0x01 }, //B1 CMPXCHG r/m16,AL,r16
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
			{ 1,1,0,0,1,0,0,0x01 }, //C0 186+ GRP opcode
			{ 1,1,1,0,1,0,0,0x01 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 1,0,0,0,0,0,0,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 1,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 1,0,0,0,0,0,0,0x00 }, //CA RETF imm16
			{ 1,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 1,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 1,0,0,0,0,0,0,0x00 }, //CD INT
			{ 1,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 1,0,0,0,0,0,0,0x00 }, //CF IRET
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 1,1,2,0,0,1,0,0x01 }, //B1 MOV REG,imm8
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
			{ 1,1,2,0,1,0,0,0x01 }, //C1 186+ GRP opcode
			{ 0,0,0,0,0,0,2,0x00 }, //C2 RET imm16
			{ 0,0,0,0,0,0,0,0x00 }, //C3 RET
			{ 0,1,1,0,0,0,0,0x00 }, //C4 LES
			{ 0,1,1,0,0,0,0,0x00 }, //C5 LDS
			{ 0,1,0,0,0,0,1,0x00 }, //C6 MOV Mem/reg,imm8
			{ 0,1,1,0,0,0,2,0x00 }, //C7 MOV Mem/reg,imm16
			{ 1,0,0,0,0,0,0,0x00 }, //C8 186+ ENTER imm16,imm8
			{ 1,0,0,0,0,0,0,0x00 }, //C9 186+ LEAVE
			{ 1,0,0,0,0,0,0,0x00 }, //CA RETF imm16
			{ 1,0,0,0,0,0,0,0x00 }, //CB RETF
			{ 1,0,0,0,0,0,0,0x00 }, //CC INT3
			{ 1,0,0,0,0,0,0,0x00 }, //CD INT
			{ 1,0,0,0,0,0,0,0x00 }, //CE INTO
			{ 1,0,0,0,0,0,0,0x00 }, //CF IRET
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
			{ 1,0,0,0,0,0,0,0x00 }, //24 Test register instructions become invalid!
			{ 0,0,0,0,0,0,2,0x04 }, //25
			{ 1,0,0,0,0,0,0,0x00 }, //26 Test register instructions become invalid!
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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
			{ 0,0,0,0,0,0,0xA,0  }, //A0 MOV AL,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A1 MOV AX,[imm16]
			{ 0,0,0,0,0,0,0xA,0  }, //A2 MOV [imm16],AL
			{ 0,0,0,0,0,0,0xA,0  }, //A3 MOV [imm16],AX
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

CPU_Timings CPUTimings[CPU_MODES][0x200]; //All normal CPU timings, which are used, for all modes available!

void generate_timings_tbl() //Generate the timings table!
{
	int opcode;
	byte mode, curmode;
	byte theCPU;

	//Normal instruction timings!
	memset(&CPUTimings,0,sizeof(CPUTimings)); //Clear the entire timing table for filling it!
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
				theCPU = (byte)EMULATED_CPU; //Start with the emulated CPU!
				goto tryopcodes; //Try the next mode!
			}
			memcpy(&CPUTimings[mode][(opcode<<1)],&CPUInformation[theCPU][curmode][opcode],sizeof(CPUTimings[mode][opcode])); //Set the mode to the highest mode detected that's available!

			//0F timings next for this opcode!
			if (EMULATED_CPU>=CPU_80286) //0F opcodes as well?
			{
				curmode = mode; //The current mode we're processing!
			tryopcodes0F: //Retry with the other mode!
				theCPU = (byte)(EMULATED_CPU-CPU_80286); //Start with the emulated CPU! The first generation to support this is the 286!
				for (;(CPUInformation0F[theCPU][curmode][opcode].used == 0) && theCPU;) --theCPU; //Goto parent while not used!
				if ((CPUInformation0F[theCPU][curmode][opcode].used == 0) && curmode) //Unused instruction and higher bit mode?
				{
					--curmode; //Try lower-bit mode!
					theCPU = (byte)(EMULATED_CPU-CPU_80286); //Start with the emulated CPU! The first generation to support this is the 286!
					goto tryopcodes0F; //Try the next mode!
				}
				memcpy(&CPUTimings[mode][(opcode<<1)|1], &CPUInformation0F[theCPU][curmode][opcode], sizeof(CPUTimings[mode][(opcode<<1)|1])); //Set the mode to the highest mode detected that's available!
			}
		}
	}
}
