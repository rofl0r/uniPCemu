#ifndef CPU_OPNECV30
#define CPU_OPNECV30
//Newer 8086+ opcodes!
void CPU186_OP37();
void CPU186_OP3F();

//New 80186+ opcodes!
void CPU186_OP60();//PUSHA
void CPU186_OP61(); //POPA
void CPU186_OP62(); //BOUND Gv,Ma
void CPU186_OP68(); //PUSH Iz
void CPU186_OP69(); //IMUL Gv,Ev,Iz
void CPU186_OP6A(); //PUSH Ib
void CPU186_OP6B(); //IMUL Gv,Ev,Ib
void CPU186_OP6C(); //INS Yb,DX
void CPU186_OP6D(); //INS Yz,DX
void CPU186_OP6E(); //OUTS DX,Xb
void CPU186_OP6F(); //OUTS DX,Xz
void CPU186_OP8E(); //MOV segreg,reg
void CPU186_OPC0(); //GRP2 Eb,Ib
void CPU186_OPC1(); //GRP2 Ev,Ib
void CPU186_OPC8(); //ENTER Iw,Ib
void CPU186_OPC9(); //LEAVE

void unkOP_186(); //Unknown opcode on 186+?

#endif