/*
Fake86: A portable, open-source 8086 PC emulator.
Copyright (C)2010-2012 Mike Chambers

This program is free software; you can redistribute it and/or
modify it under the terms FLAG_OF the GNU General Public License
as published by the Free Software Foundation; either version 2
FLAG_OF the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty FLAG_OF
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy FLAG_OF the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* cpu.c: functions to emulate the 8086/V20 CPU in software. the heart FLAG_OF Fake86. */

#include "headers/types.h" //Basic typedefs!
#include "headers/cpu/cpu.h" //Basic CPU support!
#include "headers/cpu/easyregs.h" //Register support!

#define CPU_V20

//Headers from includes:

byte dummyregb;
word dummyregw;

byte *getbytereg(byte reg)
{
	switch (reg)
	{
	case 0:
		return &REG_AL;
	case 1:
		return &REG_CL;
	case 2:
		return &REG_DL;
	case 3:
		return &REG_BL;
	case 4:
		return &REG_AH;
	case 5:
		return &REG_CH;
	case 6:
		return &REG_DH;
	case 7:
		return &REG_BH;
	}
	return &dummyregb;
}

word *getwordreg(byte reg)
{
	switch (reg)
	{
	case 0:
		return &REG_AX;
	case 1:
		return &REG_CX;
	case 2:
		return &REG_DX;
	case 3:
		return &REG_BX;
	case 4:
		return &REG_SP;
	case 5:
		return &REG_BP;
	case 6:
		return &REG_SI;
	case 7:
		return &REG_DI;
	}
	return &dummyregw;
}

word *getsegreg2(reg)
{
	switch (reg)
	{
	case 0:
		return &REG_ES;
	case 1:
		return &REG_CS;
	case 2:
		return &REG_SS;
	case 3:
		return &REG_DS;
		break;
	default:
		break;
	}
	return &dummyregw;
}

#define StepIP(x)	REG_IP += x
#define getmem8(x, y)	read86(segbase(x) + y)
#define getmem16(x, y)	readw86(segbase(x) + y)
#define putmem8(x, y, z)	write86(segbase(x) + y, z)
#define putmem16(x, y, z)	writew86(segbase(x) + y, z)
#define getreg16(regid)	*getwordreg(regid)
#define getreg8(regid)	*getbytereg(regid)
#define putreg16(regid, writeval)	*getwordreg(regid) = writeval
#define putreg8(regid, writeval)	*getbytereg(regid) = writeval
#define getsegreg(regid)	*getsegreg2(regid)
#define putsegreg(regid, writeval)	*getsegreg2(regid) = writeval
#define segbase(x)	((uint32_t) x << 4)


//Content itself:


uint8_t	opcode, segoverride, reptype;
uint16_t savecs, saveip, useseg, oldsp, frametemp;
uint8_t	tempcf, oldcf, mode, reg, rm;
uint16_t oper1, oper2, res16, disp16, temp16, dummy, stacksize;
uint8_t	oper1b, oper2b, res8, disp8, temp8, nestlev, addrbyte;
uint32_t temp1, temp2, temp3, temp4, temp5, temp32, tempaddr32, ea;
int32_t	result;

uint8_t	running = 0;

void intcall86(uint8_t intnum);

#define makeflagsword() \
	( \
	2 | (uint16_t) FLAG_CF | ((uint16_t) FLAG_PF << 2) | ((uint16_t) FLAG_AF << 4) | ((uint16_t) FLAG_ZF << 6) | ((uint16_t) FLAG_SF << 7) | \
	((uint16_t) FLAG_TF << 8) | ((uint16_t) FLAG_IF << 9) | ((uint16_t) FLAG_DF << 10) | ((uint16_t) FLAG_OF << 11) \
	)

#define decodeflagsword(x) { \
	temp16 = x; \
	FLAG_CF = temp16 & 1; \
	FLAG_PF = (temp16 >> 2) & 1; \
	FLAG_AF = (temp16 >> 4) & 1; \
	FLAG_ZF = (temp16 >> 6) & 1; \
	FLAG_SF = (temp16 >> 7) & 1; \
	FLAG_TF = (temp16 >> 8) & 1; \
	FLAG_IF = (temp16 >> 9) & 1; \
	FLAG_DF = (temp16 >> 10) & 1; \
	FLAG_OF = (temp16 >> 11) & 1; \
	}

void write86(uint32_t addr32, uint8_t value) {
	MMU_directwb_realaddr(addr32, value); //Write using our method!
}

void writew86(uint32_t addr32, uint16_t value) {
	write86(addr32, (uint8_t)value);
	write86(addr32 + 1, (uint8_t)(value >> 8));
}

uint8_t read86(uint32_t addr32) {
	return MMU_directrb_realaddr(addr32,0); //Read directly!
}

uint16_t readw86(uint32_t addr32) {
	return ((uint16_t)read86(addr32) | (uint16_t)(read86(addr32 + 1) << 8));
}

void fakeflag_szp8(uint8_t value) {
	if (!value) {
		FLAG_ZF = 1;
	}
	else {
		FLAG_ZF = 0;	/* set or clear zero flag */
	}

	if (value & 0x80) {
		FLAG_SF = 1;
	}
	else {
		FLAG_SF = 0;	/* set or clear sign flag */
	}

	FLAG_PF = parity[value]; /* retrieve parity state from lookup table */
}

void fakeflag_szp16(uint16_t value) {
	if (!value) {
		FLAG_ZF = 1;
	}
	else {
		FLAG_ZF = 0;	/* set or clear zero flag */
	}

	if (value & 0x8000) {
		FLAG_SF = 1;
	}
	else {
		FLAG_SF = 0;	/* set or clear sign flag */
	}

	FLAG_PF = parity[value & 255];	/* retrieve parity state from lookup table */
}

void fakeflag_log8(uint8_t value) {
	fakeflag_szp8(value);
	FLAG_CF = 0;
	FLAG_OF = 0; /* bitwise logic ops always clear carry and overflow */
}

void fakeflag_log16(uint16_t value) {
	fakeflag_szp16(value);
	FLAG_CF = 0;
	FLAG_OF = 0; /* bitwise logic ops always clear carry and overflow */
}

void fakeflag_adc8(uint8_t v1, uint8_t v2, uint8_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint16_t	dst;

	dst = (uint16_t)v1 + (uint16_t)v2 + (uint16_t)v3;
	fakeflag_szp8((uint8_t)dst);
	if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) {
		FLAG_OF = 1;
	}
	else {
		FLAG_OF = 0; /* set or clear overflow flag */
	}

	if (dst & 0xFF00) {
		FLAG_CF = 1;
	}
	else {
		FLAG_CF = 0; /* set or clear carry flag */
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		FLAG_AF = 1;
	}
	else {
		FLAG_AF = 0; /* set or clear auxilliary flag */
	}
}

void fakeflag_adc16(uint16_t v1, uint16_t v2, uint16_t v3) {

	uint32_t	dst;

	dst = (uint32_t)v1 + (uint32_t)v2 + (uint32_t)v3;
	fakeflag_szp16((uint16_t)dst);
	if ((((dst ^ v1) & (dst ^ v2)) & 0x8000) == 0x8000) {
		FLAG_OF = 1;
	}
	else {
		FLAG_OF = 0;
	}

	if (dst & 0xFFFF0000) {
		FLAG_CF = 1;
	}
	else {
		FLAG_CF = 0;
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		FLAG_AF = 1;
	}
	else {
		FLAG_AF = 0;
	}
}

void fakeflag_add8(uint8_t v1, uint8_t v2) {
	/* v1 = destination operand, v2 = source operand */
	uint16_t	dst;

	dst = (uint16_t)v1 + (uint16_t)v2;
	fakeflag_szp8((uint8_t)dst);
	if (dst & 0xFF00) {
		FLAG_CF = 1;
	}
	else {
		FLAG_CF = 0;
	}

	if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) {
		FLAG_OF = 1;
	}
	else {
		FLAG_OF = 0;
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		FLAG_AF = 1;
	}
	else {
		FLAG_AF = 0;
	}
}

void fakeflag_add16(uint16_t v1, uint16_t v2) {
	/* v1 = destination operand, v2 = source operand */
	uint32_t	dst;

	dst = (uint32_t)v1 + (uint32_t)v2;
	fakeflag_szp16((uint16_t)dst);
	if (dst & 0xFFFF0000) {
		FLAG_CF = 1;
	}
	else {
		FLAG_CF = 0;
	}

	if (((dst ^ v1) & (dst ^ v2) & 0x8000) == 0x8000) {
		FLAG_OF = 1;
	}
	else {
		FLAG_OF = 0;
	}

	if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
		FLAG_AF = 1;
	}
	else {
		FLAG_AF = 0;
	}
}

void fakeflag_sbb8(uint8_t v1, uint8_t v2, uint8_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint16_t	dst;

	v2 += v3;
	dst = (uint16_t)v1 - (uint16_t)v2;
	fakeflag_szp8((uint8_t)dst);
	if (dst & 0xFF00) {
		FLAG_CF = 1;
	}
	else {
		FLAG_CF = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x80) {
		FLAG_OF = 1;
	}
	else {
		FLAG_OF = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		FLAG_AF = 1;
	}
	else {
		FLAG_AF = 0;
	}
}

void fakeflag_sbb16(uint16_t v1, uint16_t v2, uint16_t v3) {

	/* v1 = destination operand, v2 = source operand, v3 = carry flag */
	uint32_t	dst;

	v2 += v3;
	dst = (uint32_t)v1 - (uint32_t)v2;
	fakeflag_szp16((uint16_t)dst);
	if (dst & 0xFFFF0000) {
		FLAG_CF = 1;
	}
	else {
		FLAG_CF = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x8000) {
		FLAG_OF = 1;
	}
	else {
		FLAG_OF = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		FLAG_AF = 1;
	}
	else {
		FLAG_AF = 0;
	}
}

void fakeflag_sub8(uint8_t v1, uint8_t v2) {

	/* v1 = destination operand, v2 = source operand */
	uint16_t	dst;

	dst = (uint16_t)v1 - (uint16_t)v2;
	fakeflag_szp8((uint8_t)dst);
	if (dst & 0xFF00) {
		FLAG_CF = 1;
	}
	else {
		FLAG_CF = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x80) {
		FLAG_OF = 1;
	}
	else {
		FLAG_OF = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		FLAG_AF = 1;
	}
	else {
		FLAG_AF = 0;
	}
}

void fakeflag_sub16(uint16_t v1, uint16_t v2) {

	/* v1 = destination operand, v2 = source operand */
	uint32_t	dst;

	dst = (uint32_t)v1 - (uint32_t)v2;
	fakeflag_szp16((uint16_t)dst);
	if (dst & 0xFFFF0000) {
		FLAG_CF = 1;
	}
	else {
		FLAG_CF = 0;
	}

	if ((dst ^ v1) & (v1 ^ v2) & 0x8000) {
		FLAG_OF = 1;
	}
	else {
		FLAG_OF = 0;
	}

	if ((v1 ^ v2 ^ dst) & 0x10) {
		FLAG_AF = 1;
	}
	else {
		FLAG_AF = 0;
	}
}

void fakeop_adc8() {
	res8 = oper1b + oper2b + FLAG_CF;
	fakeflag_adc8(oper1b, oper2b, FLAG_CF);
}

void fakeop_adc16() {
	res16 = oper1 + oper2 + FLAG_CF;
	fakeflag_adc16(oper1, oper2, FLAG_CF);
}

void fakeop_add8() {
	res8 = oper1b + oper2b;
	fakeflag_add8(oper1b, oper2b);
}

void fakeop_add16() {
	res16 = oper1 + oper2;
	fakeflag_add16(oper1, oper2);
}

void fakeop_and8() {
	res8 = oper1b & oper2b;
	fakeflag_log8(res8);
}

void fakeop_and16() {
	res16 = oper1 & oper2;
	fakeflag_log16(res16);
}

void fakeop_or8() {
	res8 = oper1b | oper2b;
	fakeflag_log8(res8);
}

void fakeop_or16() {
	res16 = oper1 | oper2;
	fakeflag_log16(res16);
}

void fakeop_xor8() {
	res8 = oper1b ^ oper2b;
	fakeflag_log8(res8);
}

void fakeop_xor16() {
	res16 = oper1 ^ oper2;
	fakeflag_log16(res16);
}

void fakeop_sub8() {
	res8 = oper1b - oper2b;
	fakeflag_sub8(oper1b, oper2b);
}

void fakeop_sub16() {
	res16 = oper1 - oper2;
	fakeflag_sub16(oper1, oper2);
}

void fakeop_sbb8() {
	res8 = oper1b - (oper2b + FLAG_CF);
	fakeflag_sbb8(oper1b, oper2b, FLAG_CF);
}

void fakeop_sbb16() {
	res16 = oper1 - (oper2 + FLAG_CF);
	fakeflag_sbb16(oper1, oper2, FLAG_CF);
}

#define modregrm() { \
	addrbyte = getmem8(REG_CS, REG_IP); \
	StepIP(1); \
	mode = addrbyte >> 6; \
	reg = (addrbyte >> 3) & 7; \
	rm = addrbyte & 7; \
	switch(mode) \
	{ \
	case 0: \
	if(rm == 6) { \
	disp16 = getmem16(REG_CS, REG_IP); \
	StepIP(2); \
		} \
	if(((rm == 2) || (rm == 3)) && !segoverride) { \
	useseg = REG_SS; \
		} \
	break; \
 \
	case 1: \
	disp16 = signext(getmem8(REG_CS, REG_IP)); \
	StepIP(1); \
	if(((rm == 2) || (rm == 3) || (rm == 6)) && !segoverride) { \
	useseg = REG_SS; \
		} \
	break; \
 \
	case 2: \
	disp16 = getmem16(REG_CS, REG_IP); \
	StepIP(2); \
	if(((rm == 2) || (rm == 3) || (rm == 6)) && !segoverride) { \
	useseg = REG_SS; \
		} \
	break; \
 \
	default: \
	disp8 = 0; \
	disp16 = 0; \
	} \
	}

void getea(uint8_t rmval) {
	uint32_t	tempea;

	tempea = 0;
	switch (mode) {
	case 0:
		switch (rmval) {
		case 0:
			tempea = REG_BX + REG_SI;
			break;
		case 1:
			tempea = REG_BX + REG_DI;
			break;
		case 2:
			tempea = REG_BP + REG_SI;
			break;
		case 3:
			tempea = REG_BP + REG_DI;
			break;
		case 4:
			tempea = REG_SI;
			break;
		case 5:
			tempea = REG_DI;
			break;
		case 6:
			tempea = disp16;
			break;
		case 7:
			tempea = REG_BX;
			break;
		}
		break;

	case 1:
	case 2:
		switch (rmval) {
		case 0:
			tempea = REG_BX + REG_SI + disp16;
			break;
		case 1:
			tempea = REG_BX + REG_DI + disp16;
			break;
		case 2:
			tempea = REG_BP + REG_SI + disp16;
			break;
		case 3:
			tempea = REG_BP + REG_DI + disp16;
			break;
		case 4:
			tempea = REG_SI + disp16;
			break;
		case 5:
			tempea = REG_DI + disp16;
			break;
		case 6:
			tempea = REG_BP + disp16;
			break;
		case 7:
			tempea = REG_BX + disp16;
			break;
		}
		break;
	}

	ea = (tempea & 0xFFFF) + (useseg << 4);
}

void push(uint16_t pushval) {
	REG_SP -= 2;
	MMU_ww(-1,REG_SS, REG_SP, pushval);
}

uint16_t pop() {

	uint16_t	tempval;

	tempval = MMU_rw(-1,REG_SS, REG_SP,0);
	REG_SP += 2;
	return tempval;
}

void reset86() {
	REG_CS = 0xFFFF;
	REG_IP = 0x0000;
	//regs.wordregs[regsp] = 0xFFFE;
}

uint16_t readrm16(uint8_t rmval) {
	if (mode < 3) {
		getea(rmval);
		return read86(ea) | ((uint16_t)read86(ea + 1) << 8);
	}
	else {
		return getreg16(rmval);
	}
}

uint8_t readrm8(uint8_t rmval) {
	if (mode < 3) {
		getea(rmval);
		return read86(ea);
	}
	else {
		return getreg8(rmval);
	}
}

void writerm16(uint8_t rmval, uint16_t value) {
	if (mode < 3) {
		getea(rmval);
		write86(ea, value & 0xFF);
		write86(ea + 1, value >> 8);
	}
	else {
		putreg16(rmval, value);
	}
}

void writerm8(uint8_t rmval, uint8_t value) {
	if (mode < 3) {
		getea(rmval);
		write86(ea, value);
	}
	else {
		putreg8(rmval, value);
	}
}

uint8_t fakeop_grp2_8(uint8_t cnt) {

	uint16_t	s;
	uint16_t	shift;
	uint16_t	oldcf;
	uint16_t	msb;

	s = oper1b;
	oldcf = FLAG_CF;
#ifdef CPU_V20 //80186/V20 class CPUs limit shift count to 31
	cnt &= 0x1F;
#endif
	switch (reg) {
	case 0: /* ROL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) {
				FLAG_CF = 1;
			}
			else {
				FLAG_CF = 0;
			}

			s = s << 1;
			s = s | FLAG_CF;
		}

		if (cnt == 1) {
			FLAG_OF = FLAG_CF ^ ((s >> 7) & 1);
		}
		break;

	case 1: /* ROR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = (s >> 1) | (FLAG_CF << 7);
		}

		if (cnt == 1) {
			FLAG_OF = (s >> 7) ^ ((s >> 6) & 1);
		}
		break;

	case 2: /* RCL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = FLAG_CF;
			if (s & 0x80) {
				FLAG_CF = 1;
			}
			else {
				FLAG_CF = 0;
			}

			s = s << 1;
			s = s | oldcf;
		}

		if (cnt == 1) {
			FLAG_OF = FLAG_CF ^ ((s >> 7) & 1);
		}
		break;

	case 3: /* RCR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = FLAG_CF;
			FLAG_CF = s & 1;
			s = (s >> 1) | (oldcf << 7);
		}

		if (cnt == 1) {
			FLAG_OF = (s >> 7) ^ ((s >> 6) & 1);
		}
		break;

	case 4:
	case 6: /* SHL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x80) {
				FLAG_CF = 1;
			}
			else {
				FLAG_CF = 0;
			}

			s = (s << 1) & 0xFF;
		}

		if ((cnt == 1) && (FLAG_CF == (s >> 7))) {
			FLAG_OF = 0;
		}
		else {
			FLAG_OF = 1;
		}

		fakeflag_szp8((uint8_t)s);
		break;

	case 5: /* SHR r/m8 */
		if ((cnt == 1) && (s & 0x80)) {
			FLAG_OF = 1;
		}
		else {
			FLAG_OF = 0;
		}

		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = s >> 1;
		}

		fakeflag_szp8((uint8_t)s);
		break;

	case 7: /* SAR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			msb = s & 0x80;
			FLAG_CF = s & 1;
			s = (s >> 1) | msb;
		}

		FLAG_OF = 0;
		fakeflag_szp8((uint8_t)s);
		break;
	}

	return s & 0xFF;
}

uint16_t fakeop_grp2_16(uint8_t cnt) {

	uint32_t	s;
	uint32_t	shift;
	uint32_t	oldcf;
	uint32_t	msb;

	s = oper1;
	oldcf = FLAG_CF;
#ifdef CPU_V20 //80186/V20 class CPUs limit shift count to 31
	cnt &= 0x1F;
#endif
	switch (reg) {
	case 0: /* ROL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) {
				FLAG_CF = 1;
			}
			else {
				FLAG_CF = 0;
			}

			s = s << 1;
			s = s | FLAG_CF;
		}

		if (cnt == 1) {
			FLAG_OF = FLAG_CF ^ ((s >> 15) & 1);
		}
		break;

	case 1: /* ROR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = (s >> 1) | (FLAG_CF << 15);
		}

		if (cnt == 1) {
			FLAG_OF = (s >> 15) ^ ((s >> 14) & 1);
		}
		break;

	case 2: /* RCL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = FLAG_CF;
			if (s & 0x8000) {
				FLAG_CF = 1;
			}
			else {
				FLAG_CF = 0;
			}

			s = s << 1;
			s = s | oldcf;
		}

		if (cnt == 1) {
			FLAG_OF = FLAG_CF ^ ((s >> 15) & 1);
		}
		break;

	case 3: /* RCR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			oldcf = FLAG_CF;
			FLAG_CF = s & 1;
			s = (s >> 1) | (oldcf << 15);
		}

		if (cnt == 1) {
			FLAG_OF = (s >> 15) ^ ((s >> 14) & 1);
		}
		break;

	case 4:
	case 6: /* SHL r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			if (s & 0x8000) {
				FLAG_CF = 1;
			}
			else {
				FLAG_CF = 0;
			}

			s = (s << 1) & 0xFFFF;
		}

		if ((cnt == 1) && (FLAG_CF == (s >> 15))) {
			FLAG_OF = 0;
		}
		else {
			FLAG_OF = 1;
		}

		fakeflag_szp16((uint16_t)s);
		break;

	case 5: /* SHR r/m8 */
		if ((cnt == 1) && (s & 0x8000)) {
			FLAG_OF = 1;
		}
		else {
			FLAG_OF = 0;
		}

		for (shift = 1; shift <= cnt; shift++) {
			FLAG_CF = s & 1;
			s = s >> 1;
		}

		fakeflag_szp16((uint16_t)s);
		break;

	case 7: /* SAR r/m8 */
		for (shift = 1; shift <= cnt; shift++) {
			msb = s & 0x8000;
			FLAG_CF = s & 1;
			s = (s >> 1) | msb;
		}

		FLAG_OF = 0;
		fakeflag_szp16((uint16_t)s);
		break;
	}

	return (uint16_t)s & 0xFFFF;
}

void fakeop_div8(uint16_t valdiv, uint8_t divisor) {
	if (divisor == 0) {
		intcall86(0);
		return;
	}

	if ((valdiv / (uint16_t)divisor) > 0xFF) {
		intcall86(0);
		return;
	}

	REG_AH = valdiv % (uint16_t)divisor;
	REG_AL = valdiv / (uint16_t)divisor;
}

void fakeop_idiv8(uint16_t valdiv, uint8_t divisor) {

	uint16_t	s1;
	uint16_t	s2;
	uint16_t	d1;
	uint16_t	d2;
	int	sign;

	if (divisor == 0) {
		intcall86(0);
		return;
	}

	s1 = valdiv;
	s2 = divisor;
	sign = (((s1 ^ s2) & 0x8000) != 0);
	s1 = (s1 < 0x8000) ? s1 : ((~s1 + 1) & 0xffff);
	s2 = (s2 < 0x8000) ? s2 : ((~s2 + 1) & 0xffff);
	d1 = s1 / s2;
	d2 = s1 % s2;
	if (d1 & 0xFF00) {
		intcall86(0);
		return;
	}

	if (sign) {
		d1 = (~d1 + 1) & 0xff;
		d2 = (~d2 + 1) & 0xff;
	}

	REG_AH = (uint8_t)d2;
	REG_AL = (uint8_t)d1;
}

void fakeop_grp3_8() {
	oper1 = signext(oper1b);
	oper2 = signext(oper2b);
	switch (reg) {
	case 0:
	case 1: /* TEST */
		fakeflag_log8(oper1b & getmem8(REG_CS, REG_IP));
		StepIP(1);
		break;

	case 2: /* NOT */
		res8 = ~oper1b;
		break;

	case 3: /* NEG */
		res8 = (~oper1b) + 1;
		fakeflag_sub8(0, oper1b);
		if (res8 == 0) {
			FLAG_CF = 0;
		}
		else {
			FLAG_CF = 1;
		}
		break;

	case 4: /* MUL */
		temp1 = (uint32_t)oper1b * (uint32_t)REG_AL;
		REG_AX = temp1 & 0xFFFF;
		fakeflag_szp8((uint8_t)temp1);
		if (REG_AH) {
			FLAG_CF = 1;
			FLAG_OF = 1;
		}
		else {
			FLAG_CF = 0;
			FLAG_OF = 0;
		}
#ifndef CPU_V20
		FLAG_ZF = 0;
#endif
		break;

	case 5: /* IMUL */
		oper1 = signext(oper1b);
		temp1 = signext(REG_AL);
		temp2 = oper1;
		if ((temp1 & 0x80) == 0x80) {
			temp1 = temp1 | 0xFFFFFF00;
		}

		if ((temp2 & 0x80) == 0x80) {
			temp2 = temp2 | 0xFFFFFF00;
		}

		temp3 = (temp1 * temp2) & 0xFFFF;
		REG_AX = temp3 & 0xFFFF;
		if (REG_AH) {
			FLAG_CF = 1;
			FLAG_OF = 1;
		}
		else {
			FLAG_CF = 0;
			FLAG_OF = 0;
		}
#ifndef CPU_V20
		FLAG_ZF = 0;
#endif
		break;

	case 6: /* DIV */
		fakeop_div8(REG_AX, oper1b);
		break;

	case 7: /* IDIV */
		fakeop_idiv8(REG_AX, oper1b);
		break;
	}
}

void fakeop_div16(uint32_t valdiv, uint16_t divisor) {
	if (divisor == 0) {
		intcall86(0);
		return;
	}

	if ((valdiv / (uint32_t)divisor) > 0xFFFF) {
		intcall86(0);
		return;
	}

	REG_DX =  valdiv % (uint32_t)divisor;
	REG_AX = valdiv / (uint32_t)divisor;
}

void fakeop_idiv16(uint32_t valdiv, uint16_t divisor) {

	uint32_t	d1;
	uint32_t	d2;
	uint32_t	s1;
	uint32_t	s2;
	int	sign;

	if (divisor == 0) {
		intcall86(0);
		return;
	}

	s1 = valdiv;
	s2 = divisor;
	s2 = (s2 & 0x8000) ? (s2 | 0xffff0000) : s2;
	sign = (((s1 ^ s2) & 0x80000000) != 0);
	s1 = (s1 < 0x80000000) ? s1 : ((~s1 + 1) & 0xffffffff);
	s2 = (s2 < 0x80000000) ? s2 : ((~s2 + 1) & 0xffffffff);
	d1 = s1 / s2;
	d2 = s1 % s2;
	if (d1 & 0xFFFF0000) {
		intcall86(0);
		return;
	}

	if (sign) {
		d1 = (~d1 + 1) & 0xffff;
		d2 = (~d2 + 1) & 0xffff;
	}

	REG_AX = d1;
	REG_DX = d2;
}

void fakeop_grp3_16() {
	switch (reg) {
	case 0:
	case 1: /* TEST */
		fakeflag_log16(oper1 & getmem16(REG_CS, REG_IP));
		StepIP(2);
		break;

	case 2: /* NOT */
		res16 = ~oper1;
		break;

	case 3: /* NEG */
		res16 = (~oper1) + 1;
		fakeflag_sub16(0, oper1);
		if (res16) {
			FLAG_CF = 1;
		}
		else {
			FLAG_CF = 0;
		}
		break;

	case 4: /* MUL */
		temp1 = (uint32_t)oper1 * (uint32_t)REG_AX;
		REG_AX = temp1 & 0xFFFF;
		REG_DX = temp1 >> 16;
		fakeflag_szp16((uint16_t)temp1);
		if (REG_DX) {
			FLAG_CF = 1;
			FLAG_OF = 1;
		}
		else {
			FLAG_CF = 0;
			FLAG_OF = 0;
		}
#ifndef CPU_V20
		FLAG_ZF = 0;
#endif
		break;

	case 5: /* IMUL */
		temp1 = REG_AX;
		temp2 = oper1;
		if (temp1 & 0x8000) {
			temp1 |= 0xFFFF0000;
		}

		if (temp2 & 0x8000) {
			temp2 |= 0xFFFF0000;
		}

		temp3 = temp1 * temp2;
		REG_AX = temp3 & 0xFFFF;	/* into register ax */
		REG_DX = temp3 >> 16;	/* into register dx */
		if (REG_DX) {
			FLAG_CF = 1;
			FLAG_OF = 1;
		}
		else {
			FLAG_CF = 0;
			FLAG_OF = 0;
		}
#ifndef CPU_V20
		FLAG_ZF = 0;
#endif
		break;

	case 6: /* DIV */
		fakeop_div16(((uint32_t)REG_DX << 16) + REG_AX, oper1);
		break;

	case 7: /* DIV */
		fakeop_idiv16(((uint32_t)REG_DX << 16) + REG_AX, oper1);
		break;
	}
}

void fakeop_grp5() {
	switch (reg) {
	case 0: /* INC Ev */
		oper2 = 1;
		tempcf = FLAG_CF;
		fakeop_add16();
		FLAG_CF = tempcf;
		writerm16(rm, res16);
		break;

	case 1: /* DEC Ev */
		oper2 = 1;
		tempcf = FLAG_CF;
		fakeop_sub16();
		FLAG_CF = tempcf;
		writerm16(rm, res16);
		break;

	case 2: /* CALL Ev */
		push(REG_IP);
		REG_IP = oper1;
		break;

	case 3: /* CALL Mp */
		push(REG_CS);
		push(REG_IP);
		getea(rm);
		REG_IP = (uint16_t)read86(ea) + (uint16_t)read86(ea + 1) * 256;
		REG_CS = (uint16_t)read86(ea + 2) + (uint16_t)read86(ea + 3) * 256;
		break;

	case 4: /* JMP Ev */
		REG_IP = oper1;
		break;

	case 5: /* JMP Mp */
		getea(rm);
		REG_IP = (uint16_t)read86(ea) + (uint16_t)read86(ea + 1) * 256;
		REG_CS = (uint16_t)read86(ea + 2) + (uint16_t)read86(ea + 3) * 256;
		break;

	case 6: /* PUSH Ev */
		push(oper1);
		break;
	}
}

uint8_t didintr = 0;
FILE	*logout;
uint8_t printops = 0;

#ifdef NETWORKING_ENABLED
extern void nethandler();
#endif
extern void diskhandler();
extern void readdisk(uint8_t drivenum, uint16_t dstseg, uint16_t dstoff, uint16_t cyl, uint16_t sect, uint16_t head, uint16_t sectcount);

void intcall86(uint8_t intnum) {
	static uint16_t lastint10ax;
	uint16_t oldregax;
	didintr = 1;

	push(makeflagsword());
	push(REG_CS);
	push(REG_IP);
	REG_CS = getmem16(0, (uint16_t)intnum * 4 + 2);
	REG_IP = getmem16(0, (uint16_t)intnum * 4);
	FLAG_IF = 0;
	FLAG_TF = 0;
}

#if defined(NETWORKING_ENABLED)
extern struct netstruct {
	uint8_t	enabled;
	uint8_t	canrecv;
	uint16_t	pktlen;
} net;
#endif
uint64_t	frametimer = 0, didwhen = 0, didticks = 0;
uint32_t	makeupticks = 0;
extern float	timercomp;
uint64_t	timerticks = 0, realticks = 0;
uint64_t	lastcountertimer = 0, counterticks = 10000;
extern uint8_t	nextintr();
extern void	timing();

void exec86() {

	uint8_t	docontinue;
	static uint16_t firstip;
	static uint16_t trap_toggle = 0;

		if (trap_toggle) {
			intcall86(1);
		}

		if (FLAG_TF) {
			trap_toggle = 1;
		}
		else {
			trap_toggle = 0;
		}

		if (!trap_toggle && (FLAG_IF && PICInterrupt())) {
			intcall86(nextintr());	/* get next interrupt from the i8259, if any */
		}

		reptype = 0;
		segoverride = 0;
		useseg = REG_DS;
		docontinue = 0;
		firstip = REG_IP;

		while (!docontinue) {
			REG_CS = REG_CS & 0xFFFF;
			REG_IP = REG_IP & 0xFFFF;
			savecs = REG_CS;
			saveip = REG_IP;
			opcode = getmem8(REG_CS, REG_IP);
			StepIP(1);

			switch (opcode) {
				/* segment prefix check */
			case 0x2E:	/* segment REG_CS */
				useseg = REG_CS;
				segoverride = 1;
				break;

			case 0x3E:	/* segment REG_DS */
				useseg = REG_DS;
				segoverride = 1;
				break;

			case 0x26:	/* segment REG_ES */
				useseg = REG_ES;
				segoverride = 1;
				break;

			case 0x36:	/* segment REG_SS */
				useseg = REG_SS;
				segoverride = 1;
				break;

				/* repetition prefix check */
			case 0xF3:	/* REP/REPE/REPZ */
				reptype = 1;
				break;

			case 0xF2:	/* REPNE/REPNZ */
				reptype = 2;
				break;

			default:
				docontinue = 1;
				break;
			}
		}

		/*
		* if (printops == 1) { printf("%04X:%04X - %s\n", savecs, saveip, oplist[opcode]);
		* }
		*/
		switch (opcode) {
		case 0x0:	/* 00 ADD Eb Gb */
			modregrm();
			oper1b = readrm8(rm);
			oper2b = getreg8(reg);
			fakeop_add8();
			writerm8(rm, res8);
			break;

		case 0x1:	/* 01 ADD Ev Gv */
			modregrm();
			oper1 = readrm16(rm);
			oper2 = getreg16(reg);
			fakeop_add16();
			writerm16(rm, res16);
			break;

		case 0x2:	/* 02 ADD Gb Eb */
			modregrm();
			oper1b = getreg8(reg);
			oper2b = readrm8(rm);
			fakeop_add8();
			putreg8(reg, res8);
			break;

		case 0x3:	/* 03 ADD Gv Ev */
			modregrm();
			oper1 = getreg16(reg);
			oper2 = readrm16(rm);
			fakeop_add16();
			putreg16(reg, res16);
			break;

		case 0x4:	/* 04 ADD REG_AL Ib */
			oper1b = REG_AL;
			oper2b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			fakeop_add8();
			REG_AL = res8;
			break;

		case 0x5:	/* 05 ADD eAX Iv */
			oper1 = (REG_AX);
			oper2 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			fakeop_add16();
			REG_AX = res16;
			break;

		case 0x6:	/* 06 PUSH REG_ES */
			push(REG_ES);
			break;

		case 0x7:	/* 07 POP REG_ES */
			REG_ES = pop();
			break;

		case 0x8:	/* 08 OR Eb Gb */
			modregrm();
			oper1b = readrm8(rm);
			oper2b = getreg8(reg);
			fakeop_or8();
			writerm8(rm, res8);
			break;

		case 0x9:	/* 09 OR Ev Gv */
			modregrm();
			oper1 = readrm16(rm);
			oper2 = getreg16(reg);
			fakeop_or16();
			writerm16(rm, res16);
			break;

		case 0xA:	/* 0A OR Gb Eb */
			modregrm();
			oper1b = getreg8(reg);
			oper2b = readrm8(rm);
			fakeop_or8();
			putreg8(reg, res8);
			break;

		case 0xB:	/* 0B OR Gv Ev */
			modregrm();
			oper1 = getreg16(reg);
			oper2 = readrm16(rm);
			fakeop_or16();
			if ((oper1 == 0xF802) && (oper2 == 0xF802)) {
				FLAG_SF = 0;	/* cheap hack to make Wolf 3D think we're a 286 so it plays */
			}

			putreg16(reg, res16);
			break;

		case 0xC:	/* 0C OR REG_AL Ib */
			oper1b = REG_AL;
			oper2b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			fakeop_or8();
			REG_AL = res8;
			break;

		case 0xD:	/* 0D OR eAX Iv */
			oper1 = REG_AX;
			oper2 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			fakeop_or16();
			REG_AX = res16;
			break;

		case 0xE:	/* 0E PUSH REG_CS */
			push(REG_CS);
			break;

		case 0xF: //0F POP CS
#ifndef CPU_V20
			REG_CS = pop();
#endif
			break;

		case 0x10:	/* 10 ADC Eb Gb */
			modregrm();
			oper1b = readrm8(rm);
			oper2b = getreg8(reg);
			fakeop_adc8();
			writerm8(rm, res8);
			break;

		case 0x11:	/* 11 ADC Ev Gv */
			modregrm();
			oper1 = readrm16(rm);
			oper2 = getreg16(reg);
			fakeop_adc16();
			writerm16(rm, res16);
			break;

		case 0x12:	/* 12 ADC Gb Eb */
			modregrm();
			oper1b = getreg8(reg);
			oper2b = readrm8(rm);
			fakeop_adc8();
			putreg8(reg, res8);
			break;

		case 0x13:	/* 13 ADC Gv Ev */
			modregrm();
			oper1 = getreg16(reg);
			oper2 = readrm16(rm);
			fakeop_adc16();
			putreg16(reg, res16);
			break;

		case 0x14:	/* 14 ADC REG_AL Ib */
			oper1b = REG_AL;
			oper2b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			fakeop_adc8();
			REG_AL = res8;
			break;

		case 0x15:	/* 15 ADC eAX Iv */
			oper1 = REG_AX;
			oper2 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			fakeop_adc16();
			REG_AX = res16;
			break;

		case 0x16:	/* 16 PUSH REG_SS */
			push(REG_SS);
			break;

		case 0x17:	/* 17 POP REG_SS */
			REG_SS = pop();
			break;

		case 0x18:	/* 18 SBB Eb Gb */
			modregrm();
			oper1b = readrm8(rm);
			oper2b = getreg8(reg);
			fakeop_sbb8();
			writerm8(rm, res8);
			break;

		case 0x19:	/* 19 SBB Ev Gv */
			modregrm();
			oper1 = readrm16(rm);
			oper2 = getreg16(reg);
			fakeop_sbb16();
			writerm16(rm, res16);
			break;

		case 0x1A:	/* 1A SBB Gb Eb */
			modregrm();
			oper1b = getreg8(reg);
			oper2b = readrm8(rm);
			fakeop_sbb8();
			putreg8(reg, res8);
			break;

		case 0x1B:	/* 1B SBB Gv Ev */
			modregrm();
			oper1 = getreg16(reg);
			oper2 = readrm16(rm);
			fakeop_sbb16();
			putreg16(reg, res16);
			break;

		case 0x1C:	/* 1C SBB REG_AL Ib */
			oper1b = REG_AL;
			oper2b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			fakeop_sbb8();
			REG_AL = res8;
			break;

		case 0x1D:	/* 1D SBB eAX Iv */
			oper1 = REG_AX;
			oper2 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			fakeop_sbb16();
			REG_AX = res16;
			break;

		case 0x1E:	/* 1E PUSH REG_DS */
			push(REG_DS);
			break;

		case 0x1F:	/* 1F POP REG_DS */
			REG_DS = pop();
			break;

		case 0x20:	/* 20 AND Eb Gb */
			modregrm();
			oper1b = readrm8(rm);
			oper2b = getreg8(reg);
			fakeop_and8();
			writerm8(rm, res8);
			break;

		case 0x21:	/* 21 AND Ev Gv */
			modregrm();
			oper1 = readrm16(rm);
			oper2 = getreg16(reg);
			fakeop_and16();
			writerm16(rm, res16);
			break;

		case 0x22:	/* 22 AND Gb Eb */
			modregrm();
			oper1b = getreg8(reg);
			oper2b = readrm8(rm);
			fakeop_and8();
			putreg8(reg, res8);
			break;

		case 0x23:	/* 23 AND Gv Ev */
			modregrm();
			oper1 = getreg16(reg);
			oper2 = readrm16(rm);
			fakeop_and16();
			putreg16(reg, res16);
			break;

		case 0x24:	/* 24 AND REG_AL Ib */
			oper1b = REG_AL;
			oper2b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			fakeop_and8();
			REG_AL = res8;
			break;

		case 0x25:	/* 25 AND eAX Iv */
			oper1 = REG_AX;
			oper2 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			fakeop_and16();
			REG_AX = res16;
			break;

		case 0x27:	/* 27 DAA */
			if (((REG_AL & 0xF) > 9) || (FLAG_AF == 1)) {
				oper1 = REG_AL + 6;
				REG_AL = oper1 & 255;
				if (oper1 & 0xFF00) {
					FLAG_CF = 1;
				}
				else {
					FLAG_CF = 0;
				}

				FLAG_AF = 1;
			}
			else {
				FLAG_AF = 0;
			}

			if (((REG_AL & 0xF0) > 0x90) || (FLAG_CF == 1)) {
				REG_AL = REG_AL + 0x60;
				FLAG_CF = 1;
			}
			else {
				FLAG_CF = 0;
			}

			REG_AL = REG_AL & 255;
			fakeflag_szp8(REG_AL);
			break;

		case 0x28:	/* 28 SUB Eb Gb */
			modregrm();
			oper1b = readrm8(rm);
			oper2b = getreg8(reg);
			fakeop_sub8();
			writerm8(rm, res8);
			break;

		case 0x29:	/* 29 SUB Ev Gv */
			modregrm();
			oper1 = readrm16(rm);
			oper2 = getreg16(reg);
			fakeop_sub16();
			writerm16(rm, res16);
			break;

		case 0x2A:	/* 2A SUB Gb Eb */
			modregrm();
			oper1b = getreg8(reg);
			oper2b = readrm8(rm);
			fakeop_sub8();
			putreg8(reg, res8);
			break;

		case 0x2B:	/* 2B SUB Gv Ev */
			modregrm();
			oper1 = getreg16(reg);
			oper2 = readrm16(rm);
			fakeop_sub16();
			putreg16(reg, res16);
			break;

		case 0x2C:	/* 2C SUB REG_AL Ib */
			oper1b = REG_AL;
			oper2b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			fakeop_sub8();
			REG_AL = res8;
			break;

		case 0x2D:	/* 2D SUB eAX Iv */
			oper1 = REG_AX;
			oper2 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			fakeop_sub16();
			REG_AX = res16;
			break;

		case 0x2F:	/* 2F DAS */
			if (((REG_AL & 15) > 9) || (FLAG_AF == 1)) {
				oper1 = REG_AL - 6;
				REG_AL = oper1 & 255;
				if (oper1 & 0xFF00) {
					FLAG_CF = 1;
				}
				else {
					FLAG_CF = 0;
				}

				FLAG_AF = 1;
			}
			else {
				FLAG_AF = 0;
			}

			if (((REG_AL & 0xF0) > 0x90) || (FLAG_CF == 1)) {
				REG_AL = REG_AL - 0x60;
				FLAG_CF = 1;
			}
			else {
				FLAG_CF = 0;
			}

			fakeflag_szp8(REG_AL);
			break;

		case 0x30:	/* 30 XOR Eb Gb */
			modregrm();
			oper1b = readrm8(rm);
			oper2b = getreg8(reg);
			fakeop_xor8();
			writerm8(rm, res8);
			break;

		case 0x31:	/* 31 XOR Ev Gv */
			modregrm();
			oper1 = readrm16(rm);
			oper2 = getreg16(reg);
			fakeop_xor16();
			writerm16(rm, res16);
			break;

		case 0x32:	/* 32 XOR Gb Eb */
			modregrm();
			oper1b = getreg8(reg);
			oper2b = readrm8(rm);
			fakeop_xor8();
			putreg8(reg, res8);
			break;

		case 0x33:	/* 33 XOR Gv Ev */
			modregrm();
			oper1 = getreg16(reg);
			oper2 = readrm16(rm);
			fakeop_xor16();
			putreg16(reg, res16);
			break;

		case 0x34:	/* 34 XOR REG_AL Ib */
			oper1b = REG_AL;
			oper2b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			fakeop_xor8();
			REG_AL = res8;
			break;

		case 0x35:	/* 35 XOR eAX Iv */
			oper1 = REG_AX;
			oper2 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			fakeop_xor16();
			REG_AX = res16;
			break;

		case 0x37:	/* 37 AAA ASCII */
			if (((REG_AL & 0xF) > 9) || (FLAG_AF == 1)) {
				REG_AL = REG_AL + 6;
				REG_AH = REG_AH + 1;
				FLAG_AF = 1;
				FLAG_CF = 1;
			}
			else {
				FLAG_AF = 0;
				FLAG_CF = 0;
			}

			REG_AL = REG_AL & 0xF;
			break;

		case 0x38:	/* 38 CMP Eb Gb */
			modregrm();
			oper1b = readrm8(rm);
			oper2b = getreg8(reg);
			fakeflag_sub8(oper1b, oper2b);
			break;

		case 0x39:	/* 39 CMP Ev Gv */
			modregrm();
			oper1 = readrm16(rm);
			oper2 = getreg16(reg);
			fakeflag_sub16(oper1, oper2);
			break;

		case 0x3A:	/* 3A CMP Gb Eb */
			modregrm();
			oper1b = getreg8(reg);
			oper2b = readrm8(rm);
			fakeflag_sub8(oper1b, oper2b);
			break;

		case 0x3B:	/* 3B CMP Gv Ev */
			modregrm();
			oper1 = getreg16(reg);
			oper2 = readrm16(rm);
			fakeflag_sub16(oper1, oper2);
			break;

		case 0x3C:	/* 3C CMP REG_AL Ib */
			oper1b = REG_AL;
			oper2b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			fakeflag_sub8(oper1b, oper2b);
			break;

		case 0x3D:	/* 3D CMP eAX Iv */
			oper1 = REG_AX;
			oper2 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			fakeflag_sub16(oper1, oper2);
			break;

		case 0x3F:	/* 3F AAS ASCII */
			if (((REG_AL & 0xF) > 9) || (FLAG_AF == 1)) {
				REG_AL = REG_AL - 6;
				REG_AH = REG_AH - 1;
				FLAG_AF = 1;
				FLAG_CF = 1;
			}
			else {
				FLAG_AF = 0;
				FLAG_CF = 0;
			}

			REG_AL = REG_AL & 0xF;
			break;

		case 0x40:	/* 40 INC eAX */
			oldcf = FLAG_CF;
			oper1 = REG_AX;
			oper2 = 1;
			fakeop_add16();
			FLAG_CF = oldcf;
			REG_AX = res16;
			break;

		case 0x41:	/* 41 INC eCX */
			oldcf = FLAG_CF;
			oper1 = REG_CX;
			oper2 = 1;
			fakeop_add16();
			FLAG_CF = oldcf;
			REG_CX = res16;
			break;

		case 0x42:	/* 42 INC eDX */
			oldcf = FLAG_CF;
			oper1 = REG_DX;
			oper2 = 1;
			fakeop_add16();
			FLAG_CF = oldcf;
			REG_DX = res16;
			break;

		case 0x43:	/* 43 INC eBX */
			oldcf = FLAG_CF;
			oper1 = REG_BX;
			oper2 = 1;
			fakeop_add16();
			FLAG_CF = oldcf;
			REG_BX = res16;
			break;

		case 0x44:	/* 44 INC eSP */
			oldcf = FLAG_CF;
			oper1 = REG_SP;
			oper2 = 1;
			fakeop_add16();
			FLAG_CF = oldcf;
			REG_SP = res16;
			break;

		case 0x45:	/* 45 INC eBP */
			oldcf = FLAG_CF;
			oper1 = REG_BP;
			oper2 = 1;
			fakeop_add16();
			FLAG_CF = oldcf;
			REG_BP = res16;
			break;

		case 0x46:	/* 46 INC eSI */
			oldcf = FLAG_CF;
			oper1 = REG_SI;
			oper2 = 1;
			fakeop_add16();
			FLAG_CF = oldcf;
			REG_SI = res16;
			break;

		case 0x47:	/* 47 INC eDI */
			oldcf = FLAG_CF;
			oper1 = REG_DI;
			oper2 = 1;
			fakeop_add16();
			FLAG_CF = oldcf;
			REG_DI = res16;
			break;

		case 0x48:	/* 48 DEC eAX */
			oldcf = FLAG_CF;
			oper1 = REG_AX;
			oper2 = 1;
			fakeop_sub16();
			FLAG_CF = oldcf;
			REG_AX = res16;
			break;

		case 0x49:	/* 49 DEC eCX */
			oldcf = FLAG_CF;
			oper1 = REG_CX;
			oper2 = 1;
			fakeop_sub16();
			FLAG_CF = oldcf;
			REG_CX = res16;
			break;

		case 0x4A:	/* 4A DEC eDX */
			oldcf = FLAG_CF;
			oper1 = REG_DX;
			oper2 = 1;
			fakeop_sub16();
			FLAG_CF = oldcf;
			REG_DX = res16;
			break;

		case 0x4B:	/* 4B DEC eBX */
			oldcf = FLAG_CF;
			oper1 = REG_BX;
			oper2 = 1;
			fakeop_sub16();
			FLAG_CF = oldcf;
			REG_BX = res16;
			break;

		case 0x4C:	/* 4C DEC eSP */
			oldcf = FLAG_CF;
			oper1 = REG_SP;
			oper2 = 1;
			fakeop_sub16();
			FLAG_CF = oldcf;
			REG_SP = res16;
			break;

		case 0x4D:	/* 4D DEC eBP */
			oldcf = FLAG_CF;
			oper1 = REG_BP;
			oper2 = 1;
			fakeop_sub16();
			FLAG_CF = oldcf;
			REG_BP = res16;
			break;

		case 0x4E:	/* 4E DEC eSI */
			oldcf = FLAG_CF;
			oper1 = REG_SI;
			oper2 = 1;
			fakeop_sub16();
			FLAG_CF = oldcf;
			REG_SI = res16;
			break;

		case 0x4F:	/* 4F DEC eDI */
			oldcf = FLAG_CF;
			oper1 = REG_DI;
			oper2 = 1;
			fakeop_sub16();
			FLAG_CF = oldcf;
			REG_DI = res16;
			break;

		case 0x50:	/* 50 PUSH eAX */
			push(REG_AX);
			break;

		case 0x51:	/* 51 PUSH eCX */
			push(REG_CX);
			break;

		case 0x52:	/* 52 PUSH eDX */
			push(REG_DX);
			break;

		case 0x53:	/* 53 PUSH eBX */
			push(REG_BX);
			break;

		case 0x54:	/* 54 PUSH eSP */
			push(REG_SP - 2);
			break;

		case 0x55:	/* 55 PUSH eBP */
			push(REG_BP);
			break;

		case 0x56:	/* 56 PUSH eSI */
			push(REG_SI);
			break;

		case 0x57:	/* 57 PUSH eDI */
			push(REG_DI);
			break;

		case 0x58:	/* 58 POP eAX */
			REG_AX = pop();
			break;

		case 0x59:	/* 59 POP eCX */
			REG_CS = pop();
			break;

		case 0x5A:	/* 5A POP eDX */
			REG_DX = pop();
			break;

		case 0x5B:	/* 5B POP eBX */
			REG_BX = pop();
			break;

		case 0x5C:	/* 5C POP eSP */
			REG_SP = pop();
			break;

		case 0x5D:	/* 5D POP eBP */
			REG_BP = pop();
			break;

		case 0x5E:	/* 5E POP eSI */
			REG_SI = pop();
			break;

		case 0x5F:	/* 5F POP eDI */
			REG_DI = pop();
			break;

#ifdef CPU_V20
		case 0x60:	/* 60 PUSHA (80186+) */
			oldsp = REG_SP;
			push(REG_AX);
			push(REG_CX);
			push(REG_DX);
			push(REG_BX);
			push(oldsp);
			push(REG_BP);
			push(REG_SI);
			push(REG_DI);
			break;

		case 0x61:	/* 61 POPA (80186+) */
			REG_DI = pop();
			REG_SI = pop();
			REG_BP = pop();
			dummy = pop();
			REG_BX = pop();
			REG_DX = pop();
			REG_CS = pop();
			REG_AX = pop();
			break;

		case 0x62: /* 62 BOUND Gv, Ev (80186+) */
			modregrm();
			getea(rm);
			if (signext32(getreg16(reg)) < signext32(getmem16(ea >> 4, ea & 15))) {
				intcall86(5); //bounds check exception
			}
			else {
				ea += 2;
				if (signext32(getreg16(reg)) > signext32(getmem16(ea >> 4, ea & 15))) {
					intcall86(5); //bounds check exception
				}
			}
			break;

		case 0x68:	/* 68 PUSH Iv (80186+) */
			push(getmem16(REG_CS, REG_IP));
			StepIP(2);
			break;

		case 0x69:	/* 69 IMUL Gv Ev Iv (80186+) */
			modregrm();
			temp1 = readrm16(rm);
			temp2 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			if ((temp1 & 0x8000L) == 0x8000L) {
				temp1 = temp1 | 0xFFFF0000L;
			}

			if ((temp2 & 0x8000L) == 0x8000L) {
				temp2 = temp2 | 0xFFFF0000L;
			}

			temp3 = temp1 * temp2;
			putreg16(reg, temp3 & 0xFFFFL);
			if (temp3 & 0xFFFF0000L) {
				FLAG_CF = 1;
				FLAG_OF = 1;
			}
			else {
				FLAG_CF = 0;
				FLAG_OF = 0;
			}
			break;

		case 0x6A:	/* 6A PUSH Ib (80186+) */
			push(getmem8(REG_CS, REG_IP));
			StepIP(1);
			break;

		case 0x6B:	/* 6B IMUL Gv Eb Ib (80186+) */
			modregrm();
			temp1 = readrm16(rm);
			temp2 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if ((temp1 & 0x8000L) == 0x8000L) {
				temp1 = temp1 | 0xFFFF0000L;
			}

			if ((temp2 & 0x8000L) == 0x8000L) {
				temp2 = temp2 | 0xFFFF0000L;
			}

			temp3 = temp1 * temp2;
			putreg16(reg, temp3 & 0xFFFFL);
			if (temp3 & 0xFFFF0000L) {
				FLAG_CF = 1;
				FLAG_OF = 1;
			}
			else {
				FLAG_CF = 0;
				FLAG_OF = 0;
			}
			break;

		case 0x6C:	/* 6E INSB */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			MMU_wb(-1,useseg, REG_SI, PORT_IN_B(REG_DX));
			if (FLAG_DF) {
				--REG_SI;
				--REG_DI;
			}
			else {
				++REG_SI;
				++REG_DI;
			}

			if (reptype) {
				--REG_CX;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0x6D:	/* 6F INSW */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			MMU_ww(-1,useseg, REG_SI, PORT_IN_W(REG_DX));
			if (FLAG_DF) {
				REG_SI -= 2;
				REG_DI -= 2;
			}
			else {
				REG_SI += 2;
				REG_DI += 2;
			}

			if (reptype) {
				--REG_CX;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0x6E:	/* 6E OUTSB */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			PORT_OUT_B(REG_DX, getmem8(useseg, REG_SI));
			if (FLAG_DF) {
				--REG_SI;
				--REG_DI;
			}
			else {
				++REG_SI;
				++REG_DI;
			}

			if (reptype) {
				--REG_CX;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0x6F:	/* 6F OUTSW */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			PORT_OUT_W(REG_DX, getmem16(useseg, REG_SI));
			if (FLAG_DF) {
				REG_SI -= 2;
				REG_DI -= 2;
			}
			else {
				REG_SI += 2;
				REG_DI += 2;
			}

			if (reptype) {
				--REG_CX;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;
#endif

		case 0x70:	/* 70 JO Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (FLAG_OF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x71:	/* 71 JNO Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (!FLAG_OF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x72:	/* 72 JB Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (FLAG_CF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x73:	/* 73 JNB Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (!FLAG_CF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x74:	/* 74 JZ Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (FLAG_ZF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x75:	/* 75 JNZ Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (!FLAG_ZF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x76:	/* 76 JBE Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (FLAG_CF || FLAG_ZF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x77:	/* 77 JA Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (!FLAG_CF && !FLAG_ZF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x78:	/* 78 JS Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (FLAG_SF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x79:	/* 79 JNS Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (!FLAG_SF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x7A:	/* 7A JPE Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (FLAG_PF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x7B:	/* 7B JPO Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (!FLAG_PF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x7C:	/* 7C JL Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (FLAG_SF != FLAG_OF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x7D:	/* 7D JGE Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (FLAG_SF == FLAG_OF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x7E:	/* 7E JLE Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if ((FLAG_SF != FLAG_OF) || FLAG_ZF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x7F:	/* 7F JG Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (!FLAG_ZF && (FLAG_SF == FLAG_OF)) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0x80:
		case 0x82:	/* 80/82 GRP1 Eb Ib */
			modregrm();
			oper1b = readrm8(rm);
			oper2b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			switch (reg) {
			case 0:
				fakeop_add8();
				break;
			case 1:
				fakeop_or8();
				break;
			case 2:
				fakeop_adc8();
				break;
			case 3:
				fakeop_sbb8();
				break;
			case 4:
				fakeop_and8();
				break;
			case 5:
				fakeop_sub8();
				break;
			case 6:
				fakeop_xor8();
				break;
			case 7:
				fakeflag_sub8(oper1b, oper2b);
				break;
			default:
				break;	/* to avoid compiler warnings */
			}

			if (reg < 7) {
				writerm8(rm, res8);
			}
			break;

		case 0x81:	/* 81 GRP1 Ev Iv */
		case 0x83:	/* 83 GRP1 Ev Ib */
			modregrm();
			oper1 = readrm16(rm);
			if (opcode == 0x81) {
				oper2 = getmem16(REG_CS, REG_IP);
				StepIP(2);
			}
			else {
				oper2 = signext(getmem8(REG_CS, REG_IP));
				StepIP(1);
			}

			switch (reg) {
			case 0:
				fakeop_add16();
				break;
			case 1:
				fakeop_or16();
				break;
			case 2:
				fakeop_adc16();
				break;
			case 3:
				fakeop_sbb16();
				break;
			case 4:
				fakeop_and16();
				break;
			case 5:
				fakeop_sub16();
				break;
			case 6:
				fakeop_xor16();
				break;
			case 7:
				fakeflag_sub16(oper1, oper2);
				break;
			default:
				break;	/* to avoid compiler warnings */
			}

			if (reg < 7) {
				writerm16(rm, res16);
			}
			break;

		case 0x84:	/* 84 TEST Gb Eb */
			modregrm();
			oper1b = getreg8(reg);
			oper2b = readrm8(rm);
			fakeflag_log8(oper1b & oper2b);
			break;

		case 0x85:	/* 85 TEST Gv Ev */
			modregrm();
			oper1 = getreg16(reg);
			oper2 = readrm16(rm);
			fakeflag_log16(oper1 & oper2);
			break;

		case 0x86:	/* 86 XCHG Gb Eb */
			modregrm();
			oper1b = getreg8(reg);
			putreg8(reg, readrm8(rm));
			writerm8(rm, oper1b);
			break;

		case 0x87:	/* 87 XCHG Gv Ev */
			modregrm();
			oper1 = getreg16(reg);
			putreg16(reg, readrm16(rm));
			writerm16(rm, oper1);
			break;

		case 0x88:	/* 88 MOV Eb Gb */
			modregrm();
			writerm8(rm, getreg8(reg));
			break;

		case 0x89:	/* 89 MOV Ev Gv */
			modregrm();
			writerm16(rm, getreg16(reg));
			break;

		case 0x8A:	/* 8A MOV Gb Eb */
			modregrm();
			putreg8(reg, readrm8(rm));
			break;

		case 0x8B:	/* 8B MOV Gv Ev */
			modregrm();
			putreg16(reg, readrm16(rm));
			break;

		case 0x8C:	/* 8C MOV Ew Sw */
			modregrm();
			writerm16(rm, getsegreg(reg));
			break;

		case 0x8D:	/* 8D LEA Gv M */
			modregrm();
			getea(rm);
			putreg16(reg, ea - segbase(useseg));
			break;

		case 0x8E:	/* 8E MOV Sw Ew */
			modregrm();
			putsegreg(reg, readrm16(rm));
			break;

		case 0x8F:	/* 8F POP Ev */
			modregrm();
			writerm16(rm, pop());
			break;

		case 0x90:	/* 90 NOP */
			break;

		case 0x91:	/* 91 XCHG eCX eAX */
			oper1 = REG_CX;
			REG_CX = REG_AX;
			REG_AX = oper1;
			break;

		case 0x92:	/* 92 XCHG eDX eAX */
			oper1 = REG_DX;
			REG_DX = REG_AX;
			REG_AX = oper1;
			break;

		case 0x93:	/* 93 XCHG eBX eAX */
			oper1 = REG_BX;
			REG_BX = REG_AX;
			REG_AX = oper1;
			break;

		case 0x94:	/* 94 XCHG eSP eAX */
			oper1 = REG_SP;
			REG_SP = REG_AX;
			REG_AX = oper1;
			break;

		case 0x95:	/* 95 XCHG eBP eAX */
			oper1 = REG_BP;
			REG_BP = REG_AX;
			REG_AX = oper1;
			break;

		case 0x96:	/* 96 XCHG eSI eAX */
			oper1 = REG_SI;
			REG_SI = REG_AX;
			REG_AX = oper1;
			break;

		case 0x97:	/* 97 XCHG eDI eAX */
			oper1 = REG_DI;
			REG_DI = REG_AX;
			REG_AX = oper1;
			break;

		case 0x98:	/* 98 CBW */
			if ((REG_AL & 0x80) == 0x80) {
				REG_AH = 0xFF;
			}
			else {
				REG_AH = 0;
			}
			break;

		case 0x99:	/* 99 CWD */
			if ((REG_AH & 0x80) == 0x80) {
				REG_DX = 0xFFFF;
			}
			else {
				REG_DX = 0;
			}
			break;

		case 0x9A:	/* 9A CALL Ap */
			oper1 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			oper2 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			push(REG_CS);
			push(REG_IP);
			REG_IP = oper1;
			REG_CS = oper2;
			break;

		case 0x9B:	/* 9B WAIT */
			break;

		case 0x9C:	/* 9C PUSHF */
			push(makeflagsword() | 0xF800);
			break;

		case 0x9D:	/* 9D POPF */
			temp16 = pop();
			decodeflagsword(temp16);
			break;

		case 0x9E:	/* 9E SAHF */
			decodeflagsword((makeflagsword() & 0xFF00) | REG_AH);
			break;

		case 0x9F:	/* 9F LAHF */
			REG_AH = makeflagsword() & 0xFF;
			break;

		case 0xA0:	/* A0 MOV REG_AL Ob */
			REG_AL = getmem8(useseg, getmem16(REG_CS, REG_IP));
			StepIP(2);
			break;

		case 0xA1:	/* A1 MOV eAX Ov */
			oper1 = getmem16(useseg, getmem16(REG_CS, REG_IP));
			StepIP(2);
			REG_AX = oper1;
			break;

		case 0xA2:	/* A2 MOV Ob REG_AL */
			putmem8(useseg, getmem16(REG_CS, REG_IP), REG_AL);
			StepIP(2);
			break;

		case 0xA3:	/* A3 MOV Ov eAX */
			putmem16(useseg, getmem16(REG_CS, REG_IP), REG_AX);
			StepIP(2);
			break;

		case 0xA4:	/* A4 MOVSB */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			putmem8(REG_ES, REG_DI, getmem8(useseg, REG_SI));
			if (FLAG_DF) {
				--REG_SI;
				--REG_DI;
			}
			else {
				++REG_SI;
				++REG_DI;
			}

			if (reptype) {
				--REG_CX;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0xA5:	/* A5 MOVSW */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			putmem16(REG_ES, REG_DI, getmem16(useseg, REG_SI));
			if (FLAG_DF) {
				REG_SI -= 2;
				REG_DI -= 2;
			}
			else {
				REG_SI += 2;
				REG_DI += 2;
			}

			if (reptype) {
				--REG_CX;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0xA6:	/* A6 CMPSB */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			oper1b = getmem8(useseg, REG_SI);
			oper2b = getmem8(REG_ES, REG_DI);
			if (FLAG_DF) {
				--REG_SI;
				--REG_DI;
			}
			else {
				++REG_SI;
				++REG_DI;
			}

			fakeflag_sub8(oper1b, oper2b);
			if (reptype) {
				--REG_CX;
			}

			if ((reptype == 1) && !FLAG_ZF) {
				break;
			}
			else if ((reptype == 2) && (FLAG_ZF == 1)) {
				break;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0xA7:	/* A7 CMPSW */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			oper1 = getmem16(useseg, REG_SI);
			oper2 = getmem16(REG_ES, REG_DI);
			if (FLAG_DF) {
				REG_SI -= 2;
				REG_DI -= 2;
			}
			else {
				REG_SI += 2;
				REG_DI += 2;
			}

			fakeflag_sub16(oper1, oper2);
			if (reptype) {
				--REG_CX;
			}

			if ((reptype == 1) && !FLAG_ZF) {
				break;
			}

			if ((reptype == 2) && (FLAG_ZF == 1)) {
				break;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0xA8:	/* A8 TEST REG_AL Ib */
			oper1b = REG_AL;
			oper2b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			fakeflag_log8(oper1b & oper2b);
			break;

		case 0xA9:	/* A9 TEST eAX Iv */
			oper1 = REG_AX;
			oper2 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			fakeflag_log16(oper1 & oper2);
			break;

		case 0xAA:	/* AA STOSB */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			putmem8(REG_ES, REG_DI, REG_AL);
			if (FLAG_DF) {
				--REG_DI;
			}
			else {
				++REG_DI;
			}

			if (reptype) {
				--REG_CX;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0xAB:	/* AB STOSW */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			putmem16(REG_ES, REG_DI, REG_AX);
			if (FLAG_DF) {
				REG_DI -= 2;
			}
			else {
				REG_DI += 2;
			}

			if (reptype) {
				--REG_CX;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0xAC:	/* AC LODSB */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			REG_AL = getmem8(useseg, REG_SI);
			if (FLAG_DF) {
				--REG_SI;
			}
			else {
				++REG_SI;
			}

			if (reptype) {
				--REG_CX;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0xAD:	/* AD LODSW */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			oper1 = getmem16(useseg, REG_SI);
			REG_AX = oper1;
			if (FLAG_DF) {
				REG_SI -= 2;
			}
			else {
				REG_SI += 2;
			}

			if (reptype) {
				--REG_CX;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0xAE:	/* AE SCASB */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			oper1b = getmem8(REG_ES, REG_DI);
			oper2b = REG_AL;
			fakeflag_sub8(oper1b, oper2b);
			if (FLAG_DF) {
				--REG_DI;
			}
			else {
				++REG_DI;
			}

			if (reptype) {
				--REG_CX;
			}

			if ((reptype == 1) && !FLAG_ZF) {
				break;
			}
			else if ((reptype == 2) && (FLAG_ZF == 1)) {
				break;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0xAF:	/* AF SCASW */
			if (reptype && (REG_CX == 0)) {
				break;
			}

			oper1 = getmem16(REG_ES, REG_DI);
			oper2 = REG_AX;
			fakeflag_sub16(oper1, oper2);
			if (FLAG_DF) {
				REG_DI -= 2;
			}
			else {
				REG_DI += 2;
			}

			if (reptype) {
				--REG_CX;
			}

			if ((reptype == 1) && !FLAG_ZF) {
				break;
			}
			else if ((reptype == 2) & (FLAG_ZF == 1)) {
				break;
			}

			if (!reptype) {
				break;
			}

			REG_IP = firstip;
			break;

		case 0xB0:	/* B0 MOV REG_AL Ib */
			REG_AL = getmem8(REG_CS, REG_IP);
			StepIP(1);
			break;

		case 0xB1:	/* B1 MOV REG_CL Ib */
			REG_CL = getmem8(REG_CS, REG_IP);
			StepIP(1);
			break;

		case 0xB2:	/* B2 MOV REG_DL Ib */
			REG_DL = getmem8(REG_CS, REG_IP);
			StepIP(1);
			break;

		case 0xB3:	/* B3 MOV REG_BL Ib */
			REG_BL = getmem8(REG_CS, REG_IP);
			StepIP(1);
			break;

		case 0xB4:	/* B4 MOV REG_AH Ib */
			REG_AH = getmem8(REG_CS, REG_IP);
			StepIP(1);
			break;

		case 0xB5:	/* B5 MOV REG_CH Ib */
			REG_CH = getmem8(REG_CS, REG_IP);
			StepIP(1);
			break;

		case 0xB6:	/* B6 MOV REG_DH Ib */
			REG_DH = getmem8(REG_CS, REG_IP);
			StepIP(1);
			break;

		case 0xB7:	/* B7 MOV REG_BH Ib */
			REG_BH = getmem8(REG_CS, REG_IP);
			StepIP(1);
			break;

		case 0xB8:	/* B8 MOV eAX Iv */
			oper1 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			REG_AX = oper1;
			break;

		case 0xB9:	/* B9 MOV eCX Iv */
			oper1 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			REG_CX = oper1;
			break;

		case 0xBA:	/* BA MOV eDX Iv */
			oper1 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			REG_DX = oper1;
			break;

		case 0xBB:	/* BB MOV eBX Iv */
			oper1 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			REG_BX = oper1;
			break;

		case 0xBC:	/* BC MOV eSP Iv */
			REG_SP = getmem16(REG_CS, REG_IP);
			StepIP(2);
			break;

		case 0xBD:	/* BD MOV eBP Iv */
			REG_BP = getmem16(REG_CS, REG_IP);
			StepIP(2);
			break;

		case 0xBE:	/* BE MOV eSI Iv */
			REG_SI = getmem16(REG_CS, REG_IP);
			StepIP(2);
			break;

		case 0xBF:	/* BF MOV eDI Iv */
			REG_DI = getmem16(REG_CS, REG_IP);
			StepIP(2);
			break;

		case 0xC0:	/* C0 GRP2 byte imm8 (80186+) */
			modregrm();
			oper1b = readrm8(rm);
			oper2b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			writerm8(rm, fakeop_grp2_8(oper2b));
			break;

		case 0xC1:	/* C1 GRP2 word imm8 (80186+) */
			modregrm();
			oper1 = readrm16(rm);
			oper2 = getmem8(REG_CS, REG_IP);
			StepIP(1);
			writerm16(rm, fakeop_grp2_16((uint8_t)oper2));
			break;

		case 0xC2:	/* C2 RET Iw */
			oper1 = getmem16(REG_CS, REG_IP);
			REG_IP = pop();
			REG_SP += oper1;
			break;

		case 0xC3:	/* C3 RET */
			REG_IP = pop();
			break;

		case 0xC4:	/* C4 LES Gv Mp */
			modregrm();
			getea(rm);
			putreg16(reg, read86(ea) + read86(ea + 1) * 256);
			REG_ES = read86(ea + 2) + read86(ea + 3) * 256;
			break;

		case 0xC5:	/* C5 LDS Gv Mp */
			modregrm();
			getea(rm);
			putreg16(reg, read86(ea) + read86(ea + 1) * 256);
			REG_DS = read86(ea + 2) + read86(ea + 3) * 256;
			break;

		case 0xC6:	/* C6 MOV Eb Ib */
			modregrm();
			writerm8(rm, getmem8(REG_CS, REG_IP));
			StepIP(1);
			break;

		case 0xC7:	/* C7 MOV Ev Iv */
			modregrm();
			writerm16(rm, getmem16(REG_CS, REG_IP));
			StepIP(2);
			break;

		case 0xC8:	/* C8 ENTER (80186+) */
			stacksize = getmem16(REG_CS, REG_IP);
			StepIP(2);
			nestlev = getmem8(REG_CS, REG_IP);
			StepIP(1);
			push(REG_BP);
			frametemp = REG_SP;
			if (nestlev) {
				for (temp16 = 1; temp16 < nestlev; temp16++) {
					REG_BP -= 2;
					push(REG_BP);
				}

				push(REG_SP);
			}

			REG_BP = frametemp;
			REG_SP = REG_BP - stacksize;

			break;

		case 0xC9:	/* C9 LEAVE (80186+) */
			REG_SP = REG_BP;
			REG_BP = pop();

			break;

		case 0xCA:	/* CA RETF Iw */
			oper1 = getmem16(REG_CS, REG_IP);
			REG_IP = pop();
			REG_CS = pop();
			REG_SP += oper1;
			break;

		case 0xCB:	/* CB RETF */
			REG_IP = pop();;
			REG_CS = pop();
			break;

		case 0xCC:	/* CC INT 3 */
			intcall86(3);
			break;

		case 0xCD:	/* CD INT Ib */
			oper1b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			intcall86(oper1b);
			break;

		case 0xCE:	/* CE INTO */
			if (FLAG_OF) {
				intcall86(4);
			}
			break;

		case 0xCF:	/* CF IRET */
			REG_IP = pop();
			REG_CS = pop();
			decodeflagsword(pop());

			/*
			* if (net.enabled) net.canrecv = 1;
			*/
			break;

		case 0xD0:	/* D0 GRP2 Eb 1 */
			modregrm();
			oper1b = readrm8(rm);
			writerm8(rm, fakeop_grp2_8(1));
			break;

		case 0xD1:	/* D1 GRP2 Ev 1 */
			modregrm();
			oper1 = readrm16(rm);
			writerm16(rm, fakeop_grp2_16(1));
			break;

		case 0xD2:	/* D2 GRP2 Eb REG_CL */
			modregrm();
			oper1b = readrm8(rm);
			writerm8(rm, fakeop_grp2_8(REG_CL));
			break;

		case 0xD3:	/* D3 GRP2 Ev REG_CL */
			modregrm();
			oper1 = readrm16(rm);
			writerm16(rm, fakeop_grp2_16(REG_CL));
			break;

		case 0xD4:	/* D4 AAM I0 */
			oper1 = getmem8(REG_CS, REG_IP);
			StepIP(1);
			if (!oper1) {
				intcall86(0);
				break;
			}	/* division by zero */

			REG_AH = (REG_AL / oper1) & 255;
			REG_AL = (REG_AL % oper1) & 255;
			fakeflag_szp16(REG_AX);
			break;

		case 0xD5:	/* D5 AAD I0 */
			oper1 = getmem8(REG_CS, REG_IP);
			StepIP(1);
			REG_AL = (REG_AH * oper1 + REG_AL) & 255;
			REG_AH = 0;
			fakeflag_szp16(REG_AH * oper1 + REG_AL);
			FLAG_SF = 0;
			break;

		case 0xD6:	/* D6 XLAT on V20/V30, SALC on 8086/8088 */
#ifndef CPU_V20
			REG_AL = FLAG_CF ? 0xFF : 0x00;
			break;
#endif

		case 0xD7:	/* D7 XLAT */
			REG_AL = read86(useseg * 16 + REG_BX + REG_AL);
			break;

		case 0xD8:
		case 0xD9:
		case 0xDA:
		case 0xDB:
		case 0xDC:
		case 0xDE:
		case 0xDD:
		case 0xDF:	/* escape to x87 FPU (unsupported) */
			modregrm();
			break;

		case 0xE0:	/* E0 LOOPNZ Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			--REG_CX;
			if ((REG_CX) && !FLAG_ZF) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0xE1:	/* E1 LOOPZ Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			--REG_CX;
			if ((REG_CX) && (FLAG_ZF == 1)) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0xE2:	/* E2 LOOP Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			--REG_CX;
			if (REG_CX) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0xE3:	/* E3 JCXZ Jb */
			temp16 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			if (!(REG_CX)) {
				REG_IP = REG_IP + temp16;
			}
			break;

		case 0xE4:	/* E4 IN REG_AL Ib */
			oper1b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			REG_AL = (uint8_t)PORT_IN_B(oper1b);
			break;

		case 0xE5:	/* E5 IN eAX Ib */
			oper1b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			REG_AX = PORT_IN_W(oper1b);
			break;

		case 0xE6:	/* E6 OUT Ib REG_AL */
			oper1b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			PORT_OUT_B(oper1b, REG_AL);
			break;

		case 0xE7:	/* E7 OUT Ib eAX */
			oper1b = getmem8(REG_CS, REG_IP);
			StepIP(1);
			PORT_OUT_W(oper1b, (REG_AX));
			break;

		case 0xE8:	/* E8 CALL Jv */
			oper1 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			push(REG_IP);
			REG_IP = REG_IP + oper1;
			break;

		case 0xE9:	/* E9 JMP Jv */
			oper1 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			REG_IP = REG_IP + oper1;
			break;

		case 0xEA:	/* EA JMP Ap */
			oper1 = getmem16(REG_CS, REG_IP);
			StepIP(2);
			oper2 = getmem16(REG_CS, REG_IP);
			REG_IP = oper1;
			REG_CS = oper2;
			break;

		case 0xEB:	/* EB JMP Jb */
			oper1 = signext(getmem8(REG_CS, REG_IP));
			StepIP(1);
			REG_IP = REG_IP + oper1;
			break;

		case 0xEC:	/* EC IN REG_AL regdx */
			oper1 = (REG_DX);
			REG_AL = (uint8_t)PORT_IN_B(oper1);
			break;

		case 0xED:	/* ED IN eAX regdx */
			oper1 = (REG_DX);
			REG_AX = PORT_IN_W(oper1);
			break;

		case 0xEE:	/* EE OUT regdx REG_AL */
			oper1 = (REG_DX);
			PORT_OUT_B(oper1, REG_AL);
			break;

		case 0xEF:	/* EF OUT regdx eAX */
			oper1 = (REG_DX);
			PORT_OUT_W(oper1, REG_AX);
			break;

		case 0xF0:	/* F0 LOCK */
			break;

		case 0xF4:	/* F4 HLT */
			REG_IP--;
			break;

		case 0xF5:	/* F5 CMC */
			if (!FLAG_CF) {
				FLAG_CF = 1;
			}
			else {
				FLAG_CF = 0;
			}
			break;

		case 0xF6:	/* F6 GRP3a Eb */
			modregrm();
			oper1b = readrm8(rm);
			fakeop_grp3_8();
			if ((reg > 1) && (reg < 4)) {
				writerm8(rm, res8);
			}
			break;

		case 0xF7:	/* F7 GRP3b Ev */
			modregrm();
			oper1 = readrm16(rm);
			fakeop_grp3_16();
			if ((reg > 1) && (reg < 4)) {
				writerm16(rm, res16);
			}
			break;

		case 0xF8:	/* F8 CLC */
			FLAG_CF = 0;
			break;

		case 0xF9:	/* F9 STC */
			FLAG_CF = 1;
			break;

		case 0xFA:	/* FA CLI */
			FLAG_IF = 0;
			break;

		case 0xFB:	/* FB STI */
			FLAG_IF = 1;
			break;

		case 0xFC:	/* FC CLD */
			FLAG_DF = 0;
			break;

		case 0xFD:	/* FD STD */
			FLAG_DF = 1;
			break;

		case 0xFE:	/* FE GRP4 Eb */
			modregrm();
			oper1b = readrm8(rm);
			oper2b = 1;
			if (!reg) {
				tempcf = FLAG_CF;
				res8 = oper1b + oper2b;
				fakeflag_add8(oper1b, oper2b);
				FLAG_CF = tempcf;
				writerm8(rm, res8);
			}
			else if (reg == 7) //---: Special: callback handler!
			{
				CB_handler(CPU_readOPw()); //Call special handler!
			}
			else {
				tempcf = FLAG_CF;
				res8 = oper1b - oper2b;
				fakeflag_sub8(oper1b, oper2b);
				FLAG_CF = tempcf;
				writerm8(rm, res8);
			}
			break;

		case 0xFF:	/* FF GRP5 Ev */
			modregrm();
			oper1 = readrm16(rm);
			fakeop_grp5();
			break;

		default:
#ifdef CPU_V20
			intcall86(6); /* trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs. */
			/* technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough. */
#endif
			break;
		}

		if (!running) {
			return;
		}
}