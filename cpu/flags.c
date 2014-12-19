#include "headers/cpu/cpu.h" //CPU!
#include "headers/cpu/easyregs.h" //EASY Regs!

byte parity16[0xFF]; //Parity lookup!

void flag_szp8(uint8_t value)
{
	if (!value) ZF = 1;
	else ZF = 0;
	if (value & 0x80) SF = 1;
	else SF = 0;
	PF = parity16[value];
}

void flag_szp16(uint16_t value)
{
	if (!value) ZF = 1;
	else ZF = 0;
	if (value & 0x8000) SF = 1;
	else SF = 0;
	PF = parity16[value & 255];
}

void flag_log8(uint8_t value)
{
	flag_szp8(value);
	CF = 0;
	OF = 0;
}

void flag_log16(uint16_t value)
{
	flag_szp16(value);
	CF = 0;
	OF = 0;
}

void flag_adc8(uint8_t v1, uint8_t v2, uint8_t v3)
{
	int16_t dst;
	dst = (int16_t)v1 + (int16_t)v2 + (int16_t)v3;
	flag_szp8(dst);
	if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) OF = 1;
	else OF = 0;
	if ((((dst & 0xFF) < (v1 & 0xFF)) || (((CF & (((v1 & 0xFF) + (v2 & 0xFF) + (v3 & 0xFF)) & 0xFF)) == (v1 & 0xFF)))))
		CF = 1;
	else CF = 0;
	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) AF = 1;
	else AF = 0;
}

void flag_adc16(uint16_t v1, uint16_t v2, uint16_t v3)
{
	int32_t dst;
	dst = (int32_t)v1 + (int32_t)v2 + (int32_t)v3;
	flag_szp16(dst);
	if (((dst ^ v1) & (dst ^ v2) & 0x8000) == 0x8000) OF = 1;
	else OF = 0;
	if ((((dst & 0xFFFF) < (v1 & 0xFFFF)) || (((CF & (((v1 & 0xFFFF) + (v2 & 0xFFFF) + (v3 & 0xFFFF)) & 0xFFFF)) == (v1 & 0xFFFF)))))
		CF = 1;
	else CF = 0;
	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) AF = 1;
	else AF = 0;
}

void flag_add8(uint8_t v1, uint8_t v2)
{
	int16_t dst;
	dst = (int16_t)v1 + (int16_t)v2;
	flag_szp8(dst);
	if (dst & 0xFF00) CF = 1;
	else CF = 0;
	if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) OF = 1;
	else OF = 0;
	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) AF = 1;
	else AF = 0;
}

void flag_add16(uint16_t v1, uint16_t v2)
{
	int32_t dst;
	dst = (int32_t)v1 + (int32_t)v2;
	flag_szp16(dst);
	if (dst & 0xFFFF0000) CF = 1;
	else CF = 0;
	if (((dst ^ v1) & (dst ^ v2) & 0x8000) == 0x8000) OF = 1;
	else OF = 0;
	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) AF = 1;
	else AF = 0;
}

void flag_sbb8(uint8_t v1, uint8_t v2, uint8_t v3)
{
	int16_t dst;
	v2 += v3;
	dst = (int16_t)v1 - (int16_t)v2;
	flag_szp8(dst & 0xFF);
	if (v1 < v2) CF = 1;
	else CF = 0;
	if ((dst ^ v1) & (v1 ^ v2) & 0x80) OF = 1;
	else OF = 0;
	if ((v1 ^ v2 ^ dst) & 0x10) AF = 1;
	else AF = 0;
}

void flag_sbb16(uint16_t v1, uint16_t v2, uint16_t v3)
{
	int32_t dst;
	v2 += v3;
	dst = (int32_t)v1 - (int32_t)v2;
	flag_szp16(dst & 0xFFFF);
	if (v1 < v2) CF = 1;
	else CF = 0;
	if ((dst ^ v1) & (v1 ^ v2) & 0x8000) OF = 1;
	else OF = 0;
	if ((v1 ^ v2 ^ dst) & 0x10) AF = 1;
	else AF = 0;
}

void flag_sub8(uint8_t v1, uint8_t v2)
{
	int16_t dst;
	dst = (int16_t)v1 - (int16_t)v2;
	flag_szp8(dst & 0xFF);
	if (v1 < v2) CF = 1;
	else CF = 0;
	if ((dst ^ v1) & (v1 ^ v2) & 0x80) OF = 1;
	else OF = 0;
	if ((v1 ^ v2 ^ dst) & 0x10) AF = 1;
	else AF = 0;
}

void flag_sub16(uint16_t v1, uint16_t v2)
{
	int32_t dst;
	dst = (int32_t)v1 - (int32_t)v2;
	flag_szp16(dst & 0xFFFF);
	if (v1 < v2) CF = 1;
	else CF = 0;
	if ((dst ^ v1) & (v1 ^ v2) & 0x8000) OF = 1;
	else OF = 0;
	if ((v1 ^ v2 ^ dst) & 0x10) AF = 1;
	else AF = 0;
}
