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

void flag_adc8(uint8_t v1, uint8_t v2, uint8_t v3)
{
	int16_t dst,add=(int16_t)v2 + (int16_t)v3;
	dst = (int16_t)v1 + add;
	flag_szp8((uint8_t)(dst&0xFF));
	if (((dst ^ v1) & (dst ^ add) & 0x80) == 0x80) FLAGW_OF(1);
	else FLAGW_OF(0);
	FLAGW_CF((((v1^add^dst) & 0x100)>>8)); //Set the carry flag accordingly!
	if (((v1 ^ add ^ dst) & 0x10) == 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}

void flag_adc16(uint16_t v1, uint16_t v2, uint16_t v3)
{
	int32_t dst,add=(int32_t)v2 + (int32_t)v3;
	dst = (int32_t)v1 + add;
	flag_szp16(dst);
	if (((dst ^ v1) & (dst ^ add) & 0x8000) == 0x8000) FLAGW_OF(1);
	else FLAGW_OF(0);
	FLAGW_CF((((v1^add^dst) & 0x10000)>>16)); //Set the carry flag accordingly!
	if (((v1 ^ add ^ dst) & 0x10) == 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}

void flag_adc32(uint32_t v1, uint32_t v2, uint32_t v3)
{
	int64_t dst, add=(int64_t)v2 + (int64_t)v3;
	dst = (int64_t)v1 + add;
	flag_szp32((uint32_t)dst);
	if (((dst ^ v1) & (dst ^ add) & 0x80000000) == 0x80000000) FLAGW_OF(1);
	else FLAGW_OF(0);
	FLAGW_CF((((v1^add^dst) & 0x100000000ULL)>>32)); //Set the carry flag accordingly!
	if (((v1 ^ add ^ dst) & 0x10) == 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}

void flag_add8(uint8_t v1, uint8_t v2)
{
	int16_t dst;
	dst = (int16_t)v1 + (int16_t)v2;
	flag_szp8((uint8_t)(dst&0xFF));
	if ((v1^v2^dst) & 0x100) FLAGW_CF(1);
	else FLAGW_CF(0);
	if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) FLAGW_OF(1);
	else FLAGW_OF(0);
	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}

void flag_add16(uint16_t v1, uint16_t v2)
{
	int32_t dst;
	dst = (int32_t)v1 + (int32_t)v2;
	flag_szp16(dst);
	if ((v1^v2^dst) & 0x10000) FLAGW_CF(1);
	else FLAGW_CF(0);
	if (((dst ^ v1) & (dst ^ v2) & 0x8000) == 0x8000) FLAGW_OF(1);
	else FLAGW_OF(0);
	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}

void flag_add32(uint32_t v1, uint32_t v2)
{
	int64_t dst;
	dst = (int64_t)v1 + (int64_t)v2;
	flag_szp32((uint32_t)dst);
	if ((v1^v2^dst) & 0x100000000ULL) FLAGW_CF(1);
	else FLAGW_CF(0);
	if (((dst ^ v1) & (dst ^ v2) & 0x80000000) == 0x80000000) FLAGW_OF(1);
	else FLAGW_OF(0);
	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}

void flag_sbb8(uint8_t v1, uint8_t v2, uint8_t v3)
{
	int16_t dst,sub;
	sub = v2;
	sub += v3;
	dst = (int16_t)v1 - (int16_t)sub;
	flag_szp8(dst & 0xFF);
	FLAGW_CF((((v1^sub^dst) & 0x100)>>8));
	if ((dst ^ v1) & (v1 ^ sub) & 0x80) FLAGW_OF(1);
	else FLAGW_OF(0);
	if ((v1 ^ sub ^ dst) & 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}

void flag_sbb16(uint16_t v1, uint16_t v2, uint16_t v3)
{
	int32_t dst,sub;
	sub = v2;
	sub += v3;
	dst = (int32_t)v1 - (int32_t)sub;
	flag_szp16(dst & 0xFFFF);
	FLAGW_CF((((v1^sub^dst) & 0x10000)>>16));
	if ((dst ^ v1) & (v1 ^ sub) & 0x8000) FLAGW_OF(1);
	else FLAGW_OF(0);
	if ((v1 ^ sub ^ dst) & 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}

void flag_sbb32(uint32_t v1, uint32_t v2, uint32_t v3)
{
	int64_t dst, sub;
	sub = v2;
	sub += v3;
	dst = (int64_t)v1 - (int64_t)sub;
	flag_szp32(dst & 0xFFFFFFFF);
	FLAGW_CF((((v1^sub^dst) & 0x100000000ULL)>>32));
	if ((dst ^ v1) & (v1 ^ sub) & 0x80000000) FLAGW_OF(1);
	else FLAGW_OF(0);
	if ((v1 ^ sub ^ dst) & 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}

void flag_sub8(uint8_t v1, uint8_t v2)
{
	int16_t dst;
	dst = (int16_t)v1 - (int16_t)v2;
	flag_szp8(dst & 0xFF);
	FLAGW_CF((((v1^v2^dst) & 0x100)>>8)); //Carry?
	if ((dst ^ v1) & (v1 ^ v2) & 0x80) FLAGW_OF(1);
	else FLAGW_OF(0);
	if ((v1 ^ v2 ^ dst) & 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}

void flag_sub16(uint16_t v1, uint16_t v2)
{
	int32_t dst;
	dst = (int32_t)v1 - (int32_t)v2;
	flag_szp16(dst & 0xFFFF);
	FLAGW_CF((((v1^v2^dst) & 0x10000)>>16)); //Carry?
	if ((dst ^ v1) & (v1 ^ v2) & 0x8000) FLAGW_OF(1);
	else FLAGW_OF(0);
	if ((v1 ^ v2 ^ dst) & 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}

void flag_sub32(uint32_t v1, uint32_t v2)
{
	int64_t dst;
	dst = (int64_t)v1 - (int64_t)v2;
	flag_szp32(dst & 0xFFFFFFFF);
	FLAGW_CF((((v1^v2^dst) & 0x100000000ULL)>>32)); //Carry?
	if ((dst ^ v1) & (v1 ^ v2) & 0x80000000) FLAGW_OF(1);
	else FLAGW_OF(0);
	if ((v1 ^ v2 ^ dst) & 0x10) FLAGW_AF(1);
	else FLAGW_AF(0);
}
