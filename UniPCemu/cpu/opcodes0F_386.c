/* Same as opcodes, but the 0F extension! */

#include "headers/types.h" //Basic types
#include "headers/cpu/cpu.h" //CPU needed!
#include "headers/cpu/mmu.h" //MMU needed!
#include "headers/cpu/easyregs.h" //Easy register compatibility!
#include "headers/cpu/modrm.h" //MODR/M compatibility!
#include "headers/support/signedness.h" //CPU support functions!
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/cpu/cpu_OP80286.h" //80286 support!
#include "headers/cpu/cpu_OP80386.h" //80386 support!
#include "headers/cpu/protection.h" //Protection support!

//Opcodes based on: http://www.logix.cz/michal/doc/i386/chp17-a3.htm#17-03-A

extern VAL64Splitter temp1, temp2, temp3, temp4, temp5; //All temporary values!

/*

First, 32-bit 80386 variants of the 80286 opcodes!

*/

//Reading of the 16-bit entries within descriptors!
#ifdef SDL_SwapLE16
#define DESC_16BITS(x) SDL_SwapLE16(x)
#define DESC_32BITS(x) SDL_SwapLE32(x)
#else
//Unsupported? Placeholder!
#define DESC_16BITS(x) (x)
#define DESC_32BITS(x) (x)
#endif

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

//0F opcodes for 286+ processors!

//Based on http://ref.x86asm.net/coder32.html

void CPU386_OP0F01() //Various extended 286+ instruction GRP opcode.
{
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params, &info, 1); //Store the address for debugging!
	switch (thereg) //What function?
	{
	case 0: //SGDT
		debugger_setcommand("SGDTD %s", info.text);
		if (params.info[1].isreg==1) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		modrm_addoffset = 0;
		if (modrm_check16(&params,1,0)) return; //Abort on fault!
		modrm_addoffset = 2;
		if (modrm_check32(&params,1,0)) return; //Abort on fault!

		modrm_addoffset = 0; //Add no bytes to the offset!
		modrm_write16(&params, 1, CPU[activeCPU].registers->GDTR.limit, 0); //Store the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			modrm_write32(&params, 1, (CPU[activeCPU].registers->GDTR.base & 0xFFFFFFFF)); //Only 24-bits of limit, high byte is cleared with 386+, set with 286!
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 1: //SIDT
		debugger_setcommand("SIDTD %s", info.text);
		if (params.info[1].isreg==1) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}

		modrm_addoffset = 0;
		if (modrm_check16(&params,1,0)) return; //Abort on fault!
		modrm_addoffset = 2;
		if (modrm_check32(&params,1,0)) return; //Abort on fault!

		modrm_addoffset = 0; //Add no bytes to the offset!
		modrm_write16(&params, 1, CPU[activeCPU].registers->IDTR.limit, 0); //Store the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			modrm_write32(&params, 1, (CPU[activeCPU].registers->IDTR.base & 0xFFFFFFFF)); //Only 24-bits of limit, high byte is cleared with 386+, set with 286!
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 2: //LGDT
		debugger_setcommand("LGDTD %s", info.text);
		if (params.info[1].isreg==1) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
		{
			THROWDESCGP(0,0,0); //Throw #GP!
			return; //Abort!
		}

		modrm_addoffset = 0;
		if (modrm_check16(&params,1,1)) return; //Abort on fault!
		modrm_addoffset = 2;
		if (modrm_check32(&params,1,1)) return; //Abort on fault!

		modrm_addoffset = 0; //Add no bytes to the offset!
		oper1 = modrm_read16(&params, 1); //Read the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			oper1d = ((uint_32)modrm_read32(&params, 1)); //Lower part of the limit!
			CPUPROT1
				CPU[activeCPU].registers->GDTR.base = oper1d; //Load the base!
				CPU[activeCPU].registers->GDTR.limit = oper1; //Load the limit!
			CPUPROT2
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 3: //LIDT
		debugger_setcommand("LIDTD %s", info.text);
		if (params.info[1].isreg==1) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
		{
			THROWDESCGP(0,0,0); //Throw #GP!
			return; //Abort!
		}

		modrm_addoffset = 0;
		if (modrm_check16(&params,1,1)) return; //Abort on fault!
		modrm_addoffset = 2;
		if (modrm_check32(&params,1,1)) return; //Abort on fault!

		modrm_addoffset = 0; //Add no bytes to the offset!
		oper1 = modrm_read16(&params, 1); //Read the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			oper1d = ((uint_32)modrm_read32(&params, 1)); //Lower part of the limit!
			CPUPROT1
				CPU[activeCPU].registers->IDTR.base = oper1d; //Load the base!
				CPU[activeCPU].registers->IDTR.limit = oper1; //Load the limit!
			CPUPROT2
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 4: //SMSW: Same as 80286!
		debugger_setcommand("SMSW %s", info.text);
		if (modrm_check32(&params,1,0)) return; //Abort on fault!
		modrm_write32(&params,1,(word)(CPU[activeCPU].registers->CR0&0xFFFF)); //Store the MSW into the specified location!
		break;
	case 6: //LMSW: Same as 80286!
		debugger_setcommand("LMSW %s", info.text);
		if (modrm_check32(&params,1,1)) return; //Abort on fault!
		if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
		{
			THROWDESCGP(0,0,0); //Throw #GP!
			return; //Abort!
		}
		CPU[activeCPU].cycles_OP = 4*16; //Make sure we last long enough for the required JMP to be fully buffered!
		oper1 = modrm_read16(&params,1); //Read the new register!
		CPUPROT1
		oper1 |= (CPU[activeCPU].registers->CR0&CR0_PE); //Keep the protected mode bit on, this isn't toggable anymore once set!
		CPU[activeCPU].registers->CR0 = (CPU[activeCPU].registers->CR0&(~0xFFFF))|oper1; //Set the MSW only!
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

#include "headers/packed.h" //Packed!
typedef struct PACKED
{
	struct
	{
		uint_32 AR;
		uint_32 BASE;
		uint_32 LIMIT;
	};
	byte data[8]; //All our descriptor cache data!
} DESCRIPTORCACHE386;
#include "headers/endpacked.h" //Finished!

#include "headers/packed.h" //Packed!
typedef struct PACKED
{
	struct
	{
		uint_32 AR;
		uint_32 BASE;
		uint_32 LIMIT;
	};
	byte data[12];
} DTRdata;
#include "headers/endpacked.h" //Finished!

void CPU386_LOADALL_LoadDescriptor(DESCRIPTORCACHE386 *source, sword segment)
{
	CPU[activeCPU].SEG_DESCRIPTOR[segment].limit_low = (DESC_32BITS(source->LIMIT)&0xFFFF);
	CPU[activeCPU].SEG_DESCRIPTOR[segment].noncallgate_info = ((DESC_32BITS(source->AR)&0xF00>>4)|((DESC_32BITS(source->LIMIT)>>16)&0xF)); //Full high limit information and remaining rights data!
	CPU[activeCPU].SEG_DESCRIPTOR[segment].base_low = (DESC_32BITS(source->BASE)&0xFFFF);
	CPU[activeCPU].SEG_DESCRIPTOR[segment].base_mid = ((DESC_32BITS(source->BASE)>>16)&0xFF); //Mid is High base in the descriptor(286 only)!
	CPU[activeCPU].SEG_DESCRIPTOR[segment].base_high = (DESC_32BITS(source->BASE)>>24); //Full 32-bits are used for the base!
	CPU[activeCPU].SEG_DESCRIPTOR[segment].AccessRights = source->AR; //Access rights is completely used. Present being 0 makes the register unfit to read (#GP is fired).
}

void CPU386_OP0F07() //Undocumented LOADALL instruction
{
	word address;
#include "headers/packed.h" //Packed!
	union PACKED
	{
		struct
		{
			uint_32 CR0;
			uint_32 EFLAGS;
			uint_32 EIP;
			uint_32 EDI;
			uint_32 ESI;
			uint_32 EBP;
			uint_32 ESP;
			uint_32 EBX;
			uint_32 EDX;
			uint_32 ECX;
			uint_32 EAX;
			uint_32 DR6;
			uint_32 DR7;
			uint_32 TR;
			uint_32 LDTR;
			uint_32 GS;
			uint_32 FS;
			uint_32	DS;
			uint_32 SS;
			uint_32 CS;
			uint_32 ES;
			DESCRIPTORCACHE386 TRdescriptor;
			DTRdata IDTR;
			DTRdata GDTR;
			DESCRIPTORCACHE386 LDTRdescriptor;
			DESCRIPTORCACHE386 GSdescriptor;
			DESCRIPTORCACHE386 FSdescriptor;
			DESCRIPTORCACHE386 DSdescriptor;
			DESCRIPTORCACHE386 SSdescriptor;
			DESCRIPTORCACHE386 CSdescriptor;
			DESCRIPTORCACHE386 ESdescriptor;
		} fields; //Fields
		uint_32 datad[51]; //Our data size!
	} LOADALLDATA;
#include "headers/endpacked.h" //Finished!

	if (getCPL() && (getcpumode()!=CPU_MODE_REAL)) //We're protected by CPL!
	{
		unkOP0F_286(); //Raise an error!
		return;
	}

	//TODO: Load the data from the location specified!
	memset(&LOADALLDATA,0,sizeof(LOADALLDATA)); //Init the structure to be used as a buffer!

	//Load the data from the used location!

	for (address=0;address<NUMITEMS(LOADALLDATA.datad);++address)
	{
		if (checkMMUaccess(CPU_SEGMENT_ES,REG_ES,REG_EDI,0,getCPL(),1)) return; //Abort on fault!
	}

	for (address=0;address<NUMITEMS(LOADALLDATA.datad);++address) //Load all remaining data in default byte order!
	{
		LOADALLDATA.datad[address] = DESC_32BITS(MMU_rdw(CPU_SEGMENT_ES,REG_ES,REG_EDI+(address<<2),0,1)); //Read the raw data to load from memory!
	}

	//Load all registers and caches, ignore any protection normally done(not checked during LOADALL)!
	//Plain registers!
	CPU[activeCPU].registers->CR0 = DESC_32BITS(LOADALLDATA.fields.CR0); //MSW! We cannot reenter real mode by clearing bit 0(Protection Enable bit)!
	CPU[activeCPU].registers->TR = DESC_16BITS(LOADALLDATA.fields.TR); //TR
	CPU[activeCPU].registers->FLAGS = DESC_32BITS(LOADALLDATA.fields.EFLAGS); //FLAGS
	CPU[activeCPU].registers->EIP = DESC_32BITS(LOADALLDATA.fields.EIP); //IP
	CPU[activeCPU].registers->LDTR = DESC_16BITS(LOADALLDATA.fields.LDTR); //LDT
	CPU[activeCPU].registers->DS = DESC_16BITS(LOADALLDATA.fields.DS); //DS
	CPU[activeCPU].registers->SS = DESC_16BITS(LOADALLDATA.fields.SS); //SS
	CPU[activeCPU].registers->CS = DESC_16BITS(LOADALLDATA.fields.CS); //CS
	CPU[activeCPU].registers->ES = DESC_16BITS(LOADALLDATA.fields.ES); //ES
	CPU[activeCPU].registers->EDI = DESC_32BITS(LOADALLDATA.fields.EDI); //DI
	CPU[activeCPU].registers->ESI = DESC_32BITS(LOADALLDATA.fields.ESI); //SI
	CPU[activeCPU].registers->EBP = DESC_32BITS(LOADALLDATA.fields.EBP); //BP
	CPU[activeCPU].registers->ESP = DESC_32BITS(LOADALLDATA.fields.ESP); //SP
	CPU[activeCPU].registers->EBX = DESC_32BITS(LOADALLDATA.fields.EBX); //BX
	CPU[activeCPU].registers->EDX = DESC_32BITS(LOADALLDATA.fields.ECX); //CX
	CPU[activeCPU].registers->ECX = DESC_32BITS(LOADALLDATA.fields.EDX); //DX
	CPU[activeCPU].registers->EAX = DESC_32BITS(LOADALLDATA.fields.EAX); //AX
	updateCPUmode(); //We're updating the CPU mode if needed, since we're reloading CR0 and FLAGS!
	CPU_flushPIQ(); //We're jumping to another address!

	//GDTR/IDTR registers!
	CPU[activeCPU].registers->GDTR.base = DESC_32BITS(LOADALLDATA.fields.GDTR.BASE); //Base!
	CPU[activeCPU].registers->GDTR.limit = DESC_32BITS(LOADALLDATA.fields.GDTR.LIMIT); //Limit
	CPU[activeCPU].registers->IDTR.base = DESC_32BITS(LOADALLDATA.fields.IDTR.BASE); //Base!
	CPU[activeCPU].registers->IDTR.limit = DESC_32BITS(LOADALLDATA.fields.IDTR.LIMIT); //Limit

	//Load all descriptors directly without checks!
	CPU386_LOADALL_LoadDescriptor(&LOADALLDATA.fields.ESdescriptor,CPU_SEGMENT_ES); //ES descriptor!
	CPU386_LOADALL_LoadDescriptor(&LOADALLDATA.fields.CSdescriptor,CPU_SEGMENT_CS); //CS descriptor!
	CPU386_LOADALL_LoadDescriptor(&LOADALLDATA.fields.SSdescriptor,CPU_SEGMENT_SS); //SS descriptor!
	CPU386_LOADALL_LoadDescriptor(&LOADALLDATA.fields.DSdescriptor,CPU_SEGMENT_DS); //DS descriptor!
	CPU386_LOADALL_LoadDescriptor(&LOADALLDATA.fields.LDTRdescriptor,CPU_SEGMENT_LDTR); //LDT descriptor!
	CPU386_LOADALL_LoadDescriptor(&LOADALLDATA.fields.TRdescriptor,CPU_SEGMENT_TR); //TSS descriptor!
}

extern byte didJump; //Did we jump?

//New: 16-bit and 32-bit variants of OP70-7F as a 0F opcode!
//16-bits variant
void CPU80386_OP0F80_16() {INLINEREGISTER sword rel16;/*JO rel8: (FLAG_OF=1)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JO",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_OF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F81_16() {INLINEREGISTER sword rel16;/*JNO rel8 : (FLAG_OF=0)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JNO",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_OF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F82_16() {INLINEREGISTER sword rel16;/*JC rel8: (FLAG_CF=1)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JC",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_CF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F83_16() {INLINEREGISTER sword rel16;/*JNC rel8 : (FLAG_CF=0)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JNC",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_CF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F84_16() {INLINEREGISTER sword rel16;/*JZ rel8: (FLAG_ZF=1)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JZ",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_ZF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F85_16() {INLINEREGISTER sword rel16;/*JNZ rel8 : (FLAG_ZF=0)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JNZ",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_ZF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F86_16() {INLINEREGISTER sword rel16;/*JBE rel8 : (FLAG_CF=1|FLAG_ZF=1)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JBE",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_CF||FLAG_ZF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F87_16() {INLINEREGISTER sword rel16;/*JA rel8: (FLAG_CF=0&FLAG_ZF=0)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JA",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_CF && !FLAG_ZF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F88_16() {INLINEREGISTER sword rel16;/*JS rel8: (FLAG_SF=1)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JS",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F89_16() {INLINEREGISTER sword rel16;/*JNS rel8 : (FLAG_SF=0)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JNS",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_SF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8A_16() {INLINEREGISTER sword rel16;/*JP rel8 : (FLAG_PF=1)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JP",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_PF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8B_16() {INLINEREGISTER sword rel16;/*JNP rel8 : (FLAG_PF=0)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JNP",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_PF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8C_16() {INLINEREGISTER sword rel16;/*JL rel8: (FLAG_SF!=FLAG_OF)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JL",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF!=FLAG_OF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8D_16() {INLINEREGISTER sword rel16;/*JGE rel8 : (FLAG_SF=FLAG_OF)*/ rel16 = imm16(); modrm_generateInstructionTEXT("JGE",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (FLAG_SF==FLAG_OF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8E_16() {INLINEREGISTER sword rel16;/*JLE rel8 : (FLAG_ZF|(FLAG_SF!=FLAG_OF))*/ rel16 = imm16(); modrm_generateInstructionTEXT("JLE",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if ((FLAG_SF!=FLAG_OF) || FLAG_ZF) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8F_16() {INLINEREGISTER sword rel16;/*JG rel8: ((FLAG_ZF=0)&&(FLAG_SF=FLAG_OF))*/ rel16 = imm16(); modrm_generateInstructionTEXT("JG",0,REG_EIP + rel16,PARAM_IMM16); /* JUMP to destination? */ if (!FLAG_ZF && (FLAG_SF==FLAG_OF)) {REG_EIP += rel16; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
//32-bits variant
void CPU80386_OP0F80_32() {INLINEREGISTER int_32 rel32;/*JO rel8: (FLAG_OF=1)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JO",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (FLAG_OF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F81_32() {INLINEREGISTER int_32 rel32;/*JNO rel8 : (FLAG_OF=0)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JNO",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (!FLAG_OF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F82_32() {INLINEREGISTER int_32 rel32;/*JC rel8: (FLAG_CF=1)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JC",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (FLAG_CF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F83_32() {INLINEREGISTER int_32 rel32;/*JNC rel8 : (FLAG_CF=0)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JNC",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (!FLAG_CF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F84_32() {INLINEREGISTER int_32 rel32;/*JZ rel8: (FLAG_ZF=1)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JZ",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (FLAG_ZF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F85_32() {INLINEREGISTER int_32 rel32;/*JNZ rel8 : (FLAG_ZF=0)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JNZ",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (!FLAG_ZF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F86_32() {INLINEREGISTER int_32 rel32;/*JBE rel8 : (FLAG_CF=1|FLAG_ZF=1)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JBE",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (FLAG_CF||FLAG_ZF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F87_32() {INLINEREGISTER int_32 rel32;/*JA rel8: (FLAG_CF=0&FLAG_ZF=0)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JA",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (!FLAG_CF && !FLAG_ZF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F88_32() {INLINEREGISTER int_32 rel32;/*JS rel8: (FLAG_SF=1)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JS",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (FLAG_SF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F89_32() {INLINEREGISTER int_32 rel32;/*JNS rel8 : (FLAG_SF=0)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JNS",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (!FLAG_SF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8A_32() {INLINEREGISTER int_32 rel32;/*JP rel8 : (FLAG_PF=1)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JP",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (FLAG_PF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8B_32() {INLINEREGISTER int_32 rel32;/*JNP rel8 : (FLAG_PF=0)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JNP",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (!FLAG_PF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8C_32() {INLINEREGISTER int_32 rel32;/*JL rel8: (FLAG_SF!=FLAG_OF)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JL",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (FLAG_SF!=FLAG_OF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8D_32() {INLINEREGISTER int_32 rel32;/*JGE rel8 : (FLAG_SF=FLAG_OF)*/ rel32 = imm32(); modrm_generateInstructionTEXT("JGE",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (FLAG_SF==FLAG_OF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8E_32() {INLINEREGISTER int_32 rel32;/*JLE rel8 : (FLAG_ZF|(FLAG_SF!=FLAG_OF))*/ rel32 = imm32(); modrm_generateInstructionTEXT("JLE",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if ((FLAG_SF!=FLAG_OF) || FLAG_ZF) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }
void CPU80386_OP0F8F_32() {INLINEREGISTER int_32 rel32;/*JG rel8: ((FLAG_ZF=0)&&(FLAG_SF=FLAG_OF))*/ rel32 = imm32(); modrm_generateInstructionTEXT("JG",0,REG_EIP + rel32,PARAM_IMM32); /* JUMP to destination? */ if (!FLAG_ZF && (FLAG_SF==FLAG_OF)) {REG_EIP += rel32; /* JUMP to destination? */ CPU_flushPIQ(); /*We're jumping to another address*/ CPU[activeCPU].cycles_OP = 16; didJump = 1; /* Branch taken */} else { CPU[activeCPU].cycles_OP = 4; /* Branch not taken */} }

//MOV [C/D]Rn instructions

OPTINLINE byte allowCRDRaccess()
{
	return ((getCPL()==0) || (getcpumode()==CPU_MODE_REAL)); //Do we allow access?
}

void CPU80386_OP0F20() {modrm_generateInstructionTEXT("MOV",32,0,PARAM_MODRM12); if (!allowCRDRaccess()) {THROWDESCGP(0,0,0); return;} modrm_write32(&params,0,modrm_read32(&params,1));} //MOV /r r32,CRn
void CPU80386_OP0F21() {modrm_generateInstructionTEXT("MOV",32,0,PARAM_MODRM12); if (!allowCRDRaccess()) {THROWDESCGP(0,0,0); return;} modrm_write32(&params,0,modrm_read32(&params,1));} //MOV /r r32,DRn
void CPU80386_OP0F22() {modrm_generateInstructionTEXT("MOV",32,0,PARAM_MODRM21); if (!allowCRDRaccess()) {THROWDESCGP(0,0,0); return;} modrm_write32(&params,1,modrm_read32(&params,0));} //MOV /r CRn,r32
void CPU80386_OP0F23() {modrm_generateInstructionTEXT("MOV",32,0,PARAM_MODRM21); if (!allowCRDRaccess()) {THROWDESCGP(0,0,0); return;} modrm_write32(&params,1,modrm_read32(&params,0));} //MOV /r DRn,r32
void CPU80386_OP0F24() {modrm_generateInstructionTEXT("MOV",32,0,PARAM_MODRM12); if (!allowCRDRaccess()) {THROWDESCGP(0,0,0); return;} modrm_write32(&params,0,modrm_read32(&params,1));} //MOV /r r32,TRn
void CPU80386_OP0F26() {modrm_generateInstructionTEXT("MOV",32,0,PARAM_MODRM21); if (!allowCRDRaccess()) {THROWDESCGP(0,0,0); return;} modrm_write32(&params,1,modrm_read32(&params,0));} //MOV /r TRn,r32

//SETCC instructions

/*

Info for different conditions:
O: FLAG_OF
NO: !FLAG_OF
C: FLAG_CF
NC: !FLAG_CF
Z: FLAG_ZF
NZ: !FLAG_ZF
BE: (FLAG_CF||FLAG_ZF)
A: (!FLAG_CF && !FLAG_ZF)
S: FLAG_SF
NS: !FLAG_SF
P: FLAG_PF
NP: !FLAG_PF
L: (FLAG_SF!=FLAG_OF)
GE: (FLAG_SF==FLAG_OF)
LE: ((FLAG_SF!=FLAG_OF) || FLAG_ZF)
G: (!FLAG_ZF && (FLAG_SF==FLAG_OF))

*/

void CPU80386_OP0F90() {modrm_generateInstructionTEXT("SETO",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,FLAG_OF);} //SETO r/m8
void CPU80386_OP0F91() {modrm_generateInstructionTEXT("SETNO",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,FLAG_OF?0:1);} //SETNO r/m8
void CPU80386_OP0F92() {modrm_generateInstructionTEXT("SETC",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,FLAG_CF);} //SETC r/m8
void CPU80386_OP0F93() {modrm_generateInstructionTEXT("SETNC",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,FLAG_CF?0:1);;} //SETAE r/m8
void CPU80386_OP0F94() {modrm_generateInstructionTEXT("SETZ",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,FLAG_ZF);} //SETE r/m8
void CPU80386_OP0F95() {modrm_generateInstructionTEXT("SETNZ",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,FLAG_ZF?0:1);} //SETNE r/m8
void CPU80386_OP0F96() {modrm_generateInstructionTEXT("SETNA",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,(FLAG_CF||FLAG_ZF)?1:0);} //SETNA r/m8
void CPU80386_OP0F97() {modrm_generateInstructionTEXT("SETA",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,(!FLAG_CF && !FLAG_ZF)?1:0);} //SETA r/m8
void CPU80386_OP0F98() {modrm_generateInstructionTEXT("SETS",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,FLAG_SF);} //SETS r/m8
void CPU80386_OP0F99() {modrm_generateInstructionTEXT("SETNS",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,FLAG_SF?0:1);} //SETNS r/m8
void CPU80386_OP0F9A() {modrm_generateInstructionTEXT("SETP",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,FLAG_PF);} //SETP r/m8
void CPU80386_OP0F9B() {modrm_generateInstructionTEXT("SETNP",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,FLAG_PF?0:1);} //SETNP r/m8
void CPU80386_OP0F9C() {modrm_generateInstructionTEXT("SETL",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,(FLAG_SF!=FLAG_OF)?1:0);} //SETL r/m8
void CPU80386_OP0F9D() {modrm_generateInstructionTEXT("SETGE",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,(FLAG_SF==FLAG_OF)?1:0);} //SETGE r/m8
void CPU80386_OP0F9E() {modrm_generateInstructionTEXT("SETLE",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,((FLAG_SF!=FLAG_OF) || FLAG_ZF)?1:0);} //SETLE r/m8
void CPU80386_OP0F9F() {modrm_generateInstructionTEXT("SETG",8,0,PARAM_MODRM1); if (modrm_check8(&params,1,0)) return; modrm_write8(&params,1,(!FLAG_ZF && (FLAG_SF==FLAG_OF))?1:0);} //SETG r/m8

//Push/pop FS
void CPU80386_OP0FA0() {modrm_generateInstructionTEXT("PUSH FS",0,0,PARAM_NONE);/*PUSH FS*/ if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_FS);/*PUSH FS*/} //PUSH FS
void CPU80386_OP0FA1() {modrm_generateInstructionTEXT("POP FS",0,0,PARAM_NONE);/*POP FS*/ if (checkStackAccess(1,0,0)) return; segmentWritten(CPU_SEGMENT_FS,CPU_POP16(),0);} //POP FS

void CPU80386_BT16(word val, word bit)
{
	FLAGW_CF(val>>(bit&0xF));
}

void CPU80386_BT32(uint_32 val, uint_32 bit)
{
	FLAGW_CF(val>>(bit&0x1F));
}

void CPU80386_OP0FA3_16() {modrm_generateInstructionTEXT("BTW",16,0,PARAM_MODRM12); word bit = modrm_read16(&params,0); modrm_addoffset = ((bit>>4)<<1); if (modrm_check16(&params,1,1)) return; CPU80386_BT16(modrm_read16(&params,1),bit);} //BT /r r/m16,r16
void CPU80386_OP0FA3_32() {modrm_generateInstructionTEXT("BTD",32,0,PARAM_MODRM12); uint_32 bit = modrm_read32(&params,0); modrm_addoffset = ((bit>>5)<<2); if (modrm_check32(&params,1,1)) return; CPU80386_BT32(modrm_read32(&params,1),bit);} //BT /r r/m32,r32

void CPU80386_SHLD_16(word *dest, word src, byte cnt)
{
	byte shift;
	if (cnt) //To actually shift?
	{
		word s = dest?*dest:modrm_read16(&params,1);
		cnt &= 0x1F;
		for (shift = 1; shift <= cnt; shift++)
		{
			if (s & 0x8000) FLAGW_CF(1); else FLAGW_CF(0);
			s = ((s << 1) & 0xFFFF)|((src>>15)&1);
			src <<= 1; //Next bit to shift in!
		}
		if ((cnt==1) && (FLAG_CF == (s >> 15))) FLAGW_OF(0); else FLAGW_OF(1);
		flag_szp16(s);
		if (dest)
		{
			*dest = s;
		}
		else
		{
			modrm_write16(&params,1,s,0);
		}
	}
}

void CPU80386_SHLD_32(uint_32 *dest, uint_32 src, byte cnt)
{
	byte shift;
	uint_32 s = dest?*dest:modrm_read32(&params,1);
	cnt &= 0x1F;
	if (cnt)
	{
		for (shift = 1; shift <= cnt; shift++)
		{
			if (s & 0x80000000) FLAGW_CF(1); else FLAGW_CF(0);
			s = ((s << 1) & 0xFFFFFFFF)|((src>>31)&1);
			src <<= 1; //Next bit to shift in!
		}
		if ((cnt==1) && (FLAG_CF == (s >> 31))) FLAGW_OF(0); else FLAGW_OF(1);
		flag_szp32(s);
		if (dest)
		{
			*dest = s;
		}
		else
		{
			modrm_write32(&params,1,s);
		}
	}
}

void CPU80386_OP0FA4_16() {modrm_generateInstructionTEXT("SHLD",16,immb,PARAM_MODRM12_IMM8); if (modrm_check16(&params,1,1)) return; if (modrm_check16(&params,0,1)) return; if (modrm_check16(&params,1,0)) return; CPU80386_SHLD_16(modrm_addr16(&params,1,0),modrm_read16(&params,0),immb);} //SHLD /r r/m16,r16,imm8
void CPU80386_OP0FA4_32() {modrm_generateInstructionTEXT("SHLD",32,immb,PARAM_MODRM12_IMM8); if (modrm_check32(&params,1,1)) return; if (modrm_check32(&params,0,1)) return; if (modrm_check32(&params,1,0)) return; CPU80386_SHLD_32(modrm_addr32(&params,1,0),modrm_read32(&params,0),immb);} //SHLD /r r/m32,r32,imm8

void CPU80386_OP0FA5_16() {modrm_generateInstructionTEXT("SHLD",16,0,PARAM_MODRM12_CL); if (modrm_check16(&params,1,1)) return; if (modrm_check16(&params,0,1)) return; if (modrm_check16(&params,1,0)) return; CPU80386_SHLD_16(modrm_addr16(&params,1,0),modrm_read16(&params,0),REG_CL);} //SHLD /r r/m16,r16,CL
void CPU80386_OP0FA5_32() {modrm_generateInstructionTEXT("SHLD",32,0,PARAM_MODRM12_CL); if (modrm_check32(&params,1,1)) return; if (modrm_check32(&params,0,1)) return; if (modrm_check32(&params,1,0)) return; CPU80386_SHLD_32(modrm_addr32(&params,1,0),modrm_read32(&params,0),REG_CL);} //SHLD /r r/m32,r32,CL

void CPU80386_OP0FA8() {modrm_generateInstructionTEXT("PUSH GS",0,0,PARAM_NONE);/*PUSH GS*/ if (checkStackAccess(1,1,0)) return; CPU_PUSH16(&REG_GS);/*PUSH FS*/} //PUSH GS
void CPU80386_OP0FA9() {modrm_generateInstructionTEXT("POP GS",0,0,PARAM_NONE);/*POP GS*/ if (checkStackAccess(1,0,0)) return; segmentWritten(CPU_SEGMENT_GS,CPU_POP16(),0);} //POP GS

//0F AA is RSM FLAGS on 386++

void CPU80386_BTS16(word *val, word bit)
{
	FLAGW_CF(*val>>(bit&0xF));
	*val |= (1<<(bit&0xF)); //Set!
}

void CPU80386_BTS32(uint_32 *val, uint_32 bit)
{
	FLAGW_CF(*val>>(bit&0x1F));
	*val |= (1<<(bit&0x1F)); //Set!
}

void CPU80386_OP0FAB_16() {modrm_generateInstructionTEXT("BTSW",16,0,PARAM_MODRM12); word bit = modrm_read16(&params,0); word val; modrm_addoffset = ((bit>>4)<<1); if (modrm_check16(&params,1,1)) return; val = modrm_read16(&params,1); CPU80386_BTS16(&val,bit); modrm_write16(&params,1,val,0); } //BTS /r r/m16,r16
void CPU80386_OP0FAB_32() {modrm_generateInstructionTEXT("BTSD",32,0,PARAM_MODRM12); uint_32 bit = modrm_read32(&params,0); uint_32 val; modrm_addoffset = ((bit>>5)<<2); if (modrm_check32(&params,1,1)) return; val = modrm_read32(&params,1); CPU80386_BTS32(&val,bit); modrm_write32(&params,1,val); } //BTS /r r/m32,r32

void CPU80386_SHRD_16(word *dest, word src, byte cnt)
{
	byte shift;
	word s = dest?*dest:modrm_read16(&params,1);
	cnt &= 0x1F;
	if (cnt)
	{
		if (cnt==1) FLAGW_OF((s & 0x8000) ? 1 : 0);
		for (shift = 1; shift <= cnt; shift++)
		{
			FLAGW_CF(s & 1);
			s = ((s >> 1)|((src&1)<<15));
			src >>= 1; //Next bit to shift in!
		}
		flag_szp16(s);
		if (dest)
		{
			*dest = s;
		}
		else
		{
			modrm_write16(&params,1,s,0);
		}
	}
}


void CPU80386_SHRD_32(uint_32 *dest, uint_32 src, byte cnt)
{
	byte shift;
	uint_32 s = dest?*dest:modrm_read32(&params,1);
	cnt &= 0x1F;
	if (cnt)
	{
		if (cnt==1) FLAGW_OF((s & 0x80000000) ? 1 : 0);
		for (shift = 1; shift <= cnt; shift++)
		{
			FLAGW_CF(s & 1);
			s = ((s >> 1)|((src&1)<<31));
			src >>= 1; //Next bit to shift in!
		}
		flag_szp32(s);
		if (dest)
		{
			*dest = s;
		}
		else
		{
			modrm_write32(&params,1,s);
		}
	}
}

void CPU80386_OP0FAC_16() {modrm_generateInstructionTEXT("SHRD",16,immb,PARAM_MODRM12_IMM8); if (modrm_check16(&params,1,1)) return; if (modrm_check16(&params,0,1)) return; if (modrm_check16(&params,1,0)) return; CPU80386_SHRD_16(modrm_addr16(&params,1,0),modrm_read16(&params,0),immb);} //SHRD /r r/m16,r16,imm8
void CPU80386_OP0FAC_32() {modrm_generateInstructionTEXT("SHRD",32,immb,PARAM_MODRM12_IMM8); if (modrm_check32(&params,1,1)) return; if (modrm_check32(&params,0,1)) return; if (modrm_check32(&params,1,0)) return; CPU80386_SHRD_32(modrm_addr32(&params,1,0),modrm_read32(&params,0),immb);} //SHRD /r r/m32,r32,imm8
void CPU80386_OP0FAD_16() {modrm_generateInstructionTEXT("SHRD",16,0,PARAM_MODRM12_CL); if (modrm_check16(&params,1,1)) return; if (modrm_check16(&params,0,1)) return; if (modrm_check16(&params,1,0)) return; CPU80386_SHRD_16(modrm_addr16(&params,1,0),modrm_read16(&params,0),REG_CL);} //SHRD /r r/m16,r16,CL
void CPU80386_OP0FAD_32() {modrm_generateInstructionTEXT("SHRD",32,0,PARAM_MODRM12_CL); if (modrm_check32(&params,1,1)) return; if (modrm_check32(&params,0,1)) return; if (modrm_check32(&params,1,0)) return; CPU80386_SHRD_32(modrm_addr32(&params,1,0),modrm_read32(&params,0),REG_CL);} //SHRD /r r/m32,r32,CL

void CPU80386_OP0FAF_16() { //IMUL /r r16,r/m16
	temp1.val32 = (uint_32)modrm_read16(&params,0); //Read reg instead! Word register = Word register * imm16!
	temp2.val32 = (uint_32)modrm_read16(&params,1); //Immediate word is second/third parameter!
	modrm_generateInstructionTEXT("IMULW",16,0,PARAM_MODRM12);
	if ((temp1.val32 &0x8000)==0x8000) temp1.val32 |= 0xFFFF0000;
	if ((temp2.val32 &0x8000)==0x8000) temp2.val32 |= 0xFFFF0000;
	temp3.val32s = temp1.val32s; //Load and...
	temp3.val32s *= temp2.val32s; //Signed multiplication!
	modrm_write16(&params,0,temp3.val16,0); //Write to the destination(register)!
	if (((temp3.val32>>15)==0) || ((temp3.val32>>15)==0x1FFFF)) FLAGW_OF(0); //Overflow flag is cleared when high word is a sign extension of the low word!
	else FLAGW_OF(1);
	FLAGW_CF(FLAG_OF); //OF=CF!
	FLAGW_SF((temp3.val16&0x8000)>>15); //Sign!
	FLAGW_PF(parity[temp3.val16&0xFF]); //Parity flag!
	FLAGW_ZF((temp3.val16==0)?1:0); //Set the zero flag!
}
void CPU80386_OP0FAF_32() { //IMUL /r r32,r/m32
	temp1.val64 = (uint_64)modrm_read32(&params,0); //Read reg instead! Word register = Word register * imm16!
	temp2.val64 = (uint_64)modrm_read32(&params,1); //Immediate word is second/third parameter!
	modrm_generateInstructionTEXT("IMULD",32,0,PARAM_MODRM12);
	if ((temp1.val64 &0x80000000ULL)==0x80000000ULL) temp1.val64 |= 0xFFFFFFFF00000000ULL;
	if ((temp2.val64 &0x80000000ULL)==0x80000000ULL) temp2.val64 |= 0xFFFFFFFF00000000ULL;
	temp3.val64s = temp1.val32s; //Load and...
	temp3.val64s *= temp2.val32s; //Signed multiplication!
	modrm_write32(&params,0,temp3.val32); //Write to the destination(register)!
	if (((temp3.val64>>31)==0ULL) || ((temp3.val64>>31)==0x1FFFFFFFFULL)) FLAGW_OF(0); //Overflow flag is cleared when high word is a sign extension of the low word!
	else FLAGW_OF(1);
	FLAGW_CF(FLAG_OF); //OF=CF!
	FLAGW_SF(((uint_64)temp3.val32&0x80000000U)>>31); //Sign!
	FLAGW_PF(parity[temp3.val32&0xFF]); //Parity flag!
	FLAGW_ZF((temp3.val32==0)?1:0); //Set the zero flag!
}

//LSS
void CPU80386_OP0FB2_16() /*LSS modr/m*/ {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("LSS",0,0,PARAM_MODRM12); CPU8086_internal_LXS(CPU_SEGMENT_SS); /*Load new SS!*/ } //LSS /r r16,m16:16
void CPU80386_OP0FB2_32() /*LSS modr/m*/ {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("LSS",0,0,PARAM_MODRM12); CPU80386_internal_LXS(CPU_SEGMENT_SS); /*Load new SS!*/ } //LSS /r r32,m16:32

void CPU80386_BTR16(word *val, word bit)
{
	FLAGW_CF(*val>>(bit&0xF));
	*val &= ~(1<<(bit&0xF)); //Reset!
}

void CPU80386_BTR32(uint_32 *val, uint_32 bit)
{
	FLAGW_CF(*val>>(bit&0x1F));
	*val &= ~(1<<(bit&0x1F)); //Reset!
}

void CPU80386_OP0FB3_16() {modrm_generateInstructionTEXT("BTRW",16,0,PARAM_MODRM12); word bit = modrm_read16(&params,0); word val; modrm_addoffset = ((bit>>4)<<1); if (modrm_check16(&params,1,1)) return; val = modrm_read16(&params,1); CPU80386_BTR16(&val,bit); modrm_write16(&params,1,val,0);} //BTR /r r/m16,r16
void CPU80386_OP0FB3_32() {modrm_generateInstructionTEXT("BTRD",32,0,PARAM_MODRM12); uint_32 bit = modrm_read32(&params,0); uint_32 val; modrm_addoffset = ((bit>>5)<<2); if (modrm_check32(&params,1,1)) return; val = modrm_read32(&params,1); CPU80386_BTR32(&val,bit); modrm_write32(&params,1,val);} //BTR /r r/m32,r32

void CPU80386_OP0FB4_16() /*LFS modr/m*/ {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("LFS",0,0,PARAM_MODRM12); CPU8086_internal_LXS(CPU_SEGMENT_FS); /*Load new FS!*/ } //LFS /r r16,m16:16
void CPU80386_OP0FB4_32() /*LFS modr/m*/ {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("LFS",0,0,PARAM_MODRM12); CPU80386_internal_LXS(CPU_SEGMENT_FS); /*Load new FS!*/ } //LFS /r r32,m16:32

void CPU80386_OP0FB5_16() /*LGS modr/m*/ {modrm_debugger16(&params,0,1); modrm_generateInstructionTEXT("LGS",0,0,PARAM_MODRM12); CPU8086_internal_LXS(CPU_SEGMENT_GS); /*Load new GS!*/ } //LGS /r r16,m16:16
void CPU80386_OP0FB5_32() /*LGS modr/m*/ {modrm_debugger32(&params,0,1); modrm_generateInstructionTEXT("LGS",0,0,PARAM_MODRM12); CPU80386_internal_LXS(CPU_SEGMENT_GS); /*Load new GS!*/ } //LGS /r r32,m16:32

//Special debugger support for the following MOV[S/Z]X instructions.

OPTINLINE void modrm_actualdebuggerSZX(char *instruction)
{
	char result[256];
	bzero(result,sizeof(result));
	strcpy(result,instruction); //Set the instruction!
	strcat(result," %s,%s"); //2 params!
	debugger_setcommand(result,modrm_param1,modrm_param2);
}

OPTINLINE void modrm_debugger16_8(char *instruction)
{
	if (cpudebugger)
	{
		bzero(modrm_param1,sizeof(modrm_param1));
		bzero(modrm_param2,sizeof(modrm_param2));
		modrm_text16(&params,1,&modrm_param1[0]);
		modrm_text8(&params,0,&modrm_param2[0]);
		modrm_actualdebuggerSZX(instruction); //Actual debugger call!
	}
}

OPTINLINE void modrm_debugger32_8(char *instruction)
{
	if (cpudebugger)
	{
		bzero(modrm_param1,sizeof(modrm_param1));
		bzero(modrm_param2,sizeof(modrm_param2));
		modrm_text32(&params,1,&modrm_param1[0]);
		modrm_text16(&params,0,&modrm_param2[0]);
		modrm_actualdebuggerSZX(instruction); //Actual debugger call!
	}
}

OPTINLINE void modrm_debugger16_16(char *instruction)
{
	if (cpudebugger)
	{
		bzero(modrm_param1,sizeof(modrm_param1));
		bzero(modrm_param2,sizeof(modrm_param2));
		modrm_text16(&params,1,&modrm_param1[0]);
		modrm_text16(&params,0,&modrm_param2[0]);
		modrm_actualdebuggerSZX(instruction); //Actual debugger call!
	}
}

OPTINLINE void modrm_debugger32_16(char *instruction)
{
	if (cpudebugger)
	{
		bzero(modrm_param1,sizeof(modrm_param1));
		bzero(modrm_param2,sizeof(modrm_param2));
		modrm_text32(&params,1,&modrm_param1[0]);
		modrm_text16(&params,0,&modrm_param2[0]);
		modrm_actualdebuggerSZX(instruction); //Actual debugger call!
	}
}

void CPU80386_OP0FB6_16() {modrm_debugger16_8("MOVZXW"); if (modrm_check8(&params,1,1)) return; if (modrm_check16(&params,0,0)) return; modrm_write16(&params,0,(word)modrm_read8(&params,1),0);} //MOVZX /r r16,r/m8
void CPU80386_OP0FB6_32() {modrm_debugger32_8("MOVZXD"); if (modrm_check8(&params,1,1)) return; if (modrm_check32(&params,0,0)) return; modrm_write32(&params,0,(uint_32)modrm_read8(&params,1));} //MOVZX /r r32,r/m8

void CPU80386_OP0FB7_16() {modrm_debugger16_16("MOVZXW"); if (modrm_check16(&params,1,1)) return; if (modrm_check16(&params,0,0)) return; modrm_write16(&params,0,modrm_read16(&params,1),0);} //MOVZX /r r16,r/m16
void CPU80386_OP0FB7_32() {modrm_debugger32_16("MOVZXD"); if (modrm_check16(&params,1,1)) return; if (modrm_check32(&params,0,0)) return; modrm_write32(&params,0,(uint_32)modrm_read16(&params,1));} //MOVZX /r r32,r/m16

void CPU80386_BTC16(word *val, word bit)
{
	FLAGW_CF(*val>>(bit&0xF));
	*val ^= (1<<(bit&0xF)); //Complement!
}

void CPU80386_BTC32(uint_32 *val, uint_32 bit)
{
	FLAGW_CF(*val>>(bit&0x1F));
	*val ^= (1<<(bit&0x1F)); //Complement!
}

void CPU80386_OP0FBA_16() {
	word val;
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params, &info, 1); //Store the address for debugging!
	switch (thereg)
	{
		case 4: //BT r/m16,imm8
			//Debugger
			if (cpudebugger)
			{
				bzero(modrm_param1,sizeof(modrm_param1));
				modrm_text16(&params,1,&modrm_param1[0]);
				debugger_setcommand("BTW %s,%02X",modrm_param1,immb); //Log the command!
			}
			//Actual execution!
			modrm_addoffset = ((immb>>4)<<1);
			if (modrm_check16(&params,1,1)) return;
			CPU80386_BT16(modrm_read16(&params,1),immb);
			break;
		case 5: //BTS r/m16,imm8
			if (cpudebugger)
			{
				bzero(modrm_param1,sizeof(modrm_param1));
				modrm_text16(&params,1,&modrm_param1[0]);
				debugger_setcommand("BTSW %s,%02X",modrm_param1,immb); //Log the command!
			}
			//Actual execution!
			modrm_addoffset = ((immb>>4)<<1);
			if (modrm_check16(&params,1,1)) return;
			if (modrm_check16(&params,1,0)) return;
			val = modrm_read16(&params,1);
			CPU80386_BTS16(&val,immb);
			modrm_write16(&params,1,val,0);
			break;
		case 6: //BTR r/m16,imm8
			if (cpudebugger)
			{
				bzero(modrm_param1,sizeof(modrm_param1));
				modrm_text16(&params,1,&modrm_param1[0]);
				debugger_setcommand("BTRW %s,%02X",modrm_param1,immb); //Log the command!
			}
			//Actual execution!
			modrm_addoffset = ((immb>>4)<<1);
			if (modrm_check16(&params,1,1)) return;
			if (modrm_check16(&params,1,0)) return;
			val = modrm_read16(&params,1);
			CPU80386_BTR16(&val,immb);
			modrm_write16(&params,1,val,0);
			break;
		case 7: //BTC r/m16,imm8
			if (cpudebugger)
			{
				bzero(modrm_param1,sizeof(modrm_param1));
				modrm_text16(&params,1,&modrm_param1[0]);
				debugger_setcommand("BTCW %s,%02X",modrm_param1,immb); //Log the command!
			}
			//Actual execution!
			modrm_addoffset = ((immb>>4)<<1);
			if (modrm_check16(&params,1,1)) return;
			if (modrm_check16(&params,1,0)) return;
			val = modrm_read16(&params,1);
			CPU80386_BTC16(&val,immb);
			modrm_write16(&params,1,val,0);
			break;
		default: //Unknown instruction?
			unkOP0F_386(); //Unknown instruction!
			break;
	}
}

void CPU80386_OP0FBA_32() {
	uint_32 val;
	thereg = MODRM_REG(params.modrm);

	modrm_decode32(&params, &info, 1); //Store the address for debugging!
	switch (thereg)
	{
		case 4: //BT r/m32,imm8
			//Debugger
			if (cpudebugger)
			{
				bzero(modrm_param1,sizeof(modrm_param1));
				modrm_text32(&params,1,&modrm_param1[0]);
				debugger_setcommand("BTD %s,%02X",modrm_param1,immb); //Log the command!
			}
			//Actual execution!
			modrm_addoffset = ((immb>>5)<<2);
			if (modrm_check32(&params,1,1)) return;
			CPU80386_BT32(modrm_read32(&params,1),immb);
			break;
		case 5: //BTS r/m32,imm8
			if (cpudebugger)
			{
				bzero(modrm_param1,sizeof(modrm_param1));
				modrm_text16(&params,1,&modrm_param1[0]);
				debugger_setcommand("BTSD %s,%02X",modrm_param1,immb); //Log the command!
			}
			//Actual execution!
			modrm_addoffset = ((immb>>5)<<2);
			if (modrm_check32(&params,1,1)) return;
			if (modrm_check32(&params,1,0)) return;
			val = modrm_read32(&params,1);
			CPU80386_BTS32(&val,immb);
			modrm_write32(&params,1,val);
			break;
		case 6: //BTR r/m32,imm8
			if (cpudebugger)
			{
				bzero(modrm_param1,sizeof(modrm_param1));
				modrm_text16(&params,1,&modrm_param1[0]);
				debugger_setcommand("BTRD %s,%02X",modrm_param1,immb); //Log the command!
			}
			//Actual execution!
			modrm_addoffset = ((immb>>5)<<2);
			if (modrm_check32(&params,1,1)) return;
			if (modrm_check32(&params,1,0)) return;
			val = modrm_read32(&params,1);
			CPU80386_BTR32(&val,immb);
			modrm_write32(&params,1,val);
			break;
		case 7: //BTC r/m32,imm8
			if (cpudebugger)
			{
				bzero(modrm_param1,sizeof(modrm_param1));
				modrm_text16(&params,1,&modrm_param1[0]);
				debugger_setcommand("BTCD %s,%02X",modrm_param1,immb); //Log the command!
			}
			//Actual execution!
			modrm_addoffset = ((immb>>5)<<2);
			if (modrm_check32(&params,1,1)) return;
			if (modrm_check32(&params,1,0)) return;
			val = modrm_read32(&params,1);
			CPU80386_BTC32(&val,immb);
			modrm_write32(&params,1,val);
			break;
		default: //Unknown instruction?
			unkOP0F_386(); //Unknown instruction!
			break;
	}
}

void CPU80386_OP0FBB_16() {modrm_generateInstructionTEXT("BTCW",16,0,PARAM_MODRM12); word bit = modrm_read16(&params,0); word val; modrm_addoffset = ((bit>>4)<<1); if (modrm_check16(&params,1,1)) return; val = modrm_read16(&params,1); CPU80386_BTC16(&val,bit); modrm_write16(&params,1,val,0);} //BTC /r r/m16,r16
void CPU80386_OP0FBB_32() {modrm_generateInstructionTEXT("BTCD",32,0,PARAM_MODRM12); uint_32 bit = modrm_read32(&params,0); uint_32 val; modrm_addoffset = ((bit>>5)<<2); if (modrm_check32(&params,1,1)) return; val = modrm_read32(&params,1); CPU80386_BTC32(&val,bit); modrm_write32(&params,1,val);} //BTC /r r/m32,r32

void CPU80386_OP0FBC_16() {
	word src,dest,temp;
	modrm_generateInstructionTEXT("BSFW",16,0,PARAM_MODRM12);
	if (modrm_check16(&params,1,1)) return;
	if (modrm_check16(&params,1,0)) return;
	src = modrm_read16(&params,1); //Read src!
	if (src==0) //Nothing?
	{
		FLAGW_ZF(1); //Set zero flag!
	}
	else
	{
		dest = modrm_read16(&params,0); //Read dest!
		FLAGW_ZF(0);
		temp = 0;
		for (;(((src>>temp)&1)==0) && (temp<16);) //Still searching?
		{
			++temp;
			dest = temp;
		}
		modrm_write16(&params,0,dest,0); //Write the result!
	}
} //BSF /r r16,r/m16
void CPU80386_OP0FBC_32() {
	uint_32 src,dest,temp;
	modrm_generateInstructionTEXT("BSFD",16,0,PARAM_MODRM12);
	if (modrm_check32(&params,1,1)) return;
	if (modrm_check32(&params,1,0)) return;
	src = modrm_read32(&params,1); //Read src!
	if (src==0) //Nothing?
	{
		FLAGW_ZF(1); //Set zero flag!
	}
	else
	{
		dest = modrm_read32(&params,0); //Read dest!
		FLAGW_ZF(0);
		temp = 0;
		for (;(((src>>temp)&1)==0) && (temp<32);) //Still searching?
		{
			++temp;
			dest = temp; //Store temporary result!
		}
		modrm_write32(&params,0,dest); //Write the result!
	}
} //BSF /r r32,r/m32

void CPU80386_OP0FBD_16() {
	word src,dest,temp;
	modrm_generateInstructionTEXT("BSRW",16,0,PARAM_MODRM12);
	if (modrm_check16(&params,1,1)) return;
	if (modrm_check16(&params,1,0)) return;
	src = modrm_read16(&params,1); //Read src!
	if (src==0) //Nothing?
	{
		FLAGW_ZF(1); //Set zero flag!
	}
	else
	{
		dest = modrm_read16(&params,0); //Read dest!
		FLAGW_ZF(0);
		temp = 15;
		for (;(((src>>temp)&1)==0) && (temp!=0xFFFF);) //Still searching?
		{
			--temp;
			dest = temp;
		}
		modrm_write16(&params,0,dest,0); //Write the result!
	}
} //BSR /r r16,r/m16
void CPU80386_OP0FBD_32() {
	uint_32 src,dest,temp;
	modrm_generateInstructionTEXT("BSRD",16,0,PARAM_MODRM12);
	if (modrm_check32(&params,1,1)) return;
	if (modrm_check32(&params,1,0)) return;
	src = modrm_read32(&params,1); //Read src!
	if (src==0) //Nothing?
	{
		FLAGW_ZF(1); //Set zero flag!
	}
	else
	{
		dest = modrm_read32(&params,0); //Read dest!
		FLAGW_ZF(0);
		temp = 15;
		for (;(((src>>temp)&1)==0) && (temp!=0xFFFFFFFF);) //Still searching?
		{
			++temp;
			dest = temp; //Store temporary result!
		}
		modrm_write32(&params,0,dest); //Write the result!
	}
} //BSR /r r32,r/m32

void CPU80386_OP0FBE_16() {modrm_debugger16_8("MOVSXW"); if (modrm_check8(&params,1,1)) return; if (modrm_check16(&params,0,0)) return; modrm_write16(&params,0,signed2unsigned16((sword)unsigned2signed8(modrm_read8(&params,1))),0);} //MOVSX /r r16,r/m8
void CPU80386_OP0FBE_32() {modrm_debugger32_8("MOVSXD"); if (modrm_check8(&params,1,1)) return; if (modrm_check32(&params,0,0)) return; modrm_write32(&params,0,signed2unsigned16((sword)unsigned2signed8(modrm_read8(&params,1))));} //MOVSX /r r32,r/m8

void CPU80386_OP0FBF_16() {modrm_debugger16_16("MOVSXW"); if (modrm_check16(&params,1,1)) return; if (modrm_check16(&params,0,0)) return; modrm_write16(&params,0,signed2unsigned16((sword)unsigned2signed16(modrm_read16(&params,1))),0);} //MOVSX /r r16,r/m16
void CPU80386_OP0FBF_32() {modrm_debugger32_16("MOVSXD"); if (modrm_check16(&params,1,1)) return; if (modrm_check32(&params,0,0)) return; modrm_write32(&params,0,signed2unsigned32((int_32)unsigned2signed16(modrm_read16(&params,1))));} //MOVSX /r r32,r/m16
