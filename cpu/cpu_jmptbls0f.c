#include "headers/cpu/cpu.h" //Need basic CPU support!
#include "headers/cpu/80286/cpu_OP80286.h" //Unknown opcodes under 80286+ and more!
#include "headers/cpu/80386/cpu_OP80386.h" //Unknown opcodes under 80386+ and more!
#include "headers/cpu/80486/cpu_OP80486.h" //Unknown opcodes under 80486+ and more!
#include "headers/cpu/80586/cpu_OP80586.h" //Unknown opcodes under 80586+ and more!


//0F opcode extensions:

//See normal opcode table, but for 0F opcodes!
Handler opcode0F_jmptbl[NUM0FEXTS][256][2] =   //Our standard internal standard interrupt jmptbl!
{
	//80286+
	{
//0x00:
		{unkOP0F_286,NULL}, //00h:
		{unkOP0F_286,NULL}, //01h
		{unkOP0F_286,NULL}, //02h:
		{unkOP0F_286,NULL}, //03h:
		{unkOP0F_286,NULL}, //04h:
		{unkOP0F_286,NULL}, //05h:
		{unkOP0F_286,NULL}, //06h:
		{unkOP0F_286,NULL}, //07h:
		{unkOP0F_286,NULL}, //08h:
		{unkOP0F_286,NULL}, //09h:
		{unkOP0F_286,NULL}, //0Ah:
		{unkOP0F_286,NULL}, //0Bh:
		{unkOP0F_286,NULL}, //0Ch:
		{unkOP0F_286,NULL}, //0Dh:
		{unkOP0F_286,NULL}, //0Eh:
		{unkOP0F_286,NULL}, //0Fh:
//0x10:
		{unkOP0F_286,NULL}, //10h: video interrupt
		{unkOP0F_286,NULL}, //11h:
		{unkOP0F_286,NULL}, //12h:
		{unkOP0F_286,NULL}, //13h: I/O for HDD/Floppy disks
		{unkOP0F_286,NULL}, //14h:
		{unkOP0F_286,NULL}, //15h:
		{unkOP0F_286,NULL}, //16h:
		{unkOP0F_286,NULL}, //17h:
		{unkOP0F_286,NULL}, //18h:
		{unkOP0F_286,NULL}, //19h:
		{unkOP0F_286,NULL}, //1Ah:
		{unkOP0F_286,NULL}, //1Bh:
		{unkOP0F_286,NULL}, //1Ch:
		{unkOP0F_286,NULL}, //1Dh:
		{unkOP0F_286,NULL}, //1Eh:
		{unkOP0F_286,NULL}, //1Fh:
//0x20:
		{unkOP0F_286,NULL}, //20h:
		{unkOP0F_286,NULL}, //21h: DOS interrupt
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0x30:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0x40:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0x50:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0x60:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0x70:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0x80:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0x90:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0xA0:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0xB0:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0xC0:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0xD0:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0xE0:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
//0xF0:
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL},
		{unkOP0F_286,NULL}
	},

	//80386
	{
	//0x00:
	{ NULL, NULL }, //00h:
	{ NULL, NULL }, //01h
	{ NULL, NULL }, //02h:
	{ NULL, NULL }, //03h:
	{ NULL, NULL }, //04h:
	{ NULL, NULL }, //05h:
	{ NULL, NULL }, //06h:
	{ NULL, NULL }, //07h:
	{ NULL, NULL }, //08h:
	{ NULL, NULL }, //09h:
	{ NULL, NULL }, //0Ah:
	{ NULL, NULL }, //0Bh:
	{ NULL, NULL }, //0Ch:
	{ NULL, NULL }, //0Dh:
	{ NULL, NULL }, //0Eh:
	{ NULL, NULL }, //0Fh:
	//0x10:
	{ NULL, NULL }, //10h:
	{ NULL, NULL }, //11h:
	{ NULL, NULL }, //12h:
	{ NULL, NULL }, //13h:
	{ NULL, NULL }, //14h:
	{ NULL, NULL }, //15h:
	{ NULL, NULL }, //16h:
	{ NULL, NULL }, //17h:
	{ NULL, NULL }, //18h:
	{ NULL, NULL }, //19h:
	{ NULL, NULL }, //1Ah:
	{ NULL, NULL }, //1Bh:
	{ NULL, NULL }, //1Ch:
	{ NULL, NULL }, //1Dh:
	{ NULL, NULL }, //1Eh:
	{ NULL, NULL }, //1Fh:
	//0x20:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0x30:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0x40:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0x50:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0x60:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0x70:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0x80:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0x90:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0xA0:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0xB0:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0xC0:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0xD0:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0xE0:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	//0xF0:
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL },
	{ NULL, NULL }
	},

	//80486
	{
		//0x00:
		{ NULL, NULL }, //00h:
		{ NULL, NULL }, //01h
		{ NULL, NULL }, //02h:
		{ NULL, NULL }, //03h:
		{ NULL, NULL }, //04h:
		{ NULL, NULL }, //05h:
		{ NULL, NULL }, //06h:
		{ NULL, NULL }, //07h:
		{ NULL, NULL }, //08h:
		{ NULL, NULL }, //09h:
		{ NULL, NULL }, //0Ah:
		{ NULL, NULL }, //0Bh:
		{ NULL, NULL }, //0Ch:
		{ NULL, NULL }, //0Dh:
		{ NULL, NULL }, //0Eh:
		{ NULL, NULL }, //0Fh:
		//0x10:
		{ NULL, NULL }, //10h:
		{ NULL, NULL }, //11h:
		{ NULL, NULL }, //12h:
		{ NULL, NULL }, //13h:
		{ NULL, NULL }, //14h:
		{ NULL, NULL }, //15h:
		{ NULL, NULL }, //16h:
		{ NULL, NULL }, //17h:
		{ NULL, NULL }, //18h:
		{ NULL, NULL }, //19h:
		{ NULL, NULL }, //1Ah:
		{ NULL, NULL }, //1Bh:
		{ NULL, NULL }, //1Ch:
		{ NULL, NULL }, //1Dh:
		{ NULL, NULL }, //1Eh:
		{ NULL, NULL }, //1Fh:
		//0x20:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x30:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x40:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x50:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x60:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x70:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x80:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x90:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xA0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ CPU486_CPUID, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xB0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xC0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xD0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xE0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xF0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL }
	},

	//PENTIUM (80586)
	{
		//0x00:
		{ NULL, NULL }, //00h:
		{ NULL, NULL }, //01h
		{ NULL, NULL }, //02h:
		{ NULL, NULL }, //03h:
		{ NULL, NULL }, //04h:
		{ NULL, NULL }, //05h:
		{ NULL, NULL }, //06h:
		{ NULL, NULL }, //07h:
		{ NULL, NULL }, //08h:
		{ NULL, NULL }, //09h:
		{ NULL, NULL }, //0Ah:
		{ NULL, NULL }, //0Bh:
		{ NULL, NULL }, //0Ch:
		{ NULL, NULL }, //0Dh:
		{ NULL, NULL }, //0Eh:
		{ NULL, NULL }, //0Fh:
		//0x10:
		{ NULL, NULL }, //10h:
		{ NULL, NULL }, //11h:
		{ NULL, NULL }, //12h:
		{ NULL, NULL }, //13h:
		{ NULL, NULL }, //14h:
		{ NULL, NULL }, //15h:
		{ NULL, NULL }, //16h:
		{ NULL, NULL }, //17h:
		{ NULL, NULL }, //18h:
		{ NULL, NULL }, //19h:
		{ NULL, NULL }, //1Ah:
		{ NULL, NULL }, //1Bh:
		{ NULL, NULL }, //1Ch:
		{ NULL, NULL }, //1Dh:
		{ NULL, NULL }, //1Eh:
		{ NULL, NULL }, //1Fh:
		//0x20:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x30:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x40:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x50:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x60:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x70:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x80:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0x90:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xA0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ CPU586_CPUID, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xB0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xC0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xD0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xE0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		//0xF0:
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL }
	}

};