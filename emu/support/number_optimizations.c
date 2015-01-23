#include "headers/types.h" //Basic types!

//Power of 2 support!

//Are we a power of 2?
//#define ISPOW2(x) 0
#define ISPOW2(x) (!(x&(x-1)))
#define OPTMODXX if (division>1) \
		{ \
			return (val&(division-1)); \
		}  //Able to mod by anding with division-1?, simple way!

//Calculates the power of 2!
uint_32 DIVMULPOW2_32(uint_32 val, uint_32 todiv, byte divide) //Find the lowest bit that's on!
{
	if (val&0x8) //8?
	{
		return divide?(todiv>>3):(todiv<<3); //Divide/multiply by 8!
	}
	if (val&0x100) //256?
	{
		return divide?(todiv>>8):(todiv<<8); //Divide/multiply by 8!
	}
	static uint_32 results[32] = {0x00000001,0x00000002,0x00000004,0x00000008,0x00000010,0x00000020,0x00000040,0x00000080,0x00000100,0x00000200,0x00000400,0x00000800,0x00001000,0x00002000,0x00004000,0x00008000,
			       0x00010000,0x00020000,0x00040000,0x00080000,0x00100000,0x00200000,0x00400000,0x00800000,0x01000000,0x02000000,0x04000000,0x08000000,0x10000000,0x20000000,0x40000000,0x80000000};
	byte result = 0; //Nothing yet!
	byte rindex = 0; //First item!
	for(;;) //Not done yet?
	{
		if (val==results[rindex]) return divide?(todiv>>result):(todiv<<result); //Divide or multiply!
		if (++result==32) break; //Next result!
		++rindex; //Next item!
	}
	return 0; //Give the original!
}

unsigned int DIVMULPOW2_16(unsigned int val, unsigned int todiv, byte divide) //Find the lowest bit that's on!
{
	if (val&0x8) //8?
	{
		return divide?(todiv>>3):(todiv<<3); //Divide/multiply by 8!
	}
	if (val&0x100) //256?
	{
		return divide?(todiv>>8):(todiv<<8); //Divide/multiply by 8!
	}
	static unsigned int results[16] = {0x0001,0x0002,0x0004,0x0008,0x0010,0x0020,0x0040,0x0080,0x0100,0x0200,0x0400,0x0800,0x1000,0x2000,0x4000,0x8000};
	byte result = 0; //Nothing yet!
	byte rindex = 0; //Index into the table!
	for(;;) //Not done yet?
	{
		if (val==results[rindex])
			return divide?(todiv>>result):(todiv<<result); //Divide or multiply!
		if (++result==16) break; //Next result!
		++rindex; //Next item!
	}
	return 0; //Give the original!
}


unsigned int OPTMUL(unsigned int val, unsigned int multiplication)
{
	if (ISPOW2(multiplication)) //Optimizable?
	{
		return DIVMULPOW2_16(multiplication,val,0); //Optimized?
	}
	return (val*multiplication); //Old fashioned multiplication, non ^2!
}

unsigned int OPTDIV(unsigned int val, unsigned int division)
{
	if (ISPOW2(division)) //Optimizable?
	{
		return DIVMULPOW2_16(division,val,1); //Optimized?
	}
	return SAFEDIV(val,division); //Divide standard, non ^2!
}

unsigned int OPTMOD(unsigned int val, unsigned int division)
{
	if (ISPOW2(division)) //Power of 2?
	{
		OPTMODXX
		return val-OPTMUL(OPTDIV(val,division),division); //Rest!
	}
	return SAFEMOD(val,division); //Normal operation!
}

uint_32 OPTMUL32(uint_32 val, uint_32 multiplication)
{
	if (ISPOW2(multiplication)) //Optimizable?
	{
		return DIVMULPOW2_32(multiplication,val,0); //Optimized?
	}
	return (val*multiplication); //Old fashioned multiplication, non ^2!
}

uint_32 OPTDIV32(uint_32 val, uint_32 division)
{
	if (ISPOW2(division)) //Optimizable?
	{
		return DIVMULPOW2_32(division,val,1); //Optimized?
	}
	return SAFEDIV(val,division); //Divide standard, non ^2!
}

uint_32 OPTMOD32(uint_32 val, uint_32 division)
{
	if (ISPOW2(division)) //Power of 2?
	{
		OPTMODXX
		return val-OPTMUL32(OPTDIV32(val,division),division); //Rest!
	}
	return SAFEMOD(val,division); //Normal operation!
}