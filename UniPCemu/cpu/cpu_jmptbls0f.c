#include "headers/cpu/cpu.h" //Need basic CPU support!
#include "headers/cpu/cpu_OP8086.h" //Unknown opcodes under 8086!
#include "headers/cpu/cpu_OPNECV30.h" //Unknown opcodes under NECV30+ and more!
#include "headers/cpu/cpu_OP80286.h" //Unknown opcodes under 80286+ and more!
#include "headers/cpu/cpu_OP80386.h" //Unknown opcodes under 80386+ and more!
#include "headers/cpu/cpu_OP80486.h" //Unknown opcodes under 80486+ and more!
#include "headers/cpu/cpu_OP80586.h" //Unknown opcodes under 80586+ and more!

//See normal opcode table, but for 0F opcodes!
Handler opcode0F_jmptbl[NUM0FEXTS][256][2] =   //Our standard internal standard interrupt jmptbl!
{
	//80286+
	{
//0x00:
		{CPU286_OP0F00,NULL}, //00h:
		{CPU286_OP0F01,NULL}, //01h
		{CPU286_OP0F02,NULL}, //02h:
		{CPU286_OP0F03,NULL}, //03h:
		{unkOP0F_286,NULL}, //04h:
		{CPU286_OP0F05,NULL}, //05h:
		{CPU286_OP0F06,NULL}, //06h:
		{unkOP0F_286,NULL}, //07h:
		{unkOP0F_286,NULL}, //08h:
		{unkOP0F_286,NULL}, //09h:
		{unkOP0F_286,NULL}, //0Ah:
		{CPU286_OP0F0B,NULL}, //0Bh:
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
		{CPU286_OP0FB9,NULL},
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
	{ NULL, CPU386_OP0F01 }, //01h
	{ NULL, NULL }, //02h:
	{ NULL, NULL }, //03h:
	{ NULL, NULL }, //04h:
	{ unkOP0F_386, NULL }, //05h: 286-only LOADALL doesn't exist anymore on a 386!
	{ NULL, NULL }, //06h:
	{ CPU386_OP0F07, NULL }, //07h: Undocumented 80386-only LOADALL instruction!
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
	{ CPU80386_OP0F_MOVCRn_modrmmodrm, NULL },
	{ CPU80386_OP0F_MOVDRn_modrmmodrm, NULL },
	{ CPU80386_OP0F_MOVCRn_modrmmodrm, NULL },
	{ CPU80386_OP0F_MOVDRn_modrmmodrm, NULL },
	{ CPU80386_OP0F_MOVTRn_modrmmodrm, NULL },
	{ NULL, NULL },
	{ CPU80386_OP0F_MOVTRn_modrmmodrm, NULL },
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
	{ CPU80386_OP0F80_16, CPU80386_OP0F80_32 },
	{ CPU80386_OP0F81_16, CPU80386_OP0F81_32 },
	{ CPU80386_OP0F82_16, CPU80386_OP0F82_32 },
	{ CPU80386_OP0F83_16, CPU80386_OP0F83_32 },
	{ CPU80386_OP0F84_16, CPU80386_OP0F84_32 },
	{ CPU80386_OP0F85_16, CPU80386_OP0F85_32 },
	{ CPU80386_OP0F86_16, CPU80386_OP0F86_32 },
	{ CPU80386_OP0F87_16, CPU80386_OP0F87_32 },
	{ CPU80386_OP0F88_16, CPU80386_OP0F88_32 },
	{ CPU80386_OP0F89_16, CPU80386_OP0F89_32 },
	{ CPU80386_OP0F8A_16, CPU80386_OP0F8A_32 },
	{ CPU80386_OP0F8B_16, CPU80386_OP0F8B_32 },
	{ CPU80386_OP0F8C_16, CPU80386_OP0F8C_32 },
	{ CPU80386_OP0F8D_16, CPU80386_OP0F8D_32 },
	{ CPU80386_OP0F8E_16, CPU80386_OP0F8E_32 },
	{ CPU80386_OP0F8F_16, CPU80386_OP0F8F_32 },
	//0x90:
	{ CPU80386_OP0F90, NULL },
	{ CPU80386_OP0F91, NULL },
	{ CPU80386_OP0F92, NULL },
	{ CPU80386_OP0F93, NULL },
	{ CPU80386_OP0F94, NULL },
	{ CPU80386_OP0F95, NULL },
	{ CPU80386_OP0F96, NULL },
	{ CPU80386_OP0F97, NULL },
	{ CPU80386_OP0F98, NULL },
	{ CPU80386_OP0F99, NULL },
	{ CPU80386_OP0F9A, NULL },
	{ CPU80386_OP0F9B, NULL },
	{ CPU80386_OP0F9C, NULL },
	{ CPU80386_OP0F9D, NULL },
	{ CPU80386_OP0F9E, NULL },
	{ CPU80386_OP0F9F, NULL },
	//0xA0:
	{ CPU80386_OP0FA0, NULL },
	{ CPU80386_OP0FA1, NULL },
	{ NULL, NULL },
	{ CPU80386_OP0FA3_16, CPU80386_OP0FA3_32 },
	{ CPU80386_OP0FA4_16, CPU80386_OP0FA4_32 },
	{ CPU80386_OP0FA5_16, CPU80386_OP0FA5_32 },
	{ NULL, NULL },
	{ NULL, NULL },
	{ CPU80386_OP0FA8, NULL },
	{ CPU80386_OP0FA9, NULL },
	{ NULL, NULL },
	{ CPU80386_OP0FAB_16, CPU80386_OP0FAB_32 },
	{ CPU80386_OP0FAC_16, CPU80386_OP0FAC_32 },
	{ CPU80386_OP0FAD_16, CPU80386_OP0FAD_32 },
	{ NULL, NULL },
	{ CPU80386_OP0FAF_16, CPU80386_OP0FAF_32 },
	//0xB0:
	{ NULL, NULL },
	{ NULL, NULL },
	{ CPU80386_OP0FB2_16, CPU80386_OP0FB2_32 },
	{ CPU80386_OP0FB3_16, CPU80386_OP0FB3_32 },
	{ CPU80386_OP0FB4_16, CPU80386_OP0FB4_32 },
	{ CPU80386_OP0FB5_16, CPU80386_OP0FB5_32 },
	{ CPU80386_OP0FB6_16, CPU80386_OP0FB6_32 },
	{ CPU80386_OP0FB7_16, CPU80386_OP0FB7_32 },
	{ NULL, NULL },
	{ NULL, NULL },
	{ CPU80386_OP0FBA_16, CPU80386_OP0FBA_32 },
	{ CPU80386_OP0FBB_16, CPU80386_OP0FBB_32 },
	{ CPU80386_OP0FBC_16, CPU80386_OP0FBC_32 },
	{ CPU80386_OP0FBD_16, CPU80386_OP0FBD_32 },
	{ CPU80386_OP0FBE_16, CPU80386_OP0FBE_32 },
	{ CPU80386_OP0FBF_16, CPU80386_OP0FBF_32 },
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
		{ CPU486_OP0F01_16, CPU486_OP0F01_32 }, //01h
		{ NULL, NULL }, //02h:
		{ NULL, NULL }, //03h:
		{ NULL, NULL }, //04h:
		{ NULL, NULL }, //05h:
		{ NULL, NULL }, //06h:
		{ NULL, NULL }, //07h:
		{ CPU486_OP0F08, NULL }, //08h:
		{ CPU486_OP0F09, NULL }, //09h:
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
		{ CPU486_OP0FB0, NULL },
		{ CPU486_OP0FB1_16, CPU486_OP0FB1_32 },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
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
		{ CPU486_OP0FC0, NULL },
		{ CPU486_OP0FC1_16, CPU486_OP0FC1_32 },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ NULL, NULL },
		{ CPU486_OP0FC8_16, CPU486_OP0FC8_32 },
		{ CPU486_OP0FC9_16, CPU486_OP0FC9_32 },
		{ CPU486_OP0FCA_16, CPU486_OP0FCA_32 },
		{ CPU486_OP0FCB_16, CPU486_OP0FCB_32 },
		{ CPU486_OP0FCC_16, CPU486_OP0FCC_32 },
		{ CPU486_OP0FCD_16, CPU486_OP0FCD_32 },
		{ CPU486_OP0FCE_16, CPU486_OP0FCE_32 },
		{ CPU486_OP0FCF_16, CPU486_OP0FCF_32 },
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
		{ unkOP0F_586, NULL }, //24h: Test register instructions become invalid!
		{ NULL, NULL },
		{ unkOP0F_586, NULL }, //26h: Test register instructions become invalid!
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

Handler CurrentCPU_opcode_jmptbl[1024]; //Our standard internal opcode jmptbl!

void generate_opcode0F_jmptbl()
{
	byte cpu; //What CPU are we processing!
	byte currentoperandsize = 0;
	word OP; //The opcode to process!
	byte currentCPU; //Current CPU to start off at!
	for (currentoperandsize = 0; currentoperandsize < 2; currentoperandsize++) //Process all operand sizes!
	{
		byte operandsize = currentoperandsize; //Operand size to use!
		for (OP = 0; OP < 0x100; OP++) //Process all opcodes!
		{
			cpu = (byte)EMULATED_CPU; //Start with the emulated CPU and work up to the predesessors!
			if (cpu >= CPU_80286) //286+?
			{
				cpu -= CPU_80286; //We start existing at the 286!
				currentCPU = cpu; //Save for restoration during searching!
				operandsize = currentoperandsize; //Initialize to our current operand size to search!
				while (!opcode0F_jmptbl[cpu][OP][operandsize]) //No opcode to handle at current CPU&operand size?
				{
					if (cpu) //We have an CPU size: switch to an earlier CPU if possible!
					{
						--cpu; //Not anymore! Look up one level!
						continue; //Try again!
					}
					else //No CPU: we're a standard, so go up one operand size and retry!
					{
						cpu = currentCPU; //Reset CPU to search!
						if (operandsize) //We've got operand sizes left?
						{
							--operandsize; //Go up one operand size!
						}
						else break; //No CPUs left? Then stop searching!
					}
				}
				if (opcode0F_jmptbl[cpu][OP][operandsize])
				{
					CurrentCPU_opcode_jmptbl[(OP << 2) | 2 | currentoperandsize] = opcode0F_jmptbl[cpu][OP][operandsize]; //Execute this instruction when we're triggered!
				}
				else
				{
					CurrentCPU_opcode_jmptbl[(OP << 2) | 2 | currentoperandsize] = &unkOP0F_286; //Execute this instruction when we're triggered!
				}
			}
			else //Too old a CPU to support 0F opcodes? Install safety handlers instead!
			{
				CurrentCPU_opcode_jmptbl[(OP << 1) | 2 | currentoperandsize] = (cpu==CPU_8086)?&unkOP_8086:&unkOP_186; //Execute this instruction when we're triggered!
			}
		}
	}
}
