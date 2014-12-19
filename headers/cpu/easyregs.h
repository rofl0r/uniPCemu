#ifndef CPU_EASYREGS_H
#define CPU_EASYREGS_H

//Accumulator register:
#define AL CPU.registers->AL
#define AH CPU.registers->AH
#define EAX CPU.registers->EAX
#define AX CPU.registers->AX

//Base register:
#define BL CPU.registers->BL
#define BH CPU.registers->BH
#define EBX CPU.registers->EBX
#define BX CPU.registers->BX

//Counter register:
#define CL CPU.registers->CL
#define CH CPU.registers->CH
#define ECX CPU.registers->ECX
#define CX CPU.registers->CX

//Data register:
#define DL CPU.registers->DL
#define DH CPU.registers->DH
#define EDX CPU.registers->EDX
#define DX CPU.registers->DX

//Segment registers
#define CS CPU.registers->CS
#define DS CPU.registers->DS
#define ES CPU.registers->ES
#define FS CPU.registers->FS
#define GS CPU.registers->GS
#define SS CPU.registers->SS

//Indexes and pointers
#define EDI CPU.registers->EDI
#define DI CPU.registers->DI
#define ESI CPU.registers->ESI
#define SI CPU.registers->SI
#define EBP CPU.registers->EBP
#define BP CPU.registers->BP
#define ESP CPU.registers->ESP
#define SP CPU.registers->SP
#define EIP CPU.registers->EIP
#define IP CPU.registers->IP
#define EFLAGS CPU.registers->EFLAGS
#define FLAGS CPU.registers->FLAGS

//Flags
#define V8 CPU.registers->SFLAGS.V8
#define RF CPU.registers->SFLAGS.RF
#define NT CPU.registers->SFLAGS.NT
#define PL CPU.registers->SFLAGS.PL
#define OF CPU.registers->SFLAGS.OF
#define DF CPU.registers->SFLAGS.DF
#define IF CPU.registers->SFLAGS.IF
#define TF CPU.registers->SFLAGS.TF
#define SF CPU.registers->SFLAGS.SF
#define ZF CPU.registers->SFLAGS.ZF
#define AF CPU.registers->SFLAGS.AF
#define PF CPU.registers->SFLAGS.PF
#define CF CPU.registers->SFLAGS.CF

//Overflow in implicit constant conversion?
#define CHECK_SF(data) \
				if (sizeof(data)==1) \
				{ \
					SF = (data&0x80)>0; \
				} \
				else if (sizeof(data)==2) \
				{ \
					SF = (data&0x8000)>0; \
				} \
				else if (sizeof(data)==4) \
				{ \
					SF = (data&0x80000000)>0; \
				}
#define CHECK_ZF(data) ZF = (data==0);
//PF: Low 8-bits only!
#define CHECK_PF(data) \
				if (sizeof(data)==1) \
				{ \
					PF = PARITY8(data); \
				} \
				else if (sizeof(data)==2) \
				{ \
					PF = PARITY16(data&0xFF); \
				} \
				else if (sizeof(data)==4) \
				{ \
					PF = PARITY32(data&0xFF); \
				}

//HIER GEBLEVEN; nog te doen: CF & AF! (Carry flag & Adjust flag (carry or borrow to the low order four bits of AL))

//Borrow from high byte!
#define CHECK_CF(data,addition) CF = (((((data+addition)^(data+addition))^addition)&(0x10<<((8*sizeof(addition))-1)))>0)
//Borrow from half-byte!
#define CHECK_AF(data,addition) AF = (((((data+addition)^(data+addition))^addition)&0x10)>0)

//OF=Klaar!
/*#define CHECK_OF(data,addition,ismultiplication) \
				if (sizeof(data)==1) \
				{ \
					OF = ismultiplication?(!multiplication_is_safe(data,addition,8)):(!addition_is_safe(data,addition,8)); \
				} \
				else if (sizeof(data)==2) \
				{ \
					OF = ismultiplication?(!multiplication_is_safe(data,addition,16)):(!addition_is_safe(data,addition,16)); \
				} \
				else if (sizeof(data)==4) \
				{ \
					OF = ismultiplication?(!multiplication_is_safe(data,addition,32)):(!addition_is_safe(data,addition,32)); \
				}
*/

//First for parity calculations:
#define PARITY8(b) ((b&0x01)^((b&0x02)>>1)^((b&0x04)>>2)^((b&0x08)>>3)^((b&0x10)>>4)^((b&0x20)>>5)^((b&0x40)>>6)^((b&0x80)>>7))
#define PARITY16(w) (PARITY8((w>>8)&0xFF)^PARITY8(w&0xFF))
#define PARITY32(w) (PARITY16((w>>16)&0xFFFF)^PARITY16(w&0xFFFF))

#endif