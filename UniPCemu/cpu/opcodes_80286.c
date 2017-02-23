#include "headers/types.h" //Basic type support etc.
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/emu/gpu/gpu.h" //Need GPU comp!
#include "headers/cpu/cpu_OP8086.h" //8086 interrupt instruction support!
#include "headers/bios/bios.h" //BIOS Support!
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/cpu/easyregs.h" //Easy register addressing!

#include "headers/emu/gpu/gpu_emu.h" //GPU EMU support!
#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!

#include "headers/cpu/protection.h" //Protection support!
#include "headers/cpu/cpu_OP80286.h" //80286 instruction support!
#include "headers/cpu/cpu_OPNECV30.h" //186+ #UD support!
#include "headers/mmu/mmuhandler.h" //Direct memory access support for LOADALL!

//Reading of the 16-bit entries within descriptors!
#define DESC_16BITS(x) SDL_SwapLE16(x)

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

void CPU286_OP63() //ARPL r/m16,r16
{
	modrm_generateInstructionTEXT("ARPL",16,0,PARAM_MODRM21); //Our instruction text!
	if (getcpumode() == CPU_MODE_REAL) //Real mode? #UD!
	{
		unkOP_186(); //Execute our unk opcode handler!
		return; //Abort!
	}
	word destRPL, srcRPL;
	if (modrm_check16(&params,1,1)) return; //Abort on fault!
	if (modrm_check16(&params,0,1)) return; //Abort on fault!
	destRPL = modrm_read16(&params,1); //Read destination RPL!
	CPUPROT1
	srcRPL = modrm_read16(&params,0); //Read source RPL!
	CPUPROT1
		if (getRPL(destRPL) < getRPL(srcRPL))
		{
			FLAGW_ZF(1); //Set ZF!
			setRPL(destRPL,getRPL(srcRPL)); //Set the new RPL!
			if (modrm_check16(&params,1,0)) return; //Abort on fault!
			modrm_write16(&params,1,destRPL,0); //Set the result!
		}
		else
		{
			FLAGW_ZF(0); //Clear ZF!
		}
	CPUPROT2
	CPUPROT2
}

void CPU286_OP9D() {
	modrm_generateInstructionTEXT("POPF", 0, 0, PARAM_NONE);/*POPF*/
	word tempflags;
	if (checkStackAccess(1,0,0)) return;
	tempflags = CPU_POP16();
	if (disallowPOPFI()) { tempflags &= ~0x200; tempflags |= REG_FLAGS&0x200; /* Ignore any changes to the Interrupt flag! */ }
	if (getCPL()) { tempflags &= ~0x3000; tempflags |= REG_FLAGS&0x3000; /* Ignore any changes to the IOPL when not at CPL 0! */ }
	REG_FLAGS = tempflags;
	updateCPUmode(); /*POPF*/
	CPU[activeCPU].cycles_OP = 8; /*POPF timing!*/
}

void CPU286_OPD6() //286+ SALC
{
	debugger_setcommand("SALC");
	REG_AL = FLAG_CF?0xFF:0x00; //Set AL if Carry flag!
}

//0F opcodes for 286+ processors!

//Based on http://ref.x86asm.net/coder32.html

void CPU286_OP0F00() //Various extended 286+ instructions GRP opcode.
{
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params, &info, 1); //Store the address for debugging!
	switch (thereg) //What function?
	{
	case 0: //SLDT
		if (getcpumode() == CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("SLDT %s", info.text);
		if (modrm_check16(&params,1,0)) return; //Abort on fault!
		modrm_write16(&params,1,CPU[activeCPU].registers->LDTR,0); //Try and write it to the address specified!
		break;
	case 1: //STR
		if (getcpumode() == CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("STR %s", info.text);
		if (modrm_check16(&params,1,0)) return; //Abort on fault!
		modrm_write16(&params,1, CPU[activeCPU].registers->TR, 0); //Try and write it to the address specified!
		break;
	case 2: //LLDT
		if (getcpumode() == CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("LLDT %s", info.text);
		if (getCPL()) //Privilege level isn't 0?
		{
			THROWDESCGP(0,0,0); //Throw #GP!
			return; //Abort!
		}
		if (modrm_check16(&params,1,1)) return; //Abort on fault!
		oper1 = modrm_read16(&params,1); //Read the descriptor!
		CPUPROT1
			segmentWritten(CPU_SEGMENT_LDTR,oper1,0); //Write the segment!
		CPUPROT2
		break;
	case 3: //LTR
		if (getcpumode() == CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("LTR %s", info.text);
		if (getCPL()) //Privilege level isn't 0?
		{
			THROWDESCGP(0,0,0); //Throw #GP!
			return; //Abort!
		}
		if (modrm_check16(&params,1,1)) return; //Abort on fault!
		oper1 = modrm_read16(&params, 1); //Read the descriptor!
		CPUPROT1
			segmentWritten(CPU_SEGMENT_TR, oper1, 0); //Write the segment!
		CPUPROT2
		break;
	case 4: //VERR
		if (getcpumode() == CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("VERR %s", info.text);
		if (modrm_check16(&params,1,1)) return; //Abort on fault!
		oper1 = modrm_read16(&params,1); //Read the descriptor!
		CPUPROT1
			SEGDESCRIPTOR_TYPE verdescriptor;
			if (LOADDESCRIPTOR(-1, oper1, &verdescriptor)) //Load the descriptor!
			{
				if (CPU_MMU_checkrights(-1, oper1, 0, 1, &verdescriptor.desc, 0)==0) //Check without address test!
				{
					FLAGW_ZF(1); //We're valid!
				}
				else
				{
					FLAGW_ZF(0); //We're invalid!
				}
			}
			else
			{
				FLAGW_ZF(0); //We're invalid!
			}
		CPUPROT2
		break;
	case 5: //VERW
		if (getcpumode() == CPU_MODE_REAL)
		{
			unkOP0F_286(); //We're not recognized in real mode!
			return;
		}
		debugger_setcommand("VERW %s", info.text);
		if (modrm_check16(&params,1,1)) return; //Abort on fault!
		oper1 = modrm_read16(&params, 1); //Read the descriptor!
		CPUPROT1
			SEGDESCRIPTOR_TYPE verdescriptor;
			if (LOADDESCRIPTOR(-1, oper1, &verdescriptor)) //Load the descriptor!
			{
				if (CPU_MMU_checkrights(-1, oper1, 0, 0, &verdescriptor.desc, 0)==0) //Check without address test!
				{
					FLAGW_ZF(1); //We're valid!
				}
				else
				{
					FLAGW_ZF(0); //We're invalid!
				}
			}
			else
			{
				FLAGW_ZF(0); //We're invalid!
			}
		CPUPROT2
		break;
	case 6: //--- Unknown Opcode! ---
	case 7: //--- Unknown Opcode! ---
		unkOP0F_286(); //Unknown opcode!
		break;
	default:
		break;
	}
}

void CPU286_OP0F01() //Various extended 286+ instruction GRP opcode.
{
	thereg = MODRM_REG(params.modrm);

	modrm_decode16(&params, &info, 1); //Store the address for debugging!
	switch (thereg) //What function?
	{
	case 0: //SGDT
		debugger_setcommand("SGDT %s", info.text);
		if (params.info[1].isreg==1) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}
		modrm_addoffset = 0;
		if (modrm_check16(&params,1,0)) return; //Abort on fault!
		modrm_addoffset = 2;
		if (modrm_check16(&params,1,0)) return; //Abort on fault!
		modrm_addoffset = 4;
		if (modrm_check16(&params,1,0)) return; //Abort on fault!

		modrm_addoffset = 0; //Add no bytes to the offset!
		modrm_write16(&params, 1, CPU[activeCPU].registers->GDTR.limit, 0); //Store the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			modrm_write16(&params, 1, (CPU[activeCPU].registers->GDTR.base & 0xFFFF),0); //Only 24-bits of limit, high byte is cleared with 386+, set with 286!
			CPUPROT1
				modrm_addoffset = 4; //Add 4 bytes to the offset!
				modrm_write16(&params, 1, ((CPU[activeCPU].registers->GDTR.base >> 16) & 0xFFFF),0); //Write rest value!
			CPUPROT2
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 1: //SIDT
		debugger_setcommand("SIDT %s", info.text);
		if (params.info[1].isreg==1) //We're storing to a register? Invalid!
		{
			unkOP0F_286();
			return; //Abort!
		}

		modrm_addoffset = 0;
		if (modrm_check16(&params,1,0)) return; //Abort on fault!
		modrm_addoffset = 2;
		if (modrm_check16(&params,1,0)) return; //Abort on fault!
		modrm_addoffset = 4;
		if (modrm_check16(&params,1,0)) return; //Abort on fault!

		modrm_addoffset = 0; //Add no bytes to the offset!
		modrm_write16(&params, 1, CPU[activeCPU].registers->IDTR.limit, 0); //Store the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			modrm_write16(&params, 1, (CPU[activeCPU].registers->IDTR.base & 0xFFFF),0); //Only 24-bits of limit, high byte is cleared with 386+, set with 286!
			CPUPROT1
				modrm_addoffset = 4; //Add 4 bytes to the offset!
				modrm_write16(&params, 1, ((CPU[activeCPU].registers->IDTR.base>>16) & 0xFFFF),0); //Write rest value!
			CPUPROT2
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 2: //LGDT
		debugger_setcommand("LGDT %s", info.text);
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
		if (modrm_check16(&params,1,1)) return; //Abort on fault!
		modrm_addoffset = 4;
		if (modrm_check16(&params,1,1)) return; //Abort on fault!

		modrm_addoffset = 0; //Add no bytes to the offset!
		oper1 = modrm_read16(&params, 1); //Read the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			oper1d = ((uint_32)modrm_read16(&params, 1)); //Lower part of the limit!
			CPUPROT1
				modrm_addoffset = 4; //Last byte!
				oper1d |= (((uint_32)(modrm_read16(&params,1)&0xFF))<<16); //Higher part of the limit!
				CPUPROT1
					CPU[activeCPU].registers->GDTR.base = oper1d; //Load the base!
					CPU[activeCPU].registers->GDTR.limit = oper1; //Load the limit!
				CPUPROT2
			CPUPROT2
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 3: //LIDT
		debugger_setcommand("LIDT %s", info.text);
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
		if (modrm_check16(&params,1,1)) return; //Abort on fault!
		modrm_addoffset = 4;
		if (modrm_check16(&params,1,1)) return; //Abort on fault!

		modrm_addoffset = 0; //Add no bytes to the offset!
		oper1 = modrm_read16(&params, 1); //Read the limit first!
		CPUPROT1
			modrm_addoffset = 2; //Add 2 bytes to the offset!
			oper1d = ((uint_32)modrm_read16(&params, 1)); //Lower part of the limit!
			CPUPROT1
				modrm_addoffset = 4; //Last byte!
				oper1d |= (((uint_32)modrm_read16(&params, 1)&0xFF) << 16); //Higher part of the limit!
				CPUPROT1
					CPU[activeCPU].registers->IDTR.base = oper1d; //Load the base!
					CPU[activeCPU].registers->IDTR.limit = oper1; //Load the limit!
				CPUPROT2
			CPUPROT2
		CPUPROT2
		modrm_addoffset = 0; //Add no bytes to the offset!
		break;
	case 4: //SMSW
		debugger_setcommand("SMSW %s", info.text);
		if (modrm_check16(&params,1,0)) return; //Abort on fault!
		modrm_write16(&params,1,(word)(CPU[activeCPU].registers->CR0&0xFFFF),0); //Store the MSW into the specified location!
		break;
	case 6: //LMSW
		debugger_setcommand("LMSW %s", info.text);
		if (modrm_check16(&params,1,1)) return; //Abort on fault!
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

void CPU286_OP0F02() //LAR /r
{
	byte isconforming = 1;
	SEGDESCRIPTOR_TYPE verdescriptor;
	if (getcpumode() == CPU_MODE_REAL)
	{
		unkOP0F_286(); //We're not recognized in real mode!
		return;
	}
	modrm_generateInstructionTEXT("LAR", 16, 0, PARAM_MODRM12); //Our instruction text!
	if (modrm_check16(&params,1,1)) return; //Abort on fault!
	oper1 = modrm_read16(&params,1); //Read the segment to check!
	CPUPROT1
		if (LOADDESCRIPTOR(-1, oper1, &verdescriptor)) //Load the descriptor!
		{
			switch (GENERALSEGMENT_TYPE(verdescriptor.desc))
			{
			case AVL_SYSTEM_RESERVED_0: //Invalid type?
			case AVL_SYSTEM_INTERRUPTGATE16BIT:
			case AVL_SYSTEM_TRAPGATE16BIT:
			case AVL_SYSTEM_RESERVED_1:
			case AVL_SYSTEM_RESERVED_2:
			case AVL_SYSTEM_RESERVED_3:
			case AVL_SYSTEM_INTERRUPTGATE32BIT:
			case AVL_SYSTEM_TRAPGATE32BIT:
				FLAGW_ZF(0); //Invalid descriptor type!
				break;
			default: //Valid type?
				switch (verdescriptor.desc.AccessRights) //What type?
				{
				case AVL_CODE_EXECUTEONLY_CONFORMING:
				case AVL_CODE_EXECUTEONLY_CONFORMING_ACCESSED:
				case AVL_CODE_EXECUTE_READONLY_CONFORMING:
				case AVL_CODE_EXECUTE_READONLY_CONFORMING_ACCESSED: //Conforming?
					isconforming = 1;
					break;
				default: //Not conforming?
					isconforming = 0;
					break;
				}
				if ((MAX(getCPL(), getRPL(oper1)) <= GENERALSEGMENT_DPL(verdescriptor.desc)) || isconforming) //Valid privilege?
				{
					if (modrm_check16(&params,1,0)) return; //Abort on fault!
					modrm_write16(&params,0,(word)(verdescriptor.desc.AccessRights<<8),0); //Write our result!
					CPUPROT1
						FLAGW_ZF(1); //We're valid!
					CPUPROT2
				}
				else
				{
					FLAGW_ZF(0); //Not valid!
				}
				break;
			}
		}
		else //Couldn't be loaded?
		{
			FLAGW_ZF(0); //Default: not loaded!
		}
	CPUPROT2
}

void CPU286_OP0F03() //LSL /r
{
	uint_32 limit;
	byte isconforming = 1;
	SEGDESCRIPTOR_TYPE verdescriptor;
	if (getcpumode() == CPU_MODE_REAL)
	{
		unkOP0F_286(); //We're not recognized in real mode!
		return;
	}
	modrm_generateInstructionTEXT("LSL", 16, 0, PARAM_MODRM12); //Our instruction text!
	if (modrm_check16(&params,1,1)) return; //Abort on fault!
	oper1 = modrm_read16(&params, 1); //Read the segment to check!
	CPUPROT1
		if (LOADDESCRIPTOR(-1, oper1, &verdescriptor)) //Load the descriptor!
		{
			switch (GENERALSEGMENT_TYPE(verdescriptor.desc))
			{
			case AVL_SYSTEM_RESERVED_0: //Invalid type?
			case AVL_SYSTEM_INTERRUPTGATE16BIT:
			case AVL_SYSTEM_TRAPGATE16BIT:
			case AVL_SYSTEM_RESERVED_1:
			case AVL_SYSTEM_RESERVED_2:
			case AVL_SYSTEM_RESERVED_3:
			case AVL_SYSTEM_INTERRUPTGATE32BIT:
			case AVL_SYSTEM_TRAPGATE32BIT:
				FLAGW_ZF(0); //Invalid descriptor type!
				break;
			default: //Valid type?
				switch (verdescriptor.desc.AccessRights) //What type?
				{
				case AVL_CODE_EXECUTEONLY_CONFORMING:
				case AVL_CODE_EXECUTEONLY_CONFORMING_ACCESSED:
				case AVL_CODE_EXECUTE_READONLY_CONFORMING:
				case AVL_CODE_EXECUTE_READONLY_CONFORMING_ACCESSED: //Conforming?
					isconforming = 1;
					break;
				default: //Not conforming?
					isconforming = 0;
					break;
				}

				limit = verdescriptor.desc.limit_low|(SEGDESC_NONCALLGATE_LIMIT_HIGH(verdescriptor.desc)<<16); //Limit!
				if ((SEGDESC_NONCALLGATE_G(verdescriptor.desc)&CPU[activeCPU].G_Mask) && (EMULATED_CPU >= CPU_80386)) //Granularity?
				{
					limit = ((limit << 12) | 0xFFF); //4KB for a limit of 4GB, fill lower 12 bits with 1!
				}

				if ((MAX(getCPL(), getRPL(oper1)) <= GENERALSEGMENT_DPL(verdescriptor.desc)) || isconforming) //Valid privilege?
				{
					if (modrm_check16(&params,1,0)) return; //Abort on fault!
					modrm_write16(&params, 0, (word)(limit&0xFFFF), 0); //Write our result!
					CPUPROT1
						FLAGW_ZF(1); //We're valid!
					CPUPROT2
				}
				else
				{
					FLAGW_ZF(0); //Not valid!
				}
				break;
			}
		}
		else //Couldn't be loaded?
		{
			FLAGW_ZF(0); //Default: not loaded!
		}
	CPUPROT2
}

#include "headers/packed.h" //Packed!
typedef struct PACKED
{
	struct
	{
		word baselow; //First word
		byte basehigh; //Second word low bits
		byte accessrights; //Present bit is valid bit instead! Second word high bits!
		word limit; //Third word
	};
	byte data[6]; //All our descriptor cache data!
} DESCRIPTORCACHE286;
#include "headers/endpacked.h" //Finished!

#include "headers/packed.h" //Packed!
typedef struct PACKED
{
	struct
	{
		word baselow; //First word
		byte basehigh; //Second word low bits
		byte shouldbezeroed; //Second word high bits
		word limit; //Third word
	};
	byte data[6];
} DTRdata;
#include "headers/endpacked.h" //Finished!

void CPU286_LOADALL_LoadDescriptor(DESCRIPTORCACHE286 *source, sword segment)
{
	CPU[activeCPU].SEG_DESCRIPTOR[segment].limit_low = DESC_16BITS(source->limit);
	CPU[activeCPU].SEG_DESCRIPTOR[segment].noncallgate_info &= ~0xF; //No high limit!
	CPU[activeCPU].SEG_DESCRIPTOR[segment].base_low = DESC_16BITS(source->baselow);
	CPU[activeCPU].SEG_DESCRIPTOR[segment].base_mid = source->basehigh; //Mid is High base in the descriptor(286 only)!
	CPU[activeCPU].SEG_DESCRIPTOR[segment].base_high = 0; //Only 24 bits are used for the base!
	CPU[activeCPU].SEG_DESCRIPTOR[segment].callgate_base_mid = 0; //Not used!
	CPU[activeCPU].SEG_DESCRIPTOR[segment].AccessRights = source->accessrights; //Access rights is completely used. Present being 0 makes the register unfit to read (#GP is fired).
}

void CPU286_OP0F05() //Undocumented LOADALL instruction
{
	word address;
#include "headers/packed.h" //Packed!
	union PACKED
	{
		struct
		{
			byte unused[6];
			word MSW;
			byte unused2[14];
			word TR;
			word flags;
			word IP;
			word LDT;
			word DS;
			word SS;
			word CS;
			word ES;
			word DI;
			word SI;
			word BP;
			word SP;
			word BX;
			word DX;
			word CX;
			word AX;
			DESCRIPTORCACHE286 ESdescriptor;
			DESCRIPTORCACHE286 CSdescriptor;
			DESCRIPTORCACHE286 SSdescriptor;
			DESCRIPTORCACHE286 DSdescriptor;
			DTRdata GDTR;
			DESCRIPTORCACHE286 LDTdescriptor;
			DTRdata IDTR;
			DESCRIPTORCACHE286 TSSdescriptor;
		} fields; //Fields
		byte data[0x66]; //All data to be loaded!
		word dataw[0x33]; //Word-sized data to be loaded, if any!
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

	for (address=0;address<27;++address) //Load all registers in the correct format!
	{
		LOADALLDATA.dataw[address] = memory_directrw(0x800|(address<<1)); //Read the data to load from memory! Take care of any conversion needed!
	}

	for (address=54;address<sizeof(LOADALLDATA.data);++address) //Load all remaining data in default byte order!
	{
		LOADALLDATA.data[address] = memory_directrb(0x800|address); //Read the data to load from memory!
	}

	//Load all registers and caches, ignore any protection normally done(not checked during LOADALL)!
	//Plain registers!
	CPU[activeCPU].registers->CR0 = LOADALLDATA.fields.MSW|(CPU[activeCPU].registers->CR0&CR0_PE); //MSW! We cannot reenter real mode by clearing bit 0(Protection Enable bit)!
	CPU[activeCPU].registers->TR = LOADALLDATA.fields.TR; //TR
	CPU[activeCPU].registers->FLAGS = LOADALLDATA.fields.flags; //FLAGS
	CPU[activeCPU].registers->EIP = (uint_32)LOADALLDATA.fields.IP; //IP
	CPU[activeCPU].registers->LDTR = LOADALLDATA.fields.LDT; //LDT
	CPU[activeCPU].registers->DS = DESC_16BITS(LOADALLDATA.fields.DS); //DS
	CPU[activeCPU].registers->SS = DESC_16BITS(LOADALLDATA.fields.SS); //SS
	CPU[activeCPU].registers->CS = DESC_16BITS(LOADALLDATA.fields.CS); //CS
	CPU[activeCPU].registers->ES = DESC_16BITS(LOADALLDATA.fields.ES); //ES
	CPU[activeCPU].registers->DI = DESC_16BITS(LOADALLDATA.fields.DI); //DI
	CPU[activeCPU].registers->SI = DESC_16BITS(LOADALLDATA.fields.SI); //SI
	CPU[activeCPU].registers->BP = DESC_16BITS(LOADALLDATA.fields.BP); //BP
	CPU[activeCPU].registers->SP = DESC_16BITS(LOADALLDATA.fields.SP); //SP
	CPU[activeCPU].registers->BX = DESC_16BITS(LOADALLDATA.fields.BX); //BX
	CPU[activeCPU].registers->DX = DESC_16BITS(LOADALLDATA.fields.CX); //CX
	CPU[activeCPU].registers->CX = DESC_16BITS(LOADALLDATA.fields.DX); //DX
	CPU[activeCPU].registers->AX = DESC_16BITS(LOADALLDATA.fields.AX); //AX
	updateCPUmode(); //We're updating the CPU mode if needed, since we're reloading CR0 and FLAGS!
	CPU_flushPIQ(); //We're jumping to another address!

	//GDTR/IDTR registers!
	CPU[activeCPU].registers->GDTR.base = (LOADALLDATA.fields.GDTR.basehigh<<2)|DESC_16BITS(LOADALLDATA.fields.GDTR.baselow); //Base!
	CPU[activeCPU].registers->GDTR.limit = DESC_16BITS(LOADALLDATA.fields.GDTR.limit); //Limit
	CPU[activeCPU].registers->IDTR.base = (LOADALLDATA.fields.IDTR.basehigh<<2)|DESC_16BITS(LOADALLDATA.fields.IDTR.baselow); //Base!
	CPU[activeCPU].registers->IDTR.limit = DESC_16BITS(LOADALLDATA.fields.IDTR.limit); //Limit

	//Load all descriptors directly without checks!
	CPU286_LOADALL_LoadDescriptor(&LOADALLDATA.fields.ESdescriptor,CPU_SEGMENT_ES); //ES descriptor!
	CPU286_LOADALL_LoadDescriptor(&LOADALLDATA.fields.CSdescriptor,CPU_SEGMENT_CS); //CS descriptor!
	CPU286_LOADALL_LoadDescriptor(&LOADALLDATA.fields.SSdescriptor,CPU_SEGMENT_SS); //SS descriptor!
	CPU286_LOADALL_LoadDescriptor(&LOADALLDATA.fields.DSdescriptor,CPU_SEGMENT_DS); //DS descriptor!
	CPU286_LOADALL_LoadDescriptor(&LOADALLDATA.fields.LDTdescriptor,CPU_SEGMENT_LDTR); //LDT descriptor!
	CPU286_LOADALL_LoadDescriptor(&LOADALLDATA.fields.TSSdescriptor,CPU_SEGMENT_TR); //TSS descriptor!
}

void CPU286_OP0F06() //CLTS
{
	debugger_setcommand("CLTS"); //Our instruction text!
	if (getCPL() && (getcpumode() != CPU_MODE_REAL)) //Privilege level isn't 0?
	{
		THROWDESCGP(0,0,0); //Throw #GP!
		return; //Abort!
	}
	CPU[activeCPU].registers->CR0 &= ~CR0_TS; //Clear the Task Switched flag!
}

void CPU286_OP0F0B() //#UD instruction
{
	unkOP0F_286(); //Deliberately #UD!
}

void CPU286_OP0FB9() //#UD instruction
{
	unkOP0F_286(); //Deliberately #UD!
}

void CPU286_OPF1() //Undefined opcode, Don't throw any exception!
{
	debugger_setcommand("ICEBP");
	CPU_INT(1,-1); //ICEBP!
}

//FPU non-existant Coprocessor support!

void FPU80287_OPDBE3(){debugger_setcommand("<UNKOP8087: FNINIT>");}
void FPU80287_OPDFE0() { debugger_setcommand("<UNKOP8087: FNINIT>"); }
void FPU80287_OPDDslash7() { debugger_setcommand("<UNKOP8087: FNSTSW>"); }
void FPU80287_OPD9slash7() { debugger_setcommand("<UNKOP8087: FNSTCW>"); }


void FPU80287_OPDB(){if (CPU[activeCPU].registers->CR0&CR0_EM) { FPU80287_noCOOP(); return; /* Emulate! */ } if ((CPU[activeCPU].registers->CR0&CR0_MP) && (CPU[activeCPU].registers->CR0&CR0_TS)) { FPU80287_noCOOP(); return; } byte subOP = immb; CPUPROT1 word oldCS = REG_CS; word oldIP = REG_IP; if (subOP==0xE3){FPU80287_OPDBE3();} else{REG_CS = oldCS; REG_IP = oldIP; CPU_flushPIQ(); FPU80287_noCOOP();} CPUPROT2 }
void FPU80287_OPDF(){if (CPU[activeCPU].registers->CR0&CR0_EM) { FPU80287_noCOOP(); return; /* Emulate! */ } if ((CPU[activeCPU].registers->CR0&CR0_MP) && (CPU[activeCPU].registers->CR0&CR0_TS)) { FPU80287_noCOOP(); return; } CPUPROT1 byte subOP = immb; CPUPROT1 word oldCS = REG_CS; word oldIP = REG_IP; if (subOP==0xE0){FPU80287_OPDFE0();} else {REG_CS = oldCS; REG_IP = oldIP; CPU_flushPIQ(); FPU80287_noCOOP();} CPUPROT2 CPUPROT2 }
void FPU80287_OPDD(){word oldCS; word oldIP; oldCS = REG_CS; oldIP = REG_IP; if (CPU[activeCPU].registers->CR0&CR0_EM) { FPU80287_noCOOP(); return; /* Emulate! */ } if ((CPU[activeCPU].registers->CR0&CR0_MP) && (CPU[activeCPU].registers->CR0&CR0_TS)) { FPU80287_noCOOP(); return; } CPUPROT1 if (MODRM_REG(params.modrm)==7){FPU80287_OPDDslash7();}else {REG_CS = oldCS; REG_IP = oldIP; CPU_flushPIQ(); FPU80287_noCOOP();} CPUPROT2}
void FPU80287_OPD9(){word oldCS; word oldIP; oldCS = REG_CS; oldIP = REG_IP; if (CPU[activeCPU].registers->CR0&CR0_EM) { FPU80287_noCOOP(); return; /* Emulate! */ } if ((CPU[activeCPU].registers->CR0&CR0_MP) && (CPU[activeCPU].registers->CR0&CR0_TS)) { FPU80287_noCOOP(); return; } CPUPROT1 if (MODRM_REG(params.modrm)==7){FPU80287_OPD9slash7();} else {REG_CS = oldCS; REG_IP = oldIP; CPU_flushPIQ(); FPU80287_noCOOP();} CPUPROT2}

void FPU80287_noCOOP() {
	debugger_setcommand("<No COprocessor OPcodes implemented!>");
	if ((CPU[activeCPU].registers->CR0&CR0_EM) || ((CPU[activeCPU].registers->CR0&CR0_MP) && (CPU[activeCPU].registers->CR0&CR0_TS))) //To be emulated or task switched?
	{
		CPU_resetOP();
		CPU_COOP_notavailable(); //Only on 286+!
	}
	CPU[activeCPU].cycles_OP = MODRM_EA(params) ? 8 + MODRM_EA(params) : 2; //No hardware interrupt to use anymore!
}