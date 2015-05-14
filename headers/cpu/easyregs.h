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

//HIER GEBLEVEN; nog te doen: FLAG_CF & FLAG_AF! (Carry flag & Adjust flag (carry or borrow to the low order four bits of AL))

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
#define REG_AL CPU[activeCPU].registers->AL
#define REG_AH CPU[activeCPU].registers->AH
#define REG_EAX CPU[activeCPU].registers->EAX
#define REG_AX CPU[activeCPU].registers->AX

//Base register:
#define REG_BL CPU[activeCPU].registers->BL
#define REG_BH CPU[activeCPU].registers->BH
#define REG_EBX CPU[activeCPU].registers->EBX
#define REG_BX CPU[activeCPU].registers->BX

//Counter register:
#define REG_CL CPU[activeCPU].registers->CL
#define REG_CH CPU[activeCPU].registers->CH
#define REG_ECX CPU[activeCPU].registers->ECX
#define REG_CX CPU[activeCPU].registers->CX

//Data register:
#define REG_DL CPU[activeCPU].registers->DL
#define REG_DH CPU[activeCPU].registers->DH
#define REG_EDX CPU[activeCPU].registers->EDX
#define REG_DX CPU[activeCPU].registers->DX

//Segment registers
#define REG_CS CPU[activeCPU].registers->CS
#define REG_DS CPU[activeCPU].registers->DS
#define REG_ES CPU[activeCPU].registers->ES
#define REG_FS CPU[activeCPU].registers->FS
#define REG_GS CPU[activeCPU].registers->GS
#define REG_SS CPU[activeCPU].registers->SS

//Indexes and pointers
#define REG_EDI CPU[activeCPU].registers->EDI
#define REG_DI CPU[activeCPU].registers->DI
#define REG_ESI CPU[activeCPU].registers->ESI
#define REG_SI CPU[activeCPU].registers->SI
#define REG_EBP CPU[activeCPU].registers->EBP
#define REG_BP CPU[activeCPU].registers->BP
#define REG_ESP CPU[activeCPU].registers->ESP
#define REG_SP CPU[activeCPU].registers->SP
#define REG_EIP CPU[activeCPU].registers->EIP
#define REG_IP CPU[activeCPU].registers->IP
#define REG_EFLAGS CPU[activeCPU].registers->EFLAGS
#define REG_FLAGS CPU[activeCPU].registers->FLAGS

//Flags
#define FLAG_V8 CPU[activeCPU].registers->SFLAGS.V8
#define FLAG_RF CPU[activeCPU].registers->SFLAGS.RF
#define FLAG_NT CPU[activeCPU].registers->SFLAGS.NT
#define FLAG_PL CPU[activeCPU].registers->SFLAGS.PL
#define FLAG_OF CPU[activeCPU].registers->SFLAGS.OF
#define FLAG_DF CPU[activeCPU].registers->SFLAGS.DF
#define FLAG_IF CPU[activeCPU].registers->SFLAGS.IF
#define FLAG_TF CPU[activeCPU].registers->SFLAGS.TF
#define FLAG_SF CPU[activeCPU].registers->SFLAGS.SF
#define FLAG_ZF CPU[activeCPU].registers->SFLAGS.ZF
#define FLAG_AF CPU[activeCPU].registers->SFLAGS.AF
#define FLAG_PF CPU[activeCPU].registers->SFLAGS.PF
#define FLAG_CF CPU[activeCPU].registers->SFLAGS.CF
#endif
