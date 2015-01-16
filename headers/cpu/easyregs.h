#ifndef CPU_EASYREGS_H
#define CPU_EASYREGS_H

//Overflow in implicit constant conversion?
#define CHECK_SF(data) \
				if (sizeof(data)==1) \
				{ \
					FLAG_SF = (data&0x80)>0; \
				} \
				else if (sizeof(data)==2) \
				{ \
					FLAG_SF = (data&0x8000)>0; \
				} \
				else if (sizeof(data)==4) \
				{ \
					FLAG_SF = (data&0x80000000)>0; \
				}
#define CHECK_ZF(data) FLAG_ZF = (data==0);
//FLAG_PF: Low 8-bits only!
#define CHECK_PF(data) \
				if (sizeof(data)==1) \
				{ \
					FLAG_PF = PARITY8(data); \
				} \
				else if (sizeof(data)==2) \
				{ \
					FLAG_PF = PARITY16(data&0xFF); \
				} \
				else if (sizeof(data)==4) \
				{ \
					FLAG_PF = PARITY32(data&0xFF); \
				}

//HIER GEBLEVEN; nog te doen: FLAG_CF & FLAG_AF! (Carry flag & Adjust flag (carry or borrow to the low order four bits of REG_AL))

//Borrow from high byte!
#define CHECK_CF(data,addition) FLAG_CF = (((((data+addition)^(data+addition))^addition)&(0x10<<((8*sizeof(addition))-1)))>0)
//Borrow from half-byte!
#define CHECK_AF(data,addition) FLAG_AF = (((((data+addition)^(data+addition))^addition)&0x10)>0)

//FLAG_OF=Klaar!
/*#define CHECK_OF(data,addition,ismultiplication) \
				if (sizeof(data)==1) \
				{ \
					FLAG_OF = ismultiplication?(!multiplication_is_safe(data,addition,8)):(!addition_is_safe(data,addition,8)); \
				} \
				else if (sizeof(data)==2) \
				{ \
					FLAG_OF = ismultiplication?(!multiplication_is_safe(data,addition,16)):(!addition_is_safe(data,addition,16)); \
				} \
				else if (sizeof(data)==4) \
				{ \
					FLAG_OF = ismultiplication?(!multiplication_is_safe(data,addition,32)):(!addition_is_safe(data,addition,32)); \
				}
*/

#ifndef parity
extern byte parity[0x100]; //Our parity table!
#endif

//First for parity calculations:
#define PARITY8(b) parity[b]
#define PARITY16(w) parity[w&0xFF]
#define PARITY32(dw) parity[dw&0xFF]

//Accumulator register:
#define REG_AL CPU.registers->AL
#define REG_AH CPU.registers->AH
#define REG_EAX CPU.registers->EAX
#define REG_AX CPU.registers->AX

//Base register:
#define REG_BL CPU.registers->BL
#define REG_BH CPU.registers->BH
#define REG_EBX CPU.registers->EBX
#define REG_BX CPU.registers->BX

//Counter register:
#define REG_CL CPU.registers->CL
#define REG_CH CPU.registers->CH
#define REG_ECX CPU.registers->ECX
#define REG_CX CPU.registers->CX

//Data register:
#define REG_DL CPU.registers->DL
#define REG_DH CPU.registers->DH
#define REG_EDX CPU.registers->EDX
#define REG_DX CPU.registers->DX

//Segment registers
#define REG_CS CPU.registers->CS
#define REG_DS CPU.registers->DS
#define REG_ES CPU.registers->ES
#define REG_FS CPU.registers->FS
#define REG_GS CPU.registers->GS
#define REG_SS CPU.registers->SS

//Indexes and pointers
#define REG_EDI CPU.registers->EDI
#define REG_DI CPU.registers->DI
#define REG_ESI CPU.registers->ESI
#define REG_SI CPU.registers->SI
#define REG_EBP CPU.registers->EBP
#define REG_BP CPU.registers->BP
#define REG_ESP CPU.registers->ESP
#define REG_SP CPU.registers->SP
#define REG_EIP CPU.registers->EIP
#define REG_IP CPU.registers->IP
#define REG_EFLAGS CPU.registers->EFLAGS
#define REG_FLAGS CPU.registers->FLAGS

//Flags
#define FLAG_V8 CPU.registers->SFLAGS.V8
#define FLAG_RF CPU.registers->SFLAGS.RF
#define FLAG_NT CPU.registers->SFLAGS.NT
#define FLAG_PL CPU.registers->SFLAGS.PL
#define FLAG_OF CPU.registers->SFLAGS.OF
#define FLAG_DF CPU.registers->SFLAGS.DF
#define FLAG_IF CPU.registers->SFLAGS.IF
#define FLAG_TF CPU.registers->SFLAGS.TF
#define FLAG_SF CPU.registers->SFLAGS.SF
#define FLAG_ZF CPU.registers->SFLAGS.ZF
#define FLAG_AF CPU.registers->SFLAGS.AF
#define FLAG_PF CPU.registers->SFLAGS.PF
#define FLAG_CF CPU.registers->SFLAGS.CF
#endif
