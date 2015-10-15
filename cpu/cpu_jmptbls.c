#include "headers/cpu/cpu.h" //Need basic CPU support!
#include "headers/cpu/cpu_OP8086.h" //8086 REAL opcode functions!
#include "headers/cpu/fpu_OP8087.h" //8086 REAL opcode functions!
#include "headers/cpu/cpu_OP80186.h" //80186 REAL opcode functions!
#include "headers/cpu/cpu_OP80286.h" //Unknown opcodes under 80286+!

/*

Indexes:
0: 8086
1: 80186
2: 80286
3: 80386
4: 80486
5: PENTIUM
0F Opcode:
0: 80286
1: 80386
2: 80486
3: PENTIUM

*/

//Structure: opcode_jmptbl[whatcpu][opcode][addresssize]
//address size=0 for 16-bits, 1 for 32-bits.

Handler opcode_jmptbl[NUMCPUS][256][2] =   //Our standard internal standard interrupt jmptbl!
{
	//8086
	{
//0x00:
		{CPU8086_OP00,NULL}, //00h:
		{CPU8086_OP01,NULL}, //01h:
		{CPU8086_OP02,NULL}, //02h:
		{CPU8086_OP03,NULL}, //03h:
		{CPU8086_OP04,NULL}, //04h:
		{CPU8086_OP05,NULL}, //05h:
		{CPU8086_OP06,NULL}, //06h:
		{CPU8086_OP07,NULL}, //07h:
		{CPU8086_OP08,NULL}, //08h:
		{CPU8086_OP09,NULL}, //09h:
		{CPU8086_OP0A,NULL}, //0Ah:
		{CPU8086_OP0B,NULL}, //0Bh:
		{CPU8086_OP0C,NULL}, //0Ch:
		{CPU8086_OP0D,NULL}, //0Dh:
		{CPU8086_OP0E,NULL}, //0Eh:
		{CPU8086_OP0F,NULL}, //0Fh: 8086 specific OPcode!
//0x10:
		{CPU8086_OP10,NULL}, //10h:
		{CPU8086_OP11,NULL}, //11h:
		{CPU8086_OP12,NULL}, //12h:
		{CPU8086_OP13,NULL}, //13h:
		{CPU8086_OP14,NULL}, //14h:
		{CPU8086_OP15,NULL}, //15h:
		{CPU8086_OP16,NULL}, //16h:
		{CPU8086_OP17,NULL}, //17h:
		{CPU8086_OP18,NULL}, //18h:
		{CPU8086_OP19,NULL}, //19h:
		{CPU8086_OP1A,NULL}, //1Ah:
		{CPU8086_OP1B,NULL}, //1Bh:
		{CPU8086_OP1C,NULL}, //1Ch:
		{CPU8086_OP1D,NULL}, //1Dh:
		{CPU8086_OP1E,NULL}, //1Eh:
		{CPU8086_OP1F,NULL}, //1Fh:
//0x20:
		{CPU8086_OP20,NULL}, //20h:
		{CPU8086_OP21,NULL}, //21h:
		{CPU8086_OP22,NULL}, //22h:
		{CPU8086_OP23,NULL}, //23h:
		{CPU8086_OP24,NULL}, //24h:
		{CPU8086_OP25,NULL}, //25h:
		{unkOP_8086,NULL}, //26h: Special
		{CPU8086_OP27,NULL}, //27h:
		{CPU8086_OP28,NULL}, //28h:
		{CPU8086_OP29,NULL}, //29h:
		{CPU8086_OP2A,NULL}, //2Ah:
		{CPU8086_OP2B,NULL}, //2Bh:
		{CPU8086_OP2C,NULL}, //2Ch:
		{CPU8086_OP2D,NULL}, //2Dh:
		{unkOP_8086,NULL}, //2Eh: Special
		{CPU8086_OP2F,NULL}, //2Fh:
//0x30:
		{CPU8086_OP30,NULL}, //30h:
		{CPU8086_OP31,NULL}, //31h:
		{CPU8086_OP32,NULL}, //32h:
		{CPU8086_OP33,NULL}, //33h:
		{CPU8086_OP34,NULL}, //34h:
		{CPU8086_OP35,NULL}, //35h:
		{unkOP_8086,NULL}, //36h: Special
		{CPU8086_OP37,NULL}, //37h:
		{CPU8086_OP38,NULL}, //38h:
		{CPU8086_OP39,NULL}, //39h:
		{CPU8086_OP3A,NULL}, //3Ah:
		{CPU8086_OP3B,NULL}, //3Bh:
		{CPU8086_OP3C,NULL}, //3Ch:
		{CPU8086_OP3D,NULL}, //3Dh:
		{unkOP_8086,NULL}, //3Eh: Special
		{CPU8086_OP3F,NULL}, //3Fh:
//0x40:
		{CPU8086_OP40,NULL}, //40h:
		{CPU8086_OP41,NULL}, //41h:
		{CPU8086_OP42,NULL}, //42h:
		{CPU8086_OP43,NULL}, //43h:
		{CPU8086_OP44,NULL}, //44h:
		{CPU8086_OP45,NULL}, //45h:
		{CPU8086_OP46,NULL}, //46h:
		{CPU8086_OP47,NULL}, //47h:
		{CPU8086_OP48,NULL}, //48h:
		{CPU8086_OP49,NULL}, //49h:
		{CPU8086_OP4A,NULL}, //4Ah:
		{CPU8086_OP4B,NULL}, //4Bh:
		{CPU8086_OP4C,NULL}, //4Ch:
		{CPU8086_OP4D,NULL}, //4Dh:
		{CPU8086_OP4E,NULL}, //4Eh:
		{CPU8086_OP4F,NULL}, //4Fh:
//0x50:
		{CPU8086_OP50,NULL}, //50h:
		{CPU8086_OP51,NULL}, //51h:
		{CPU8086_OP52,NULL}, //52h:
		{CPU8086_OP53,NULL}, //53h:
		{CPU8086_OP54,NULL}, //54h:
		{CPU8086_OP55,NULL}, //55h:
		{CPU8086_OP56,NULL}, //56h:
		{CPU8086_OP57,NULL}, //57h:
		{CPU8086_OP58,NULL}, //58h:
		{CPU8086_OP59,NULL}, //59h:
		{CPU8086_OP5A,NULL}, //5Ah:
		{CPU8086_OP5B,NULL}, //5Bh:
		{CPU8086_OP5C,NULL}, //Ch:
		{CPU8086_OP5D,NULL}, //5Dh:
		{CPU8086_OP5E,NULL}, //Eh:
		{CPU8086_OP5F,NULL}, //5Fh:
//0x60: Aliases of 0x80 on the 8086 only.
		{CPU8086_OP70,NULL}, //60h:
		{CPU8086_OP71,NULL}, //61h:
		{CPU8086_OP72,NULL}, //62h:
		{CPU8086_OP73,NULL}, //63h:
		{CPU8086_OP74,NULL}, //64h:
		{CPU8086_OP75,NULL}, //65h:
		{CPU8086_OP76,NULL}, //66h:
		{CPU8086_OP77,NULL}, //67h:
		{CPU8086_OP78,NULL}, //68h:
		{CPU8086_OP79,NULL}, //69h:
		{CPU8086_OP7A,NULL}, //6Ah:
		{CPU8086_OP7B,NULL}, //6Bh:
		{CPU8086_OP7C,NULL}, //6Ch:
		{CPU8086_OP7D,NULL}, //6Dh:
		{CPU8086_OP7E,NULL}, //6Eh:
		{CPU8086_OP7F,NULL}, //6Fh:
//0x70: Conditional JMP OPcodes:
		{CPU8086_OP70,NULL}, //70h:
		{CPU8086_OP71,NULL}, //71h:
		{CPU8086_OP72,NULL}, //72h:
		{CPU8086_OP73,NULL}, //73h:
		{CPU8086_OP74,NULL}, //74h:
		{CPU8086_OP75,NULL}, //75h:
		{CPU8086_OP76,NULL}, //76h:
		{CPU8086_OP77,NULL}, //77h:
		{CPU8086_OP78,NULL}, //78h:
		{CPU8086_OP79,NULL}, //79h:
		{CPU8086_OP7A,NULL}, //7Ah:
		{CPU8086_OP7B,NULL}, //7Bh:
		{CPU8086_OP7C,NULL}, //7Ch:
		{CPU8086_OP7D,NULL}, //7Dh:
		{CPU8086_OP7E,NULL}, //7Eh:
		{CPU8086_OP7F,NULL}, //7Fh:
//0x80:
		{CPU8086_OP80,NULL}, //80h:
		{CPU8086_OP81,NULL}, //81h:
		{CPU8086_OP82,NULL}, //82h:
		{CPU8086_OP83,NULL}, //83h:
		{CPU8086_OP84,NULL}, //84h:
		{CPU8086_OP85,NULL}, //85h:
		{CPU8086_OP86,NULL}, //86h:
		{CPU8086_OP87,NULL}, //87h:
		{CPU8086_OP88,NULL}, //88h:
		{CPU8086_OP89,NULL}, //89h:
		{CPU8086_OP8A,NULL}, //8Ah:
		{CPU8086_OP8B,NULL}, //8Bh:
		{CPU8086_OP8C,NULL}, //8Ch:
		{CPU8086_OP8D,NULL}, //8Dh:
		{CPU8086_OP8E,NULL}, //8Eh:
		{CPU8086_OP8F,NULL}, //8Fh:
//0x90:
		{CPU8086_OP90,NULL}, //90h:
		{CPU8086_OP91,NULL}, //91h:
		{CPU8086_OP92,NULL}, //92h:
		{CPU8086_OP93,NULL}, //93h:
		{CPU8086_OP94,NULL}, //94h:
		{CPU8086_OP95,NULL}, //95h:
		{CPU8086_OP96,NULL}, //96h:
		{CPU8086_OP97,NULL}, //97h:
		{CPU8086_OP98,NULL}, //98h:
		{CPU8086_OP99,NULL}, //99h:
		{CPU8086_OP9A,NULL}, //9Ah:
		{CPU8086_OP9B,NULL}, //9Bh:
		{CPU8086_OP9C,NULL}, //9Ch:
		{CPU8086_OP9D,NULL}, //9Dh:
		{CPU8086_OP9E,NULL}, //9Eh:
		{CPU8086_OP9F,NULL}, //9Fh:
//0xA0:
		{CPU8086_OPA0,NULL}, //A0h:
		{CPU8086_OPA1,NULL}, //A1h:
		{CPU8086_OPA2,NULL}, //A2h:
		{CPU8086_OPA3,NULL}, //A3h:
		{CPU8086_OPA4,NULL}, //A4h:
		{CPU8086_OPA5,NULL}, //A5h:
		{CPU8086_OPA6,NULL}, //A6h:
		{CPU8086_OPA7,NULL}, //A7h:
		{CPU8086_OPA8,NULL}, //A8h:
		{CPU8086_OPA9,NULL}, //A9h:
		{CPU8086_OPAA,NULL}, //AAh:
		{CPU8086_OPAB,NULL}, //ABh:
		{CPU8086_OPAC,NULL}, //ACh:
		{CPU8086_OPAD,NULL}, //ADh:
		{CPU8086_OPAE,NULL}, //AEh:
		{CPU8086_OPAF,NULL}, //AFh:
//0xB0:
		{CPU8086_OPB0,NULL}, //B0h:
		{CPU8086_OPB1,NULL}, //B1h:
		{CPU8086_OPB2,NULL}, //B2h:
		{CPU8086_OPB3,NULL}, //B3h:
		{CPU8086_OPB4,NULL}, //B4h:
		{CPU8086_OPB5,NULL}, //B5h:
		{CPU8086_OPB6,NULL}, //B6h:
		{CPU8086_OPB7,NULL}, //B7h:
		{CPU8086_OPB8,NULL}, //B8h:
		{CPU8086_OPB9,NULL}, //B9h:
		{CPU8086_OPBA,NULL}, //BAh:
		{CPU8086_OPBB,NULL}, //BBh:
		{CPU8086_OPBC,NULL}, //BCh:
		{CPU8086_OPBD,NULL}, //BDh:
		{CPU8086_OPBE,NULL}, //BEh:
		{CPU8086_OPBF,NULL}, //BFh:
//0xC0:
		{unkOP_8086,NULL}, //C0h: UNK
		{unkOP_8086,NULL}, //C1h: UNK
		{CPU8086_OPC2,NULL}, //C2h:
		{CPU8086_OPC3,NULL}, //C3h:
		{CPU8086_OPC4,NULL}, //C4h:
		{CPU8086_OPC5,NULL}, //C5h:
		{CPU8086_OPC6,NULL}, //C6h:
		{CPU8086_OPC7,NULL}, //C7h:
		{unkOP_8086,NULL}, //C8h: UNK
		{unkOP_8086,NULL}, //C9h: UNK
		{CPU8086_OPCA,NULL}, //CAh:
		{CPU8086_OPCB,NULL}, //CBh:
		{CPU8086_OPCC,NULL}, //CCh:
		{CPU8086_OPCD,NULL}, //CDh:
		{CPU8086_OPCE,NULL}, //CEh:
		{CPU8086_OPCF,NULL}, //CFh:
//0xD0:
		{CPU8086_OPD0,NULL}, //D0h:
		{CPU8086_OPD1,NULL}, //D1h:
		{CPU8086_OPD2,NULL}, //D2h:
		{CPU8086_OPD3,NULL}, //D3h:
		{CPU8086_OPD4,NULL}, //D4h:
		{CPU8086_OPD5,NULL}, //D5h:
		{CPU8086_OPD6,NULL}, //D6h: SALC!
		{CPU8086_OPD7,NULL}, //D7h:
		{FPU8087_noCOOP,NULL}, //D8h: CoProcessor UNK
		{FPU8087_noCOOP,NULL}, //D9h: CoProcessor Minimum
		{FPU8087_noCOOP,NULL}, //DAh: CoProcessor
		{FPU8087_noCOOP,NULL}, //DBh: CoProcessor Minimum
		{FPU8087_noCOOP,NULL}, //DCh: CoProcessor
		{FPU8087_noCOOP,NULL}, //DDh: CoProcessor Minimum
		{FPU8087_noCOOP,NULL}, //DEh: CoProcessor
		{FPU8087_noCOOP,NULL}, //DFh: COProcessor minimum
//0xE0:
		{CPU8086_OPE0,NULL}, //E0h:
		{CPU8086_OPE1,NULL}, //E1h:
		{CPU8086_OPE2,NULL}, //E2h:
		{CPU8086_OPE3,NULL}, //E3h:
		{CPU8086_OPE4,NULL}, //E4h:
		{CPU8086_OPE5,NULL}, //E5h:
		{CPU8086_OPE6,NULL}, //E6h:
		{CPU8086_OPE7,NULL}, //E7h:
		{CPU8086_OPE8,NULL}, //E8h:
		{CPU8086_OPE9,NULL}, //E9h:
		{CPU8086_OPEA,NULL}, //EAh:
		{CPU8086_OPEB,NULL}, //EBh:
		{CPU8086_OPEC,NULL}, //ECh:
		{CPU8086_OPED,NULL}, //EDh:
		{CPU8086_OPEE,NULL}, //EEh:
		{CPU8086_OPEF,NULL}, //EFh:
//0xF0:
		{unkOP_8086,NULL}, //F0h: Special
		{CPU8086_OPF1,NULL}, //F1h: UNK
		{unkOP_8086,NULL}, //F2h: Special
		{unkOP_8086,NULL}, //F3h: Special
		{CPU8086_OPF4,NULL}, //F4h:
		{CPU8086_OPF5,NULL}, //F5h:
		{CPU8086_OPF6,NULL}, //F6h:
		{CPU8086_OPF7,NULL}, //F7h:
		{CPU8086_OPF8,NULL}, //F8h:
		{CPU8086_OPF9,NULL}, //F9h:
		{CPU8086_OPFA,NULL}, //FAh:
		{CPU8086_OPFB,NULL}, //FBh:
		{CPU8086_OPFC,NULL}, //FCh:
		{CPU8086_OPFD,NULL}, //FDh:
		{CPU8086_OPFE,NULL}, //FEh:
		{CPU8086_OPFF,NULL},  //FFh:
	},

	//80186
	{
//0x00:
		{NULL,NULL}, //00h:
		{NULL,NULL}, //01h:
		{NULL,NULL}, //02h:
		{NULL,NULL}, //03h:
		{NULL,NULL}, //04h:
		{NULL,NULL}, //05h:
		{NULL,NULL}, //06h:
		{NULL,NULL}, //07h:
		{NULL,NULL}, //08h:
		{NULL,NULL}, //09h:
		{NULL,NULL}, //0Ah:
		{NULL,NULL}, //0Bh:
		{NULL,NULL}, //0Ch:
		{NULL,NULL}, //0Dh:
		{NULL,NULL}, //0Eh:
		{unkOP_186,NULL}, //0Fh: 80186 specific OPcode!
//0x10:
		{NULL,NULL}, //10h:
		{NULL,NULL}, //11h:
		{NULL,NULL}, //12h:
		{NULL,NULL}, //13h:
		{NULL,NULL}, //14h:
		{NULL,NULL}, //15h:
		{NULL,NULL}, //16h:
		{NULL,NULL}, //17h:
		{NULL,NULL}, //18h:
		{NULL,NULL}, //19h:
		{NULL,NULL}, //1Ah:
		{NULL,NULL}, //1Bh:
		{NULL,NULL}, //1Ch:
		{NULL,NULL}, //1Dh:
		{NULL,NULL}, //1Eh:
		{NULL,NULL}, //1Fh:
//0x20:
		{NULL,NULL}, //20h:
		{NULL,NULL}, //21h:
		{NULL,NULL}, //22h:
		{NULL,NULL}, //23h:
		{NULL,NULL}, //24h:
		{NULL,NULL}, //25h:
		{NULL,NULL}, //26h: Special
		{NULL,NULL}, //27h:
		{NULL,NULL}, //28h:
		{NULL,NULL}, //29h:
		{NULL,NULL}, //2Ah:
		{NULL,NULL}, //2Bh:
		{NULL,NULL}, //2Ch:
		{NULL,NULL}, //2Dh:
		{NULL,NULL}, //2Eh: Special
		{NULL,NULL}, //2Fh:
//0x30:
		{NULL,NULL}, //30h:
		{NULL,NULL}, //31h:
		{NULL,NULL}, //32h:
		{NULL,NULL}, //33h:
		{NULL,NULL}, //34h:
		{NULL,NULL}, //35h:
		{NULL,NULL}, //36h: Special
		{NULL,NULL}, //37h:
		{NULL,NULL}, //38h:
		{NULL,NULL}, //39h:
		{NULL,NULL}, //3Ah:
		{NULL,NULL}, //3Bh:
		{NULL,NULL}, //3Ch:
		{NULL,NULL}, //3Dh:
		{NULL,NULL}, //3Eh: Special
		{NULL,NULL}, //3Fh:
//0x40:
		{NULL,NULL}, //40h:
		{NULL,NULL}, //41h:
		{NULL,NULL}, //42h:
		{NULL,NULL}, //43h:
		{NULL,NULL}, //44h:
		{NULL,NULL}, //45h:
		{NULL,NULL}, //46h:
		{NULL,NULL}, //47h:
		{NULL,NULL}, //48h:
		{NULL,NULL}, //49h:
		{NULL,NULL}, //4Ah:
		{NULL,NULL}, //4Bh:
		{NULL,NULL}, //4Ch:
		{NULL,NULL}, //4Dh:
		{NULL,NULL}, //4Eh:
		{NULL,NULL}, //4Fh:
//0x50:
		{NULL,NULL}, //50h:
		{NULL,NULL}, //51h:
		{NULL,NULL}, //52h:
		{NULL,NULL}, //53h:
		{NULL,NULL}, //54h:
		{NULL,NULL}, //55h:
		{NULL,NULL}, //56h:
		{NULL,NULL}, //57h:
		{NULL,NULL}, //58h:
		{NULL,NULL}, //59h:
		{NULL,NULL}, //5Ah:
		{NULL,NULL}, //5Bh:
		{NULL,NULL}, //5Ch:
		{NULL,NULL}, //5Dh:
		{NULL,NULL}, //5Eh:
		{NULL,NULL}, //5Fh:
//0x60:
		{CPU186_OP60,NULL}, //60h: PUSHA(removed here)
		{CPU186_OP61,NULL}, //61h: POPA(removed here)
		{CPU186_OP62,NULL}, //UNK
		{unkOP_186,NULL}, //UNK
		{unkOP_186,NULL}, //UNK
		{unkOP_186,NULL}, //UNK
		{unkOP_186,NULL}, //UNK
		{unkOP_186,NULL}, //UNK
		{CPU186_OP68,NULL}, //68h:
		{CPU186_OP69,NULL}, //69h:
		{CPU186_OP6A,NULL}, //6Ah:
		{CPU186_OP6B,NULL}, //6Bh:
		{CPU186_OP6C,NULL}, //6Ch:
		{CPU186_OP6D,NULL}, //6Dh:
		{CPU186_OP6E,NULL}, //6Eh:
		{CPU186_OP6F,NULL}, //6Fh:
//0x70: Conditional JMP OPcodes:
		{NULL,NULL}, //70h:
		{NULL,NULL}, //71h:
		{NULL,NULL}, //72h:
		{NULL,NULL}, //73h:
		{NULL,NULL}, //74h:
		{NULL,NULL}, //75h:
		{NULL,NULL}, //76h:
		{NULL,NULL}, //77h:
		{NULL,NULL}, //78h:
		{NULL,NULL}, //79h:
		{NULL,NULL}, //7Ah:
		{NULL,NULL}, //7Bh:
		{NULL,NULL}, //7Ch:
		{NULL,NULL}, //7Dh:
		{NULL,NULL}, //7Eh:
		{NULL,NULL}, //7Fh:
//0x80:
		{NULL,NULL}, //80h:
		{NULL,NULL}, //81h:
		{NULL,NULL}, //82h:
		{NULL,NULL}, //83h:
		{NULL,NULL}, //84h:
		{NULL,NULL}, //85h:
		{NULL,NULL}, //86h:
		{NULL,NULL}, //87h:
		{NULL,NULL}, //88h:
		{NULL,NULL}, //89h:
		{NULL,NULL}, //8Ah:
		{NULL,NULL}, //8Bh:
		{NULL,NULL}, //8Ch:
		{NULL,NULL}, //8Dh:
		{NULL,NULL}, //8Eh:
		{NULL,NULL}, //8Fh:
//0x90:
		{NULL,NULL}, //90h:
		{NULL,NULL}, //91h:
		{NULL,NULL}, //92h:
		{NULL,NULL}, //93h:
		{NULL,NULL}, //94h:
		{NULL,NULL}, //95h:
		{NULL,NULL}, //96h:
		{NULL,NULL}, //97h:
		{NULL,NULL}, //98h:
		{NULL,NULL}, //99h:
		{NULL,NULL}, //9Ah:
		{NULL,NULL}, //9Bh:
		{NULL,NULL}, //9Ch:
		{NULL,NULL}, //9Dh:
		{NULL,NULL}, //9Eh:
		{NULL,NULL}, //9Fh:
//0xA0:
		{NULL,NULL}, //A0h:
		{NULL,NULL}, //A1h:
		{NULL,NULL}, //A2h:
		{NULL,NULL}, //A3h:
		{NULL,NULL}, //A4h:
		{NULL,NULL}, //A5h:
		{NULL,NULL}, //A6h:
		{NULL,NULL}, //A7h:
		{NULL,NULL}, //A8h:
		{NULL,NULL}, //A9h:
		{NULL,NULL}, //AAh:
		{NULL,NULL}, //ABh:
		{NULL,NULL}, //ACh:
		{NULL,NULL}, //ADh:
		{NULL,NULL}, //AEh:
		{NULL,NULL}, //AFh:
//0xB0:
		{NULL,NULL}, //B0h:
		{NULL,NULL}, //B1h:
		{NULL,NULL}, //B2h:
		{NULL,NULL}, //B3h:
		{NULL,NULL}, //B4h:
		{NULL,NULL}, //B5h:
		{NULL,NULL}, //B6h:
		{NULL,NULL}, //B7h:
		{NULL,NULL}, //B8h:
		{NULL,NULL}, //B9h:
		{NULL,NULL}, //BAh:
		{NULL,NULL}, //BBh:
		{NULL,NULL}, //BCh:
		{NULL,NULL}, //BDh:
		{NULL,NULL}, //BEh:
		{NULL,NULL}, //BFh:
//0xC0:
		{CPU186_OPC0,NULL}, //C0h:
		{CPU186_OPC1,NULL}, //C1h:
		{NULL,NULL}, //C2h:
		{NULL,NULL}, //C3h:
		{NULL,NULL}, //C4h:
		{NULL,NULL}, //C5h:
		{NULL,NULL}, //C6h:
		{NULL,NULL}, //C7h:
		{CPU186_OPC8,NULL}, //C8h:
		{CPU186_OPC9,NULL}, //C9h:
		{NULL,NULL}, //CAh:
		{NULL,NULL}, //CBh:
		{NULL,NULL}, //CCh:
		{NULL,NULL}, //CDh:
		{NULL,NULL}, //CEh:
		{NULL,NULL}, //CFh:
//0xD0:
		{NULL,NULL}, //D0h:
		{NULL,NULL}, //D1h:
		{NULL,NULL}, //D2h:
		{NULL,NULL}, //D3h:
		{NULL,NULL}, //D4h:
		{NULL,NULL}, //D5h:
		{unkOP_186,NULL}, //D6h: UNK
		{NULL,NULL}, //D7h:
		{NULL,NULL}, //D8h: UNK
		{NULL,NULL}, //D9h: CoProcessor Minimum
		{NULL,NULL}, //DAh: UNK
		{NULL,NULL}, //DBh: CoProcessor Minimum
		{NULL,NULL}, //DCh: UNK
		{NULL,NULL}, //DDh: CoProcessor Minimum
		{NULL,NULL}, //DEh: UNK
		{NULL,NULL}, //DFh: COProcessor minimum
//0xE0:
		{NULL,NULL}, //E0h:
		{NULL,NULL}, //E1h:
		{NULL,NULL}, //E2h:
		{NULL,NULL}, //E3h:
		{NULL,NULL}, //E4h:
		{NULL,NULL}, //E5h:
		{NULL,NULL}, //E6h:
		{NULL,NULL}, //E7h:
		{NULL,NULL}, //E8h:
		{NULL,NULL}, //E9h:
		{NULL,NULL}, //EAh:
		{NULL,NULL}, //EBh:
		{NULL,NULL}, //ECh:
		{NULL,NULL}, //EDh:
		{NULL,NULL}, //EEh:
		{NULL,NULL}, //EFh:
//0xF0:
		{NULL,NULL}, //F0h: Special
		{unkOP_186,NULL}, //F1h: UNK
		{NULL,NULL}, //F2h: Special
		{NULL,NULL}, //F3h: Special
		{NULL,NULL}, //F4h:
		{NULL,NULL}, //F5h:
		{NULL,NULL}, //F6h:
		{NULL,NULL}, //F7h:
		{NULL,NULL}, //F8h:
		{NULL,NULL}, //F9h:
		{NULL,NULL}, //FAh:
		{NULL,NULL}, //FBh:
		{NULL,NULL}, //FCh:
		{NULL,NULL}, //FDh:
		{NULL,NULL}, //FEh:
		{CPU8086_OPFF,NULL}  //FFh:
	},

	//80286
	{
		//0x00:
		{ NULL, NULL }, //00h:
		{ NULL, NULL }, //01h:
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
		{ NULL, NULL }, //0Fh: 80186 specific OPcode!
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
		{ NULL, NULL }, //20h:
		{ NULL, NULL }, //21h:
		{ NULL, NULL }, //22h:
		{ NULL, NULL }, //23h:
		{ NULL, NULL }, //24h:
		{ NULL, NULL }, //25h:
		{ NULL, NULL }, //26h: Special
		{ NULL, NULL }, //27h:
		{ NULL, NULL }, //28h:
		{ NULL, NULL }, //29h:
		{ NULL, NULL }, //2Ah:
		{ NULL, NULL }, //2Bh:
		{ NULL, NULL }, //2Ch:
		{ NULL, NULL }, //2Dh:
		{ NULL, NULL }, //2Eh: Special
		{ NULL, NULL }, //2Fh:
		//0x30:
		{ NULL, NULL }, //30h:
		{ NULL, NULL }, //31h:
		{ NULL, NULL }, //32h:
		{ NULL, NULL }, //33h:
		{ NULL, NULL }, //34h:
		{ NULL, NULL }, //35h:
		{ NULL, NULL }, //36h: Special
		{ NULL, NULL }, //37h:
		{ NULL, NULL }, //38h:
		{ NULL, NULL }, //39h:
		{ NULL, NULL }, //3Ah:
		{ NULL, NULL }, //3Bh:
		{ NULL, NULL }, //3Ch:
		{ NULL, NULL }, //3Dh:
		{ NULL, NULL }, //3Eh: Special
		{ NULL, NULL }, //3Fh:
		//0x40:
		{ NULL, NULL }, //40h:
		{ NULL, NULL }, //41h:
		{ NULL, NULL }, //42h:
		{ NULL, NULL }, //43h:
		{ NULL, NULL }, //44h:
		{ NULL, NULL }, //45h:
		{ NULL, NULL }, //46h:
		{ NULL, NULL }, //47h:
		{ NULL, NULL }, //48h:
		{ NULL, NULL }, //49h:
		{ NULL, NULL }, //4Ah:
		{ NULL, NULL }, //4Bh:
		{ NULL, NULL }, //4Ch:
		{ NULL, NULL }, //4Dh:
		{ NULL, NULL }, //4Eh:
		{ NULL, NULL }, //4Fh:
		//0x50:
		{ NULL, NULL }, //50h:
		{ NULL, NULL }, //51h:
		{ NULL, NULL }, //52h:
		{ NULL, NULL }, //53h:
		{ NULL, NULL }, //54h:
		{ NULL, NULL }, //55h:
		{ NULL, NULL }, //56h:
		{ NULL, NULL }, //57h:
		{ NULL, NULL }, //58h:
		{ NULL, NULL }, //59h:
		{ NULL, NULL }, //5Ah:
		{ NULL, NULL }, //5Bh:
		{ NULL, NULL }, //5Ch:
		{ NULL, NULL }, //5Dh:
		{ NULL, NULL }, //5Eh:
		{ NULL, NULL }, //5Fh:
		//0x60:
		{ NULL, NULL }, //60h: PUSHA(removed here)
		{ NULL, NULL }, //61h: POPA(removed here)
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //68h:
		{ NULL, NULL }, //69h:
		{ NULL, NULL }, //6Ah:
		{ NULL, NULL }, //6Bh:
		{ NULL, NULL }, //6Ch:
		{ NULL, NULL }, //6Dh:
		{ NULL, NULL }, //6Eh:
		{ NULL, NULL }, //6Fh:
		//0x70: Conditional JMP OPcodes:
		{ NULL, NULL }, //70h:
		{ NULL, NULL }, //71h:
		{ NULL, NULL }, //72h:
		{ NULL, NULL }, //73h:
		{ NULL, NULL }, //74h:
		{ NULL, NULL }, //75h:
		{ NULL, NULL }, //76h:
		{ NULL, NULL }, //77h:
		{ NULL, NULL }, //78h:
		{ NULL, NULL }, //79h:
		{ NULL, NULL }, //7Ah:
		{ NULL, NULL }, //7Bh:
		{ NULL, NULL }, //7Ch:
		{ NULL, NULL }, //7Dh:
		{ NULL, NULL }, //7Eh:
		{ NULL, NULL }, //7Fh:
		//0x80:
		{ NULL, NULL }, //80h:
		{ NULL, NULL }, //81h:
		{ NULL, NULL }, //82h:
		{ NULL, NULL }, //83h:
		{ NULL, NULL }, //84h:
		{ NULL, NULL }, //85h:
		{ NULL, NULL }, //86h:
		{ NULL, NULL }, //87h:
		{ NULL, NULL }, //88h:
		{ NULL, NULL }, //89h:
		{ NULL, NULL }, //8Ah:
		{ NULL, NULL }, //8Bh:
		{ NULL, NULL }, //8Ch:
		{ NULL, NULL }, //8Dh:
		{ NULL, NULL }, //8Eh:
		{ NULL, NULL }, //8Fh:
		//0x90:
		{ NULL, NULL }, //90h:
		{ NULL, NULL }, //91h:
		{ NULL, NULL }, //92h:
		{ NULL, NULL }, //93h:
		{ NULL, NULL }, //94h:
		{ NULL, NULL }, //95h:
		{ NULL, NULL }, //96h:
		{ NULL, NULL }, //97h:
		{ NULL, NULL }, //98h:
		{ NULL, NULL }, //99h:
		{ NULL, NULL }, //9Ah:
		{ NULL, NULL }, //9Bh:
		{ NULL, NULL }, //9Ch:
		{ NULL, NULL }, //9Dh:
		{ NULL, NULL }, //9Eh:
		{ NULL, NULL }, //9Fh:
		//0xA0:
		{ NULL, NULL }, //A0h:
		{ NULL, NULL }, //A1h:
		{ NULL, NULL }, //A2h:
		{ NULL, NULL }, //A3h:
		{ NULL, NULL }, //A4h:
		{ NULL, NULL }, //A5h:
		{ NULL, NULL }, //A6h:
		{ NULL, NULL }, //A7h:
		{ NULL, NULL }, //A8h:
		{ NULL, NULL }, //A9h:
		{ NULL, NULL }, //AAh:
		{ NULL, NULL }, //ABh:
		{ NULL, NULL }, //ACh:
		{ NULL, NULL }, //ADh:
		{ NULL, NULL }, //AEh:
		{ NULL, NULL }, //AFh:
		//0xB0:
		{ NULL, NULL }, //B0h:
		{ NULL, NULL }, //B1h:
		{ NULL, NULL }, //B2h:
		{ NULL, NULL }, //B3h:
		{ NULL, NULL }, //B4h:
		{ NULL, NULL }, //B5h:
		{ NULL, NULL }, //B6h:
		{ NULL, NULL }, //B7h:
		{ NULL, NULL }, //B8h:
		{ NULL, NULL }, //B9h:
		{ NULL, NULL }, //BAh:
		{ NULL, NULL }, //BBh:
		{ NULL, NULL }, //BCh:
		{ NULL, NULL }, //BDh:
		{ NULL, NULL }, //BEh:
		{ NULL, NULL }, //BFh:
		//0xC0:
		{ NULL, NULL }, //C0h:
		{ NULL, NULL }, //C1h:
		{ NULL, NULL }, //C2h:
		{ NULL, NULL }, //C3h:
		{ NULL, NULL }, //C4h:
		{ NULL, NULL }, //C5h:
		{ NULL, NULL }, //C6h:
		{ NULL, NULL }, //C7h:
		{ NULL, NULL }, //C8h:
		{ NULL, NULL }, //C9h:
		{ NULL, NULL }, //CAh:
		{ NULL, NULL }, //CBh:
		{ NULL, NULL }, //CCh:
		{ NULL, NULL }, //CDh:
		{ NULL, NULL }, //CEh:
		{ NULL, NULL }, //CFh:
		//0xD0:
		{ NULL, NULL }, //D0h:
		{ NULL, NULL }, //D1h:
		{ NULL, NULL }, //D2h:
		{ NULL, NULL }, //D3h:
		{ NULL, NULL }, //D4h:
		{ NULL, NULL }, //D5h:
		{ NULL, NULL }, //D6h: UNK
		{ NULL, NULL }, //D7h:
		{ NULL, NULL }, //D8h: UNK
		{ NULL, NULL }, //D9h: CoProcessor Minimum
		{ NULL, NULL }, //DAh: UNK
		{ NULL, NULL }, //DBh: CoProcessor Minimum
		{ NULL, NULL }, //DCh: UNK
		{ NULL, NULL }, //DDh: CoProcessor Minimum
		{ NULL, NULL }, //DEh: UNK
		{ NULL, NULL }, //DFh: COProcessor minimum
		//0xE0:
		{ NULL, NULL }, //E0h:
		{ NULL, NULL }, //E1h:
		{ NULL, NULL }, //E2h:
		{ NULL, NULL }, //E3h:
		{ NULL, NULL }, //E4h:
		{ NULL, NULL }, //E5h:
		{ NULL, NULL }, //E6h:
		{ NULL, NULL }, //E7h:
		{ NULL, NULL }, //E8h:
		{ NULL, NULL }, //E9h:
		{ NULL, NULL }, //EAh:
		{ NULL, NULL }, //EBh:
		{ NULL, NULL }, //ECh:
		{ NULL, NULL }, //EDh:
		{ NULL, NULL }, //EEh:
		{ NULL, NULL }, //EFh:
		//0xF0:
		{ NULL, NULL }, //F0h: Special
		{ NULL, NULL }, //F1h: UNK
		{ NULL, NULL }, //F2h: Special
		{ NULL, NULL }, //F3h: Special
		{ NULL, NULL }, //F4h:
		{ NULL, NULL }, //F5h:
		{ NULL, NULL }, //F6h:
		{ NULL, NULL }, //F7h:
		{ NULL, NULL }, //F8h:
		{ NULL, NULL }, //F9h:
		{ NULL, NULL }, //FAh:
		{ NULL, NULL }, //FBh:
		{ NULL, NULL }, //FCh:
		{ NULL, NULL }, //FDh:
		{ NULL, NULL }, //FEh:
		{ NULL, NULL }  //FFh:
	},

	//80386
	{

		//0x00:
		{ NULL, NULL }, //00h:
		{ NULL, NULL }, //01h:
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
		{ NULL, NULL }, //0Fh: 80186 specific OPcode!
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
		{ NULL, NULL }, //20h:
		{ NULL, NULL }, //21h:
		{ NULL, NULL }, //22h:
		{ NULL, NULL }, //23h:
		{ NULL, NULL }, //24h:
		{ NULL, NULL }, //25h:
		{ NULL, NULL }, //26h: Special
		{ NULL, NULL }, //27h:
		{ NULL, NULL }, //28h:
		{ NULL, NULL }, //29h:
		{ NULL, NULL }, //2Ah:
		{ NULL, NULL }, //2Bh:
		{ NULL, NULL }, //2Ch:
		{ NULL, NULL }, //2Dh:
		{ NULL, NULL }, //2Eh: Special
		{ NULL, NULL }, //2Fh:
		//0x30:
		{ NULL, NULL }, //30h:
		{ NULL, NULL }, //31h:
		{ NULL, NULL }, //32h:
		{ NULL, NULL }, //33h:
		{ NULL, NULL }, //34h:
		{ NULL, NULL }, //35h:
		{ NULL, NULL }, //36h: Special
		{ NULL, NULL }, //37h:
		{ NULL, NULL }, //38h:
		{ NULL, NULL }, //39h:
		{ NULL, NULL }, //3Ah:
		{ NULL, NULL }, //3Bh:
		{ NULL, NULL }, //3Ch:
		{ NULL, NULL }, //3Dh:
		{ NULL, NULL }, //3Eh: Special
		{ NULL, NULL }, //3Fh:
		//0x40:
		{ NULL, NULL }, //40h:
		{ NULL, NULL }, //41h:
		{ NULL, NULL }, //42h:
		{ NULL, NULL }, //43h:
		{ NULL, NULL }, //44h:
		{ NULL, NULL }, //45h:
		{ NULL, NULL }, //46h:
		{ NULL, NULL }, //47h:
		{ NULL, NULL }, //48h:
		{ NULL, NULL }, //49h:
		{ NULL, NULL }, //4Ah:
		{ NULL, NULL }, //4Bh:
		{ NULL, NULL }, //4Ch:
		{ NULL, NULL }, //4Dh:
		{ NULL, NULL }, //4Eh:
		{ NULL, NULL }, //4Fh:
		//0x50:
		{ NULL, NULL }, //50h:
		{ NULL, NULL }, //51h:
		{ NULL, NULL }, //52h:
		{ NULL, NULL }, //53h:
		{ NULL, NULL }, //54h:
		{ NULL, NULL }, //55h:
		{ NULL, NULL }, //56h:
		{ NULL, NULL }, //57h:
		{ NULL, NULL }, //58h:
		{ NULL, NULL }, //59h:
		{ NULL, NULL }, //5Ah:
		{ NULL, NULL }, //5Bh:
		{ NULL, NULL }, //5Ch:
		{ NULL, NULL }, //5Dh:
		{ NULL, NULL }, //5Eh:
		{ NULL, NULL }, //5Fh:
		//0x60:
		{ NULL, NULL }, //60h: PUSHA(removed here)
		{ NULL, NULL }, //61h: POPA(removed here)
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //68h:
		{ NULL, NULL }, //69h:
		{ NULL, NULL }, //6Ah:
		{ NULL, NULL }, //6Bh:
		{ NULL, NULL }, //6Ch:
		{ NULL, NULL }, //6Dh:
		{ NULL, NULL }, //6Eh:
		{ NULL, NULL }, //6Fh:
		//0x70: Conditional JMP OPcodes:
		{ NULL, NULL }, //70h:
		{ NULL, NULL }, //71h:
		{ NULL, NULL }, //72h:
		{ NULL, NULL }, //73h:
		{ NULL, NULL }, //74h:
		{ NULL, NULL }, //75h:
		{ NULL, NULL }, //76h:
		{ NULL, NULL }, //77h:
		{ NULL, NULL }, //78h:
		{ NULL, NULL }, //79h:
		{ NULL, NULL }, //7Ah:
		{ NULL, NULL }, //7Bh:
		{ NULL, NULL }, //7Ch:
		{ NULL, NULL }, //7Dh:
		{ NULL, NULL }, //7Eh:
		{ NULL, NULL }, //7Fh:
		//0x80:
		{ NULL, NULL }, //80h:
		{ NULL, NULL }, //81h:
		{ NULL, NULL }, //82h:
		{ NULL, NULL }, //83h:
		{ NULL, NULL }, //84h:
		{ NULL, NULL }, //85h:
		{ NULL, NULL }, //86h:
		{ NULL, NULL }, //87h:
		{ NULL, NULL }, //88h:
		{ NULL, NULL }, //89h:
		{ NULL, NULL }, //8Ah:
		{ NULL, NULL }, //8Bh:
		{ NULL, NULL }, //8Ch:
		{ NULL, NULL }, //8Dh:
		{ NULL, NULL }, //8Eh:
		{ NULL, NULL }, //8Fh:
		//0x90:
		{ NULL, NULL }, //90h:
		{ NULL, NULL }, //91h:
		{ NULL, NULL }, //92h:
		{ NULL, NULL }, //93h:
		{ NULL, NULL }, //94h:
		{ NULL, NULL }, //95h:
		{ NULL, NULL }, //96h:
		{ NULL, NULL }, //97h:
		{ NULL, NULL }, //98h:
		{ NULL, NULL }, //99h:
		{ NULL, NULL }, //9Ah:
		{ NULL, NULL }, //9Bh:
		{ NULL, NULL }, //9Ch:
		{ NULL, NULL }, //9Dh:
		{ NULL, NULL }, //9Eh:
		{ NULL, NULL }, //9Fh:
		//0xA0:
		{ NULL, NULL }, //A0h:
		{ NULL, NULL }, //A1h:
		{ NULL, NULL }, //A2h:
		{ NULL, NULL }, //A3h:
		{ NULL, NULL }, //A4h:
		{ NULL, NULL }, //A5h:
		{ NULL, NULL }, //A6h:
		{ NULL, NULL }, //A7h:
		{ NULL, NULL }, //A8h:
		{ NULL, NULL }, //A9h:
		{ NULL, NULL }, //AAh:
		{ NULL, NULL }, //ABh:
		{ NULL, NULL }, //ACh:
		{ NULL, NULL }, //ADh:
		{ NULL, NULL }, //AEh:
		{ NULL, NULL }, //AFh:
		//0xB0:
		{ NULL, NULL }, //B0h:
		{ NULL, NULL }, //B1h:
		{ NULL, NULL }, //B2h:
		{ NULL, NULL }, //B3h:
		{ NULL, NULL }, //B4h:
		{ NULL, NULL }, //B5h:
		{ NULL, NULL }, //B6h:
		{ NULL, NULL }, //B7h:
		{ NULL, NULL }, //B8h:
		{ NULL, NULL }, //B9h:
		{ NULL, NULL }, //BAh:
		{ NULL, NULL }, //BBh:
		{ NULL, NULL }, //BCh:
		{ NULL, NULL }, //BDh:
		{ NULL, NULL }, //BEh:
		{ NULL, NULL }, //BFh:
		//0xC0:
		{ NULL, NULL }, //C0h:
		{ NULL, NULL }, //C1h:
		{ NULL, NULL }, //C2h:
		{ NULL, NULL }, //C3h:
		{ NULL, NULL }, //C4h:
		{ NULL, NULL }, //C5h:
		{ NULL, NULL }, //C6h:
		{ NULL, NULL }, //C7h:
		{ NULL, NULL }, //C8h:
		{ NULL, NULL }, //C9h:
		{ NULL, NULL }, //CAh:
		{ NULL, NULL }, //CBh:
		{ NULL, NULL }, //CCh:
		{ NULL, NULL }, //CDh:
		{ NULL, NULL }, //CEh:
		{ NULL, NULL }, //CFh:
		//0xD0:
		{ NULL, NULL }, //D0h:
		{ NULL, NULL }, //D1h:
		{ NULL, NULL }, //D2h:
		{ NULL, NULL }, //D3h:
		{ NULL, NULL }, //D4h:
		{ NULL, NULL }, //D5h:
		{ NULL, NULL }, //D6h: UNK
		{ NULL, NULL }, //D7h:
		{ NULL, NULL }, //D8h: UNK
		{ NULL, NULL }, //D9h: CoProcessor Minimum
		{ NULL, NULL }, //DAh: UNK
		{ NULL, NULL }, //DBh: CoProcessor Minimum
		{ NULL, NULL }, //DCh: UNK
		{ NULL, NULL }, //DDh: CoProcessor Minimum
		{ NULL, NULL }, //DEh: UNK
		{ NULL, NULL }, //DFh: COProcessor minimum
		//0xE0:
		{ NULL, NULL }, //E0h:
		{ NULL, NULL }, //E1h:
		{ NULL, NULL }, //E2h:
		{ NULL, NULL }, //E3h:
		{ NULL, NULL }, //E4h:
		{ NULL, NULL }, //E5h:
		{ NULL, NULL }, //E6h:
		{ NULL, NULL }, //E7h:
		{ NULL, NULL }, //E8h:
		{ NULL, NULL }, //E9h:
		{ NULL, NULL }, //EAh:
		{ NULL, NULL }, //EBh:
		{ NULL, NULL }, //ECh:
		{ NULL, NULL }, //EDh:
		{ NULL, NULL }, //EEh:
		{ NULL, NULL }, //EFh:
		//0xF0:
		{ NULL, NULL }, //F0h: Special
		{ NULL, NULL }, //F1h: UNK
		{ NULL, NULL }, //F2h: Special
		{ NULL, NULL }, //F3h: Special
		{ NULL, NULL }, //F4h:
		{ NULL, NULL }, //F5h:
		{ NULL, NULL }, //F6h:
		{ NULL, NULL }, //F7h:
		{ NULL, NULL }, //F8h:
		{ NULL, NULL }, //F9h:
		{ NULL, NULL }, //FAh:
		{ NULL, NULL }, //FBh:
		{ NULL, NULL }, //FCh:
		{ NULL, NULL }, //FDh:
		{ NULL, NULL }, //FEh:
		{ NULL, NULL }  //FFh:
	},

	//80486
	{
		//0x00:
		{ NULL, NULL }, //00h:
		{ NULL, NULL }, //01h:
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
		{ NULL, NULL }, //0Fh: 80186 specific OPcode!
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
		{ NULL, NULL }, //20h:
		{ NULL, NULL }, //21h:
		{ NULL, NULL }, //22h:
		{ NULL, NULL }, //23h:
		{ NULL, NULL }, //24h:
		{ NULL, NULL }, //25h:
		{ NULL, NULL }, //26h: Special
		{ NULL, NULL }, //27h:
		{ NULL, NULL }, //28h:
		{ NULL, NULL }, //29h:
		{ NULL, NULL }, //2Ah:
		{ NULL, NULL }, //2Bh:
		{ NULL, NULL }, //2Ch:
		{ NULL, NULL }, //2Dh:
		{ NULL, NULL }, //2Eh: Special
		{ NULL, NULL }, //2Fh:
		//0x30:
		{ NULL, NULL }, //30h:
		{ NULL, NULL }, //31h:
		{ NULL, NULL }, //32h:
		{ NULL, NULL }, //33h:
		{ NULL, NULL }, //34h:
		{ NULL, NULL }, //35h:
		{ NULL, NULL }, //36h: Special
		{ NULL, NULL }, //37h:
		{ NULL, NULL }, //38h:
		{ NULL, NULL }, //39h:
		{ NULL, NULL }, //3Ah:
		{ NULL, NULL }, //3Bh:
		{ NULL, NULL }, //3Ch:
		{ NULL, NULL }, //3Dh:
		{ NULL, NULL }, //3Eh: Special
		{ NULL, NULL }, //3Fh:
		//0x40:
		{ NULL, NULL }, //40h:
		{ NULL, NULL }, //41h:
		{ NULL, NULL }, //42h:
		{ NULL, NULL }, //43h:
		{ NULL, NULL }, //44h:
		{ NULL, NULL }, //45h:
		{ NULL, NULL }, //46h:
		{ NULL, NULL }, //47h:
		{ NULL, NULL }, //48h:
		{ NULL, NULL }, //49h:
		{ NULL, NULL }, //4Ah:
		{ NULL, NULL }, //4Bh:
		{ NULL, NULL }, //4Ch:
		{ NULL, NULL }, //4Dh:
		{ NULL, NULL }, //4Eh:
		{ NULL, NULL }, //4Fh:
		//0x50:
		{ NULL, NULL }, //50h:
		{ NULL, NULL }, //51h:
		{ NULL, NULL }, //52h:
		{ NULL, NULL }, //53h:
		{ NULL, NULL }, //54h:
		{ NULL, NULL }, //55h:
		{ NULL, NULL }, //56h:
		{ NULL, NULL }, //57h:
		{ NULL, NULL }, //58h:
		{ NULL, NULL }, //59h:
		{ NULL, NULL }, //5Ah:
		{ NULL, NULL }, //5Bh:
		{ NULL, NULL }, //5Ch:
		{ NULL, NULL }, //5Dh:
		{ NULL, NULL }, //5Eh:
		{ NULL, NULL }, //5Fh:
		//0x60:
		{ NULL, NULL }, //60h: PUSHA(removed here)
		{ NULL, NULL }, //61h: POPA(removed here)
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //68h:
		{ NULL, NULL }, //69h:
		{ NULL, NULL }, //6Ah:
		{ NULL, NULL }, //6Bh:
		{ NULL, NULL }, //6Ch:
		{ NULL, NULL }, //6Dh:
		{ NULL, NULL }, //6Eh:
		{ NULL, NULL }, //6Fh:
		//0x70: Conditional JMP OPcodes:
		{ NULL, NULL }, //70h:
		{ NULL, NULL }, //71h:
		{ NULL, NULL }, //72h:
		{ NULL, NULL }, //73h:
		{ NULL, NULL }, //74h:
		{ NULL, NULL }, //75h:
		{ NULL, NULL }, //76h:
		{ NULL, NULL }, //77h:
		{ NULL, NULL }, //78h:
		{ NULL, NULL }, //79h:
		{ NULL, NULL }, //7Ah:
		{ NULL, NULL }, //7Bh:
		{ NULL, NULL }, //7Ch:
		{ NULL, NULL }, //7Dh:
		{ NULL, NULL }, //7Eh:
		{ NULL, NULL }, //7Fh:
		//0x80:
		{ NULL, NULL }, //80h:
		{ NULL, NULL }, //81h:
		{ NULL, NULL }, //82h:
		{ NULL, NULL }, //83h:
		{ NULL, NULL }, //84h:
		{ NULL, NULL }, //85h:
		{ NULL, NULL }, //86h:
		{ NULL, NULL }, //87h:
		{ NULL, NULL }, //88h:
		{ NULL, NULL }, //89h:
		{ NULL, NULL }, //8Ah:
		{ NULL, NULL }, //8Bh:
		{ NULL, NULL }, //8Ch:
		{ NULL, NULL }, //8Dh:
		{ NULL, NULL }, //8Eh:
		{ NULL, NULL }, //8Fh:
		//0x90:
		{ NULL, NULL }, //90h:
		{ NULL, NULL }, //91h:
		{ NULL, NULL }, //92h:
		{ NULL, NULL }, //93h:
		{ NULL, NULL }, //94h:
		{ NULL, NULL }, //95h:
		{ NULL, NULL }, //96h:
		{ NULL, NULL }, //97h:
		{ NULL, NULL }, //98h:
		{ NULL, NULL }, //99h:
		{ NULL, NULL }, //9Ah:
		{ NULL, NULL }, //9Bh:
		{ NULL, NULL }, //9Ch:
		{ NULL, NULL }, //9Dh:
		{ NULL, NULL }, //9Eh:
		{ NULL, NULL }, //9Fh:
		//0xA0:
		{ NULL, NULL }, //A0h:
		{ NULL, NULL }, //A1h:
		{ NULL, NULL }, //A2h:
		{ NULL, NULL }, //A3h:
		{ NULL, NULL }, //A4h:
		{ NULL, NULL }, //A5h:
		{ NULL, NULL }, //A6h:
		{ NULL, NULL }, //A7h:
		{ NULL, NULL }, //A8h:
		{ NULL, NULL }, //A9h:
		{ NULL, NULL }, //AAh:
		{ NULL, NULL }, //ABh:
		{ NULL, NULL }, //ACh:
		{ NULL, NULL }, //ADh:
		{ NULL, NULL }, //AEh:
		{ NULL, NULL }, //AFh:
		//0xB0:
		{ NULL, NULL }, //B0h:
		{ NULL, NULL }, //B1h:
		{ NULL, NULL }, //B2h:
		{ NULL, NULL }, //B3h:
		{ NULL, NULL }, //B4h:
		{ NULL, NULL }, //B5h:
		{ NULL, NULL }, //B6h:
		{ NULL, NULL }, //B7h:
		{ NULL, NULL }, //B8h:
		{ NULL, NULL }, //B9h:
		{ NULL, NULL }, //BAh:
		{ NULL, NULL }, //BBh:
		{ NULL, NULL }, //BCh:
		{ NULL, NULL }, //BDh:
		{ NULL, NULL }, //BEh:
		{ NULL, NULL }, //BFh:
		//0xC0:
		{ NULL, NULL }, //C0h:
		{ NULL, NULL }, //C1h:
		{ NULL, NULL }, //C2h:
		{ NULL, NULL }, //C3h:
		{ NULL, NULL }, //C4h:
		{ NULL, NULL }, //C5h:
		{ NULL, NULL }, //C6h:
		{ NULL, NULL }, //C7h:
		{ NULL, NULL }, //C8h:
		{ NULL, NULL }, //C9h:
		{ NULL, NULL }, //CAh:
		{ NULL, NULL }, //CBh:
		{ NULL, NULL }, //CCh:
		{ NULL, NULL }, //CDh:
		{ NULL, NULL }, //CEh:
		{ NULL, NULL }, //CFh:
		//0xD0:
		{ NULL, NULL }, //D0h:
		{ NULL, NULL }, //D1h:
		{ NULL, NULL }, //D2h:
		{ NULL, NULL }, //D3h:
		{ NULL, NULL }, //D4h:
		{ NULL, NULL }, //D5h:
		{ NULL, NULL }, //D6h: UNK
		{ NULL, NULL }, //D7h:
		{ NULL, NULL }, //D8h: UNK
		{ NULL, NULL }, //D9h: CoProcessor Minimum
		{ NULL, NULL }, //DAh: UNK
		{ NULL, NULL }, //DBh: CoProcessor Minimum
		{ NULL, NULL }, //DCh: UNK
		{ NULL, NULL }, //DDh: CoProcessor Minimum
		{ NULL, NULL }, //DEh: UNK
		{ NULL, NULL }, //DFh: COProcessor minimum
		//0xE0:
		{ NULL, NULL }, //E0h:
		{ NULL, NULL }, //E1h:
		{ NULL, NULL }, //E2h:
		{ NULL, NULL }, //E3h:
		{ NULL, NULL }, //E4h:
		{ NULL, NULL }, //E5h:
		{ NULL, NULL }, //E6h:
		{ NULL, NULL }, //E7h:
		{ NULL, NULL }, //E8h:
		{ NULL, NULL }, //E9h:
		{ NULL, NULL }, //EAh:
		{ NULL, NULL }, //EBh:
		{ NULL, NULL }, //ECh:
		{ NULL, NULL }, //EDh:
		{ NULL, NULL }, //EEh:
		{ NULL, NULL }, //EFh:
		//0xF0:
		{ NULL, NULL }, //F0h: Special
		{ NULL, NULL }, //F1h: UNK
		{ NULL, NULL }, //F2h: Special
		{ NULL, NULL }, //F3h: Special
		{ NULL, NULL }, //F4h:
		{ NULL, NULL }, //F5h:
		{ NULL, NULL }, //F6h:
		{ NULL, NULL }, //F7h:
		{ NULL, NULL }, //F8h:
		{ NULL, NULL }, //F9h:
		{ NULL, NULL }, //FAh:
		{ NULL, NULL }, //FBh:
		{ NULL, NULL }, //FCh:
		{ NULL, NULL }, //FDh:
		{ NULL, NULL }, //FEh:
		{ NULL, NULL }  //FFh:
	},

	//80586(Pentium)
	{
		//0x00:
		{ NULL, NULL }, //00h:
		{ NULL, NULL }, //01h:
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
		{ NULL, NULL }, //0Fh: 80186 specific OPcode!
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
		{ NULL, NULL }, //20h:
		{ NULL, NULL }, //21h:
		{ NULL, NULL }, //22h:
		{ NULL, NULL }, //23h:
		{ NULL, NULL }, //24h:
		{ NULL, NULL }, //25h:
		{ NULL, NULL }, //26h: Special
		{ NULL, NULL }, //27h:
		{ NULL, NULL }, //28h:
		{ NULL, NULL }, //29h:
		{ NULL, NULL }, //2Ah:
		{ NULL, NULL }, //2Bh:
		{ NULL, NULL }, //2Ch:
		{ NULL, NULL }, //2Dh:
		{ NULL, NULL }, //2Eh: Special
		{ NULL, NULL }, //2Fh:
		//0x30:
		{ NULL, NULL }, //30h:
		{ NULL, NULL }, //31h:
		{ NULL, NULL }, //32h:
		{ NULL, NULL }, //33h:
		{ NULL, NULL }, //34h:
		{ NULL, NULL }, //35h:
		{ NULL, NULL }, //36h: Special
		{ NULL, NULL }, //37h:
		{ NULL, NULL }, //38h:
		{ NULL, NULL }, //39h:
		{ NULL, NULL }, //3Ah:
		{ NULL, NULL }, //3Bh:
		{ NULL, NULL }, //3Ch:
		{ NULL, NULL }, //3Dh:
		{ NULL, NULL }, //3Eh: Special
		{ NULL, NULL }, //3Fh:
		//0x40:
		{ NULL, NULL }, //40h:
		{ NULL, NULL }, //41h:
		{ NULL, NULL }, //42h:
		{ NULL, NULL }, //43h:
		{ NULL, NULL }, //44h:
		{ NULL, NULL }, //45h:
		{ NULL, NULL }, //46h:
		{ NULL, NULL }, //47h:
		{ NULL, NULL }, //48h:
		{ NULL, NULL }, //49h:
		{ NULL, NULL }, //4Ah:
		{ NULL, NULL }, //4Bh:
		{ NULL, NULL }, //4Ch:
		{ NULL, NULL }, //4Dh:
		{ NULL, NULL }, //4Eh:
		{ NULL, NULL }, //4Fh:
		//0x50:
		{ NULL, NULL }, //50h:
		{ NULL, NULL }, //51h:
		{ NULL, NULL }, //52h:
		{ NULL, NULL }, //53h:
		{ NULL, NULL }, //54h:
		{ NULL, NULL }, //55h:
		{ NULL, NULL }, //56h:
		{ NULL, NULL }, //57h:
		{ NULL, NULL }, //58h:
		{ NULL, NULL }, //59h:
		{ NULL, NULL }, //5Ah:
		{ NULL, NULL }, //5Bh:
		{ NULL, NULL }, //5Ch:
		{ NULL, NULL }, //5Dh:
		{ NULL, NULL }, //5Eh:
		{ NULL, NULL }, //5Fh:
		//0x60:
		{ NULL, NULL }, //60h: PUSHA(removed here)
		{ NULL, NULL }, //61h: POPA(removed here)
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //UNK
		{ NULL, NULL }, //68h:
		{ NULL, NULL }, //69h:
		{ NULL, NULL }, //6Ah:
		{ NULL, NULL }, //6Bh:
		{ NULL, NULL }, //6Ch:
		{ NULL, NULL }, //6Dh:
		{ NULL, NULL }, //6Eh:
		{ NULL, NULL }, //6Fh:
		//0x70: Conditional JMP OPcodes:
		{ NULL, NULL }, //70h:
		{ NULL, NULL }, //71h:
		{ NULL, NULL }, //72h:
		{ NULL, NULL }, //73h:
		{ NULL, NULL }, //74h:
		{ NULL, NULL }, //75h:
		{ NULL, NULL }, //76h:
		{ NULL, NULL }, //77h:
		{ NULL, NULL }, //78h:
		{ NULL, NULL }, //79h:
		{ NULL, NULL }, //7Ah:
		{ NULL, NULL }, //7Bh:
		{ NULL, NULL }, //7Ch:
		{ NULL, NULL }, //7Dh:
		{ NULL, NULL }, //7Eh:
		{ NULL, NULL }, //7Fh:
		//0x80:
		{ NULL, NULL }, //80h:
		{ NULL, NULL }, //81h:
		{ NULL, NULL }, //82h:
		{ NULL, NULL }, //83h:
		{ NULL, NULL }, //84h:
		{ NULL, NULL }, //85h:
		{ NULL, NULL }, //86h:
		{ NULL, NULL }, //87h:
		{ NULL, NULL }, //88h:
		{ NULL, NULL }, //89h:
		{ NULL, NULL }, //8Ah:
		{ NULL, NULL }, //8Bh:
		{ NULL, NULL }, //8Ch:
		{ NULL, NULL }, //8Dh:
		{ NULL, NULL }, //8Eh:
		{ NULL, NULL }, //8Fh:
		//0x90:
		{ NULL, NULL }, //90h:
		{ NULL, NULL }, //91h:
		{ NULL, NULL }, //92h:
		{ NULL, NULL }, //93h:
		{ NULL, NULL }, //94h:
		{ NULL, NULL }, //95h:
		{ NULL, NULL }, //96h:
		{ NULL, NULL }, //97h:
		{ NULL, NULL }, //98h:
		{ NULL, NULL }, //99h:
		{ NULL, NULL }, //9Ah:
		{ NULL, NULL }, //9Bh:
		{ NULL, NULL }, //9Ch:
		{ NULL, NULL }, //9Dh:
		{ NULL, NULL }, //9Eh:
		{ NULL, NULL }, //9Fh:
		//0xA0:
		{ NULL, NULL }, //A0h:
		{ NULL, NULL }, //A1h:
		{ NULL, NULL }, //A2h:
		{ NULL, NULL }, //A3h:
		{ NULL, NULL }, //A4h:
		{ NULL, NULL }, //A5h:
		{ NULL, NULL }, //A6h:
		{ NULL, NULL }, //A7h:
		{ NULL, NULL }, //A8h:
		{ NULL, NULL }, //A9h:
		{ NULL, NULL }, //AAh:
		{ NULL, NULL }, //ABh:
		{ NULL, NULL }, //ACh:
		{ NULL, NULL }, //ADh:
		{ NULL, NULL }, //AEh:
		{ NULL, NULL }, //AFh:
		//0xB0:
		{ NULL, NULL }, //B0h:
		{ NULL, NULL }, //B1h:
		{ NULL, NULL }, //B2h:
		{ NULL, NULL }, //B3h:
		{ NULL, NULL }, //B4h:
		{ NULL, NULL }, //B5h:
		{ NULL, NULL }, //B6h:
		{ NULL, NULL }, //B7h:
		{ NULL, NULL }, //B8h:
		{ NULL, NULL }, //B9h:
		{ NULL, NULL }, //BAh:
		{ NULL, NULL }, //BBh:
		{ NULL, NULL }, //BCh:
		{ NULL, NULL }, //BDh:
		{ NULL, NULL }, //BEh:
		{ NULL, NULL }, //BFh:
		//0xC0:
		{ NULL, NULL }, //C0h:
		{ NULL, NULL }, //C1h:
		{ NULL, NULL }, //C2h:
		{ NULL, NULL }, //C3h:
		{ NULL, NULL }, //C4h:
		{ NULL, NULL }, //C5h:
		{ NULL, NULL }, //C6h:
		{ NULL, NULL }, //C7h:
		{ NULL, NULL }, //C8h:
		{ NULL, NULL }, //C9h:
		{ NULL, NULL }, //CAh:
		{ NULL, NULL }, //CBh:
		{ NULL, NULL }, //CCh:
		{ NULL, NULL }, //CDh:
		{ NULL, NULL }, //CEh:
		{ NULL, NULL }, //CFh:
		//0xD0:
		{ NULL, NULL }, //D0h:
		{ NULL, NULL }, //D1h:
		{ NULL, NULL }, //D2h:
		{ NULL, NULL }, //D3h:
		{ NULL, NULL }, //D4h:
		{ NULL, NULL }, //D5h:
		{ NULL, NULL }, //D6h: UNK
		{ NULL, NULL }, //D7h:
		{ NULL, NULL }, //D8h: UNK
		{ NULL, NULL }, //D9h: CoProcessor Minimum
		{ NULL, NULL }, //DAh: UNK
		{ NULL, NULL }, //DBh: CoProcessor Minimum
		{ NULL, NULL }, //DCh: UNK
		{ NULL, NULL }, //DDh: CoProcessor Minimum
		{ NULL, NULL }, //DEh: UNK
		{ NULL, NULL }, //DFh: COProcessor minimum
		//0xE0:
		{ NULL, NULL }, //E0h:
		{ NULL, NULL }, //E1h:
		{ NULL, NULL }, //E2h:
		{ NULL, NULL }, //E3h:
		{ NULL, NULL }, //E4h:
		{ NULL, NULL }, //E5h:
		{ NULL, NULL }, //E6h:
		{ NULL, NULL }, //E7h:
		{ NULL, NULL }, //E8h:
		{ NULL, NULL }, //E9h:
		{ NULL, NULL }, //EAh:
		{ NULL, NULL }, //EBh:
		{ NULL, NULL }, //ECh:
		{ NULL, NULL }, //EDh:
		{ NULL, NULL }, //EEh:
		{ NULL, NULL }, //EFh:
		//0xF0:
		{ NULL, NULL }, //F0h: Special
		{ NULL, NULL }, //F1h: UNK
		{ NULL, NULL }, //F2h: Special
		{ NULL, NULL }, //F3h: Special
		{ NULL, NULL }, //F4h:
		{ NULL, NULL }, //F5h:
		{ NULL, NULL }, //F6h:
		{ NULL, NULL }, //F7h:
		{ NULL, NULL }, //F8h:
		{ NULL, NULL }, //F9h:
		{ NULL, NULL }, //FAh:
		{ NULL, NULL }, //FBh:
		{ NULL, NULL }, //FCh:
		{ NULL, NULL }, //FDh:
		{ NULL, NULL }, //FEh:
		{ NULL, NULL }  //FFh:
	}
};

Handler CurrentCPU_opcode_jmptbl[512]; //Our standard internal standard opcode jmptbl!

void unhandled_CPUjmptblitem()
{
	raiseError("CPU", "Unhandled instruction JMPTBL, ROP: %02X, Operand size: %i!", CPU[activeCPU].lastopcode, CPU_Operand_size[activeCPU]); //Log the opcode we're executing!
}

void generate_opcode_jmptbl()
{
	byte cpu; //What CPU are we processing!
	byte currentoperandsize = 0;
	word OP; //The opcode to process!
	for (currentoperandsize = 0; currentoperandsize < 2; currentoperandsize++) //Process all operand sizes!
	{
		byte operandsize = currentoperandsize; //Operand size to use!
		for (OP = 0; OP < 0x100; OP++) //Process all opcodes!
		{
			cpu = (byte)EMULATED_CPU; //Start with the emulated CPU and work up to the predesessors!
			while (!opcode_jmptbl[cpu][OP][operandsize]) //No opcode to handle at current CPU&operand size?
			{
				if (operandsize) //We have an operand size: switch to standard if possible!
				{
					operandsize = 0; //Not anymore!
					continue; //Try again!
				}
				else //No operand size: we're a standard, so go up one cpu and retry!
				{
					operandsize = currentoperandsize; //Reset operand size!
					if (cpu) //We've got CPUs left?
					{
						--cpu; //Go up one CPU!
						operandsize = currentoperandsize; //Reset operand size to search!
					}
					else //No CPUs left!
					{
						break; //Stop searching!
					}
				}
			}
			if (opcode_jmptbl[cpu][OP][operandsize])
			{
				CurrentCPU_opcode_jmptbl[(OP << 1) | currentoperandsize] = opcode_jmptbl[cpu][OP][operandsize]; //Execute this instruction when we're triggered!
			}
			else
			{
				CurrentCPU_opcode_jmptbl[(OP << 1) | currentoperandsize] = &unhandled_CPUjmptblitem; //Execute this instruction when we're triggered!
			}
		}
	}
}
