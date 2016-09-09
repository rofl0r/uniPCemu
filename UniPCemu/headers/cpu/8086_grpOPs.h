#ifndef GRPOP_8086_H
#define GRPOP_8086_H

#include "headers/types.h" //Basic typedefs!

byte op_grp2_8(byte cnt, byte varshift);
word op_grp2_16(byte cnt, byte varshift);
void op_grp3_8();
void op_grp3_16();
void op_grp5();

typedef union
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
} VAL32Splitter; //Our 32-bit value splitter!

typedef union
{
	struct
	{
		union
		{
			uint_32 val32;
			int_32 val32s;
		};
		uint_32 val32high; //Filler
	};
	uint_32 val64; //Normal
	int_32 val64s; //Signed
} VAL64Splitter; //Our 32-bit value splitter!

#endif