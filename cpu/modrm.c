#include "headers/cpu/modrm.h" //Need support!
#include "headers/cpu/modrm.h" //Need support!
#include "headers/cpu/cpu.h" //For the registers!
#include "headers/mmu/mmu.h" //For MMU!
#include "headers/cpu/easyregs.h" //Easy register compatibility!
#include "headers/support/signedness.h" //Basic CPU support (mainly for unsigned2signed)
#include "headers/cpu/80286/protection.h" //Protection support!
#include "headers/support/zalloc.h" //memory and cpu register protection!
#include "headers/support/log.h" //For logging invalid registers!

//For CPU support:
#include "headers/cpu/modrm.h" //MODR/M (type) support!
#include "headers/debugger/debugger.h" //debugging() functionality!
#include "headers/emu/gpu/gpu_text.h" //Text support!

//Log invalid registers?
#define LOG_INVALID_REGISTERS 0

//First write 8-bits, 16-bits and 32-bits!

//Pres for our functions calling them (read/write functions):
void modrm_decode8(MODRM_PARAMS *params, MODRM_PTR *result, byte whichregister);
void modrm_decode16(MODRM_PARAMS *params, MODRM_PTR *result, byte whichregister); //16-bit address/reg decoder!
void modrm_decode32(MODRM_PARAMS *params, MODRM_PTR *result, byte whichregister); //32-bit address/reg decoder!


int modrm_isregister(MODRM_PARAMS *params)
{
	return (MODRM_MOD(params->modrm)==MOD_REG); //Register?
}

int modrm_ismemory(MODRM_PARAMS *params)
{
	return (MODRM_MOD(params->modrm)!=MOD_REG); //No register (must be memory!)?
}

int modrm_useSIB(MODRM_PARAMS *params, int size) //Use SIB byte?
{
	if (size==4) //32-bit mode?
	{
		if (MODRM_RM(params->modrm) == 4 && MODRM_MOD(params->modrm) != 3) //Have a SIB byte?
		{
			return 1; //We use a SIB byte!
		}
	}
	return 0; //NO SIB!
}

int modrm_useDisplacement(MODRM_PARAMS *params, int size)
{
	/*
	Result:
	0: No displacement
	1: Byte displacement
	2: Word displacement
	4: DWord displacement
	*/

	if (params->slashr) //No displacement on /r operands: REQUIRED FOR SOME OPCODES!!!
	{
		return 0; //No displacement!
	}

	if ((size==2) || (size==1))   //16 bits operand size?
	{
		//figure out 16 bit displacement size
		switch (MODRM_MOD(params->modrm)) //MOD?
		{
		case 0:
			if (MODRM_RM(params->modrm) == 6) //[sword]?
				return 2; //Word displacement!
			else
				return 0; //No displacement!
			break;
		case 1:
			return 1; //Byte displacement!
			break;
		case 2:
			return 2; //Word displacement!
			break;
		default:
		case 3:
			return 0; //No displacement!
			break;
		}
	}
	else     //32/64 bit operand size?
	{
		//figure out 32/64 bit displacement size
		switch (MODRM_MOD(params->modrm)) //MOD?
		{
		case 0:
			if (MODRM_RM(params->modrm) == 6) //[sword]?
				return 4; //DWord displacement!
			else
				return 0; //No displacement!
			break;
		case 1:
			return 1; //Byte displacement!
			break;
		case 2:
			return 4; //DWord displacement!
			break;
		default:
		case 3:
			return 0; //No displacement!
			break;
		}
	}

	return 0; //Give displacement size in bytes (unknown)!
//Use displacement (1||2||4) else 0?
}

//Retrieves base offset to use
uint_32 modrm_internal_offset8(byte regval)
{
	return (uint_32)(regval&0xFF); //8-bit offset. Add EIP higher rest bits?
}

uint_32 modrm_internal_offset16(word regval) //Offset for word register!
{
	return (uint_32)(regval&0xFFFF); //16-bit offset. Add EIP higher rest bits?
}

uint_32 modrm_internal_offset32(uint_32 regval) //Offset for dword register!
{
	return regval; //Use the register: 32-bits already!
}

int modrm_getmod(MODRM_PARAMS *params) //Get MOD bonus parameter size!
{
	if (MODRM_MOD(params->modrm)==0 || MODRM_MOD(params->modrm)==3) //None?
	{
		return 0; //No bonus!
	}
	else //Bonus parameter?
	{
		return MODRM_MOD(params->modrm); //Give bonus parameter size!
	}
	return MODRM_MOD(params->modrm); //Dummy: not executed!
}

void modrm_updatedsegment(word *location, word value, byte isJMPorCALL) //Check for updated segment registers!
{
	//Check for updated registers!
	int index = get_segment_index(location); //Get the index!
	if (index!=-1) //Gotten?
	{
		segmentWritten(index,value,isJMPorCALL); //Update when possible!
	}
}

word modrm_lastsegment;
uint_32 modrm_lastoffset;
byte last_modrm; //Is the last opcode a modr/m read?

void reset_modrm()
{
	last_modrm = 0; //Last wasn't a modr/m anymore by default!
}

void modrm_write8(MODRM_PARAMS *params, int whichregister, byte value)
{
	MODRM_PTR info; //To contain the info!
	modrm_decode8(params,&info,whichregister); //Get data!
	byte *result; //The result holder if needed!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		result = (byte *)memprotect(info.reg8,1,"CPU_REGISTERS"); //Give register!
		if (result) //Gotten?
		{
			*result = value; //Write the data to the result!
		}
		else if (LOG_INVALID_REGISTERS)
		{
			dolog("debugger","Write to 8-bit register failed: not registered!");
		}
		break;
	case 2: //Memory?
		last_modrm = 1; //ModR/M!
		modrm_lastsegment = info.mem_segment;
		modrm_lastoffset = info.mem_offset;
		MMU_wb(info.segmentregister_index,info.mem_segment,info.mem_offset,value); //Write the data to memory using byte depth!
		break;
		//return result; //Give memory!
	default:
		halt_modrm("MODRM: Unknown MODR/M8!");
		break;
	}
}

void modrm_write16(MODRM_PARAMS *params, int whichregister, word value, byte isJMPorCALL)
{
	MODRM_PTR info; //To contain the info!
	modrm_decode16(params,&info,whichregister); //Get data!
	word *result; //The result holder if needed!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		result = (word *)memprotect(info.reg16,2,"CPU_REGISTERS"); //Give register!
		if (result) //Gotten?
		{
			*result = value; //Write the data to the result!
			modrm_updatedsegment(result,value,0); //Plain update of the segment register, if needed!
		}
		else if (LOG_INVALID_REGISTERS)
		{
			dolog("debugger","Write to 16-bit register failed: not registered!");
		}
		break;
	case 2: //Memory?
		MMU_ww(info.segmentregister_index,info.mem_segment,info.mem_offset,value); //Write the data to memory using byte depth!
		break;
	default:
		halt_modrm("MODRM: Unknown MODR/M16!");
		break;
	}	
}

void modrm_write32(MODRM_PARAMS *params, int whichregister, uint_32 value)
{
	MODRM_PTR info; //To contain the info!
	modrm_decode8(params,&info,whichregister); //Get data!
	uint_32 *result; //The result holder if needed!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		result = (uint_32 *)memprotect(info.reg32,4,"CPU_REGISTERS"); //Give register!
		if (result) //Gotten?
		{
			*result = value; //Write the data to the result!
		}
		else if (LOG_INVALID_REGISTERS)
		{
			dolog("debugger","Write to 32-bit register failed: not registered!");
		}
		break;
	case 2: //Memory?
		last_modrm = 1; //ModR/M!
		modrm_lastsegment = info.mem_segment;
		modrm_lastoffset = info.mem_offset;
		
		MMU_wdw(info.segmentregister_index,info.mem_segment,info.mem_offset,value); //Write the data to memory using byte depth!
		break;
	default:
		halt_modrm("MODRM: Unknown MODR/M32!");
		break;
	}
}

byte modrm_read8(MODRM_PARAMS *params, int whichregister)
{
	MODRM_PTR info; //To contain the info!
	modrm_decode8(params,&info,whichregister); //Get data!
	byte *result; //The result holder if needed!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		result = (byte *)memprotect(info.reg8,1,"CPU_REGISTERS"); //Give register!
		if (result) //Valid?
		{
			return *result; //Read register!
		}
		else if (LOG_INVALID_REGISTERS)
		{
			dolog("debugger","Read from 8-bit register failed: not registered!");
		}
		break;
	case 2: //Memory?
		return MMU_rb(info.segmentregister_index,info.mem_segment,info.mem_offset,0); //Read the value from memory!
	default:
		halt_modrm("MODRM: Unknown MODR/M8!");
		return 0; //Unknown!
	}	
	return 0; //Default: unknown value!!
}

word modrm_read16(MODRM_PARAMS *params, int whichregister)
{
	MODRM_PTR info; //To contain the info!
	modrm_decode16(params,&info,whichregister); //Get data!
	word *result; //The result holder if needed!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		result = (word *)memprotect(info.reg16,2,"CPU_REGISTERS"); //Give register!
		if (result) //Valid?
		{
			return *result; //Read register!
		}
		else if (LOG_INVALID_REGISTERS)
		{
			dolog("debugger","Read from 16-bit register failed: not registered!");
		}
		break;
	case 2: //Memory?
		return MMU_rw(info.segmentregister_index,info.mem_segment,info.mem_offset,0); //Read the value from memory!
		
	default:
		halt_modrm("MODRM: Unknown MODR/M16!");
		return 0; //Unknown!
	}
	
	return 0; //Default: not reached!
}

uint_32 modrm_read32(MODRM_PARAMS *params, int whichregister)
{
	MODRM_PTR info; //To contain the info!
	modrm_decode32(params,&info,whichregister); //Get data!
	uint_32 *result; //The result holder if needed!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		result = (uint_32 *)memprotect(info.reg32,4,"CPU_REGISTERS"); //Give register!
		if (result) //Valid?
		{
			return *result; //Read register!
		}
		else if (LOG_INVALID_REGISTERS)
		{
			dolog("debugger","Read from 32-bit register failed: not registered!");
		}
		break;
	case 2: //Memory?
		return MMU_rdw(info.segmentregister_index,info.mem_segment,info.mem_offset,0); //Read the value from memory!
		
	default:
		halt_modrm("MODRM: Unknown MODR/M32!");
		return 0; //Unknown!
	}	
	
	return 0; //Default: not reached!
}











uint_32 dummy_ptr; //Dummy pointer!
word dummy_ptr16; //16-bit dummy ptr!









//Simple adressing functions:

//Conversion to signed text:

char signednumbertext[256];

char *unsigned2signedtext8(byte c)
{
	bzero(signednumbertext,sizeof(signednumbertext));
	char s;
	s = unsigned2signed8(c); //Convert to signed!
	if (s<0) //Negative?
	{
		sprintf(signednumbertext,"-%02X",0-s); //Show signed!
	}
	else //Positive?
	{
		sprintf(signednumbertext,"+%02X",s); //Show signed!
	}
	return &signednumbertext[0]; //Give pointer!
}

char *unsigned2signedtext16(word c)
{
	bzero(signednumbertext,sizeof(signednumbertext));
	int s;
	s = unsigned2signed16(c); //Convert to signed!
	if (s<0) //Negative?
	{
		sprintf(signednumbertext,"-%04X",0-s); //Show signed!
	}
	else //Positive?
	{
		sprintf(signednumbertext,"+%04X",s); //Show signed!
	}
	return &signednumbertext[0]; //Give pointer!
}

char *unsigned2signedtext32(uint_32 c)
{
	bzero(signednumbertext,sizeof(signednumbertext));
	int_32 s;
	s = unsigned2signed32(c); //Convert to signed!
	if (s<0) //Negative?
	{
		sprintf(signednumbertext,"-%08X",0-s); //Show signed!
	}
	else //Positive?
	{
		sprintf(signednumbertext,"+%08X",s); //Show signed!
	}
	return &signednumbertext[0]; //Give pointer!
}




















//Our decoders:

//Parts of modrm byte:
byte MODRM_MOD(byte modrm)
{
	return ((modrm&0xC0)>>6); //MOD
}

byte MODRM_REG(byte modrm)
{
	return ((modrm&0x38)>>3); //REG
}

byte MODRM_RM(byte modrm)
{
	return (modrm&0x07); //RM
}

void modrm_get_segmentregister(byte reg, MODRM_PTR *result) //REG1/2 is segment register!
{
	result->isreg = 1; //Register!
	result->regsize = 2; //Word register!
	switch (reg) //What segment register?
	{
	case MODRM_SEG_ES:
		result->reg16 = CPU.SEGMENT_REGISTERS[CPU_SEGMENT_ES];
		strcpy(result->text,"ES");
		break;
	case MODRM_SEG_CS:
		result->reg16 = CPU.SEGMENT_REGISTERS[CPU_SEGMENT_CS];
		strcpy(result->text,"CS");
		break;
	case MODRM_SEG_SS:
		result->reg16 = CPU.SEGMENT_REGISTERS[CPU_SEGMENT_SS];
		strcpy(result->text,"SS");
		break;
	case MODRM_SEG_DS:
		result->reg16 = CPU.SEGMENT_REGISTERS[CPU_SEGMENT_DS];
		strcpy(result->text,"DS");
		break;
	case MODRM_SEG_FS:
		result->reg16 = CPU.SEGMENT_REGISTERS[CPU_SEGMENT_FS];
		strcpy(result->text,"FS");
		break;
	case MODRM_SEG_GS:
		result->reg16 = CPU.SEGMENT_REGISTERS[CPU_SEGMENT_GS];
		strcpy(result->text,"GS");
		break;

	default: //Catch handler!
		result->reg16 = NULL; //Unknown!
		strcpy(result->text,"<UNKSREG>");
		break;

	}
}



//Whichregister:
/*
0=Invalid/unimplemented
1=reg1
2=reg2 as register (params->slashr==1) or mode 0 (RM value).
*/

//First the decoders:

uint_32 modrm_SIB_reg(int reg, int mod, uint_32 disp32, int is_base, char *result)
{
	if (is_base && mod==0 && reg==4)
	{
		sprintf(result,"%08X",disp32); //Set for display!
		return disp32; //No base: DISP32+index!
	}
	switch (reg)
	{
	case MODRM_REG_EAX:
		strcpy(result,"EAX");
		return REG_EAX;
		break;
	case MODRM_REG_EBX:
		strcpy(result,"EBX");
		return REG_EBX;
		break;
	case MODRM_REG_ECX:
		strcpy(result,"ECX");
		return REG_ECX;
		break;
	case MODRM_REG_EDX:
		strcpy(result,"EDX");
		return REG_EDX;
		break;
	case MODRM_REG_EBP:
		strcpy(result,"EBP");
		return REG_EBP;
		break;
	case MODRM_REG_ESP:
		strcpy(result,"0");
		return 0;
		break; //SIB doesn't have ESP!
	case MODRM_REG_ESI:
		strcpy(result,"ESI");
		return REG_ESI;
		break;
	case MODRM_REG_EDI:
		strcpy(result,"EDI");
		return REG_EDI;
		break;
	}
	halt_modrm("Unknown modr/mSIB16: reg: %i",reg);
	return 0; //Unknown register!
}

void modrm_decode32(MODRM_PARAMS *params, MODRM_PTR *result, byte whichregister) //32-bit address/reg decoder!
{
	bzero(result,sizeof(*result)); //Init!
	byte curreg = 0;
	byte reg; //What register?

	if (whichregister!=1) //reg2?
	{
		reg = MODRM_RM(params->modrm); //Take reg2!
		curreg = 2; //reg2!
	}
	else //1 or default (unknown)?
	{
		reg = MODRM_REG(params->modrm); //Default/reg1!
		curreg = 1; //reg1!
	}

	int isregister;
	isregister = 0; //Init!
	if (whichregister==1) //REG1?
	{
		isregister = 1; //Register!
	}
	else if (params->slashr) //Register (R/M with /r)?
	{
		isregister = 1; //Register!
	}
	else if (MODRM_MOD(params->modrm)==MOD_REG) //Register R/M?
	{
		isregister = 1; //Register!
	}
	else //No register?
	{
		isregister = 0; //No register (R/M=Memory Address)!
	}

	if (isregister) //Is register data?
	{
		result->isreg = 1; //Is register!
		result->regsize = 4; //DWord register!
		switch (reg) //Which register?
		{
		case MODRM_REG_EAX: //AX?
			strcpy(result->text,"EAX");
			result->reg32 = &REG_EAX; //Give addr!
			break;
		case MODRM_REG_EBX: //BX?
			strcpy(result->text,"EBX");
			result->reg32 = &REG_EBX; //Give addr!
			break;
		case MODRM_REG_ECX: //CX?
			strcpy(result->text,"ECX");
			result->reg32 = &REG_ECX; //Give addr!
			break;
		case MODRM_REG_EDX: //DX?
			strcpy(result->text,"EDX");
			result->reg32 = &REG_EDX; //Give addr!
			break;
		case MODRM_REG_EBP: //BP?
			strcpy(result->text,"EBP");
			result->reg32 = &REG_EBP; //Give addr!
			break;
		case MODRM_REG_ESP: //SP?
			strcpy(result->text,"ESP");
			result->reg32 = &REG_ESP; //Give addr!
			break;
		case MODRM_REG_ESI: //SI?
			strcpy(result->text,"ESI");
			result->reg32 = &REG_ESI; //Give addr!
			break;
		case MODRM_REG_EDI: //DI?
			strcpy(result->text,"EDI");
			result->reg32 = &REG_EDI; //Give addr!
			break;
		} //register?
	}












	uint_32 index; //Going to contain the values for SIB!
	uint_32 base; //Going to contain the values for SIB!
	char indexstr[256]; //Index!
	char basestr[256]; //Base!

	switch (MODRM_MOD(params->modrm)) //Which mod?
	{
	case MOD_MEM: //[register]
		switch (reg) //Which register?
		{
		case MODRM_MEM_EAX: //[EAX] etc.?
			sprintf(result->text,"[%s:EAX]",CPU_textsegment(CPU_SEGMENT_DS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_EAX); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_EBX: //EBX?
			sprintf(result->text,"[%s:EBX]",CPU_textsegment(CPU_SEGMENT_DS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_EBX); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_ECX: //ECX
			sprintf(result->text,"[%s:ECX]",CPU_textsegment(CPU_SEGMENT_DS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_ECX); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_EDX: //EDX
			sprintf(result->text,"[%s:EDX]",CPU_textsegment(CPU_SEGMENT_DS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_EDX); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_ESI: //ESI
			sprintf(result->text,"[%s:ESI]",CPU_textsegment(CPU_SEGMENT_DS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_ESI); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_EDI: //EDI
			sprintf(result->text,"[%s:EDI]",CPU_textsegment(CPU_SEGMENT_DS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_EDI); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_SIB: //SIB(reg1) or ESP(reg2)?
			if (curreg==1) //SIB?
			{
				//SIB
				index = modrm_SIB_reg(params->SIB.index,MOD_MEM,unsigned2signed32(params->displacement.dword),0,&indexstr[0]);
				base = modrm_SIB_reg(params->SIB.base,MOD_MEM,unsigned2signed32(params->displacement.dword),1,&basestr[0]);

				sprintf(result->text,"[%s:%s*%02X+%s]",CPU_textsegment(CPU_SEGMENT_DS),indexstr,(1<<params->SIB.scale),basestr); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(index*(1<<params->SIB.scale)+base);
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
			}
			else //ESP?
			{
				sprintf(result->text,"[%s:ESP]",CPU_textsegment(CPU_SEGMENT_SS));
				result->mem_segment = CPU_segment(CPU_SEGMENT_SS); //Default to SS!
				result->mem_offset = modrm_internal_offset32(REG_ESP); //Give addr!
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
			}
			break;
		case MODRM_MEM_DISP32: //EBP->32-bit Displacement-Only mode?
			sprintf(result->text,"[%s:%08X]",CPU_textsegment(CPU_SEGMENT_SS),params->displacement.dword);
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = params->displacement.dword; //Give addr (Displacement Only)!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return;
			break;
		}
		break;
	case MOD_MEM_DISP8: //[register+DISP8]
		switch (reg) //Which register?
		{
		case MODRM_MEM_EAX: //EAX?
			sprintf(result->text,"[%s:EAX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_EAX)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_EBX: //EBX?
			sprintf(result->text,"[%s:EBX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_EBX)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_ECX: //ECX?
			sprintf(result->text,"[%s:ECX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_ECX)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_EDX: //EDX?
			sprintf(result->text,"[%s:EDX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_EDX)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_ESI: //ESI?
			sprintf(result->text,"[%s:ESI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_ESI)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_EDI: //EDI?
			sprintf(result->text,"[%s:EDI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_EDI)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			return; //Give addr!
			break;
		case MODRM_MEM_SIB: //SIB/ESP?
			if (curreg==1) //SIB?
			{
				index = modrm_SIB_reg(params->SIB.index,MOD_MEM_DISP8,unsigned2signed8(params->displacement.low16_low),0,indexstr);
				base = modrm_SIB_reg(params->SIB.base,MOD_MEM_DISP8,unsigned2signed8(params->displacement.low16_low),1,basestr);

				sprintf(result->text,"[%s:%s*%02X+%s]",CPU_textsegment(CPU_SEGMENT_DS),indexstr,(1<<params->SIB.scale),basestr); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(index*(1<<params->SIB.scale)+base);
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return;
			}
			else //ESP?
			{
				sprintf(result->text,"[%s:ESP%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext8(params->displacement.low16_low));
				result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
				result->mem_offset = modrm_internal_offset32(REG_ESP)+unsigned2signed8(params->displacement.low16_low); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return;
			}
			break;
		case MODRM_MEM_EBP: //EBP?
			sprintf(result->text,"[%s:EBP%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low));
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset32(REG_EBP)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		}
		break;
	case MOD_MEM_DISP32: //[register+DISP32]
		if (CPU_Operand_size) //Operand size is 32-bits?
		{
			switch (reg) //Which register?
			{
			case MODRM_MEM_EAX: //EAX?
				sprintf(result->text,"[%s:EAX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_EAX)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_EBX: //EBX?
				sprintf(result->text,"[%s:EBX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_EBX)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_ECX: //ECX?
				sprintf(result->text,"[%s:ECX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_ECX)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_EDX: //EDX?
				sprintf(result->text,"[%s:EDX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_EDX)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_ESI: //ESI?
				sprintf(result->text,"[%s:ESI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_ESI)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_EDI: //EDI?
				sprintf(result->text,"[%s:EDI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_EDI)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_SIB: //SIB/ESP?
				if (curreg==1) //SIB?
				{
					index = modrm_SIB_reg(params->SIB.index,MOD_MEM_DISP32,unsigned2signed32(params->displacement.dword),0,indexstr);
					base = modrm_SIB_reg(params->SIB.base,MOD_MEM_DISP32,unsigned2signed32(params->displacement.dword),1,basestr);

					sprintf(result->text,"[%s:%s*%02X+%s]",CPU_textsegment(CPU_SEGMENT_DS),indexstr,(1<<params->SIB.scale),basestr); //Give addr!
					result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
					result->mem_offset = modrm_internal_offset32(index*(1<<params->SIB.scale)+base);
					result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
					result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
					return;
				}
				else //ESP?
				{
					sprintf(result->text,"[%s:ESP%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext32(params->displacement.dword));
					result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
					result->mem_offset = modrm_internal_offset32(REG_ESP)+unsigned2signed32(params->displacement.dword); //Give addr!
					result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
					result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
					return;
				}
				break;
			case MODRM_MEM_EBP: //EBP?
				sprintf(result->text,"[%s:EBP%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword));
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_EBP)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			}
			break;
		}
		else //16-bits?
		{
			switch (reg) //Which register?
			{
			case MODRM_MEM_EAX: //EAX?
				sprintf(result->text,"[%s:EAX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16_low)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_EAX)+unsigned2signed16(params->displacement.low16_low); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_EBX: //EBX?
				sprintf(result->text,"[%s:EBX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16_low)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_EBX)+unsigned2signed16(params->displacement.low16_low); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_ECX: //ECX?
				sprintf(result->text,"[%s:ECX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16_low)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_ECX)+unsigned2signed16(params->displacement.low16_low); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_EDX: //EDX?
				sprintf(result->text,"[%s:EDX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16_low)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_EDX)+unsigned2signed16(params->displacement.low16_low); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_ESI: //ESI?
				sprintf(result->text,"[%s:ESI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16_low)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_ESI)+unsigned2signed16(params->displacement.low16_low); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_EDI: //EDI?
				sprintf(result->text,"[%s:EDI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16_low)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_EDI)+unsigned2signed16(params->displacement.low16_low); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				return; //Give addr!
				break;
			case MODRM_MEM_SIB: //SIB/ESP?
				if (curreg==1) //SIB?
				{
					index = modrm_SIB_reg(params->SIB.index,MOD_MEM_DISP16,unsigned2signed16(params->displacement.low16_low),0,indexstr);
					base = modrm_SIB_reg(params->SIB.base,MOD_MEM_DISP16,unsigned2signed16(params->displacement.low16_low),1,basestr);

					sprintf(result->text,"[%s:%s*%02X+%s]",CPU_textsegment(CPU_SEGMENT_DS),indexstr,(1<<params->SIB.scale),basestr); //Give addr!
					result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
					result->mem_offset = modrm_internal_offset32(index*(1<<params->SIB.scale)+base);
					result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
					result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
					return;
				}
				else //ESP?
				{
					sprintf(result->text,"[%s:ESP%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext16(params->displacement.low16_low));
					result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
					result->mem_offset = modrm_internal_offset32(REG_ESP)+unsigned2signed16(params->displacement.low16_low); //Give addr!
					result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
					result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
					return;
				}
				break;
			case MODRM_MEM_EBP: //EBP?
				sprintf(result->text,"[%s:EBP%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16_low));
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_EBP)+unsigned2signed16(params->displacement.low16_low); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			}
			break;
		}
		break;
	case MOD_REG: //register
		break;
	} //Which MOD?
	halt_modrm("Unknown modr/m32: MOD:%i, reg: %i, operand size: %i",MODRM_MOD(params->modrm),reg,CPU_Operand_size);
	return; //Unknown!
}

void modrm_decode16(MODRM_PARAMS *params, MODRM_PTR *result, byte whichregister) //16-bit address/reg decoder!
{
	bzero(result,sizeof(*result)); //Init!
	byte reg = 0;

	if (whichregister!=1) //reg2?
	{
		reg = MODRM_RM(params->modrm); //Take rm!
	}
	else //1 or default (unknown)?
	{
		reg = MODRM_REG(params->modrm); //Default/reg!
	}

	bzero(result->text,sizeof(result->text)); //Init text!

	if (params->reg_is_segmentregister && (whichregister==1)) //Segment register?
	{
		modrm_get_segmentregister(reg,result); //Return segment register!
		return; //Give the segment register!
	}

	int isregister;
	isregister = 0; //Init!
	if (whichregister==1) //REG1?
	{
		isregister = 1; //Register!
	}
	else if (params->slashr) //Register (R/M with /r)?
	{
		isregister = 1; //Register!
	}
	else if (MODRM_MOD(params->modrm)==MOD_REG) //Register R/M?
	{
		isregister = 1; //Register!
	}
	else //No register?
	{
		isregister = 0; //No register (R/M=Memory Address)!
	}

	if (isregister) //Is register data?
	{
		result->isreg = 1; //Is register!
		result->regsize = 2; //Word register!
		switch (reg) //What register to use?
		{
		case MODRM_REG_AX: //AX
			strcpy(result->text,"AX");
			result->reg16 = &REG_AX;
			return;
		case MODRM_REG_CX: //CX
			strcpy(result->text,"CX");
			result->reg16 = &REG_CX;
			return;
		case MODRM_REG_DX: //DX
			strcpy(result->text,"DX");
			result->reg16 = &REG_DX;
			return;
		case MODRM_REG_BX: //BX
			strcpy(result->text,"BX");
			result->reg16 = &REG_BX;
			return;
		case MODRM_REG_SP: //SP
			strcpy(result->text,"SP");
			result->reg16 = &REG_SP;
			return;
		case MODRM_REG_BP: //BP
			strcpy(result->text,"BP");
			result->reg16 = &REG_BP;
			return;
		case MODRM_REG_SI: //SI
			strcpy(result->text,"SI");
			result->reg16 = &REG_SI;
			return;
		case MODRM_REG_DI: //DI
			strcpy(result->text,"DI");
			result->reg16 = &REG_DI;
			return;
		}
		result->isreg = 0; //Unknown!
		strcpy(result->text,"<UNKREG>"); //Unknown!

		halt_modrm("Unknown modr/m16Reg: MOD:%i, RM: %i, operand size: %i",MODRM_MOD(params->modrm),reg,CPU_Operand_size);
		return;
	}

	//Determine R/M (reg2=>RM) pointer!

	result->isreg = 2; //Memory!

	switch (MODRM_MOD(params->modrm)) //Which mod?
	{
	case MOD_MEM: //[register]
		switch (reg) //Which register?
		{
		case MODRM_MEM_BXSI: //BX+SI?
			sprintf(result->text,"[%s:BX+SI]",CPU_textsegment(CPU_SEGMENT_DS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset16(REG_BX+REG_SI); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		case MODRM_MEM_BXDI: //BX+DI?
			sprintf(result->text,"[%s:BX+DI]",CPU_textsegment(CPU_SEGMENT_DS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset16(REG_BX+REG_DI); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		case MODRM_MEM_BPSI: //BP+SI?
			sprintf(result->text,"[%s:BP+SI]",CPU_textsegment(CPU_SEGMENT_SS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
			result->mem_offset = modrm_internal_offset16(REG_BP+REG_SI); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
			break;
		case MODRM_MEM_BPDI: //BP+DI?
			sprintf(result->text,"[%s:BP+DI]",CPU_textsegment(CPU_SEGMENT_SS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
			result->mem_offset = modrm_internal_offset16(REG_BP+REG_DI); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
			break;
		case MODRM_MEM_SI: //SI?
			sprintf(result->text,"[%s:SI]",CPU_textsegment(CPU_SEGMENT_DS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset16(REG_SI); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		case MODRM_MEM_DI: //DI?
			sprintf(result->text,"[%s:DI]",CPU_textsegment(CPU_SEGMENT_DS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset16(REG_DI); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		case MODRM_MEM_DISP16: //BP = disp16?
			sprintf(result->text,"[%s:%04X]",CPU_textsegment(CPU_SEGMENT_DS),params->displacement.low16); //Simple [word] displacement!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = params->displacement.low16; //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		case MODRM_MEM_BX: //BX?
			sprintf(result->text,"[%s:BX]",CPU_textsegment(CPU_SEGMENT_DS)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset16(REG_BX); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		default:
			halt_modrm("Unknown modr/m16: MOD:%i, RM: %i, operand size: %i",MODRM_MOD(params->modrm),reg,CPU_Operand_size);
			result->isreg = 0; //Unknown modr/m!
			return;
			break;
		}
		break;
	case MOD_MEM_DISP8: //[register+DISP8]
		switch (reg) //Which register?
		{
		case MODRM_MEM_BXSI: //BX+SI?
			sprintf(result->text,"[%s:BX+SI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset16(REG_BX+REG_SI)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		case MODRM_MEM_BXDI: //BX+DI?
			sprintf(result->text,"[%s:BX+DI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset16(REG_BX+REG_DI)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		case MODRM_MEM_BPSI: //BP+SI?
			sprintf(result->text,"[%s:BP+SI%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
			result->mem_offset = modrm_internal_offset16(REG_BP+REG_SI)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
			break;
		case MODRM_MEM_BPDI: //BP+DI?
			sprintf(result->text,"[%s:BP+DI%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
			result->mem_offset = modrm_internal_offset16(REG_BP+REG_DI)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
			break;
		case MODRM_MEM_SI: //SI?
			sprintf(result->text,"[%s:SI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset16(REG_SI)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		case MODRM_MEM_DI: //DI?
			sprintf(result->text,"[%s:DI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset16(REG_DI)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		case MODRM_MEM_BP: //BP?
			sprintf(result->text,"[%s:BP%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
			result->mem_offset = modrm_internal_offset16(REG_BP)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
			break;
		case MODRM_MEM_BX: //BX?
			sprintf(result->text,"[%s:BX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext8(params->displacement.low16_low)); //Give addr!
			result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
			result->mem_offset = modrm_internal_offset16(REG_BX)+unsigned2signed8(params->displacement.low16_low); //Give addr!
			result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
			result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
			break;
		default:
			halt_modrm("Unknown modr/m16: MOD:%i, RM: %i, operand size: %i",MODRM_MOD(params->modrm),reg,CPU_Operand_size);
			result->isreg = 0; //Unknown modr/m!
			return;
			break;
		}
		break;
	case MOD_MEM_DISP32: //[register+DISP32]
		if (CPU_Operand_size) //Operand size is 32-bits?
		{
			switch (reg) //Which register?
			{
			case MODRM_MEM_BXSI: //BX+SI?
				sprintf(result->text,"[%s:BX+SI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset= modrm_internal_offset32(REG_BX+REG_SI)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			case MODRM_MEM_BXDI: //BX+DI?
				sprintf(result->text,"[%s:BX+DI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_BX+REG_DI)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			case MODRM_MEM_BPSI: //BP+SI?
				sprintf(result->text,"[%s:BP+SI%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
				result->mem_offset = modrm_internal_offset32(REG_BP+REG_SI)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
				break;
			case MODRM_MEM_BPDI: //BP+DI?
				sprintf(result->text,"[%s:BP+DI%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
				result->mem_offset = modrm_internal_offset32(REG_BP+REG_DI)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
				break;
			case MODRM_MEM_SI: //SI?
				sprintf(result->text,"[%s:SI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_SI)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			case MODRM_MEM_DI: //DI?
				sprintf(result->text,"[%s:DI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset= modrm_internal_offset32(REG_DI)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			case MODRM_MEM_BP: //BP?
				sprintf(result->text,"[%s:BP%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
				result->mem_offset = modrm_internal_offset32(REG_BP)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
				break;
			case MODRM_MEM_BX: //BX?
				sprintf(result->text,"[%s:BX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext32(params->displacement.dword)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset32(REG_BX)+unsigned2signed32(params->displacement.dword); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			}
			default:
				halt_modrm("Unknown modr/m16: MOD:%i, RM: %i, operand size: %i",MODRM_MOD(params->modrm),reg,CPU_Operand_size);
				result->isreg = 0; //Unknown modr/m!
				return;
				break;
		}
		else //Operand size is 16-bits?
		{
			switch (reg) //Which register?
			{
			case MODRM_MEM_BXSI: //BX+SI?
				sprintf(result->text,"[%s:BX+SI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset16(REG_BX+REG_SI)+unsigned2signed16(params->displacement.low16); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			case MODRM_MEM_BXDI: //BX+DI?
				sprintf(result->text,"[%s:BX+DI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset16(REG_BX+REG_DI)+unsigned2signed16(params->displacement.low16); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			case MODRM_MEM_BPSI: //BP+SI?
				sprintf(result->text,"[%s:BP+SI%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext16(params->displacement.low16)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
				result->mem_offset = modrm_internal_offset16(REG_BP+REG_SI)+unsigned2signed16(params->displacement.low16); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
				break;
			case MODRM_MEM_BPDI: //BP+DI?
				sprintf(result->text,"[%s:BP+DI%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext16(params->displacement.low16)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
				result->mem_offset = modrm_internal_offset16(REG_BP+REG_DI)+unsigned2signed16(params->displacement.low16); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
				break;
			case MODRM_MEM_SI: //SI?
				sprintf(result->text,"[%s:SI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset16(REG_SI)+unsigned2signed16(params->displacement.low16); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			case MODRM_MEM_DI: //DI?
				sprintf(result->text,"[%s:DI%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset16(REG_DI)+unsigned2signed16(params->displacement.low16); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			case MODRM_MEM_BP: //BP?
				sprintf(result->text,"[%s:BP%s]",CPU_textsegment(CPU_SEGMENT_SS),unsigned2signedtext16(params->displacement.low16)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_SS);
				result->mem_offset = modrm_internal_offset16(REG_BP)+unsigned2signed16(params->displacement.low16); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_SS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_SS);
				break;
			case MODRM_MEM_BX: //REG_BX?
				sprintf(result->text,"[%s:BX%s]",CPU_textsegment(CPU_SEGMENT_DS),unsigned2signedtext16(params->displacement.low16)); //Give addr!
				result->mem_segment = CPU_segment(CPU_SEGMENT_DS);
				result->mem_offset = modrm_internal_offset16(REG_BX)+unsigned2signed16(params->displacement.low16); //Give addr!
				result->segmentregister = CPU_segment_ptr(CPU_SEGMENT_DS);
				result->segmentregister_index = CPU_segment_index(CPU_SEGMENT_DS);
				break;
			default:
				halt_modrm("Unknown modr/m16: MOD:%i, RM: %i, operand size: %i",MODRM_MOD(params->modrm),reg,CPU_Operand_size);
				result->isreg = 0; //Unknown modr/m!
				return;
				break;
			}
			break;
		}
		break;
	} //Which MOD?
	last_modrm = 1; //ModR/M!
	modrm_lastsegment = result->mem_segment;
	modrm_lastoffset = result->mem_offset;
}


void modrm_decode8(MODRM_PARAMS *params, MODRM_PTR *result, byte whichregister)
{
	bzero(result,sizeof(*result)); //Init!
	byte reg = 0;

	if (whichregister!=1) //reg2?
	{
		reg = MODRM_RM(params->modrm); //Take reg2/RM!
	}
	else //1 or default (unknown)?
	{
		reg = MODRM_REG(params->modrm); //Default/reg1!
	}

	bzero(result->text,sizeof(result->text)); //Init text!

	int isregister;
	if (whichregister==1) //REG1?
	{
		isregister = 1; //Register!
	}
	else if (params->slashr) //Register (R/M with /r)?
	{
		isregister = 1; //Register!
	}
	else if (MODRM_MOD(params->modrm)==MOD_REG) //Register R/M?
	{
		isregister = 1; //Register!
	}
	else //No register?
	{
		isregister = 0; //No register, so use memory R/M!
	}

	if (isregister) //Is register data?
	{
		result->isreg = 1; //Register!
		result->regsize = 1; //Byte register!
		switch (reg)
		{
		case MODRM_REG_AL:
			result->reg8 = &REG_AL;
			strcpy(result->text,"AL");
			return;
		case MODRM_REG_CL:
			result->reg8 = &REG_CL;
			strcpy(result->text,"CL");
			return;
		case MODRM_REG_DL:
			result->reg8 = &REG_DL;
			strcpy(result->text,"DL");
			return;
		case MODRM_REG_BL:
			result->reg8 = &REG_BL;
			strcpy(result->text,"BL");
			return;
		case MODRM_REG_AH:
			result->reg8 = &REG_AH;
			strcpy(result->text,"AH");
			return;
		case MODRM_REG_CH:
			result->reg8 = &REG_CH;
			strcpy(result->text,"CH");
			return;
		case MODRM_REG_DH:
			result->reg8 = &REG_DH;
			strcpy(result->text,"DH");
			return;
		case MODRM_REG_BH:
			result->reg8 = &REG_BH;
			strcpy(result->text,"BH");
			return;
		}
		result->isreg = 0; //Unknown register!

		halt_modrm("Unknown modr/m8Reg: %02x; MOD:%i, reg: %i, operand size: %i",MODRM_MOD(params->modrm),reg,CPU_Operand_size);
		return; //Stop decoding!
	}


	switch (MODRM_MOD(params->modrm)) //Which mod?
	{
	case MOD_MEM: //[register]
	case MOD_MEM_DISP8: //[register+DISP8]
	case MOD_MEM_DISP32: //[register+DISP32]
		modrm_decode16(params,result,whichregister); //Use 16-bit decoder!
		return;
	default: //Shouldn't be here!
		halt_modrm("Reg MODRM when shouldn't be!");
		return;
	} //Which MOD?
	halt_modrm("Unknown modr/m8: %02x; MOD:%i, reg: %i, operand size: %i",MODRM_MOD(params->modrm),reg,CPU_Operand_size);
	result->isreg = 0; //Unknown modr/m!
}































byte *modrm_addr8(MODRM_PARAMS *params, int whichregister, int forreading)
{
	MODRM_PTR info; //To contain the info!
	modrm_decode8(params,&info,whichregister); //Get data!
	byte *result; //The result holder if needed!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		if (info.reg8==NULL)
		{
			halt_modrm("MODRM:NULL REG8\nValue:%s",info.text);
		}
		return (byte *)memprotect(info.reg8,1,"CPU_REGISTERS"); //Give register!
	case 2: //Memory?
		return NULL; //We don't do memory addresses! Use direct memory access here!
		result = (byte *)MMU_ptr(info.segmentregister_index,info.mem_segment,info.mem_offset,forreading,1); //Give memory!
		if (result==NULL)
		{
			halt_modrm("MODRM:NULL MEMPTR8 %X:%X\nValue:%s",info.mem_segment,info.mem_offset,info.text);
		}
		return result; //Give memory!
	default:
		halt_modrm("MODRM: Unknown MODR/M8!");
		return NULL; //Unknown!
	}
}

word *modrm_addr16(MODRM_PARAMS *params, int whichregister, int forreading)
{
	MODRM_PTR info; //To contain the info!
	modrm_decode16(params,&info,whichregister); //Get data!
	word *result; //The result holder if needed!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		if (info.reg16==NULL)
		{
			halt_modrm("MODRM:NULL REG16\nValue:%s",info.text);
		}
		return (word *)memprotect(info.reg16,2,"CPU_REGISTERS"); //Give register!
	case 2: //Memory?
		return NULL; //We don't do memory addresses! Use direct memory access here!
		result = (word *)MMU_ptr(info.segmentregister_index,info.mem_segment,info.mem_offset,forreading,2); //Give memory!
		if (result==NULL)
		{
			halt_modrm("MODRM:NULL MEMPTR16 %X:%X\nValue:%s",info.mem_segment,info.mem_offset,info.text);
		}
		return result; //Give the result!
	default:
		halt_modrm("MODRM: Unknown MODR/M16!");
		return NULL; //Unknown!
	}
}

void modrm_text8(MODRM_PARAMS *params, int whichregister, char *result)
{
	MODRM_PTR info; //To contain the info!
	modrm_decode8(params,&info,whichregister); //Get data!
	strcpy(result,info.text); //Use the text representation!
}

void modrm_text16(MODRM_PARAMS *params, int whichregister, char *result)
{
	MODRM_PTR info; //To contain the info!
	modrm_decode16(params,&info,whichregister); //Get data!
	strcpy(result,info.text); //Use the text representation!
}

word modrm_lea16(MODRM_PARAMS *params, int whichregister) //For LEA instructions!
{
	MODRM_PTR info; //To contain the info!
	modrm_decode16(params,&info,whichregister); //Get data!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		return (modrm_lastoffset&0xFFFF); //No registers allowed officially, but we return the last offset in this case (undocumented)!
	case 2: //Memory?
		return info.mem_offset; //Give memory offset!
	default:
		return 0; //Unknown!
	}
}

void modrm_lea16_text(MODRM_PARAMS *params, int whichregister, char *result) //For LEA instructions!
{
	MODRM_PTR info; //To contain the info!
	modrm_decode16(params,&info,whichregister); //Get data!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		strcpy(result,info.text); //No registers allowed!
		return;
	case 2: //Memory?
		//sprintf(result,"%04x",info.mem_offset); //Give memory offset!
		strcpy(result,info.text); //Set the text literally!
		return; //Memory is valid!
	default:
		strcpy(result,"<UNKNOWN>");
		return; //Unknown!
	}
}

//modrm_offset16: same as lea16, but allow registers too!
word modrm_offset16(MODRM_PARAMS *params, int whichregister) //Gives address for JMP, CALL etc.!
{
	MODRM_PTR info; //To contain the info!
	modrm_decode16(params,&info,whichregister); //Get data!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		return *info.reg16; //Give register value!
	case 2: //Memory?
		return info.mem_offset; //Give memory offset!
	default:
		return 0; //Unknown!
	}
}

//Used for LDS, LES, LSS, LEA
word *modrm_addr_reg16(MODRM_PARAMS *params, int whichregister) //For LEA related instructions, returning the register!
{
	MODRM_PTR info; //To contain the info!
	modrm_decode16(params,&info,whichregister); //Get data!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		if (info.reg16==NULL)
		{
			halt_modrm("NULL REG16LEA");
		}
		return info.reg16; //Give register itself!
	case 2: //Memory?
		if (info.segmentregister==NULL)
		{
			halt_modrm("NULL REG16LEA_SEGMENT");
		}
		return info.segmentregister; //Give memory offset!
	default:
		halt_modrm("REG16LEA_UNK");
		return NULL; //Unknown!
	}
}











/*

32-bit functionality

32-bit addresses are unpatched&unused yet.

*/

uint_32 *modrm_addr32(MODRM_PARAMS *params, int whichregister, int forreading)
{
	MODRM_PTR info; //To contain the info!
	modrm_decode32(params,&info,whichregister); //Get data!
	uint_32 *result; //The result holder if needed!
	switch (info.isreg) //What type?
	{
	case 1: //Register?
		if (info.reg32==NULL)
		{
			halt_modrm("NULL REG32");
		}
		return (uint_32 *)memprotect(info.reg32,4,"CPU_REGISTERS"); //Give register!
	case 2: //Memory?
		return NULL; //We don't do memory addresses! Use direct memory access here!
		result = (uint_32 *)MMU_ptr(info.segmentregister_index,info.mem_segment,info.mem_offset,forreading,4); //Give memory!
		if (result==NULL)
		{
			halt_modrm("NULL MEMPTR16");
		}
	default:
		return NULL; //Unknown!
	}

}












//CPU kernel functions

/*

Slashr:
0: No slashr! (Use displacement if needed!)
1: RM=> REG2 (No displacement etc.)
2: REG(1)=> SEGMENTREGISTER (Use displacement if needed!)

*/

extern GPU_TEXTSURFACE *frameratesurface;

void modrm_readparams(MODRM_PARAMS *param, int size, int slashr)
{
	uint_32 startseg;
	uint_32 startoffs;
	startseg = REG_CS;
	startoffs = REG_EIP; //Our address!

//Special data we already know:
	param->slashr = 0; //No /r modr/m!
	param->reg_is_segmentregister = 0; //REG2 is NORMAL!

	if ((slashr==1) || (slashr==2)) //Is this a /r modr/m?
	{
		param->slashr = 1; //Is this a /r modr/m?
	}

	if (slashr==2) //reg2 is segment register?
	{
		param->reg_is_segmentregister = 1; //REG2 is segment register!
	}

	param->modrm = CPU_readOP(); /* modrm byte first */
	param->SIB.SIB = modrm_useSIB(param,size)?CPU_readOP():0; //Read SIB byte or 0!
	param->displacement.dword = 0; //Reset DWORD (biggest) value (reset value to 0)!

	switch (modrm_useDisplacement(param,size)) //Displacement?
	{
	case 1: //DISP8?
		param->displacement.low16_low = CPU_readOP(); //Use 8-bit!
		break;
	case 2: //DISP16?
		param->displacement.low16 = CPU_readOPw(); //Use 16-bit!
		break;
	case 4: //DISP32?
		param->displacement.dword = CPU_readOPdw(); //Use 32-bit!
		break;
	default: //Unknown/no displacement?
		break; //No displacement!
	}
	
	/*if (debugging())
	{
		GPU_textgotoxy(frameratesurface,0,GPU_TEXT_INFOROW); //Goto info row!
		char str[256];
		bzero(str,sizeof(str)); //For clearing!
		int i;
		for (i=0; i<DEBUGSCREEN_WIDTH; i++) //Clear unneeded!
		{
			GPU_textprintf(frameratesurface,0,0," "); //Clear all unneeded!
		}
		GPU_textgotoxy(frameratesurface,0,GPU_TEXT_INFOROW);
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"MODRM: MOD: %i,RM: %i,REG: %i,size: %i,/r: %i,seg: %i,b:%02X/%02X@%04X:%04X",MODRM_MOD(param->modrm),MODRM_REG(param->modrm),MODRM_RM(param->modrm),size,param->slashr,param->reg_is_segmentregister,param->modrm,MMU_rb(startseg,startoffs),startseg,startoffs); //Debug!
		//printf("MODRM: %02X/%02X@%04X:%04X",param->modrm,MMU_rb(startseg,startoffs),startseg,startoffs);
	}*/
}