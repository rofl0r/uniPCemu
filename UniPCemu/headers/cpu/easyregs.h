#ifndef CPU_EASYREGS_H
#define CPU_EASYREGS_H

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

//Flags(read version default)
#define FLAG_AC FLAGREGR_AC(CPU[activeCPU].registers)
#define FLAG_V8 FLAGREGR_V8(CPU[activeCPU].registers)
#define FLAG_RF FLAGREGR_RF(CPU[activeCPU].registers)
#define FLAG_NT FLAGREGR_NT(CPU[activeCPU].registers)
#define FLAG_PL FLAGREGR_IOPL(CPU[activeCPU].registers)
#define FLAG_OF FLAGREGR_OF(CPU[activeCPU].registers)
#define FLAG_DF FLAGREGR_DF(CPU[activeCPU].registers)
#define FLAG_IF FLAGREGR_IF(CPU[activeCPU].registers)
#define FLAG_TF FLAGREGR_TF(CPU[activeCPU].registers)
#define FLAG_SF FLAGREGR_SF(CPU[activeCPU].registers)
#define FLAG_ZF FLAGREGR_ZF(CPU[activeCPU].registers)
#define FLAG_AF FLAGREGR_AF(CPU[activeCPU].registers)
#define FLAG_PF FLAGREGR_PF(CPU[activeCPU].registers)
#define FLAG_CF FLAGREGR_CF(CPU[activeCPU].registers)
#define FLAG_VIF FLAGREGR_VIF(CPU[activeCPU].registers)
#define FLAG_VIP FLAGREGR_VIP(CPU[activeCPU].registers)

//Flags(write version default)
#define FLAGW_AC(val) FLAGREGW_AC(CPU[activeCPU].registers,val)
#define FLAGW_V8(val) FLAGREGW_V8(CPU[activeCPU].registers,val)
#define FLAGW_RF(val) FLAGREGW_RF(CPU[activeCPU].registers,val)
#define FLAGW_NT(val) FLAGREGW_NT(CPU[activeCPU].registers,val)
#define FLAGW_PL(val) FLAGREGW_IOPL(CPU[activeCPU].registers,val)
#define FLAGW_OF(val) FLAGREGW_OF(CPU[activeCPU].registers,val)
#define FLAGW_DF(val) FLAGREGW_DF(CPU[activeCPU].registers,val)
#define FLAGW_IF(val) FLAGREGW_IF(CPU[activeCPU].registers,val)
#define FLAGW_TF(val) FLAGREGW_TF(CPU[activeCPU].registers,val)
#define FLAGW_SF(val) FLAGREGW_SF(CPU[activeCPU].registers,val)
#define FLAGW_ZF(val) FLAGREGW_ZF(CPU[activeCPU].registers,val)
#define FLAGW_AF(val) FLAGREGW_AF(CPU[activeCPU].registers,val)
#define FLAGW_PF(val) FLAGREGW_PF(CPU[activeCPU].registers,val)
#define FLAGW_CF(val) FLAGREGW_CF(CPU[activeCPU].registers,val)
#define FLAGW_VIF(val) FLAGREGW_VIF(CPU[activeCPU].registers,val)
#define FLAGW_VIP(val) FLAGREGW_VIP(CPU[activeCPU].registers,val)

#endif
