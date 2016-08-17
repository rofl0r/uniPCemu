#ifndef MODRM_H
#define MODRM_H

#include "headers/types.h" //Needs type support!
#include "headers/cpu/cpu.h" //Need CPU support for types!

//MODR/M decoders:
byte MODRM_MOD(byte modrm); //MOD
byte MODRM_REG(byte modrm); //REG
byte MODRM_RM(byte modrm); //RM

//REG (depends on size specified by stack B-bit&opcode 66h&done with 'register' MOD (see below)):

//8-bit registers:
#define MODRM_REG_AL 0
#define MODRM_REG_CL 1
#define MODRM_REG_DL 2
#define MODRM_REG_BL 3
#define MODRM_REG_AH 4
#define MODRM_REG_CH 5
#define MODRM_REG_DH 6
#define MODRM_REG_BH 7

//16-bit registers (both /r and mod3 operands):
#define MODRM_REG_AX 0
#define MODRM_REG_CX 1
#define MODRM_REG_DX 2
#define MODRM_REG_BX 3
#define MODRM_REG_SP 4
#define MODRM_REG_BP 5
#define MODRM_REG_SI 6
#define MODRM_REG_DI 7

//16-bit segment registers (SReg operands)
#define MODRM_SEG_ES 0
#define MODRM_SEG_CS 1
#define MODRM_SEG_SS 2
#define MODRM_SEG_DS 3
#define MODRM_SEG_FS 4
#define MODRM_SEG_GS 5

//32-bit registers (32-bit variant of 16-bit registers):
#define MODRM_REG_EAX 0
#define MODRM_REG_ECX 1
#define MODRM_REG_EDX 2
#define MODRM_REG_EBX 3

//SP in mod3 operand
#define MODRM_REG_ESP 4
//SIB in all others.
#define MODRM_REG_SIB 4

#define MODRM_REG_EBP 5
#define MODRM_REG_ESI 6
#define MODRM_REG_EDI 7

//Finally: the data for r/m operands:

#define MODRM_MEM_BXSI 0
#define MODRM_MEM_BXDI 1
#define MODRM_MEM_BPSI 2
#define MODRM_MEM_BPDI 3
#define MODRM_MEM_SI 4
#define MODRM_MEM_DI 5

//Only in MOD0 instances, 16-bit address, instead of [BP]:
#define MODRM_MEM_DISP16 6
//Only in MOD0 instances, 32-bit address, instead of [EBP]:
#define MODRM_MEM_DISP32 5
//All other instances:
#define MODRM_MEM_BP 6

#define MODRM_MEM_BX 7

//Same as above, but for 32-bits memory addresses (mod 0-2)!

#define MODRM_MEM_EAX 0
#define MODRM_MEM_ECX 1
#define MODRM_MEM_EDX 2
#define MODRM_MEM_EBX 3
#define MODRM_MEM_SIB 4
//Mod>0 below:
#define MODRM_MEM_EBP 5
#define MODRM_MEM_ESI 6
#define MODRM_MEM_EDI 7



//MOD:

//e.g. [XXX] (location in memory)
#define MOD_MEM 0

//e.g. [XXX+disp8] (location in memory)
#define MOD_MEM_DISP8 1

//Below depends on MOD.
//e.g. [XXX+disp16] (location in memory) as /r param.
#define MOD_MEM_DISP16 2
//e.g. [XXX+disp32] (location in memory)
#define MOD_MEM_DISP32 2

//register (Xl/Xx/eXx; e.g. al/ax/eax) source/dest. (see above for further specification!)
#define MOD_REG 3

//Struct containing the MODRM info:

typedef struct
{
	byte isreg; //1 for register, 2 for memory, other is unknown!
	byte regsize; //1 for byte, 2 for word, 3 for dword
	struct
	{
		uint_32 *reg32;
		word *reg16;
		byte *reg8;
	}; //Register direct access!

//When not register:

	char text[20]; //String representation of reg or memory address!
	word mem_segment; //Segment of memory address!
	word *segmentregister; //The segment register (LEA related functions)!
	int segmentregister_index; //Segment register index!
	uint_32 mem_offset; //Offset of memory address!
} MODRM_PTR; //ModRM decoded pointer!

typedef struct
{
	byte modrm; //MODR/M!
	SIBType SIB; //SIB Byte if applied.
	dwordsplitterb displacement; //byte/word/dword!
	byte slashr; //Is this a /r MODR/M (=RM is reg2)?
	byte reg_is_segmentregister; //REG is segment register?
	MODRM_PTR info[3]; //All versions of info!
	byte EA_cycles; //The effective address cycles we're using!
} MODRM_PARAMS;

#define MODRM_MOD(modrm) ((modrm & 0xC0) >> 6)
#define MODRM_REG(modrm) ((modrm & 0x38) >> 3)
#define MODRM_RM(modrm) (modrm & 0x07)
#define modrm_isregister(params) (MODRM_MOD(params.modrm) == MOD_REG)
#define modrm_ismemory(params) (MODRM_MOD(params.modrm)!=MOD_REG)
#define MODRM_EA(params) params.EA_cycles

/*
Warning: SIB=Scaled index byte modes
*/

//Direct addressing of MODR/M bytes:

byte *modrm_addr8(MODRM_PARAMS *params, int whichregister, int forreading);
word *modrm_addr16(MODRM_PARAMS *params, int whichregister, int forreading);
uint_32 *modrm_addr32(MODRM_PARAMS *params, int whichregister, int forreading);

//Read/write things on MODR/M:

byte modrm_read8(MODRM_PARAMS *params, int whichregister);
word modrm_read16(MODRM_PARAMS *params, int whichregister);
uint_32 modrm_read32(MODRM_PARAMS *params, int whichregister);

void modrm_write8(MODRM_PARAMS *params, int whichregister, byte value);
void modrm_write16(MODRM_PARAMS *params, int whichregister, word value, byte isJMPorCALL);
void modrm_write32(MODRM_PARAMS *params, int whichregister, uint_32 value);

//Just the adressing:
word modrm_lea16(MODRM_PARAMS *params, int whichregister); //For LEA instructions!
void modrm_lea16_text(MODRM_PARAMS *params, int whichregister, char *result); //For LEA instructions!
word modrm_offset16(MODRM_PARAMS *params, int whichregister); //Gives address for JMP, CALL etc.!
word *modrm_addr_reg16(MODRM_PARAMS *params, int whichregister); //For LEA related instructions, returning the register!

void modrm_text8(MODRM_PARAMS *params, int whichregister, char *result); //8-bit text representation!
void modrm_text16(MODRM_PARAMS *params, int whichregister, char *result); //16-bit text representation!

void halt_modrm(char *message, ...); //Modr/m error?

void reset_modrm(); //Resets the modrm info for the current opcode!

//For converting unsigned text to signed text!
char *unsigned2signedtext8(byte c);
char *unsigned2signedtext16(word c);
char *unsigned2signedtext32(uint_32 c);

//For NECV30+
void modrm_decode16(MODRM_PARAMS *params, MODRM_PTR *result, byte whichregister); //16-bit address/reg decoder!

//For CPU itself:
void modrm_readparams(MODRM_PARAMS *param, byte size, byte slashr); //Read params for modr/m processing from CS:(E)IP
#endif