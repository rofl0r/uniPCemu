#include "headers/types.h"
#include "headers/cpu/cpu.h" //We're debugging the CPU?
#include "headers/mmu/mmu.h" //MMU support for opcode!
#include "headers/emu/input.h" //Input support!
#include "headers/cpu/interrupts.h" //Interrupt support!
#include "headers/emu/debugger/debugger.h" //Constant support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/emu/gpu/gpu.h" //GPU support!
#include "headers/support/log.h" //Log support!
#include "headers/interrupts/interrupt10.h" //Interrupt10h support!
#include "headers/emu/gpu/gpu_renderer.h" //GPU renderer support!
#include "headers/emu/gpu/gpu_text.h" //Text support!
#include "headers/emu/emucore.h" //for pause/resumeEMU support!
#include "headers/fopen64.h" //64-bit fopen support!
#include "headers/support/locks.h" //Locking support!
#include "headers/emu/threads.h" //Thread support!

//Log flags only?
//#define LOGFLAGSONLY

byte forcerepeat = 0; //Force repeat log?

byte allow_debuggerstep = 0; //Disabled by default: needs to be enabled by our BIOS!

char debugger_prefix[256] = ""; //The prefix!
char debugger_command_text[256] = ""; //Current command!
byte debugger_set = 0; //Debugger set?
uint_32 debugger_index = 0; //Current debugger index!

extern byte dosoftreset; //To soft-reset?
extern BIOS_Settings_TYPE BIOS_Settings; //The BIOS for CPU info!

byte singlestep; //Enforce single step by CPU/hardware special debugging effects?

CPU_registers debuggerregisters; //Backup of the CPU's register states before the CPU starts changing them!

extern uint_32 MMU_lastwaddr; //What address is last addresses in actual memory?
extern byte MMU_lastwdata;

#include "headers/packed.h" //Packed!
typedef struct PACKED
{
	word CS, SS, DS, ES; //16-bit segment registers!
	word AX, BX, CX, DX; //16-bit GP registers!
	word SI, DI, SP, BP;
	word IP;
	word FLAGS;
	word type; //Special type indication!
} VERIFICATIONDATA;
#include "headers/endpacked.h" //End packed!

OPTINLINE byte readverification(uint_32 index, VERIFICATIONDATA *entry)
{
	const word size = sizeof(*entry);
	FILE *f;
	f = emufopen64("debuggerverify16.dat", "rb"); //Open verify data!
	if (emufseek64(f, index*size, SEEK_SET) == 0) //OK?
	{
		if (emufread64(entry, 1, size, f) == size) //OK?
		{
			emufclose64(f); //Close the file!
			return 1; //Read!
		}
	}
	emufclose64(f); //Close the file!
	return 0; //Error reading the entry!
}

extern byte HWINT_nr, HWINT_saved; //HW interrupt saved?

byte startreached = 0;

OPTINLINE byte debugging() //Debugging?
{
	if (singlestep) //EMU enforced single step?
	{
		return 1; //We're enabled now!
	}
	if (!(DEBUGGER_ENABLED && allow_debuggerstep))
	{
		return 0; //No debugger enabled!
	}
	else if (DEBUGGER_ALWAYS_DEBUG)
	{
		return 1; //Always debug!
	}
	else if ((psp_keypressed(BUTTON_RTRIGGER) || (DEBUGGER_ALWAYS_STEP > 0))) //Forced step?
	{
		return 1; //Always step!
	}
	return psp_keypressed(BUTTON_LTRIGGER); //Debugging according to LTRIGGER!!!
}

byte debugger_logging()
{
	byte enablelog=0; //Default: disabled!
	switch (DEBUGGER_LOG) //What log method?
	{
	case DEBUGGERLOG_ALWAYS:
		enablelog = 1; //Always enabled!
		break;
	case DEBUGGERLOG_DEBUGGING:
		enablelog = debugging(); //Enable log when debugging!
		break;
	default:
		break;
	}
	enablelog |= startreached; //Start logging from this point!
	enablelog &= allow_debuggerstep; //Are we allowed to debug?
	return enablelog; //Logging?
}

byte needdebugger() //Do we need to generate debugging information?
{
	byte result;
	result = debugger_logging(); //Are we logging?
	result |= debugging(); //Are we debugging?
	return result; //Do we need debugger information?
}

void debugger_beforeCPU() //Action before the CPU changes it's registers!
{
	if (needdebugger()) //To apply the debugger generator?
	{
		static VERIFICATIONDATA verify, originalverify;
		memcpy(&debuggerregisters, CPU[activeCPU].registers, sizeof(debuggerregisters)); //Copy the registers to our buffer for logging and debugging etc.
		//Initialise debugger texts!
		bzero(debugger_prefix,sizeof(debugger_prefix));
		bzero(debugger_command_text,sizeof(debugger_command_text)); //Init vars!
		strcpy(debugger_prefix,"");
		strcpy(debugger_command_text,"<DEBUGGER UNKOP NOT IMPLEMENTED>"); //Standard: unknown opcode!
		debugger_set = 0; //Default: the debugger isn't implemented!

		if (file_exists("debuggerverify16.dat")) //Verification file exists?
		{
			if (HWINT_saved) //Saved HW interrupt?
			{
				switch (HWINT_saved)
				{
				case 1: //Trap/SW Interrupt?
					dolog("debugger", "Trapped interrupt: %04X", HWINT_nr);
					break;
				case 2: //PIC Interrupt toggle?
					dolog("debugger", "HW interrupt: %04X", HWINT_nr);
					break;
				default: //Unknown?
					break;
				}
			}
			nextspecial: //Special entry loop!
			if (readverification(debugger_index, &verify)) //Read the current index?
			{
				if (verify.type) //Special type?
				{
					switch (verify.type) //What type?
					{
					case 1: //Trap/SW Interrupt?
						dolog("debugger", "debuggerverify.dat: Trapped Interrupt: %04X", verify.CS); //Trap interrupt!
						break;
					case 2: //PIC Interrupt toggle?
						dolog("debugger", "debuggerverify.dat: HW Interrupt: %04X", verify.CS); //HW interrupt!
						break;
					default: //Unknown?
						break; //Skip unknown special types: we don't handle them!
					}
					++debugger_index; //Skip this entry!
					goto nextspecial; //Check the next entry for special types/normal type!
				}
			}
			originalverify.CS = debuggerregisters.CS;
			originalverify.SS = debuggerregisters.SS;
			originalverify.DS = debuggerregisters.DS;
			originalverify.ES = debuggerregisters.ES;
			originalverify.SI = debuggerregisters.SI;
			originalverify.DI = debuggerregisters.DI;
			originalverify.SP = debuggerregisters.SP;
			originalverify.BP = debuggerregisters.BP;
			originalverify.AX = debuggerregisters.AX;
			originalverify.BX = debuggerregisters.BX;
			originalverify.CX = debuggerregisters.CX;
			originalverify.DX = debuggerregisters.DX;
			originalverify.IP = debuggerregisters.IP;
			originalverify.FLAGS = debuggerregisters.FLAGS;
			if (readverification(debugger_index,&verify)) //Read the verification entry!
			{
				if (EMULATED_CPU < CPU_80286) //Special case for 80(1)86 from fake86!
				{
					originalverify.FLAGS &= ~0xF000; //Clear the high 4 bits: they're not set in the dump!
				}
				if (memcmp(&verify, &originalverify, sizeof(verify)) != 0) //Not equal?
				{
					dolog("debugger", "Invalid data according to debuggerverify.dat before executing the following instruction(Entry number %08X):",debugger_index); //Show where we got our error!
					debugger_logregisters("debugger",&debuggerregisters); //Log the original registers!
					//Apply the debugger registers to the actual register set!
					CPU[activeCPU].registers->CS = verify.CS;
					CPU[activeCPU].registers->SS = verify.SS;
					CPU[activeCPU].registers->DS = verify.DS;
					CPU[activeCPU].registers->ES = verify.ES;
					CPU[activeCPU].registers->SI = verify.SI;
					CPU[activeCPU].registers->DI = verify.DI;
					CPU[activeCPU].registers->SP = verify.SP;
					CPU[activeCPU].registers->BP = verify.BP;
					CPU[activeCPU].registers->AX = verify.AX;
					CPU[activeCPU].registers->BX = verify.BX;
					CPU[activeCPU].registers->CX = verify.CX;
					CPU[activeCPU].registers->DX = verify.DX;
					CPU[activeCPU].registers->IP = verify.IP;
					CPU[activeCPU].registers->FLAGS = verify.FLAGS;
					updateCPUmode(); //Update the CPU mode: flags have been changed!
					dolog("debugger", "Expected:");
					debugger_logregisters("debugger",CPU[activeCPU].registers); //Log the correct registers!
					//Refresh our debugger registers!
					memcpy(&debuggerregisters,CPU[activeCPU].registers, sizeof(debuggerregisters)); //Copy the registers to our buffer for logging and debugging etc.
					forcerepeat = 1; //Force repeat log!
				}
			}
			++debugger_index; //Apply next index!
		}
	} //Are we logging or needing info?
}

char flags[256]; //Flags as a text!
OPTINLINE char *debugger_generateFlags(CPU_registers *registers)
{
	memset(&flags,0,sizeof(flags)); //Clear/init flags!
	sprintf(flags,"%s%c",flags,registers->SFLAGS.CF?'C':'c');
	sprintf(flags,"%s%i",flags,registers->SFLAGS.unmapped2);
	sprintf(flags,"%s%c",flags,registers->SFLAGS.PF?'P':'p');
	sprintf(flags,"%s%i",flags,registers->SFLAGS.unmapped8);
	sprintf(flags,"%s%c",flags,registers->SFLAGS.AF?'A':'a');
	sprintf(flags,"%s%i",flags,registers->SFLAGS.unmapped32);
	sprintf(flags,"%s%c",flags,registers->SFLAGS.ZF?'Z':'z');
	sprintf(flags,"%s%c",flags,registers->SFLAGS.SF?'S':'s');
	sprintf(flags,"%s%c",flags,registers->SFLAGS.TF?'T':'t');
	sprintf(flags,"%s%c",flags,registers->SFLAGS.IF?'I':'i');
	sprintf(flags,"%s%c",flags,registers->SFLAGS.DF?'D':'d');
	sprintf(flags,"%s%c",flags,registers->SFLAGS.OF?'O':'o');
	if (EMULATED_CPU>=CPU_80286) //286+?
	{
		sprintf(flags,"%s%i",flags,registers->SFLAGS.IOPL&1);
		sprintf(flags,"%s%i",flags,((registers->SFLAGS.IOPL&2)>>1));
		sprintf(flags,"%s%c",flags,registers->SFLAGS.NT?'N':'n');
	}
	else //186-? Display as numbers!
	{
		sprintf(flags,"%s%i",flags,registers->SFLAGS.IOPL&1);
		sprintf(flags,"%s%i",flags,((registers->SFLAGS.IOPL&2)>>1));
		sprintf(flags,"%s%i",flags,registers->SFLAGS.NT);
	}
	//Higest 16-bit value!
	sprintf(flags,"%s%i",flags,registers->SFLAGS.unmapped32768);
	
	//Now the high word (80386+)!
	if (EMULATED_CPU>=CPU_80386) //386+?
	{
		sprintf(flags,"%s%c",flags,registers->SFLAGS.RF?'R':'r');
		sprintf(flags,"%s%c",flags,registers->SFLAGS.V8?'V':'v');
		if (EMULATED_CPU>=CPU_80486) //486+?
		{
			sprintf(flags,"%s%c",flags,registers->SFLAGS.AC?'A':'a');
		}
		else //386?
		{
			sprintf(flags,"%s%i",flags,registers->SFLAGS.AC); //Literal bit!
		}
		if (EMULATED_CPU>=CPU_PENTIUM) //Pentium+?
		{
			sprintf(flags,"%s%c",flags,registers->SFLAGS.VIF?'F':'f');
			sprintf(flags,"%s%c",flags,registers->SFLAGS.VIP?'P':'p');
			sprintf(flags,"%s%c",flags,registers->SFLAGS.ID?'I':'i');
		}
		else //386/486?
		{
			sprintf(flags,"%s%i",flags,registers->SFLAGS.VIF);
			sprintf(flags,"%s%i",flags,registers->SFLAGS.VIP);
			sprintf(flags,"%s%i",flags,registers->SFLAGS.ID);
		}
		//Unmapped high bits!
		int i; //For counting the current bit!
		word j; //For holding the current bit!
		j = 1; //Start with value 1!
		for (i=0;i<10;i++) //10-bits value rest!
		{
			if (registers->SFLAGS.unmappedhi&j) //Bit set?
			{
				sprintf(flags,"%s1",flags); //1!
			}
			else //Bit cleared?
			{
				sprintf(flags,"%s0",flags); //0!
			}
			j <<= 1; //Shift to the next bit!
		}
	}
	return &flags[0]; //Give the flags for quick reference!
}

void debugger_logregisters(char *filename, CPU_registers *registers)
{
	if (!registers || !filename) //Invalid?
	{
		dolog(filename,"Log registers called with invalid argument!");
		return; //Abort!
	}
	if (EMULATED_CPU<CPU_80286) //Emulating 80(1)86?
	{
		#ifndef LOGFLAGSONLY
		dolog(filename,"Registers:"); //Start of the registers!
		dolog(filename,"AX: %04X, BX: %04X, CX: %04X, DX: %04X",registers->AX,registers->BX,registers->CX,registers->DX); //Basic registers!
		dolog(filename,"CS: %04X, DS: %04X, ES: %04X, SS: %04X",registers->CS,registers->DS,registers->ES,registers->SS); //Segment registers!
		dolog(filename,"SP: %04X, BP: %04X, SI: %04X, DI: %04X",registers->SP,registers->BP,registers->SI,registers->DI); //Segment registers!
		dolog(filename,"IP: %04X, FLAGS: %04X",registers->IP,registers->FLAGS); //Rest!
		#endif
		dolog(filename,"FLAGSINFO:%s",debugger_generateFlags(registers)); //Log the flags!
//More aren't implemented in the 8086!
	}
	else //80286+?
	{
		dolog(filename,"Registers:"); //Start of the registers!
		#ifndef LOGFLAGSONLY
		dolog(filename,"EAX: %08x, EBX: %08x, ECX: %08x, EDX: %08x",registers->EAX,registers->EBX,registers->ECX,registers->EDX); //Basic registers!
		
		if (EMULATED_CPU<CPU_80386) //286-?
		{
			dolog(filename,"CS: %04X, DS: %04X, ES: %04X, SS: %04X",registers->CS,registers->DS,registers->ES,registers->SS); //Segment registers!
		}
		else //386+?
		{
			dolog(filename,"CS: %04X, DS: %04X, ES: %04X, FS: %04X, GS: %04X SS: %04X",registers->CS,registers->DS,registers->ES,registers->FS,registers->GS,registers->SS); //Segment registers!
		}
		dolog(filename,"ESP: %08x, EBP: %08x, ESI: %08x, EDI: %08x",registers->ESP,registers->EBP,registers->ESI,registers->EDI); //Segment registers!
		dolog(filename,"EIP: %08x, EFLAGS: %08x",registers->EIP,registers->EFLAGS); //Rest!
		#endif
		//Finally, flags seperated!
		dolog(filename,"FLAGSINFO:%s",debugger_generateFlags(registers)); //Log the flags!
	}
}

extern word modrm_lastsegment;
extern uint_32 modrm_lastoffset;
extern byte last_modrm; //Is the last opcode a modr/m read?

extern byte OPbuffer[256];
extern word OPlength; //The length of the OPbuffer!

OPTINLINE void debugger_autolog()
{
	if ((debuggerregisters.EIP == CPU[activeCPU].registers->EIP) && (debuggerregisters.CS == CPU[activeCPU].registers->CS) && (!CPU[activeCPU].faultraised) && (!forcerepeat))
	{
		return; //Are we the same address as the executing command and no fault has been raised? We're a repeat operation!
	}
	forcerepeat = 0; //Don't force repeats anymore if forcing!

	if (debugger_logging()) //To log?
	{
		//Now generate debugger information!
		if (last_modrm)
		{
			if (getcpumode()==CPU_MODE_REAL) //16-bits addresses?
			{
				dolog("debugger","ModR/M address: %04X:%04X=%08X",modrm_lastsegment,modrm_lastoffset,((modrm_lastsegment<<4)+modrm_lastoffset));
			}
			else
			{
				dolog("debugger","ModR/M address: %04X:%08X",modrm_lastsegment,modrm_lastoffset);
			}
		}
		if (MMU_invaddr()) //We've detected an invalid address?
		{
			dolog("debugger", "MMU has detected that the addressed data isn't valid! The memory is not paged, protected or non-existant.");
		}
		if (CPU[activeCPU].faultraised) //Fault has been raised?
		{
			dolog("debugger", "The CPU has raised an exception.");
		}
		char fullcmd[256];
		bzero(fullcmd,sizeof(fullcmd)); //Init!
		int i; //A counter for opcode data dump!
		if (!debugger_set) //No debugger set?
		{
			strcpy(fullcmd,"<Debugger not implemented: "); //Set to the last opcode!
			for (i = 0; i < (int)OPlength; i++) //List the full command!
			{
				sprintf(fullcmd, "%s%02X", debugger_command_text, OPbuffer[i]); //Add part of the opcode!
			}
			strcat(fullcmd, ">"); //End of #UNKOP!
		}
		else
		{
			strcpy(fullcmd, "(");
			for (i = 0; i < (int)OPlength; i++) //List the full command!
			{
				sprintf(fullcmd, "%s%02X", fullcmd, OPbuffer[i]); //Add part of the opcode!
			}
			strcat(fullcmd, ")"); //Our opcode before disassembly!
			strcat(fullcmd, debugger_prefix); //The prefix(es)!
			strcat(fullcmd, debugger_command_text); //Command itself!
		}

		if (getcpumode() == CPU_MODE_REAL) //Emulating 80(1)86? Use IP!
		{
			dolog("debugger","%04X:%04X %s",debuggerregisters.CS,debuggerregisters.IP,fullcmd); //Log command, 16-bit disassembler style!
		}
		else //286+? Use EIP!
		{
			dolog("debugger","%04X:%08X %s",debuggerregisters.CS,debuggerregisters.EIP,fullcmd); //Log command, 32-bit disassembler style!
		}
		debugger_logregisters("debugger",&debuggerregisters); //Log the previous (initial) register status!
		dolog("debugger",""); //Empty line between comands!
	} //Allow logging?
}

extern GPU_TEXTSURFACE *frameratesurface; //The framerate surface!

OPTINLINE void debugger_screen() //Show debugger info on-screen!
{
	if (frameratesurface) //We can show?
	{
		GPU_text_locksurface(frameratesurface); //Lock!
		uint_32 fontcolor = RGB(0xFF, 0xFF, 0xFF); //Font color!
		uint_32 backcolor = RGB(0x00, 0x00, 0x00); //Back color!
		char str[256];
		bzero(str, sizeof(str)); //For clearing!
		int i;
		GPU_textgotoxy(frameratesurface, safe_strlen(debugger_prefix, sizeof(debugger_prefix)) + safe_strlen(debugger_command_text, sizeof(debugger_command_text)), GPU_TEXT_DEBUGGERROW); //Goto start of clearing!
		for (i = (safe_strlen(debugger_prefix, sizeof(debugger_prefix)) + safe_strlen(debugger_command_text, sizeof(debugger_command_text))); i < GPU_TEXTSURFACE_WIDTH - 6; i++) //Clear unneeded!
		{
			GPU_textprintf(frameratesurface, 0, 0, " "); //Clear all unneeded!
		}

		GPU_textgotoxy(frameratesurface, 0, GPU_TEXT_DEBUGGERROW);
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "Command: %s%s", debugger_prefix, debugger_command_text); //Show our command!
		word debugrow = GPU_TEXT_DEBUGGERROW; //The debug row we're writing to!	
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 5, debugrow++); //First debug row!
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "OP:%02X", MMU_rb(-1, debuggerregisters.CS, debuggerregisters.IP, 1)); //Debug opcode!
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 6, debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "ROP:%02X", CPU[activeCPU].lastopcode); //Real OPCode!

		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debugrow++); //Second debug row!
		//First: location!
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "CS:%04X", debuggerregisters.CS); //Debug CS!
		if (getcpumode() == CPU_MODE_REAL) //Real mode?
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "IP:%04X", debuggerregisters.IP); //Debug IP!
		}
		else //286+?
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 12, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "EIP:%08X", debuggerregisters.EIP); //Debug IP!
		}

		//Now: Rest segments!
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "DS:%04X", debuggerregisters.DS); //Debug DS!
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "ES:%04X", debuggerregisters.ES); //Debug ES!
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "SS:%04X", debuggerregisters.SS); //Debug SS!
		if (EMULATED_CPU >= CPU_80286) //286+?
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "FS:%04X", debuggerregisters.FS); //Debug FS!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "GS:%04X", debuggerregisters.GS); //Debug GS!
		}


		//General purpose registers!
		if (getcpumode() == CPU_MODE_REAL) //Real mode?
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "AX:%04X", debuggerregisters.AX); //Debug AX!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "BX:%04X", debuggerregisters.BX); //Debug BX!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "CX:%04X", debuggerregisters.CX); //Debug CX!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "DX:%04X", debuggerregisters.DX); //Debug DX!

			//Pointers and indexes!

			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, GPU_TEXT_DEBUGGERROW + 11); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "SP:%04X", debuggerregisters.SP); //Debug DX!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, GPU_TEXT_DEBUGGERROW + 12); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "BP:%04X", debuggerregisters.BP); //Debug DX!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, GPU_TEXT_DEBUGGERROW + 13); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "SI:%04X", debuggerregisters.SI); //Debug DX!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, GPU_TEXT_DEBUGGERROW + 14); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "DI:%04X", debuggerregisters.DI); //Debug DX!
		}
		else //286+?
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 12, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "EAX:%08X", debuggerregisters.EAX); //Debug EAX!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 12, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "EBX:%08X", debuggerregisters.EBX); //Debug EBX!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 12, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "ECX:%08X", debuggerregisters.ECX); //Debug ECX!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 12, debugrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "EDX:%08X", debuggerregisters.EDX); //Debug EDX!

			//Pointers and indexes!

			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 12, GPU_TEXT_DEBUGGERROW + 11); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "ESP:%08X", debuggerregisters.ESP); //Debug ESP!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 12, GPU_TEXT_DEBUGGERROW + 12); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "EBP:%08X", debuggerregisters.EBP); //Debug EBP!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 12, GPU_TEXT_DEBUGGERROW + 13); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "ESI:%08X", debuggerregisters.ESI); //Debug ESI!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 12, GPU_TEXT_DEBUGGERROW + 14); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "EDI:%08X", debuggerregisters.EDI); //Debug EDI!
		}

		//Finally, the flags!
		//First, flags fully...
		if (getcpumode() == CPU_MODE_REAL) //Real mode?
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, GPU_TEXT_DEBUGGERROW + 15); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "F :%04X", debuggerregisters.FLAGS); //Debug FLAGS!
		}
		else //286+
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 11, GPU_TEXT_DEBUGGERROW + 15); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "F :%08X", debuggerregisters.EFLAGS); //Debug FLAGS!
		}

		//Finally, flags seperated!
		char *flags = debugger_generateFlags(&debuggerregisters); //Generate the flags as text!
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - strlen(flags), GPU_TEXT_DEBUGGERROW + 16); //Second flags row!
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "%s", flags); //All flags, seperated!
		GPU_text_releasesurface(frameratesurface); //Unlock!
	}
}

void debuggerThread()
{
	static byte skipopcodes = 0; //Skip none!
	pauseEMU(); //Pause it!

	restartdebugger: //Restart the debugger during debugging!
	debugger_screen(); //Show debugger info on-screen!
	int done = 0;
	done = 0; //Init: not done yet!
	for (;!(done || skipopcodes);) //Still not done or skipping?
	{
		if (DEBUGGER_ALWAYS_STEP || singlestep) //Always step?
		{
			//We're going though like a normal STEP. Ignore RTRIGGER.
		}
		else if (DEBUGGER_KEEP_RUNNING) //Always keep running?
		{
			done = 1; //Keep running!
		}
		else
		{
			done = (!psp_keypressed(BUTTON_RTRIGGER)); //Continue when release hold (except when forcing stepping), singlestep prevents this!
		}

		if (psp_keypressed(BUTTON_CROSS)) //Step (wait for release and break)?
		{
			while (psp_keypressed(BUTTON_CROSS)) //Wait for release!
			{
			}
			//If single stepping, keep doing so!
			break;
		}
		if (psp_keypressed(BUTTON_TRIANGLE)) //Skip 10 commands?
		{
			while (psp_keypressed(BUTTON_TRIANGLE)) //Wait for release!
			{
			}
			skipopcodes = 9; //Skip 9 additional opcodes!
			break;
		}
		if (psp_keypressed(BUTTON_SQUARE)) //Refresh screen?
		{
			renderHWFrame(); //Refresh it!
			while (psp_keypressed(BUTTON_SQUARE)) //Wait for release to show debugger again!
			{
			}
			debugger_screen(); //Show the debugger again!
		}
		if (psp_keypressed(BUTTON_CIRCLE)) //Dump memory?
		{
			while (psp_keypressed(BUTTON_CIRCLE)) //Wait for release!
			{
			}
			MMU_dumpmemory("memory.dat"); //Dump the MMU memory!
		}
		if (psp_keypressed(BUTTON_SELECT) && !is_gamingmode()) //Goto BIOS?
		{
			runBIOS(0); //Run the BIOS!
			if (debugging()) //Recheck the debugger!
			{
				goto restartdebugger; //Restart the debugger!
			}
		}
		delay(0); //Wait a bit!
	} //While not done
	if (skipopcodes) //Skipping?
	{
		--skipopcodes; //Skipped one opcode!
	}
	resumeEMU(); //Resume it!
}

ThreadParams_p debugger_thread = NULL; //The debugger thread, if any!

void debugger_step() //Processes the debugging step!
{
	if (debugger_thread) //Debugger not running yet?
	{
		if (threadRunning(debugger_thread,"debugger")) //Still running?
		{
			return; //We're still running, so start nothing!
		}
	}
	debugger_thread = NULL; //Not a running thread!
	debugger_autolog(); //Log when enabled!
	if (debugging()) //Debugging step or single step enforced?
	{
		debugger_thread = startThread(debuggerThread,"debugger",NULL); //Start the debugger!
	} //Step mode?
}

void debugger_setcommand(char *text, ...)
{
	va_list args; //Going to contain the list!
	va_start (args, text); //Start list!
	vsprintf (debugger_command_text, text, args); //Compile list!
	va_end (args); //Destroy list!
	debugger_set = 1; //We've set the debugger!
}

void debugger_setprefix(char *text)
{
	bzero(debugger_prefix,sizeof(debugger_prefix)); //Init!
	strcpy(debugger_prefix,text); //Set prefix!
}