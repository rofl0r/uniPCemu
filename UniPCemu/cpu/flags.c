#include "headers/types.h" //Basic types!
byte parity[0x100] = { //All parity values!
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};

#include "headers/cpu/cpu.h" //CPU!
#include "headers/cpu/easyregs.h" //EASY Regs!

//Sign and parity logic

void flag_p8(uint8_t value)
{
	FLAGW_PF(parity[value]);	
}

void flag_p16(uint16_t value)
{
	FLAGW_PF(parity[value&0xFF]);	
}

void flag_p32(uint32_t value)
{
	FLAGW_PF(parity[value&0xFF]);	
}

void flag_s8(uint8_t value)
{
	if (value & 0x80) FLAGW_SF(1);
	else FLAGW_SF(0);
}

void flag_s16(uint16_t value)
{
	if (value & 0x8000) FLAGW_SF(1);
	else FLAGW_SF(0);
}

void flag_s32(uint32_t value)
{
	if (value & 0x80000000) FLAGW_SF(1);
	else FLAGW_SF(0);
}

//Sign, Zero and Parity logic

void flag_szp8(uint8_t value)
{
	if (!value) FLAGW_ZF(1);
	else FLAGW_ZF(0);
	if (value & 0x80) FLAGW_SF(1);
	else FLAGW_SF(0);
	FLAGW_PF(parity[value]);
}

void flag_szp16(uint16_t value)
{
	if (!value) FLAGW_ZF(1);
	else FLAGW_ZF(0);
	if (value & 0x8000) FLAGW_SF(1);
	else FLAGW_SF(0);
	FLAGW_PF(parity[value & 255]);
}

void flag_szp32(uint32_t value)
{
	if (!value) FLAGW_ZF(1);
	else FLAGW_ZF(0);
	if (value & 0x80000000) FLAGW_SF(1);
	else FLAGW_SF(0);
	FLAGW_PF(parity[value & 255]);
}

//Logarithmic logic

void flag_log8(uint8_t value)
{
	flag_szp8(value);
	FLAGW_CF(0);
	FLAGW_OF(0);
}

void flag_log16(uint16_t value)
{
	flag_szp16(value);
	FLAGW_CF(0);
	FLAGW_OF(0);
}

void flag_log32(uint32_t value)
{
	flag_szp32(value);
	FLAGW_CF(0);
	FLAGW_OF(0);
}

//Negate a variable!
#define NEG(x) ((~(x))+1)

//Addition Carry, Overflow, Adjust logic
//Tables based on http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
//Index Bit0=sumsign, Bit1=num2sign(add or sub negated value), Bit2=num1sign(v1)
byte addoverflow[8] = {0,1,0,0,0,0,1,0};
byte suboverflow[8] = {0,0,0,1,1,0,0,0};

//Flags(Aux/Carry):
//ADD: ((op1)^(op2))^(((op1)^(result))&~((op1)^(op2)))^result
//SUB: ((op1)^(op2))^(((op1)^(result))&((op1)^(op2)))^result
//Auxiliary flag: ((op1^op2)^((op1^result)&(~(op1^op2))^result
//Auxiliary flag: ((op1)^(op2))^(((op1)^(result))&~((op1)^(op2)))^result
//borrow/carry bits
#define bcbitsa(v1,v2) (((v1)^(v2))^(((v1)^(dst))&~((v1)^(v2)))^dst)
#define bcbitss(v1,v2) (((v1)^(v2))^(((v1)^(dst))&((v1)^(v2)))^dst)

void flag_adcoa8(uint8_t v1, uint16_t add, uint16_t dst)
{
	uint16_t bba = bcbitsa((uint16_t)v1,add); //Get carry!
	FLAGW_CF((bba>>8)&1); //Carry?
	FLAGW_OF(addoverflow[((dst>>7)&1)|(((add>>6)&2))|((v1>>5)&4)]); //Overflow?
	FLAGW_AF((bba&0x10)>>4); //Adjust?
}

void flag_adcoa16(uint16_t v1, uint32_t add, uint32_t dst)
{
	uint32_t bba = bcbitsa((uint32_t)v1,add); //Get carry!
	FLAGW_CF((bba>>16)&1); //Carry?
	FLAGW_OF(addoverflow[((dst>>15)&1)|(((add>>14)&2))|((v1>>13)&4)]); //Overflow?
	FLAGW_AF((bba&0x10)>>4); //Adjust?
}

void flag_adcoa32(uint32_t v1, uint64_t add, uint64_t dst)
{
	uint64_t bba = bcbitsa((uint64_t)v1,add); //Get carry!
	FLAGW_CF((bba>>32)&1); //Carry?
	FLAGW_OF(addoverflow[((dst>>31)&1)|(((add>>30)&2))|((v1>>29)&4)]); //Overflow?
	FLAGW_AF((bba&0x10)>>4); //Adjust?
}

//Substract Carry, Overflow, Adjust logic
void flag_subcoa8(uint8_t v1, uint16_t sub, uint16_t dst)
{
	uint16_t bbs = bcbitss((uint16_t)v1,sub); //Get carry!
	FLAGW_CF((bbs>>8)&1); //Carry?
	FLAGW_OF(suboverflow[((dst>>7)&1)|((sub>>6)&2)|((v1>>5)&4)]); //Overflow?
	FLAGW_AF((bbs&0x10)>>4); //Adjust?
}

void flag_subcoa16(uint16_t v1, uint32_t sub, uint32_t dst)
{
	uint32_t bbs = bcbitss((uint32_t)v1,sub); //Get carry!
	FLAGW_CF((bbs>>16)&1); //Carry?
	FLAGW_OF(suboverflow[((dst>>15)&1)|((sub>>14)&2)|((v1>>13)&4)]); //Overflow?
	FLAGW_AF((bbs&0x10)>>4); //Adjust?
}

void flag_subcoa32(uint32_t v1, uint64_t sub, uint64_t dst)
{
	uint64_t bbs = bcbitss((uint64_t)v1,sub); //Get carry!
	FLAGW_CF((bbs>>32)&1); //Carry?
	FLAGW_OF(suboverflow[((dst>>31)&1)|((sub>>30)&2)|((v1>>29)&4)]); //Overflow?
	FLAGW_AF((bbs&0x10)>>4); //Adjust?
}

//Start of the externally used calls to calculate flags!

void flag_adc8(uint8_t v1, uint8_t v2, uint8_t v3)
{
	uint16_t dst;
	uint16_t add=(uint16_t)v2 + (uint16_t)v3;
	dst = (uint16_t)v1 + add;
	flag_szp8((uint8_t)(dst&0xFF));
	flag_adcoa8(v1,add,dst);
}

void flag_adc16(uint16_t v1, uint16_t v2, uint16_t v3)
{
	uint32_t dst;
	uint32_t add=(uint32_t)v2 + (uint32_t)v3;
	dst = (uint32_t)v1 + add;
	flag_szp16(dst);
	flag_adcoa16(v1,add,dst);
}

void flag_adc32(uint32_t v1, uint32_t v2, uint32_t v3)
{
	uint64_t dst;
	uint64_t add=(uint64_t)v2 + (uint64_t)v3;
	dst = (uint64_t)v1 + add;
	flag_szp32((uint32_t)dst);
	flag_adcoa32(v1,add,dst);
}

void flag_add8(uint8_t v1, uint8_t v2)
{
	uint16_t dst;
	dst = (uint16_t)v1 + (uint16_t)v2;
	flag_szp8((uint8_t)(dst&0xFF));
	flag_adcoa8(v1,v2,dst);
}

void flag_add16(uint16_t v1, uint16_t v2)
{
	uint32_t dst;
	dst = (uint32_t)v1 + (uint32_t)v2;
	flag_szp16(dst);
	flag_adcoa16(v1,v2,dst);
}

void flag_add32(uint32_t v1, uint32_t v2)
{
	uint64_t dst;
	dst = (uint64_t)v1 + (uint64_t)v2;
	flag_szp32((uint32_t)dst);
	flag_adcoa32(v1,v2,dst);
}

void flag_sbb8(uint8_t v1, uint8_t v2, uint8_t v3)
{
	uint16_t dst,sub;
	sub = (uint16_t)v2+(uint16_t)v3;
	dst = (uint16_t)v1 - sub;
	flag_szp8(dst & 0xFF);
	flag_subcoa8(v1,sub,dst);
}

void flag_sbb16(uint16_t v1, uint16_t v2, uint16_t v3)
{
	uint32_t dst,sub;
	sub = (uint32_t)v2+(uint32_t)v3;
	dst = (uint32_t)v1 - sub;
	flag_szp16(dst & 0xFFFF);
	flag_subcoa16(v1,sub,dst);
}

void flag_sbb32(uint32_t v1, uint32_t v2, uint32_t v3)
{
	uint64_t dst, sub;
	sub = (uint64_t)v2+(uint64_t)v3;
	dst = (uint64_t)v1 - sub;
	flag_szp32(dst & 0xFFFFFFFF);
	flag_subcoa32(v1,sub,dst);
}

void flag_sub8(uint8_t v1, uint8_t v2)
{
	uint16_t dst,sub;
	sub = (uint16_t)v2;
	dst = (uint16_t)v1 - sub;
	flag_szp8(dst&0xFF);
	flag_subcoa8(v1,sub,dst);
}

void flag_sub16(uint16_t v1, uint16_t v2)
{
	uint32_t dst,sub;
	sub = (uint32_t)v2;
	dst = (uint32_t)v1 - sub;
	flag_szp16(dst & 0xFFFF);
	flag_subcoa16(v1,sub,dst);
}

void flag_sub32(uint32_t v1, uint32_t v2)
{
	uint64_t dst,sub;
	sub = (uint64_t)v2;
	dst = (uint64_t)v1 - sub;
	flag_szp32(dst & 0xFFFFFFFF);
	flag_subcoa32(v1,sub,dst);
}
