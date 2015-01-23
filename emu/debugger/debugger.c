#include "headers/types.h"
#include "headers/cpu/cpu.h" //We're debugging the CPU?
#include "headers/mmu/mmu.h" //MMU support for opcode!
#include "headers/emu/input.h" //Input support!
#include "headers/cpu/interrupts.h" //Interrupt support!
#include "headers/debugger/debugger.h" //Constant support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/emu/gpu/gpu.h" //GPU support!
#include "headers/support/log.h" //Log support!
#include "headers/interrupts/interrupt10.h" //Interrupt10h support!
#include "headers/emu/gpu/gpu_renderer.h" //GPU renderer support!
#include "headers/emu/gpu/gpu_text.h" //Text support!
#include "headers/emu/emucore.h" //for pause/resumeEMU support!

//Log flags only?
#define LOGFLAGSONLY

char debugger_prefix[256] = ""; //The prefix!
char debugger_command_text[256] = ""; //Current command!
byte debugger_set = 0; //Debugger set?

extern byte dosoftreset; //To soft-reset?
extern BIOS_Settings_TYPE BIOS_Settings; //The BIOS for CPU info!

byte singlestep; //Enforce single step by CPU/hardware special debugging effects?

CPU_registers debuggerregisters; //Backup of the CPU's register states before the CPU starts changing them!

byte debugger_active()
{
	return (debugging() || DEBUGGER_LOG || singlestep);
}

void debugger_beforeCPU() //Action before the CPU changes it's registers!
{
	memcpy(&debuggerregisters,CPU.registers,sizeof(debuggerregisters)); //Copy the registers to our buffer for logging and debugging etc.
	//Initialise debugger texts!
	bzero(debugger_prefix,sizeof(debugger_prefix));
	bzero(debugger_command_text,sizeof(debugger_command_text)); //Init vars!
	strcpy(debugger_prefix,"");
	strcpy(debugger_command_text,"<DEBUGGER UNKOP NOT IMPLEMENTED>"); //Standard: unknown opcode!
	debugger_set = 0; //Default: the debugger isn't implemented!
}

char flags[256]; //Flags as a text!
char *debugger_generateFlags(CPU_registers *registers)
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

void debugger_logregisters(CPU_registers *registers)
{
	if (!registers) //Invalid?
	{
		dolog("debugger","Log registers called with invalid argument!");
		return; //Abort!
	}
	if (EMULATED_CPU<CPU_80286) //Emulating 80(1)86?
	{
		#ifndef LOGFLAGSONLY
		dolog("debugger","Registers:"); //Start of the registers!
		dolog("debugger","AX: %04X, BX: %04X, CX: %04X, DX: %04X",registers->AX,registers->BX,registers->CX,registers->DX); //Basic registers!
		dolog("debugger","CS: %04X, DS: %04X, ES: %04X, SS: %04X",registers->CS,registers->DS,registers->ES,registers->SS); //Segment registers!
		dolog("debugger","SP: %04X, BP: %04X, SI: %04X, DI: %04X",registers->SP,registers->BP,registers->SI,registers->DI); //Segment registers!
		dolog("debugger","IP: %04X, FLAGS: %04X",registers->IP,registers->FLAGS); //Rest!
		#endif
		dolog("debugger","FLAGSINFO:%s",debugger_generateFlags(registers)); //Log the flags!
//More aren't implemented in the 8086!
	}
	else //80286+?
	{
		dolog("debugger","Registers:"); //Start of the registers!
		#ifndef LOGFLAGSONLY
		dolog("debugger","EAX: %08x, EBX: %08x, ECX: %08x, EDX: %08x",registers->EAX,registers->EBX,registers->ECX,registers->EDX); //Basic registers!
		
		if (EMULATED_CPU<CPU_80386) //286-?
		{
			dolog("debugger","CS: %04X, DS: %04X, ES: %04X, SS: %04X",registers->CS,registers->DS,registers->ES,registers->SS); //Segment registers!
		}
		else //386+?
		{
			dolog("debugger","CS: %04X, DS: %04X, ES: %04X, FS: %04X, GS: %04X SS: %04X",registers->CS,registers->DS,registers->ES,registers->FS,registers->GS,registers->SS); //Segment registers!
		}
		dolog("debugger","ESP: %08x, EBP: %08x, ESI: %08x, EDI: %08x",registers->ESP,registers->EBP,registers->ESI,registers->EDI); //Segment registers!
		dolog("debugger","EIP: %08x, EFLAGS: %08x",registers->EIP,registers->EFLAGS); //Rest!
		#endif
		//Finally, flags seperated!
		dolog("debugger","FLAGSINFO:%s",debugger_generateFlags(registers)); //Log the flags!
	}
}

extern word modrm_lastsegment;
extern uint_32 modrm_lastoffset;
extern byte last_modrm; //Is the last opcode a modr/m read?

void debugger_autolog()
{
	if (DEBUGGER_LOG && debugger_active()) //To log?
	{
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
		char fullcmd[256];
		bzero(fullcmd,sizeof(fullcmd)); //Init!
		if (!debugger_set) //No debugger set?
		{
			sprintf(debugger_command_text,"<Debugger not implemented: %02X>",CPU.lastopcode); //Set to the last opcode!
		}
		sprintf(fullcmd,"(%02X)%s%s",CPU.lastopcode,debugger_prefix,debugger_command_text); //Get our full command!

		if (EMULATED_CPU<CPU_80286) //Emulating 80(1)86? Use IP!
		{
			dolog("debugger","%04X:%04X %s",debuggerregisters.CS,debuggerregisters.IP,fullcmd); //Log command, 16-bit disassembler style!
		}
		else //286+? Use EIP!
		{
			dolog("debugger","%04X:%08X %s",debuggerregisters.CS,debuggerregisters.EIP,fullcmd); //Log command, 32-bit disassembler style!
		}
		debugger_logregisters(&debuggerregisters); //Log the previous (initial) register status!
		dolog("debugger",""); //Empty line between comands!
	} //Allow logging?
}

extern GPU_TEXTSURFACE *frameratesurface; //The framerate surface!

void debugger_screen() //Show debugger info on-screen!
{
	uint_32 fontcolor = RGB(0xFF,0xFF,0xFF); //Font color!
	uint_32 backcolor = RGB(0x00,0x00,0x00); //Back color!
	char str[256];
	bzero(str,sizeof(str)); //For clearing!
	int i;
	GPU_textgotoxy(frameratesurface,safe_strlen(debugger_prefix,sizeof(debugger_prefix))+safe_strlen(debugger_command_text,sizeof(debugger_command_text)),GPU_TEXT_DEBUGGERROW); //Goto start of clearing!
	for (i=(safe_strlen(debugger_prefix,sizeof(debugger_prefix))+safe_strlen(debugger_command_text,sizeof(debugger_command_text))); i<GPU_TEXTSURFACE_WIDTH-6; i++) //Clear unneeded!
	{
		GPU_textprintf(frameratesurface,0,0," "); //Clear all unneeded!
	}

	GPU_textgotoxy(frameratesurface,0,GPU_TEXT_DEBUGGERROW);
	GPU_textprintf(frameratesurface,fontcolor,backcolor,"Command: %s%s",debugger_prefix,debugger_command_text); //Show our command!
	word debugrow = GPU_TEXT_DEBUGGERROW; //The debug row we're writing to!	
	GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-5,debugrow++); //First debug row!
	GPU_textprintf(frameratesurface,fontcolor,backcolor,"OP:%02X",MMU_rb(-1,debuggerregisters.CS,debuggerregisters.IP,1)); //Debug opcode!
	GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,debugrow++); //Second debug row!
	GPU_textprintf(frameratesurface,fontcolor,backcolor,"ROP:%02X",CPU.lastopcode); //Real OPCode!

	GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,debugrow++); //Second debug row!
//First: location!
	GPU_textprintf(frameratesurface,fontcolor,backcolor,"CS:%04X",debuggerregisters.CS); //Debug CS!
	if (EMULATED_CPU<=CPU_80186) //186-?
	{
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"IP:%04X",debuggerregisters.IP); //Debug IP!
	}
	else //286+?
	{
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-11,debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"EIP:%08X",debuggerregisters.EIP); //Debug IP!
	}

//Now: Rest segments!
	GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,debugrow++); //Second debug row!
	GPU_textprintf(frameratesurface,fontcolor,backcolor,"DS:%04X",debuggerregisters.DS); //Debug DS!
	GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,debugrow++); //Second debug row!
	GPU_textprintf(frameratesurface,fontcolor,backcolor,"ES:%04X",debuggerregisters.ES); //Debug ES!
	GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,debugrow++); //Second debug row!
	GPU_textprintf(frameratesurface,fontcolor,backcolor,"SS:%04X",debuggerregisters.SS); //Debug SS!


//General purpose registers!
	if (EMULATED_CPU<=CPU_80186) //186-?
	{
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"AX:%04X",debuggerregisters.AX); //Debug AX!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"BX:%04X",debuggerregisters.BX); //Debug BX!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"CX:%04X",debuggerregisters.CX); //Debug CX!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"DX:%04X",debuggerregisters.DX); //Debug DX!
	
	//Pointers and indexes!
	
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,GPU_TEXT_DEBUGGERROW+11); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"SP:%04X",debuggerregisters.SP); //Debug DX!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,GPU_TEXT_DEBUGGERROW+12); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"BP:%04X",debuggerregisters.BP); //Debug DX!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,GPU_TEXT_DEBUGGERROW+13); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"SI:%04X",debuggerregisters.SI); //Debug DX!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,GPU_TEXT_DEBUGGERROW+14); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"DI:%04X",debuggerregisters.DI); //Debug DX!
	}
	else //286+?
	{
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-11,debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"EAX:%08X",debuggerregisters.EAX); //Debug EAX!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-11,debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"EBX:%08X",debuggerregisters.EBX); //Debug EBX!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-11,debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"ECX:%08X",debuggerregisters.ECX); //Debug ECX!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-11,debugrow++); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"EDX:%08X",debuggerregisters.EDX); //Debug EDX!
	
	//Pointers and indexes!
	
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-11,GPU_TEXT_DEBUGGERROW+11); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"ESP:%08X",debuggerregisters.ESP); //Debug ESP!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-11,GPU_TEXT_DEBUGGERROW+12); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"EBP:%08X",debuggerregisters.EBP); //Debug EBP!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-11,GPU_TEXT_DEBUGGERROW+13); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"ESI:%08X",debuggerregisters.ESI); //Debug ESI!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-11,GPU_TEXT_DEBUGGERROW+14); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"EDI:%08X",debuggerregisters.EDI); //Debug EDI!
	}

//Finally, the flags!
	//First, flags fully...
	if (EMULATED_CPU<=CPU_80186) //186-?
	{
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-6,GPU_TEXT_DEBUGGERROW+15); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"F :%04X",debuggerregisters.FLAGS); //Debug FLAGS!
	}
	else //286+
	{
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-10,GPU_TEXT_DEBUGGERROW+15); //Second debug row!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"F :%08X",debuggerregisters.EFLAGS); //Debug FLAGS!
	}

	//Finally, flags seperated!
	char *flags = debugger_generateFlags(&debuggerregisters); //Generate the flags as text!
	GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-strlen(flags),GPU_TEXT_DEBUGGERROW+16); //Second flags row!
	GPU_textprintf(frameratesurface,fontcolor,backcolor,"%s",flags); //All flags, seperated!
}

void debugger_step() //Processes the debugging step!
{
recheckdebugger: //For getting from the BIOS!
	if ((debugging() && (psp_keypressed(BUTTON_RTRIGGER) || (DEBUGGER_ALWAYS_STEP>0))) || (singlestep)) //Debugging step or single step enforced?
	{
		pauseEMU(); //Pause it!

		int done = 0;
		done = 0; //Init: not done yet!
		while (!done) //Still not done?
		{
			if (singlestep) //Hardware enforced single step?
			{
				//Just like DEBUGGER_ALWAYS_STEP, but enforced by hardware calls!
			}
			else if (DEBUGGER_KEEP_RUNNING) //Always keep running?
			{
				done = 1; //Keep running!
			}
			else if (DEBUGGER_ALWAYS_STEP) //Always step?
			{
				//We're going though like a normal STEP. Ignore RTRIGGER.
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
				return; //Done!
				break;
			}
			if (psp_keypressed(BUTTON_TRIANGLE)) //Dump screen?
			{
				while (psp_keypressed(BUTTON_TRIANGLE)) //Wait for release!
				{
				}
				int10_dumpscreen(); //Dump it!
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
			if (psp_keypressed(BUTTON_CIRCLE)) //Reset?
			{
				while (psp_keypressed(BUTTON_CIRCLE)) //Wait for release!
				{
				}
				resetCPU(); //Reset the CPU to reboot!
				return; //We're resetting!
				break;
			}
			if (psp_keypressed(BUTTON_SELECT) && !is_gamingmode()) //Goto BIOS?
			{
				runBIOS(); //Run the BIOS!
				goto recheckdebugger; //Recheck the debugger!
			}
		} //While not done
		resumeEMU(); //Resume it!
	} //Step mode?

	debugger_autolog(); //Log when enabled!
}

int debugging() //Debugging?
{
	if (!DEBUGGER_ENABLED)
	{
		return 0; //No debugger enabled!
	}
	if (singlestep && DEBUGGER_ENABLED) //Hardware enforced single step?
	{
		return 1; //Enforced by hardware single step!
	}
	if (DEBUGGER_ALWAYS_DEBUG)
	{
		return 1; //Always debug!
	}
	return psp_keypressed(BUTTON_LTRIGGER); //Debugging according to LTRIGGER!!!
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