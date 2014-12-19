#include "headers/cpu/cpu.h" //CPU!
#include "headers/cpu/easyregs.h" //EASY Regs!

uint8_t parity8[0x100] = {
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};

void flag_szp8 (uint8_t value) {
	ZF = (value==0);
	SF = ((value&0x80)>0);
	PF = parity8[value]; /* retrieve parity state from lookup table */
}

void flag_szp16 (uint16_t value) {
	ZF = (value==0);
	SF = ((value&0x8000)>0);
	PF = parity8[value & 0xFF];	/* retrieve parity state from lookup table */
}

void flag_log8 (uint8_t value) {
	flag_szp8 (value);
	CF = 0;
	OF = 0; /* bitwise logic ops always clear carry and overflow */
}

void flag_log16 (uint16_t value) {
	flag_szp16 (value);
	CF = 0;
	OF = 0; /* bitwise logic ops always clear carry and overflow */
}

void flag_adc8 (uint8_t v1, uint8_t v2, uint8_t v3) {
	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint16_t	dst;
	dst = (uint16_t) v1 + (uint16_t) v2 + (uint16_t) v3;
	flag_szp8 ( (uint8_t) dst);
	OF = ( ( (dst ^ v1) & (dst ^ v2) & 0x80) == 0x80);
	CF = ((dst&0xFF00)>0);
	AF = ( ( (v1 ^ v2 ^ dst) & 0x10) == 0x10);
}

void flag_adc16 (uint16_t v1, uint16_t v2, uint16_t v3) {

	uint32_t	dst;
	dst = (uint32_t) v1 + (uint32_t) v2 + (uint32_t) v3;
	flag_szp16 ( (uint16_t) dst);
	OF = ( ( ( (dst ^ v1) & (dst ^ v2) ) & 0x8000) == 0x8000);
	CF = ((dst & 0xFFFF0000)>0);
	AF = ( ( (v1 ^ v2 ^ dst) & 0x10) == 0x10);
}

void flag_add8 (uint8_t v1, uint8_t v2) {
	/* v1 = destination operand, v2 = source operand */
	uint16_t	dst;
	dst = (uint16_t) v1 + (uint16_t) v2;
	flag_szp8 ( (uint8_t) dst);
	CF = ((dst & 0xFF00)>0);
	OF = ( ( (dst ^ v1) & (dst ^ v2) & 0x80) == 0x80);
	AF = ( ( (v1 ^ v2 ^ dst) & 0x10) == 0x10);
}

void flag_add16 (uint16_t v1, uint16_t v2) {
	/* v1 = destination operand, v2 = source operand */
	uint32_t	dst;
	dst = (uint32_t) v1 + (uint32_t) v2;
	flag_szp16 ( (uint16_t) dst);
	CF = ((dst & 0xFFFF0000)>0);
	OF = ( ( (dst ^ v1) & (dst ^ v2) & 0x8000) == 0x8000);
	AF = ( ( (v1 ^ v2 ^ dst) & 0x10) == 0x10);
}

void flag_sbb8 (uint8_t v1, uint8_t v2, uint8_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint16_t	dst;
	v2 += v3;
	dst = (uint16_t) v1 - (uint16_t) v2;
	flag_szp8 ( (uint8_t) dst);
	CF = ((dst & 0xFF00)>0);
	OF = (( (dst ^ v1) & (v1 ^ v2) & 0x80)>0);
	AF = (( (v1 ^ v2 ^ dst) & 0x10)>0);
}

void flag_sbb16 (uint16_t v1, uint16_t v2, uint16_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint32_t	dst;
	v2 += v3;
	dst = (uint32_t) v1 - (uint32_t) v2;
	flag_szp16 ( (uint16_t) dst);
	CF = ((dst & 0xFFFF0000)>0);
	OF = (( (dst ^ v1) & (v1 ^ v2) & 0x8000)>0);
	AF = (( (v1 ^ v2 ^ dst) & 0x10)>0);
}

void flag_sub8 (uint8_t v1, uint8_t v2) {

	/* v1 = destination operand, v2 = source operand */
	uint16_t	dst;
	dst = (uint16_t) v1 - (uint16_t) v2;
	flag_szp8 ( (uint8_t) dst);
	CF = ((dst & 0xFF00)>0);
	OF = (( (dst ^ v1) & (v1 ^ v2) & 0x80)>0);
	AF = (( (v1 ^ v2 ^ dst) & 0x10)>0);
}

void flag_sub16 (uint16_t v1, uint16_t v2) {

	/* v1 = destination operand, v2 = source operand */
	uint32_t	dst;
	dst = (uint32_t) v1 - (uint32_t) v2;
	flag_szp16 ( (uint16_t) dst);
	CF = ((dst & 0xFFFF0000)>0);
	OF = (( (dst ^ v1) & (v1 ^ v2) & 0x8000)>0);
	AF = (( (v1 ^ v2 ^ dst) & 0x10)>0);
}