#include "headers/types.h" //Basic type support!
//Number (signed/unsigned) conversions!

union
{
	byte u; //Unsigned version!
	sbyte s; //Signed version!
} convertor8;

union
{
	word u; //Unsigned version!
	sword s; //Signed version!
} convertor16;

union
{
	uint_32 u; //Unsigned version!
	int_32 s; //Signed version!
} convertor32;


sbyte unsigned2signed8(byte u)
{
	convertor8.u = u;
	return convertor8.s; //Give signed!
}

sword unsigned2signed16(word u)
{
	convertor16.u = u;
	return convertor16.s; //Give signed!
}

int_32 unsigned2signed32(uint_32 u)
{
	convertor32.u = u;
	return convertor32.s; //Give signed!
}

byte signed2unsigned8(sbyte s)
{
	convertor8.s = s;
	return convertor8.u; //Give unsigned!
}
word signed2unsigned16(sword s)
{
	convertor16.s = s;
	return convertor16.u; //Give unsigned!
}
uint_32 signed2unsigned32(int_32 s)
{
	convertor32.s = s;
	return convertor32.u; //Give unsigned!
}