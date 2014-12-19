#include "headers/types.h" //Basic types!
#include "headers/cpu/8086/cpu_OP8086.h" //8086 Opcode support!
#include "headers/cpu/cpu.h" //For registers!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/cpu/flags.h" //Flag modifiers!
#include "headers/support/signedness.h" //For signext replacement!
#include "headers/mmu/mmu.h" //MMU support!
#include "headers/cpu/80286/protection.h" //Protection support!

byte immb; //For CPU_readOP result!
word immw; //For CPU_readOPw result!
byte oper1b, oper2b; //Byte variants!
word oper1, oper2; //Word variants!
byte res8; //Result 8-bit!
word res16; //Result 16-bit!
byte reg; //For function number!
uint_32 ea; //From RM OFfset (GRP5 Opcodes only!)
byte tempCF;

union
{
	struct
	{
		union
		{
			struct
			{
				union
				{
					byte val8;
					sbyte val8s;
				};
				byte val8high;
			};
			word val16; //Normal
			sword val16s; //Signed
		};
		word val16high; //Filler
	};
	uint_32 val32; //Normal
	int_32 val32s; //Signed
} temp1, temp2, temp3, temp4, temp5; //All temporary values!
uint_32 temp32, tempaddr32;
extern MODRM_PARAMS params; //The modr/m params!

void flag_sp16(uint16_t value) //Extension of old sp16!
{
	byte oldz;
	oldz = ZF; //Save!
	flag_szp16(value); //Flags!
	ZF = oldz; //Restore!
}

extern uint8_t parity8[0x100]; //Our parity table!

void flag_sp32(uint8_t value) //New created from sp16!
{
	SF = ((value&0x80000000)>0);
	PF = parity8[value & 0xFF];	/* retrieve parity state from lookup table */
}

byte op_grp2_8(byte cnt) {
	//word d,
	word s, shift, oldCF, msb;
	//if (cnt>0x8) return(oper1b); //80186+ limits shift count
	s = oper1b;
	oldCF = CF;
	switch (reg) {
		case 0: //ROL r/m8
		for (shift=1; shift<=cnt; shift++) {
			if (s & 0x80) CF = 1; else CF = 0;
			s = s << 1;
			s = s | CF;
		}
		if (cnt==1) OF = CF ^ ((s >> 7) & 1);
		break;
		
		case 1: //ROR r/m8
		for (shift=1; shift<=cnt; shift++) {
			CF = s & 1;
			s = (s >> 1) | (CF << 7);
		}
		if (cnt==1) OF = (s >> 7) ^ ((s >> 6) & 1);
		break;
		
		case 2: //RCL r/m8
		for (shift=1; shift<=cnt; shift++) {
			oldCF = CF;
			if (s & 0x80) CF = 1; else CF = 0;
			s = s << 1;
			s = s | oldCF;
		}
		if (cnt==1) OF = CF ^ ((s >> 7) & 1);
		break;
		
		case 3: //RCR r/m8
		for (shift=1; shift<=cnt; shift++) {
			oldCF = CF;
			CF = s & 1;
			s = (s >> 1) | (oldCF << 7);
		}
		if (cnt==1) OF = (s >> 7) ^ ((s >> 6) & 1);
		break;
		
		case 4: case 6: //SHL r/m8
		for (shift=1; shift<=cnt; shift++) {
			if (s & 0x80) CF = 1; else CF = 0;
			s = (s << 1) & 0xFF;
		}
		if ((cnt==1) && (CF==(s>>7))) OF = 0; else OF = 1;
		flag_szp8(s); break;
		
		case 5: //SHR r/m8
		if ((cnt==1) && (s & 0x80)) OF = 1; else OF = 0;
		for (shift=1; shift<=cnt; shift++) {
			CF = s & 1;
			s = s >> 1;
		}
		flag_szp8(s); break;
		
		case 7: //SAR r/m8
		for (shift=1; shift<=cnt; shift++) {
			msb = s & 0x80;
			CF = s & 1;
			s = (s >> 1) | msb;
		}
		OF = 0;
		flag_szp8(s); break;
		
	}
	return(s & 0xFF);
}

word op_grp2_16(byte cnt) {
	//uint32_t d,
	uint_32 s, shift, oldCF, msb;
	//if (cnt>0x10) return(oper1); //80186+ limits shift count
	s = oper1;
	oldCF = CF;
	dolog("debugger","before shift: %04X,%04X",s,cnt); //Show the old value!
	switch (reg) {
		case 0: //ROL r/m8
		for (shift=1; shift<=cnt; shift++) {
			if (s & 0x8000) CF = 1; else CF = 0;
			s = s << 1;
			s = s | CF;
		}
		if (cnt==1) OF = CF ^ ((s >> 15) & 1);
		break;
		
		case 1: //ROR r/m8
		for (shift=1; shift<=cnt; shift++) {
			CF = s & 1;
			s = (s >> 1) | (CF << 15);
		}
		if (cnt==1) OF = (s >> 15) ^ ((s >> 14) & 1);
		break;
		
		case 2: //RCL r/m8
		for (shift=1; shift<=cnt; shift++) {
			oldCF = CF;
			if (s & 0x8000) CF = 1; else CF = 0;
			s = s << 1;
			s = s | oldCF;
			//oldCF = ((s&0x8000)>>15)&1; //Save CF!
			//s = (s<<1)+CF;
			//CF = oldCF;
		}
		if (cnt==1) OF = CF ^ ((s >> 15) & 1);
		break;
		
		case 3: //RCR r/m8
		if (cnt==1) OF = ((s>>15)&1)^CF;
		for (shift=1; shift<=cnt; shift++) {
			oldCF = CF;
			CF = s & 1;
			s = (s >> 1) | (oldCF << 15);
			//oldCF = s&1;
			//s = (s<<1)+(CF<<16);
			//CF = oldCF;
		}
		if (cnt==1) OF = (s >> 15) ^ ((s >> 14) & 1);
		break;
		
		case 4: case 6: //SHL r/m8
		for (shift=1; shift<=cnt; shift++) {
			if (s & 0x8000) CF = 1; else CF = 0;
			s = (s << 1) & 0xFFFF;
		}
		if ((cnt==1) && (CF==(s>>15))) OF = 0; else OF = 1;
		flag_szp16(s); break;
		
		case 5: //SHR r/m8
		if ((cnt==1) && (s & 0x8000)) OF = 1; else OF = 0;
		for (shift=1; shift<=cnt; shift++) {
			CF = s & 1;
			s = s >> 1;
		}
		flag_szp16(s); break;
		
		case 7: //SAR r/m8
		for (shift=1; shift<=cnt; shift++) {
			msb = s & 0x8000;
			CF = s & 1;
			s = (s >> 1) | msb;
		}
		OF = 0;
		flag_szp16(s); break;
	}
	dolog("debugger","after shift: %04X",s&0xFFFF); //Show the old value!	
	return(s & 0xFFFF);
}

void op_div8(word valdiv, byte divisor) {
	if (!divisor) { CPU_exDIV0(); return; }
	if ((valdiv / (word)divisor) > 0xFF) { CPU_exDIV0(); return; }
	AH = valdiv % (word)divisor;
	AL = valdiv / (word)divisor;
}

void op_idiv8(word valdiv, byte divisor) {
	//word v1, v2,
	if (divisor==0) { CPU_exDIV0(); return; }
	/*
	word s1, s2, d1, d2;
	int sign;
	s1 = valdiv;
	s2 = divisor;
	sign = (((s1 ^ s2) & 0x8000) != 0);
	s1 = (s1 < 0x8000) ? s1 : ((~s1 + 1) & 0xffff);
	s2 = (s2 < 0x8000) ? s2 : ((~s2 + 1) & 0xffff);
	d1 = s1 / s2;
	d2 = s1 % s2;
	if (d1 & 0xFF00) { CPU_exDIV0(); return; }
	if (sign) {
		d1 = (~d1 + 1) & 0xff;
		d2 = (~d2 + 1) & 0xff;
	}
	AH = d2;
	AL = d1;
	*/
	
	//Same, but with normal instructions!
	union
	{
		word valdivw;
		sword valdivs;
	} dataw1, //For loading the signed value of the registers!
		dataw2; //For performing calculations!
	
	union
	{
		byte divisorb;
		sbyte divisors;
	} datab1, //For loading the data
		datab2; //For loading the result and test it against overflow!
		//For converting the data to signed values!
	
	dataw1.valdivw = valdiv; //Load word!
	datab1.divisorb = divisor; //Load divisor!
	
	dataw2.valdivs = dataw1.valdivs; //Set and...
	dataw2.valdivs /= datab1.divisors; //... Divide!
	
	datab2.divisors = (sbyte)dataw2.valdivs; //Try to load the signed result!	
	if (datab2.divisors!=dataw2.valdivs) {CPU_exDIV0(); return;} //Overflow (data loss)!
	
	AL = datab2.divisors; //Divided!
	dataw2.valdivs = dataw1.valdivs; //Reload and...
	dataw2.valdivs %= datab1.divisors; //... Modulo!
	datab1.divisors = (sbyte)dataw2.valdivs; //Convert to 8-bit!
	AH = datab1.divisorb; //Move rest into result!
	
	//if (valdiv > 32767) v1 = valdiv - 65536; else v1 = valdiv;
	//if (divisor > 127) v2 = divisor - 256; else v2 = divisor;
	//v1 = valdiv;
	//v2 = signext(divisor);
	//if ((v1/v2) > 255) { CPU8086_int(0); return; }
	//regs.byteregs[regal] = (v1/v2) & 255;
	//regs.byteregs[regah] = (v1 % v2) & 255;
}

void op_grp3_8() {
	//uint32_t d1, d2, s1, s2, sign;
	//word d, s;
	oper1 = signext(oper1b); oper2 = signext(oper2b);
	switch (reg) {
		case 0: case 1: //TEST
		flag_log8(oper1b & immb);
		break;
		
		case 2: //NOT
		res8 = ~oper1b; break;
		
		case 3: //NEG
		res8 = (~oper1b)+1;
		flag_sub8(0, oper1b);
		if (res8==0) CF = 0; else CF = 1;
		break;
		
		case 4: //MULB
		temp1.val32 = (uint32_t)oper1b * (uint32_t)AL;
		AX = temp1.val16 & 0xFFFF;
		flag_szp8(temp1.val16);
		CF = OF = (AX!=AL);
		break;
		
		case 5: //IMULB
		oper1 = oper1b;
		temp1.val32 = AL;
		temp2.val32 = oper1;
		//Sign extend!
		if ((temp1.val8 & 0x80)==0x80) temp1.val32 |= 0xFFFFFF00;
		if ((temp2.val8 & 0x80)==0x80) temp2.val32 |= 0xFFFFFF00;
		//Multiply and convert to 16-bit!
		temp3.val32s = temp1.val32s; //Load and...
		temp3.val32s *= temp2.val32s; //Multiply!
		dolog("debugger","IMULB:%ix%i=%i",temp1.val32s,temp2.val32s,temp3.val32s); //Show the result!
		AX = temp3.val16; //Load into AX!
		flag_sp16(AX); //Flags!
		CF = OF = (temp3.val8s!=temp3.val16s);
		break;
		
		case 6: //DIV
		op_div8(AX, oper1b);
		break;
		
		case 7: //IDIV
		op_idiv8(AX, oper1b);
		break;
	}
}

void op_div16(uint32_t valdiv, word divisor) {
	//word v1, v2;
	if (!divisor) { CPU_exDIV0(); return; }
	if ((valdiv / (uint32_t)divisor) > 0xFFFF) { CPU_exDIV0(); return; }
	DX = valdiv % (uint32_t)divisor;
	AX = valdiv / (uint32_t)divisor;
}

void op_idiv16(uint32_t valdiv, word divisor) {
	//uint32_t v1, v2,
	
	if (!divisor) { CPU_exDIV0(); return; }
	/*
	uint_32 d1, d2, s1, s2;
	int sign;
	s1 = valdiv;
	s2 = divisor;
	s2 = (s2 & 0x8000) ? (s2 | 0xffff0000) : s2;
	sign = (((s1 ^ s2) & 0x80000000) != 0);
	s1 = (s1 < 0x80000000) ? s1 : ((~s1 + 1) & 0xffffffff);
	s2 = (s2 < 0x80000000) ? s2 : ((~s2 + 1) & 0xffffffff);
	d1 = s1 / s2;
	d2 = s1 % s2;
	if (d1 & 0xFFFF0000) { CPU_exDIV0(); return; }
	if (sign) {
		d1 = (~d1 + 1) & 0xffff;
		d2 = (~d2 + 1) & 0xffff;
	}
	AX = d1;
	DX = d2;
	*/
	
		//Same, but with normal instructions!
	union
	{
		uint_32 valdivw;
		int_32 valdivs;
	} dataw1, //For loading the signed value of the registers!
		dataw2; //For performing calculations!
	
	union
	{
		word divisorb;
		sword divisors;
	} datab1, datab2; //For converting the data to signed values!
	
	dataw1.valdivw = valdiv; //Load word!
	datab1.divisorb = divisor; //Load divisor!
	
	dataw2.valdivs = dataw1.valdivs; //Set and...
	dataw2.valdivs /= datab1.divisors; //... Divide!
	
	datab2.divisors = (sword)dataw2.valdivs; //Try to load the signed result!
	if (dataw2.valdivw!=datab2.divisors) {CPU_exDIV0(); return;} //Overflow (data loss)!
	
	AX = datab2.divisorb; //Divided!
	dataw2.valdivs = dataw1.valdivs; //Reload and...
	dataw2.valdivs %= datab1.divisors; //... Modulo!
	datab1.divisors = (sword)dataw2.valdivs; //Convert to 8-bit!
	DX = datab1.divisorb; //Move rest into result!

	//if (valdiv > 0x7FFFFFFF) v1 = valdiv - 0xFFFFFFFF - 1; else v1 = valdiv;
	//if (divisor > 32767) v2 = divisor - 65536; else v2 = divisor;
	//if ((v1/v2) > 65535) { CPU8086_int(0); return; }
	//temp3 = (v1/v2) & 65535;
	//regs.wordregs[regax] = temp3;
	//temp3 = (v1%v2) & 65535;
	//regs.wordregs[regdx] = temp3;
}

void op_grp3_16() {
	//uint32_t d1, d2, s1, s2, sign;
	//word d, s;
	//oper1 = signext(oper1b); oper2 = signext(oper2b);
	//sprintf(msg, "  Oper1: %04X    Oper2: %04X\n", oper1, oper2); print(msg);
	switch (reg) {
		case 0: case 1: //TEST
		flag_log16(oper1 & immw); break;
		case 2: //NOT
		res16 = ~oper1; break;
		case 3: //NEG
		res16 = (~oper1) + 1;
		flag_sub16(0, oper1);
		if (res16) CF = 1; else CF = 0;
		break;
		case 4: //MULW
		temp1.val32 = (uint32_t)oper1 * (uint32_t)AX;
		AX = temp1.val16;
		DX = (temp1.val32 >> 16);
		flag_szp16(temp1.val16);
		if (DX) { CF = OF = 1; } else { CF = OF = 0; }
		break;
		case 5: //IMULW
		temp1.val32 = AX;
		temp2.val32 = oper1;
		//Sign extend!
		if (temp1.val16 & 0x8000) temp1.val32 |= 0xFFFF0000;
		if (temp2.val16 & 0x8000) temp2.val32 |= 0xFFFF0000;
		temp3.val32s = temp1.val32s; //Load and...
		temp3.val32s *= temp2.val32s; //Signed multiplication!
		dolog("debugger","IMULW:%ix%i=%i",temp1.val32s,temp2.val32s,temp3.val32s); //Show the result!
		AX = temp3.val16; //into register ax
		DX = temp3.val16high; //into register dx
		
		CF = OF = (temp3.val16s!=temp3.val32s); //Overflow occurred?
		flag_sp32(temp3.val32); //Flags!
		break;
		case 6: //DIV
		op_div16(((uint32_t)DX<<16) | AX, oper1); break;
		case 7: //IDIV
		op_idiv16(((uint32_t)DX<<16) | AX, oper1); break;
	}
}

void op_grp5() {
	MODRM_PTR info; //To contain the info!
	switch (reg) {
		case 0: //INC Ev
		oper2 = 1;
		tempCF = CF;
		op_add16();
		CF = tempCF;
		modrm_write16(&params,2, res16,0); break;
		case 1: //DEC Ev
		oper2 = 1;
		tempCF = CF;
		op_sub16();
		CF = tempCF;
		modrm_write16(&params,2, res16,0); break;
		case 2: //CALL Ev
		CPU_PUSH16(&IP);
		IP = oper1; break;
		case 3: //CALL Mp
		CPU_PUSH16(&CS); CPU_PUSH16(&IP);
		modrm_decode16(&params,&info,2); //Get data!
		IP = (word)MMU_rw(get_segment_index(info.segmentregister),info.mem_segment,info.mem_offset,0);
		segmentWritten(CPU_SEGMENT_CS,MMU_rw(get_segment_index(info.segmentregister),info.mem_segment,info.mem_offset + 2,0),2);
		break;
		case 4: //JMP Ev
		IP = oper1; break;
		case 5: //JMP Mp
		modrm_decode16(&params,&info,2); //Get data!
		IP = (word)MMU_rw(get_segment_index(info.segmentregister),info.mem_segment,info.mem_offset,0);
		segmentWritten(CPU_SEGMENT_CS,MMU_rw(get_segment_index(info.segmentregister),info.mem_segment,info.mem_offset + 2,0),1);
		break;
		case 6: //PUSH Ev
		CPU_PUSH16(&oper1); break;
	}
}