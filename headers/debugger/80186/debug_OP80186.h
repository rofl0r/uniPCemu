#ifndef CPU_DEBUG80186_H
#define CPU_DEBUG80186_H

//80186 processor used opcodes:

void DEBUG186_OP60(); //PUSHA
void DEBUG186_OP61(); //POPA
void DEBUG186_OP62(); //BOUND Gv,Ma
void DEBUG186_OP68(); //PUSH Iz
void DEBUG186_OP69(); //IMUL Gv,Ev,Iz
void DEBUG186_OP6A(); //PUSH Ib
void DEBUG186_OP6B(); //IMUL Gv,Ev,Ib
void DEBUG186_OP6C(); //INS Yb,REG_DX
void DEBUG186_OP6D(); //INS Yz,REG_DX
void DEBUG186_OP6E(); //OUTS REG_DX,Xb
void DEBUG186_OP6F(); //OUTS REG_DX,Xz
void DEBUG186_OPC8(); //ENTER Iw,Ib
void DEBUG186_OPC9(); //LEAVE
void DEBUG186_OPC0(); //GRP2 Eb,Ib
void DEBUG186_OPC1(); //GRP2 Ev,Ib


#endif