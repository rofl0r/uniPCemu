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

//Addition Carry, Overflow, Adjust logic
//Tables based on http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
//Index Bit0=sumsign, Bit1=num2sign(add or sub negated value), Bit2=num1sign(v1)
byte addoverflow[8] = {0,1,0,0,0,0,1,0};
byte suboverflow[8] = {0,0,0,1,1,0,0,0};

byte addcarry[8] = {0,0,1,0,1,0,0,1};
byte auxaddcarry[8] = {0,0,1,0,1,0,0,1};
#define OVERFLOW_DSTMASK 1
#define OVERFLOW_NUM2MASK 2
#define OVERFLOW_NUM1MASK 4
#define OVERFLOW8_DST 7
#define OVERFLOW8_NUM2 6
#define OVERFLOW8_NUM1 5
#define OVERFLOW16_DST 15
#define OVERFLOW16_NUM2 14
#define OVERFLOW16_NUM1 13
#define OVERFLOW32_DST 31
#define OVERFLOW32_NUM2 30
#define OVERFLOW32_NUM1 29

#define CARRY_DSTMASK 1
#define CARRY_NUM2MASK 2
#define CARRY_NUM1MASK 4
#define CARRY8_DST 7
#define CARRY8_NUM2 6
#define CARRY8_NUM1 5
#define CARRY16_DST 15
#define CARRY16_NUM2 14
#define CARRY16_NUM1 13
#define CARRY32_DST 31
#define CARRY32_NUM2 30
#define CARRY32_NUM1 29

#define AUXCARRY_DSTMASK 1
#define AUXCARRY_NUM2MASK 2
#define AUXCARRY_NUM1MASK 4
#define AUXCARRY8_DST 7
#define AUXCARRY8_NUM2 6
#define AUXCARRY8_NUM1 5
#define AUXCARRY16_DST 15
#define AUXCARRY16_NUM2 14
#define AUXCARRY16_NUM1 13
#define AUXCARRY32_DST 31
#define AUXCARRY32_NUM2 30
#define AUXCARRY32_NUM1 29

//#define bcbitsxs bcbitss
//What bits in the bcbits are the bit to give?

#define NEG(x) ((~x)+1)

//General macros defining add/sub carry!
#define CARRYA8(v1,add,dst) (addcarry[((dst>>CARRY8_DST)&CARRY_DSTMASK)|(((add>>CARRY8_NUM2)&CARRY_NUM2MASK))|((v1>>CARRY8_NUM1)&CARRY_NUM1MASK)])
#define CARRYA16(v1,add,dst) (addcarry[((dst>>CARRY16_DST)&CARRY_DSTMASK)|(((add>>CARRY16_NUM2)&CARRY_NUM2MASK))|((v1>>CARRY16_NUM1)&CARRY_NUM1MASK)])
#define CARRYA32(v1,add,dst) (addcarry[((dst>>CARRY32_DST)&CARRY_DSTMASK)|(((add>>CARRY32_NUM2)&CARRY_NUM2MASK))|((v1>>CARRY32_NUM1)&CARRY_NUM1MASK)])
#define CARRYS8(v1,sub,dst) (CARRYA8(v1,NEG(sub),dst)^1)
#define CARRYS16(v1,sub,dst) (CARRYA16(v1,NEG(sub),dst)^1)
#define CARRYS32(v1,sub,dst) (CARRYA32(v1,NEG(sub),dst)^1)
//Aux variants:
#define AUXA8(v1,add,dst) (auxaddcarry[((dst>>AUXCARRY8_DST)&AUXCARRY_DSTMASK)|(((add>>AUXCARRY8_NUM2)&AUXCARRY_NUM2MASK))|((v1>>AUXCARRY8_NUM1)&AUXCARRY_NUM1MASK)])
#define AUXA16(v1,add,dst) (auxaddcarry[((dst>>AUXCARRY16_DST)&AUXCARRY_DSTMASK)|(((add>>AUXCARRY16_NUM2)&AUXCARRY_NUM2MASK))|((v1>>AUXCARRY16_NUM1)&AUXCARRY_NUM1MASK)])
#define AUXA32(v1,add,dst) (auxaddcarry[((dst>>AUXCARRY32_DST)&AUXCARRY_DSTMASK)|(((add>>AUXCARRY32_NUM2)&AUXCARRY_NUM2MASK))|((v1>>AUXCARRY32_NUM1)&AUXCARRY_NUM1MASK)])
#define AUXS8(v1,sub,dst) ((AUXA8(v1,NEG(sub),dst))^1)
#define AUXS16(v1,sub,dst) ((AUXA16(v1,NEG(sub),dst))^1)
#define AUXS32(v1,sub,dst) ((AUXA32(v1,NEG(sub),dst))^1)

void flag_adcoa8(uint8_t v1, uint16_t add, uint16_t dst)
{
	FLAGW_CF(CARRYA8(v1,add,dst)); //Carry?
	FLAGW_OF(addoverflow[((dst>>OVERFLOW8_DST)&OVERFLOW_DSTMASK)|(((add>>OVERFLOW8_NUM2)&OVERFLOW_NUM2MASK))|((v1>>OVERFLOW8_NUM1)&OVERFLOW_NUM1MASK)]); //Overflow?
	FLAGW_AF(AUXA8(v1,add,dst)); //Adjust?
}

void flag_adcoa16(uint16_t v1, uint32_t add, uint32_t dst)
{
	FLAGW_CF(CARRYA16(v1,add,dst)); //Carry?
	FLAGW_OF(addoverflow[((dst>>OVERFLOW16_DST)&OVERFLOW_DSTMASK)|(((add>>OVERFLOW16_NUM2)&OVERFLOW_NUM2MASK))|((v1>>OVERFLOW16_NUM1)&OVERFLOW_NUM1MASK)]); //Overflow?
	FLAGW_AF(AUXA16(v1,add,dst)); //Adjust?
}

void flag_adcoa32(uint32_t v1, uint64_t add, uint64_t dst)
{
	FLAGW_CF(CARRYA32(v1,add,dst)); //Carry?
	FLAGW_OF(addoverflow[((dst>>OVERFLOW32_DST)&OVERFLOW_DSTMASK)|(((add>>OVERFLOW32_NUM2)&OVERFLOW_NUM2MASK))|((v1>>OVERFLOW32_NUM1)&OVERFLOW_NUM1MASK)]); //Overflow?
	FLAGW_AF(AUXA32(v1,add,dst)); //Adjust?
}

//Substract Carry, Overflow, Adjust logic
void flag_subcoa8(uint8_t v1, uint16_t sub, uint16_t dst)
{
	FLAGW_CF(CARRYS8(v1,sub,dst)); //Carry?
	FLAGW_OF(suboverflow[((dst>>OVERFLOW8_DST)&OVERFLOW_DSTMASK)|((sub>>OVERFLOW8_NUM2)&OVERFLOW_NUM2MASK)|((v1>>OVERFLOW8_NUM1)&OVERFLOW_NUM1MASK)]); //Overflow?
	FLAGW_AF(AUXS8(v1,sub,dst)); //Adjust?
}

void flag_subcoa16(uint16_t v1, uint32_t sub, uint32_t dst)
{
	FLAGW_CF(CARRYS16(v1,sub,dst)); //Carry?
	FLAGW_OF(suboverflow[((dst>>OVERFLOW16_DST)&OVERFLOW_DSTMASK)|((sub>>OVERFLOW16_NUM2)&OVERFLOW_NUM2MASK)|((v1>>OVERFLOW16_NUM1)&OVERFLOW_NUM1MASK)]); //Overflow?
	FLAGW_AF(AUXS16(v1,sub,dst)); //Adjust?
}

void flag_subcoa32(uint32_t v1, uint64_t sub, uint64_t dst)
{
	FLAGW_CF(CARRYS32(v1,sub,dst)); //Carry?
	FLAGW_OF(suboverflow[((dst>>OVERFLOW32_DST)&OVERFLOW_DSTMASK)|((sub>>OVERFLOW32_NUM2)&OVERFLOW_NUM2MASK)|((v1>>OVERFLOW32_NUM1)&OVERFLOW_NUM1MASK)]); //Overflow?
	FLAGW_AF(AUXS32(v1,sub,dst)); //Adjust?
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
