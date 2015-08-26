#ifndef GRPOP_8086_H
#define GRPOP_8086_H

#include "headers/types.h" //Basic typedefs!

byte op_grp2_8(byte cnt);
word op_grp2_16(byte cnt);
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

#endif