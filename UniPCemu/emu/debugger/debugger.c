#include "headers/types.h"
#include "headers/cpu/cpu.h" //We're debugging the CPU?
#include "headers/cpu/mmu.h" //MMU support for opcode!
#include "headers/emu/input.h" //Input support!
#include "headers/cpu/interrupts.h" //Interrupt support!
#include "headers/emu/debugger/debugger.h" //Constant support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/emu/gpu/gpu.h" //GPU support!
#include "headers/support/log.h" //Log support!
#include "headers/emu/gpu/gpu.h" //GPU resolution support!
#include "headers/emu/gpu/gpu_renderer.h" //GPU renderer support!
#include "headers/emu/gpu/gpu_text.h" //Text support!
#include "headers/emu/emucore.h" //for pause/resumeEMU support!
#include "headers/fopen64.h" //64-bit fopen support!
#include "headers/support/locks.h" //Locking support!
#include "headers/emu/threads.h" //Thread support!
#include "headers/hardware/pic.h" //Interrupt support!
#include "headers/hardware/vga/vga_renderer.h" //Renderer support!
#include "headers/bios/biosmenu.h" //Support for running the BIOS from the debugger!
#include "headers/mmu/mmuhandler.h" //MMU_invaddr and MMU_dumpmemory support!
#include "headers/cpu/biu.h" //BIU support!

//Log flags only?
//#define LOGFLAGSONLY

//Debug logging for protected mode?
//#define DEBUG_PROTECTEDMODE

byte debugger_loggingtimestamp = 1; //Are we to log timestamps?

byte log_timestampbackup; //Backup of the original timestamp value!

//Debugger skipping functionality
uint_32 skipopcodes = 0; //Skip none!
byte skipstep = 0; //Skip while stepping? 1=repeating, 2=EIP destination, 3=Stop asap.
uint_32 skipopcodes_destEIP = 0; //Wait for EIP to become this value?

//Repeat log?
byte forcerepeat = 0; //Force repeat log?

byte allow_debuggerstep = 0; //Disabled by default: needs to be enabled by our BIOS!

char debugger_prefix[256] = ""; //The prefix!
char debugger_command_text[256] = ""; //Current command!
byte debugger_set = 0; //Debugger set?
uint_32 debugger_index = 0; //Current debugger index!

byte debugger_logtimings = 1; //Are we to log the full timings of hardware and CPU as well?

extern byte dosoftreset; //To soft-reset?
extern BIOS_Settings_TYPE BIOS_Settings; //The BIOS for CPU info!

byte singlestep; //Enforce single step by CPU/hardware special debugging effects? 0=Don't step, 1=Step this instruction(invalid state when activated during the execution of the instruction), 2+ step next instruction etc.

CPU_registers debuggerregisters; //Backup of the CPU's register states before the CPU starts changing them!
byte debuggerHLT = 0;
byte debuggerReset = 0; //Are we a reset CPU?

extern uint_32 MMU_lastwaddr; //What address is last addresses in actual memory?
extern byte MMU_lastwdata;

extern PIC i8259;

extern GPU_type GPU; //GPU itself!

byte debugger_simplifiedlog = 0; //Are we to produce a simplified log?

byte verifyfile = 0; //Check for verification file?

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
byte harddebugging = 0; //Hard-coded debugger set?

OPTINLINE byte debugging() //Debugging?
{
	if (singlestep==1) //EMU enforced single step?
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

byte debuggerINT = 0; //Interrupt special trigger?

extern word waitingforiret; //Logging until IRET?

byte debugger_logging()
{
	byte enablelog=0; //Default: disabled!
	switch (DEBUGGER_LOG) //What log method?
	{
	case DEBUGGERLOG_ALWAYS:
	case DEBUGGERLOG_ALWAYS_NOREGISTERS: //Same, but no register state logging?
	case DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP: //Always, but also when skipping?
	case DEBUGGERLOG_ALWAYS_SINGLELINE: //Always log, even during skipping, single line format
	case DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED: //Always log, even during skipping, single line format, simplfied
	case DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT: //Always log, common log format
	case DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT: //Always log, even during skip step, common log format
		enablelog = 1; //Always enabled!
		break;
	case DEBUGGERLOG_DEBUGGING:
	case DEBUGGERLOG_DEBUGGING_SINGLELINE:
	case DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED:
	case DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT:
		enablelog = debugging(); //Enable log when debugging!
		break;
	case DEBUGGERLOG_INT: //Interrupts only?
		enablelog = debuggerINT; //Debug this(interrupt)!
		break;
	case DEBUGGERLOG_DIAGNOSTICCODES: //Diagnostic codes only
		enablelog = debugging(); //Enable log when debugging only!
		break; //Don't enable the log by debugging only!
	default:
		break;
	}
	enablelog |= startreached; //Start logging from this point(emulator internal debugger)!
	enablelog |= harddebugging; //Same as startreached, but special operations only!
	enablelog |= waitingforiret; //Waiting for IRET?
	enablelog &= allow_debuggerstep; //Are we allowed to debug?
	if (skipstep && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP)) enablelog = 0; //Disable when skipping?
	return enablelog; //Logging?
}

byte isDebuggingPOSTCodes()
{
	return (DEBUGGER_LOG==DEBUGGERLOG_DIAGNOSTICCODES); //Log Diagnostic codes only?
}

byte needdebugger() //Do we need to generate debugging information?
{
	byte result;
	result = debugger_logging(); //Are we logging?
	result |= (DEBUGGER_LOG == DEBUGGERLOG_INT); //Interrupts are needed, but logging is another story!
	result |= debugging(); //Are we debugging?
	return result; //Do we need debugger information?
}

OPTINLINE char stringsafeDebugger(byte x)
{
	return (x && (x!=0xD) && (x!=0xA))?x:(char)0x20;
}

char debugger_memoryaccess_text[1024]; //Memory access text!
char debugger_memoryaccess_line[256];

void debugger_logmemoryaccess(byte iswrite, uint_32 address, byte value, byte type)
{
	if ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Common log format?
	{
		return; //No memory access logging, we're disabled for now!
	}
	if (iswrite)
	{
		switch (type)
		{
			case LOGMEMORYACCESS_NORMAL:
				if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) //Not using a single line?
				{
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger","Writing to normal memory: %08X=%02X (%c)",address,value,stringsafeDebugger(value));
					log_logtimestamp(log_timestampbackup); //Restore state!
				}
				else
				{
					if (strcmp(debugger_memoryaccess_text,"")==0) //Nothing logged yet?
					{
						sprintf(debugger_memoryaccess_text,"Normal(w):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
					}
					else
					{
						sprintf(debugger_memoryaccess_line,"Normal(w):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
						strcat(debugger_memoryaccess_text,"; ");
						strcat(debugger_memoryaccess_text,debugger_memoryaccess_line); //Add the line!
					}
				}
				break;
			case LOGMEMORYACCESS_PAGED:
				if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) //Not using a single line?
				{
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger","Writing to paged memory: %08X=%02X (%c)",address,value,stringsafeDebugger(value));
					log_logtimestamp(log_timestampbackup); //Restore state!
				}
				else
				{
					if (strcmp(debugger_memoryaccess_text,"")==0) //Nothing logged yet?
					{
						sprintf(debugger_memoryaccess_text,"Paged(w):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
					}
					else
					{
						sprintf(debugger_memoryaccess_line,"Paged(w):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
						strcat(debugger_memoryaccess_text,"; ");
						strcat(debugger_memoryaccess_text,debugger_memoryaccess_line); //Add the line!
					}
				}
				break;
			case LOGMEMORYACCESS_DIRECT:
				if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) //Not using a single line?
				{
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger","Writing to physical memory: %08X=%02X (%c)",address,value,stringsafeDebugger(value));
					log_logtimestamp(log_timestampbackup); //Restore state!
				}
				else
				{
					if (strcmp(debugger_memoryaccess_text,"")==0) //Nothing logged yet?
					{
						sprintf(debugger_memoryaccess_text,"Physical(w):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
					}
					else
					{
						sprintf(debugger_memoryaccess_line,"Physical(w):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
						strcat(debugger_memoryaccess_text,"; ");
						strcat(debugger_memoryaccess_text,debugger_memoryaccess_line); //Add the line!
					}
				}
				break;
			default:
			case LOGMEMORYACCESS_RAM:
				if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) //Not using a single line?
				{
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger","Writing to RAM: %08X=%02X (%c)",address,value,stringsafeDebugger(value));
					log_logtimestamp(log_timestampbackup); //Restore state!
				}
				else
				{
					if (strcmp(debugger_memoryaccess_text,"")==0) //Nothing logged yet?
					{
						sprintf(debugger_memoryaccess_text,"RAM(w):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
					}
					else
					{
						sprintf(debugger_memoryaccess_line,"RAM(w):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
						strcat(debugger_memoryaccess_text,"; ");
						strcat(debugger_memoryaccess_text,debugger_memoryaccess_line); //Add the line!
					}
				}
				break;
			case LOGMEMORYACCESS_RAM_LOGMMUALL:
				if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) //Not using a single line?
				{
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger","MMU: Writing to real %08X=%02X (%c)",address,value,stringsafeDebugger(value));
					log_logtimestamp(log_timestampbackup); //Restore state!
				}
				else
				{
					if (strcmp(debugger_memoryaccess_text,"")==0) //Nothing logged yet?
					{
						sprintf(debugger_memoryaccess_text,"RealRAM(w):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
					}
					else
					{
						sprintf(debugger_memoryaccess_line,"RealRAM(w):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
						strcat(debugger_memoryaccess_text,"; ");
						strcat(debugger_memoryaccess_text,debugger_memoryaccess_line); //Add the line!
					}
				}
				break;
		}
	}
	else
	{
		switch (type)
		{
			case LOGMEMORYACCESS_NORMAL:
				if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) //Not using a single line?
				{
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger","Reading from normal memory: %08X=%02X (%c)",address,value,stringsafeDebugger(value));
					log_logtimestamp(log_timestampbackup); //Restore state!
				}
				else
				{
					if (strcmp(debugger_memoryaccess_text,"")==0) //Nothing logged yet?
					{
						sprintf(debugger_memoryaccess_text,"Normal(r):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
					}
					else
					{
						sprintf(debugger_memoryaccess_line,"Normal(r):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
						strcat(debugger_memoryaccess_text,"; ");
						strcat(debugger_memoryaccess_text,debugger_memoryaccess_line); //Add the line!
					}
				}
				break;
			case LOGMEMORYACCESS_PAGED:
				if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) //Not using a single line?
				{
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger","Reading from paged memory: %08X=%02X (%c)",address,value,stringsafeDebugger(value));
					log_logtimestamp(log_timestampbackup); //Restore state!
				}
				else
				{
					if (strcmp(debugger_memoryaccess_text,"")==0) //Nothing logged yet?
					{
						sprintf(debugger_memoryaccess_text,"Paged(r):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
					}
					else
					{
						sprintf(debugger_memoryaccess_line,"Paged(r):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
						strcat(debugger_memoryaccess_text,"; ");
						strcat(debugger_memoryaccess_text,debugger_memoryaccess_line); //Add the line!
					}
				}
				break;
			case LOGMEMORYACCESS_DIRECT:
				if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) //Not using a single line?
				{
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger","Reading from physical memory: %08X=%02X (%c)",address,value,stringsafeDebugger(value));
					log_logtimestamp(log_timestampbackup); //Restore state!
				}
				else
				{
					if (strcmp(debugger_memoryaccess_text,"")==0) //Nothing logged yet?
					{
						sprintf(debugger_memoryaccess_text,"Physical(r):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
					}
					else
					{
						sprintf(debugger_memoryaccess_line,"Physical(r):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
						strcat(debugger_memoryaccess_text,"; ");
						strcat(debugger_memoryaccess_text,debugger_memoryaccess_line); //Add the line!
					}
				}
				break;
			default:
			case LOGMEMORYACCESS_RAM:
				if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) //Not using a single line?
				{
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger","Reading from RAM: %08X=%02X (%c)",address,value,stringsafeDebugger(value));
					log_logtimestamp(log_timestampbackup); //Restore state!
				}
				else
				{
					if (strcmp(debugger_memoryaccess_text,"")==0) //Nothing logged yet?
					{
						sprintf(debugger_memoryaccess_text,"RAM(r):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
					}
					else
					{
						sprintf(debugger_memoryaccess_line,"RAM(r):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
						strcat(debugger_memoryaccess_text,"; ");
						strcat(debugger_memoryaccess_text,debugger_memoryaccess_line); //Add the line!
					}
				}
				break;
			case LOGMEMORYACCESS_RAM_LOGMMUALL:
				if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) //Not using a single line?
				{
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger","MMU: Reading from real %08X=%02X (%c)",address,value,stringsafeDebugger(value));
					log_logtimestamp(log_timestampbackup); //Restore state!
				}
				else
				{
					if (strcmp(debugger_memoryaccess_text,"")==0) //Nothing logged yet?
					{
						sprintf(debugger_memoryaccess_text,"RealRAM(r):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
					}
					else
					{
						sprintf(debugger_memoryaccess_line,"RealRAM(r):%08X=%02X(%c)",address,value,stringsafeDebugger(value)); //Compact version!
						strcat(debugger_memoryaccess_text,"; ");
						strcat(debugger_memoryaccess_text,debugger_memoryaccess_line); //Add the line!
					}
				}
				break;
		}
	}
}

void debugger_beforeCPU() //Action before the CPU changes it's registers!
{
	if (needdebugger()) //To apply the debugger generator?
	{
		static VERIFICATIONDATA verify, originalverify;
		memcpy(&debuggerregisters, CPU[activeCPU].registers, sizeof(debuggerregisters)); //Copy the registers to our buffer for logging and debugging etc.
		//Initialise debugger texts!
		cleardata(&debugger_prefix[0],sizeof(debugger_prefix));
		cleardata(&debugger_command_text[0],sizeof(debugger_command_text)); //Init vars!
		strcpy(debugger_prefix,""); //Initialise the prefix(es)!
		strcpy(debugger_command_text,"<DEBUGGER UNKOP NOT IMPLEMENTED>"); //Standard: unknown opcode!
		debugger_set = 0; //Default: the debugger isn't implemented!
		debuggerHLT = CPU[activeCPU].halt; //Are we halted?
		debuggerReset = CPU[activeCPU].is_reset|(CPU[activeCPU].permanentreset<<1); //Are we reset?

		if (verifyfile) //Verification file exists?
		{
			if (!file_exists("debuggerverify16.dat")) return; //Abort if it doesn't exist anymore!
			if (HWINT_saved) //Saved HW interrupt?
			{
				switch (HWINT_saved)
				{
				case 1: //Trap/SW Interrupt?
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger", "Trapped interrupt: %04X", HWINT_nr);
					log_logtimestamp(log_timestampbackup); //Restore state!
					break;
				case 2: //PIC Interrupt toggle?
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger", "HW interrupt: %04X", HWINT_nr);
					log_logtimestamp(log_timestampbackup); //Restore state!
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
						log_timestampbackup = log_logtimestamp(2); //Save state!
						log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
						dolog("debugger", "debuggerverify.dat: Trapped Interrupt: %04X", verify.CS); //Trap interrupt!
						log_logtimestamp(log_timestampbackup); //Restore state!
						break;
					case 2: //PIC Interrupt toggle?
						log_timestampbackup = log_logtimestamp(2); //Save state!
						log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
						dolog("debugger", "debuggerverify.dat: HW Interrupt: %04X", verify.CS); //HW interrupt!
						log_logtimestamp(log_timestampbackup); //Restore state!
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
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger", "Invalid data according to debuggerverify.dat before executing the following instruction(Entry number %08X):",debugger_index); //Show where we got our error!
					debugger_logregisters("debugger",&debuggerregisters,debuggerHLT,debuggerReset); //Log the original registers!
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
					log_timestampbackup = log_logtimestamp(2); //Save state!
					log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
					dolog("debugger", "Expected:");
					log_logtimestamp(log_timestampbackup); //Restore state!
					debugger_logregisters("debugger",CPU[activeCPU].registers,debuggerHLT,debuggerReset); //Log the correct registers!
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
static char *debugger_generateFlags(CPU_registers *registers)
{
	strcpy(flags,""); //Clear the flags!
	sprintf(flags,"%s%c",flags,(char)(FLAGREGR_CF(registers)?'C':'c'));
	sprintf(flags,"%s%u",flags,FLAGREGR_UNMAPPED2(registers));
	sprintf(flags,"%s%c",flags,(char)(FLAGREGR_PF(registers)?'P':'p'));
	sprintf(flags,"%s%u",flags,FLAGREGR_UNMAPPED8(registers));
	sprintf(flags,"%s%c",flags,(char)(FLAGREGR_AF(registers)?'A':'a'));
	sprintf(flags,"%s%u",flags,FLAGREGR_UNMAPPED32(registers));
	sprintf(flags,"%s%c",flags,(char)(FLAGREGR_ZF(registers)?'Z':'z'));
	sprintf(flags,"%s%c",flags,(char)(FLAGREGR_SF(registers)?'S':'s'));
	sprintf(flags,"%s%c",flags,(char)(FLAGREGR_TF(registers)?'T':'t'));
	sprintf(flags,"%s%c",flags,(char)(FLAGREGR_IF(registers)?'I':'i'));
	sprintf(flags,"%s%c",flags,(char)(FLAGREGR_DF(registers)?'D':'d'));
	sprintf(flags,"%s%c",flags,(char)(FLAGREGR_OF(registers)?'O':'o'));
	if (EMULATED_CPU>=CPU_80286) //286+?
	{
		sprintf(flags,"%s%u",flags,(word)(FLAGREGR_IOPL(registers)&1));
		sprintf(flags,"%s%u",flags,(word)((FLAGREGR_IOPL(registers)&2)>>1));
		sprintf(flags,"%s%c",flags,(char)(FLAGREGR_NT(registers)?'N':'n'));
	}
	else //186-? Display as numbers!
	{
		sprintf(flags,"%s%u",flags,(word)(FLAGREGR_IOPL(registers)&1));
		sprintf(flags,"%s%u",flags,(word)((FLAGREGR_IOPL(registers)&2)>>1));
		sprintf(flags,"%s%u",flags,FLAGREGR_NT(registers));
	}
	//Higest 16-bit value!
	sprintf(flags,"%s%u",flags,FLAGREGR_UNMAPPED32768(registers));
	
	//Now the high word (80386+)!
	if (EMULATED_CPU>=CPU_80386) //386+?
	{
		sprintf(flags,"%s%c",flags,(char)(FLAGREGR_RF(registers)?'R':'r'));
		sprintf(flags,"%s%c",flags,(char)(FLAGREGR_V8(registers)?'V':'v'));
		if (EMULATED_CPU>=CPU_80486) //486+?
		{
			sprintf(flags,"%s%c",flags,(char)(FLAGREGR_AC(registers)?'A':'a'));
		}
		else //386?
		{
			sprintf(flags,"%s%u",flags,FLAGREGR_AC(registers)); //Literal bit!
		}
		if (EMULATED_CPU>=CPU_PENTIUM) //Pentium+?
		{
			sprintf(flags,"%s%c",flags,(char)(FLAGREGR_VIF(registers)?'F':'f'));
			sprintf(flags,"%s%c",flags,(char)(FLAGREGR_VIP(registers)?'P':'p'));
			sprintf(flags,"%s%c",flags,(char)(FLAGREGR_ID(registers)?'I':'i'));
		}
		else //386/486?
		{
			sprintf(flags,"%s%u",flags,FLAGREGR_VIF(registers));
			sprintf(flags,"%s%u",flags,FLAGREGR_VIP(registers));
			sprintf(flags,"%s%u",flags,FLAGREGR_ID(registers));
		}
		//Unmapped high bits!
		int i; //For counting the current bit!
		word j; //For holding the current bit!
		j = 1; //Start with value 1!
		for (i=0;i<10;i++) //10-bits value rest!
		{
			if (FLAGREGR_UNMAPPEDHI(registers)&j) //Bit set?
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

OPTINLINE char decodeHLTreset(byte halted,byte isreset)
{
	if (halted)
	{
		return 'H'; //We're halted!
	}
	else if (isreset)
	{
		if (isreset&2) //Permanently reset?
		{
			return '*'; //We're permanently reset!
		}
		//Normal reset?
		return 'R'; //We're reset!
	}
	return ' '; //Nothing to report, give empty!
}

void debugger_logregisters(char *filename, CPU_registers *registers, byte halted, byte isreset)
{
	if ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Common log format?
	{
		return; //No register logging, we're disabled for now!
	}
	if ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_NOREGISTERS) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_SINGLELINE) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_SINGLELINE) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) return; //Don't log the register state?
	if (!registers || !filename) //Invalid?
	{
		log_timestampbackup = log_logtimestamp(2); //Save state!
		log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
		dolog(filename,"Log registers called with invalid argument!");
		log_logtimestamp(log_timestampbackup); //Restore state!
		return; //Abort!
	}
	log_timestampbackup = log_logtimestamp(2); //Save state!
	log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
	if (EMULATED_CPU<=CPU_80286) //Emulating 80(1)86 registers?
	{
		#ifndef LOGFLAGSONLY
		dolog(filename,"Registers:"); //Start of the registers!
		dolog(filename,"AX: %04X, BX: %04X, CX: %04X, DX: %04X",registers->AX,registers->BX,registers->CX,registers->DX); //Basic registers!
		if (registers->CR0&1) //Protected mode?
		{
			dolog(filename,"CS: %04X, DS: %04X, ES: %04X, SS: %04X, TR: %04X, LDTR:%04X",registers->CS,registers->DS,registers->ES,registers->SS,registers->TR,registers->LDTR); //Segment registers!
		}
		else //Real mode?
		{
			dolog(filename,"CS: %04X, DS: %04X, ES: %04X, SS: %04X",registers->CS,registers->DS,registers->ES,registers->SS); //Segment registers!
		}
		dolog(filename,"SP: %04X, BP: %04X, SI: %04X, DI: %04X",registers->SP,registers->BP,registers->SI,registers->DI); //Segment registers!
		dolog(filename,"IP: %04X, FLAGS: %04X",registers->IP,registers->FLAGS); //Rest!
		if (EMULATED_CPU==CPU_80286) //80286 has CR0 as well?
		{
			dolog(filename, "CR0: %04X", (registers->CR0&0xFFFF)); //Rest!
			dolog(filename,"GDTR: " LONGLONGSPRINTX ", IDTR: " LONGLONGSPRINTX,(LONG64SPRINTF)registers->GDTR.data,(LONG64SPRINTF)registers->IDTR.data); //GDTR/IDTR!
		}
		#endif
		dolog(filename,"FLAGSINFO:%s%c",debugger_generateFlags(registers),decodeHLTreset(halted,isreset)); //Log the flags!
		//More aren't implemented in the 80(1/2)86!
	}
	else //80386+? 32-bit registers!
	{
		dolog(filename,"Registers:"); //Start of the registers!
		#ifndef LOGFLAGSONLY
		dolog(filename,"EAX: %08x, EBX: %08x, ECX: %08x, EDX: %08x",registers->EAX,registers->EBX,registers->ECX,registers->EDX); //Basic registers!
		
		if (registers->CR0&1) //Protected mode?
		{
			dolog(filename,"CS: %04X, DS: %04X, ES: %04X, FS: %04X, GS: %04X SS: %04X, TR: %04X, LDTR:%04X",registers->CS,registers->DS,registers->ES,registers->FS,registers->GS,registers->SS,registers->TR,registers->LDTR); //Segment registers!
		}
		else //Real mode?
		{
			dolog(filename,"CS: %04X, DS: %04X, ES: %04X, FS: %04X, GS: %04X SS: %04X",registers->CS,registers->DS,registers->ES,registers->FS,registers->GS,registers->SS); //Segment registers!
		}

		dolog(filename,"ESP: %08x, EBP: %08x, ESI: %08x, EDI: %08x",registers->ESP,registers->EBP,registers->ESI,registers->EDI); //Segment registers!
		dolog(filename,"EIP: %08x, EFLAGS: %08x",registers->EIP,registers->EFLAGS); //Rest!
		
		dolog(filename, "CR0: %08X; CR1: %08X; CR2: %08X; CR3: %08X", registers->CR0, registers->CR1, registers->CR2, registers->CR3); //Rest!
		dolog(filename, "CR4: %08X; CR5: %08X; CR6: %08X; CR7: %08X", registers->CR4, registers->CR5, registers->CR6, registers->CR7); //Rest!

		dolog(filename, "DR0: %08X; DR1: %08X; DR2: %08X; CR3: %08X", registers->DR0, registers->DR1, registers->DR2, registers->DR3); //Rest!
		if (EMULATED_CPU>=CPU_PENTIUM) //Pentium has DR4?
		{
			dolog(filename, "DR4: %08X; DR6: %08X; DR5&7: %08X", registers->DR4, registers->DR6, registers->DR7); //Rest!
		}
		else //DR4=>DR6
		{
			dolog(filename, "DR6: %08X; DR5&7: %08X", registers->DR6, registers->DR7); //Rest!
		}

		dolog(filename,"GDTR: " LONGLONGSPRINTX ", IDTR: " LONGLONGSPRINTX,(LONG64SPRINTF)registers->GDTR.data,(LONG64SPRINTF)registers->IDTR.data); //GDTR/IDTR!
		#endif
		//Finally, flags seperated!
		dolog(filename,"FLAGSINFO:%s%c",debugger_generateFlags(registers),(char)(halted?'H':' ')); //Log the flags!
	}
	log_logtimestamp(log_timestampbackup); //Restore state!
}

void debugger_logmisc(char *filename, CPU_registers *registers, byte halted, byte isreset, CPU_type *theCPU)
{
	if ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Common log format?
	{
		return; //No misc logging, we're disabled for now!
	}
	if ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_NOREGISTERS) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_SINGLELINE) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_SINGLELINE) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED)) return; //Don't log us: don't log register state!
	int i;
	//Full interrupt status!
	char buffer[0x11] = ""; //Empty buffer to fill!
	strcpy(buffer,""); //Clear the buffer!
	for (i = 0xF;i >= 0;i--) //All 16 interrupt flags!
	{
		sprintf(buffer,"%s%u",buffer,(i8259.irr[(i&8)>>3]>>(i&7))&1); //Show the interrupt status!
	}
	log_timestampbackup = log_logtimestamp(2); //Save state!
	log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
	dolog(filename,"Interrupt status: %s",buffer); //Log the interrupt status!
	strcpy(buffer,""); //Clear the buffer!
	for (i = 0xF;i >= 0;i--) //All 16 interrupt flags!
	{
		sprintf(buffer,"%s%u",buffer,(i8259.imr[(i&8)>>3]>>(i&7))&1); //Show the interrupt status!
	}
	dolog(filename,"Interrupt mask: %s",buffer); //Log the interrupt status!
	if (getActiveVGA() && debugger_logtimings) //Gotten an active VGA?
	{
		dolog(filename,"VGA@%u,%u(CRT:%u,%u)",((SEQ_DATA *)getActiveVGA()->Sequencer)->x,((SEQ_DATA *)getActiveVGA()->Sequencer)->Scanline,getActiveVGA()->CRTC.x,getActiveVGA()->CRTC.y);
		dolog(filename,"Display=%u,%u",GPU.xres,GPU.yres);
	}
	log_logtimestamp(log_timestampbackup); //Restore state!
}

extern word modrm_lastsegment;
extern uint_32 modrm_lastoffset;
extern byte last_modrm; //Is the last opcode a modr/m read?

extern byte OPbuffer[256];
extern word OPlength; //The length of the OPbuffer!

extern BIU_type BIU[MAXCPUS]; //The BIU we're emulating!

extern byte DMA_S; //DMA state of transfer(clocks S0-S3), when active!

extern char DMA_States_text[6][256]; //DMA states!

char executedinstruction[256];
char statelog[256];
char executedinstructionstatelog[2048];

OPTINLINE static void debugger_autolog()
{
	if (CPU[activeCPU].executed) //Are we executed?
	{
		if ((debuggerregisters.EIP == CPU[activeCPU].registers->EIP) && (debuggerregisters.CS == CPU[activeCPU].registers->CS) && (!CPU[activeCPU].faultraised) && (!forcerepeat) && (!debuggerHLT))
		{
			return; //Are we the same address as the executing command and no fault or HLT state has been raised? We're a repeat operation!
		}
		forcerepeat = 0; //Don't force repeats anymore if forcing!
	}

	if (debugger_logging()) //To log?
	{
		log_timestampbackup = log_logtimestamp(2); //Save state!
		log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
		if (CPU[activeCPU].executed)
		{
			//Now generate debugger information!
			if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Not single-line?
			{
				if (last_modrm)
				{
					if (EMULATED_CPU<=CPU_80286) //16-bits addresses?
					{
						dolog("debugger","ModR/M address: %04X:%04X=%08X",modrm_lastsegment,modrm_lastoffset,((modrm_lastsegment<<4)+modrm_lastoffset));
					}
					else //386+? Unknown addresses, so just take it as given!
					{
						dolog("debugger","ModR/M address: %04X:%08X",modrm_lastsegment,modrm_lastoffset);
					}
				}
				if (MMU_invaddr()) //We've detected an invalid address?
				{
					switch (MMU_invaddr()) //What error?
					{
					case 0: //OK!
						break;
					case 1: //Memory not found!
						dolog("debugger", "MMU has detected that the addressed data isn't valid! The memory is non-existant.");
						break;
					case 2: //Paging or protection fault!
						dolog("debugger", "MMU has detected that the addressed data isn't valid! The memory is not paged or protected.");
						break;
					default:
						dolog("debugger", "MMU has detected that the addressed data isn't valid! The cause is unknown.");
						break;
					}
				}
				if (CPU[activeCPU].faultraised) //Fault has been raised?
				{
					dolog("debugger", "The CPU has raised an exception.");
				}
			}
			char fullcmd[65536];
			cleardata(&fullcmd[0],sizeof(fullcmd)); //Init!
			int i; //A counter for opcode data dump!
			if (!debugger_set) //No debugger set?
			{
				if (!((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT))) strcpy(fullcmd,"<Debugger not implemented: "); //Set to the last opcode!
				for (i = 0; i < (int)OPlength; i++) //List the full command!
				{
					if (!((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT))) sprintf(fullcmd, "%s%02X", debugger_command_text, OPbuffer[i]); //Add part of the opcode!
					else sprintf(fullcmd, fullcmd[0]?"%s %02X":"%s%02X", debugger_command_text, OPbuffer[i]); //Add part of the opcode!
				}
				if (!((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT))) strcat(fullcmd, ">"); //End of #UNKOP!
			}
			else
			{
				if (!((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT))) strcpy(fullcmd, "(");
				for (i = 0; i < (int)OPlength; i++) //List the full command!
				{
					if (!((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT))) sprintf(fullcmd, "%s%02X", fullcmd, OPbuffer[i]); //Add part of the opcode!
					else { sprintf(fullcmd, fullcmd[0]?"%s %02X":"%s%02X", fullcmd, OPbuffer[i]); } //Add part of the opcode!
				}
				if (!((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT))) strcat(fullcmd, ")"); //Our opcode before disassembly!
				else strcat(fullcmd, " "); //Our opcode before disassembly!
				strcat(fullcmd, debugger_prefix); //The prefix(es)!
				strcat(fullcmd, debugger_command_text); //Command itself!
			}

			if (HWINT_saved && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Saved HW interrupt?
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

			strcpy(executedinstruction,""); //Clear instruction!
			if ((debuggerregisters.CR0&1)==0) //Emulating 80(1)86? Use IP!
			{
				if ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Common log format?
				{
					sprintf(executedinstruction,"%04X:%08X %s",debuggerregisters.CS,debuggerregisters.IP,fullcmd); //Log command, 16-bit disassembler style!
				}
				else //8086 compatible log?
				{
					sprintf(executedinstruction,"%04X:%04X %s",debuggerregisters.CS,debuggerregisters.IP,fullcmd); //Log command, 16-bit disassembler style!
				}
			}
			else //286+? Use EIP!
			{
				if ((EMULATED_CPU>CPU_80286) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Newer? Use 32-bits addressing when newer or Common log format!
				{
					sprintf(executedinstruction,"%04X:%08X %s",debuggerregisters.CS,debuggerregisters.EIP,fullcmd); //Log command, 32-bit disassembler style!
				}
				else //16-bits offset?
				{
					sprintf(executedinstruction,"%04X:%04X %s",debuggerregisters.CS,debuggerregisters.EIP,fullcmd); //Log command, 32-bit disassembler style!
				}
			}
			if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Not single line?
			{
				dolog("debugger",executedinstruction); //The executed instruction!
			}
		}

		if (debugger_logtimings) //Logging the timings?
		{
			strcpy(statelog,""); //Default to empty!
			if (DEBUGGER_LOGSTATES) //Are we logging states?
			{
				if (BIU[activeCPU].stallingBUS && ((BIU[activeCPU].stallingBUS!=3) || ((BIU[activeCPU].stallingBUS==3) && (CPU[activeCPU].BUSactive==0)))) //Stalling the BUS?
				{
					strcpy(statelog,"BIU --"); //Stalling the BIU!
				}
				else if (BIU[activeCPU].TState<0xFE) //Not a special state?
				{
					if (debugger_simplifiedlog) //Simplified log?
					{
						sprintf(statelog,"BIU T%u",
							(BIU[activeCPU].TState+1) //Current T-state!
							);
					}
					else //Normal full log?
					{
						sprintf(statelog,"BIU T%u: EU&BIU cycles: %u, Operation cycles: %u, HW interrupt cycles: %u, Prefix cycles: %u, Exception cycles: %u, Prefetching cycles: %u, BIU prefetching cycles(1 each): %u, BIU DMA cycles: %u",
							(BIU[activeCPU].TState+1), //Current T-state!
							CPU[activeCPU].cycles, //Cycles executed by the BIU!
							CPU[activeCPU].cycles_OP, //Total number of cycles for an operation!
							CPU[activeCPU].cycles_HWOP, //Total number of cycles for an hardware interrupt!
							CPU[activeCPU].cycles_Prefix, //Total number of cycles for the prefix!
							CPU[activeCPU].cycles_Exception, //Total number of cycles for an exception!
							CPU[activeCPU].cycles_Prefetch, //Total number of cycles for prefetching from memory!
							CPU[activeCPU].cycles_Prefetch_BIU, //BIU cycles actually spent on prefetching during the remaining idle BUS time!
							CPU[activeCPU].cycles_Prefetch_DMA //BIU cycles actually spent on prefetching during the remaining idle BUS time!
							);
					}
				}
				else
				{
					switch (BIU[activeCPU].TState) //What state?
					{
						default: //Unknown?
						case 0xFE: //DMA cycle?
							if (debugger_simplifiedlog) //Simplified log?
							{
								sprintf(statelog,"DMA %s",
									DMA_States_text[DMA_S] //Current S-state!
									);
							}
							else //Normal full log?
							{
								sprintf(statelog,"DMA %s: EU&BIU cycles: %u, Operation cycles: %u, HW interrupt cycles: %u, Prefix cycles: %u, Exception cycles: %u, Prefetching cycles: %u, BIU prefetching cycles(1 each): %u, BIU DMA cycles: %u",
									DMA_States_text[DMA_S], //Current S-state!
									CPU[activeCPU].cycles, //Cycles executed by the BIU!
									CPU[activeCPU].cycles_OP, //Total number of cycles for an operation!
									CPU[activeCPU].cycles_HWOP, //Total number of cycles for an hardware interrupt!
									CPU[activeCPU].cycles_Prefix, //Total number of cycles for the prefix!
									CPU[activeCPU].cycles_Exception, //Total number of cycles for an exception!
									CPU[activeCPU].cycles_Prefetch, //Total number of cycles for prefetching from memory!
									CPU[activeCPU].cycles_Prefetch_BIU, //BIU cycles actually spent on prefetching during the remaining idle BUS time!
									CPU[activeCPU].cycles_Prefetch_DMA //BIU cycles actually spent on prefetching during the remaining idle BUS time!
									);
							}
							break;
						case 0xFF: //Waitstate RAM!
							if (debugger_simplifiedlog) //Simplified log?
							{
								sprintf(statelog,"BIU W"
									);
							}
							else //Normal full log?
							{
								sprintf(statelog,"BIU W: EU&BIU cycles: %u, Operation cycles: %u, HW interrupt cycles: %u, Prefix cycles: %u, Exception cycles: %u, Prefetching cycles: %u, BIU prefetching cycles(1 each): %u, BIU DMA cycles: %u",
									CPU[activeCPU].cycles, //Cycles executed by the BIU!
									CPU[activeCPU].cycles_OP, //Total number of cycles for an operation!
									CPU[activeCPU].cycles_HWOP, //Total number of cycles for an hardware interrupt!
									CPU[activeCPU].cycles_Prefix, //Total number of cycles for the prefix!
									CPU[activeCPU].cycles_Exception, //Total number of cycles for an exception!
									CPU[activeCPU].cycles_Prefetch, //Total number of cycles for prefetching from memory!
									CPU[activeCPU].cycles_Prefetch_BIU, //BIU cycles actually spent on prefetching during the remaining idle BUS time!
									CPU[activeCPU].cycles_Prefetch_DMA //BIU cycles actually spent on prefetching during the remaining idle BUS time!
									);
							}
							break;
					}
				}
			}
			if ((DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Not logging single lines?
			{
				if (strlen(statelog))
				{
					dolog("debugger",statelog); //Log the state log only!
				}
			}
			else //Logging single line?
			{
				strcpy(executedinstructionstatelog,""); //Init!
				if (strlen(executedinstruction) && CPU[activeCPU].executed) //Executed instruction?
				{
					if ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Special case?
					{
						sprintf(executedinstructionstatelog,"%s",executedinstruction); //Universal format!
						goto finishstatelog; //Skip other data for now!
					}
					else
					{
						sprintf(executedinstructionstatelog,"%s\t%s",statelog,executedinstruction);
					}
				}
				else //State only?
				{
					sprintf(executedinstructionstatelog,"%s\t",statelog);
				}
				finishstatelog:
				if (strlen(debugger_memoryaccess_text)) //memory access?
				{
					dolog("debugger","%s\t%s",executedinstructionstatelog,debugger_memoryaccess_text);
				}
				else //(Instruction+)State only?
				{
					if (strcmp(executedinstructionstatelog,"\t")!=0) //Valid state(not containing nothing at all)?
					{
						if ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Special case?
						{
							dolog("debugger","%s",executedinstructionstatelog); //Instruction/State only!
						}
						else
						{
							dolog("debugger","%s\t",executedinstructionstatelog); //Instruction/State only!
						}
					}
				}
			}
			strcpy(debugger_memoryaccess_text,""); //Clear the text to apply: we're done!
		}
		log_logtimestamp(log_timestampbackup); //Restore state!

		if (CPU[activeCPU].executed && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) && (DEBUGGER_LOG!=DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) && (DEBUGGER_LOG!=DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Multiple lines and finished executing?
		{
			debugger_logregisters("debugger",&debuggerregisters,debuggerHLT,debuggerReset); //Log the previous (initial) register status!
		
			debugger_logmisc("debugger",&debuggerregisters,debuggerHLT,debuggerReset,&CPU[activeCPU]); //Log misc stuff!

			log_timestampbackup = log_logtimestamp(2); //Save state!
			log_logtimestamp(debugger_loggingtimestamp); //Are we to log the timestamp?
			dolog("debugger",""); //Empty line between comands!
			log_logtimestamp(log_timestampbackup); //Restore state!
			debuggerINT = 0; //Don't continue after an INT has been used!
		}
	} //Allow logging?
}

extern GPU_TEXTSURFACE *frameratesurface; //The framerate surface!

word debuggerrow; //Debugger row after the final row!

extern word CPU_exec_lastCS; //OPCode CS
extern uint_32 CPU_exec_lastEIP; //OPCode EIP

OPTINLINE void debugger_screen() //Show debugger info on-screen!
{
	if (frameratesurface) //We can show?
	{
		GPU_text_locksurface(frameratesurface); //Lock!
		uint_32 fontcolor = RGB(0xFF, 0xFF, 0xFF); //Font color!
		uint_32 backcolor = RGB(0x00, 0x00, 0x00); //Back color!
		char str[256];
		cleardata(&str[0], sizeof(str)); //For clearing!
		int i;
		GPU_textgotoxy(frameratesurface, safe_strlen(debugger_prefix, sizeof(debugger_prefix)) + safe_strlen(debugger_command_text, sizeof(debugger_command_text)), GPU_TEXT_DEBUGGERROW); //Goto start of clearing!
		for (i = (safe_strlen(debugger_prefix, sizeof(debugger_prefix)) + safe_strlen(debugger_command_text, sizeof(debugger_command_text))); i < GPU_TEXTSURFACE_WIDTH - 6; i++) //Clear unneeded!
		{
			GPU_textprintf(frameratesurface, 0, 0, " "); //Clear all unneeded!
		}

		GPU_textgotoxy(frameratesurface, 0, GPU_TEXT_DEBUGGERROW);
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "Command: %s%s", debugger_prefix, debugger_command_text); //Show our command!
		debuggerrow = GPU_TEXT_DEBUGGERROW; //The debug row we're writing to!	
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 21, debuggerrow++); //First debug row!
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "Prefix(0):%02X; ROP: %02X", OPbuffer[0], CPU[activeCPU].lastopcode); //Debug opcode and executed opcode!

		//First: location!
		if ((((debuggerregisters.CR0&1)==0) || (debuggerregisters.EFLAGS&F_V8)) || (EMULATED_CPU == CPU_80286)) //Real mode, virtual 8086 mode or normal real-mode registers used in 16-bit protected mode?
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 15, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "CS:IP %04X:%04X", debuggerregisters.CS, debuggerregisters.IP); //Debug CS:IP!
		}
		else //386+?
		{
			if (EMULATED_CPU>=CPU_80386) //32-bit CPU?
			{
				GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 20, debuggerrow++); //Second debug row!
				GPU_textprintf(frameratesurface, fontcolor, backcolor, "CS:EIP %04X:%08X", debuggerregisters.CS, debuggerregisters.EIP); //Debug IP!
			}
			else //286-?
			{
				GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 16, debuggerrow++); //Second debug row!
				GPU_textprintf(frameratesurface, fontcolor, backcolor, "CS:IP %04X:%04X", debuggerregisters.CS, debuggerregisters.IP); //Debug IP!
			}
		}

		if ((((debuggerregisters.CR0&1)==0) || (debuggerregisters.EFLAGS&F_V8)) || (EMULATED_CPU == CPU_80286)) //Real mode, virtual 8086 mode or normal real-mode registers used in 16-bit protected mode?
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 18, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "P: CS:IP %04X:%04X", CPU_exec_lastCS, CPU_exec_lastEIP); //Debug CS:IP!
		}
		else //386+?
		{
			if (EMULATED_CPU>=CPU_80386) //32-bit CPU?
			{
				GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 23, debuggerrow++); //Second debug row!
				GPU_textprintf(frameratesurface, fontcolor, backcolor, "P: CS:EIP %04X:%08X", CPU_exec_lastCS, CPU_exec_lastEIP); //Debug IP!
			}
			else //286-?
			{
				GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 19, debuggerrow++); //Second debug row!
				GPU_textprintf(frameratesurface, fontcolor, backcolor, "P: CS:IP %04X:%04X", CPU_exec_lastCS, CPU_exec_lastEIP); //Debug IP!
			}
		}

		//Now: Rest segments!
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 16, debuggerrow++); //Second debug row!
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "DS:%04X; ES:%04X", debuggerregisters.DS, debuggerregisters.ES); //Debug DS&ES!
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debuggerrow++); //Second debug row!
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "SS:%04X", debuggerregisters.SS); //Debug SS!
		if (EMULATED_CPU >= CPU_80386) //386+?
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 16, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "FS:%04X; GS:%04X", debuggerregisters.FS, debuggerregisters.GS); //Debug FS&GS!
		}
		if (debuggerregisters.CR0&1) //Protected mode?
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 18, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "TR:%04X; LDTR:%04X", debuggerregisters.TR, debuggerregisters.LDTR); //Debug TR&LDTR!
		}

		//General purpose registers!
		if (EMULATED_CPU<=CPU_80286) //Real mode, virtual 8086 mode or normal real-mode registers used in 16-bit protected mode?
		{
			//General purpose registers!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 17, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "AX:%04X; BX: %04X", debuggerregisters.AX, debuggerregisters.BX); //Debug AX&BX!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 17, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "CX:%04X; DX: %04X", debuggerregisters.CX, debuggerregisters.DX); //Debug CX&DX!

			//Pointers and indexes!

			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 17, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "SP:%04X; BP: %04X", debuggerregisters.SP, debuggerregisters.BP); //Debug SP&BP!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 17, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "SI:%04X; DI: %04X", debuggerregisters.SI, debuggerregisters.DI); //Debug SI&DI!

			if (EMULATED_CPU>=CPU_80286) //We have an extra register?
			{
				//Control registers!
				GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 8, debuggerrow++); //Second debug row!
				GPU_textprintf(frameratesurface, fontcolor, backcolor, "CR0:%04X", (debuggerregisters.CR0&0xFFFF)); //Debug CR0!
			}
		}
		else //386+?
		{
			//General purpose registers!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 27, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "EAX:%08X; EBX: %08X", debuggerregisters.EAX, debuggerregisters.EBX); //Debug EAX&EBX!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 27, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "ECX:%08X; EDX: %08X", debuggerregisters.ECX, debuggerregisters.EDX); //Debug ECX&EDX!

			//Pointers and indexes!

			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 27, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "ESP:%08X; EBP: %08X", debuggerregisters.ESP, debuggerregisters.EBP); //Debug ESP&EBP!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 27, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "ESI:%08X; EDI: %08X", debuggerregisters.ESI, debuggerregisters.EDI); //Debug ESI&EDI!

			//Control Registers!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 27, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "CR0:%08X; CR1:%08X", debuggerregisters.CR0, debuggerregisters.CR1); //Debug CR0&CR1!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 27, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "CR2:%08X; CR3:%08X", debuggerregisters.CR2, debuggerregisters.CR3); //Debug CR2&CR3!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 27, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "CR4:%08X; CR5:%08X", debuggerregisters.CR4, debuggerregisters.CR5); //Debug CR4&CR5!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 27, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "CR6:%08X; CR7:%08X", debuggerregisters.CR6, debuggerregisters.CR7); //Debug CR6&CR7!

			//Debugger registers!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 27, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "DR0:%08X; DR1:%08X", debuggerregisters.DR[0], debuggerregisters.DR[1]); //Debug DR0&DR1!
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 27, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "DR2:%08X; DR3:%08X", debuggerregisters.DR[2], debuggerregisters.DR[3]); //Debug DR2&DR3!
			if (EMULATED_CPU<CPU_PENTIUM) //DR4=>DR6?
			{
				GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 31, debuggerrow++); //Second debug row!
				GPU_textprintf(frameratesurface, fontcolor, backcolor, "DR4&6:%08X; DR5&7:%08X", debuggerregisters.DR[4], debuggerregisters.DR[5]); //Debug DR4/6&DR5/7!
			}
			else //DR4 and DR6 are seperated and both implemented on Pentium+!
			{
				GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 42, debuggerrow++); //Second debug row!
				GPU_textprintf(frameratesurface, fontcolor, backcolor, "DR4:%08X; DR6:%08X DR5&7:%08X", debuggerregisters.DR[4], debuggerregisters.DR[5]); //Debug DR4/6&DR5/7!
			}
		}

		if (EMULATED_CPU>=CPU_80286) //We have extra registers in all modes?
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 44, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "GDTR:" LONGLONGSPRINTX "; IDTR:" LONGLONGSPRINTX, (LONG64SPRINTF)debuggerregisters.GDTR.data, (LONG64SPRINTF)debuggerregisters.IDTR.data); //Debug GDTR&IDTR!
		}

		//Finally, the flags!
		//First, flags fully...
		if (EMULATED_CPU <= CPU_80286) //Real mode, virtual 8086 mode or normal real-mode registers used in 16-bit protected mode? 80386 virtual 8086 mode uses 32-bit flags!
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 7, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "F :%04X", debuggerregisters.FLAGS); //Debug FLAGS!
		}
		else //386+
		{
			GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 11, debuggerrow++); //Second debug row!
			GPU_textprintf(frameratesurface, fontcolor, backcolor, "F :%08X", debuggerregisters.EFLAGS); //Debug FLAGS!
		}

		//Finally, flags seperated!
		char *theflags = debugger_generateFlags(&debuggerregisters); //Generate the flags as text!
		GPU_textgotoxy(frameratesurface, (GPU_TEXTSURFACE_WIDTH - strlen(theflags)) - 1, debuggerrow++); //Second flags row! Reserve one for our special HLT flag!
		GPU_textprintf(frameratesurface, fontcolor, backcolor, "%s%c", theflags, decodeHLTreset(debuggerHLT,debuggerReset)); //All flags, seperated!

		//Full interrupt status!
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-18,debuggerrow++); //Interrupt status!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"R:"); //Show the interrupt request status!
		for (i = 0xF;i >= 0;i--) //All 16 interrupt flags!
		{
			GPU_textprintf(frameratesurface,fontcolor,backcolor,"%u",(i8259.irr[(i&8)>>3]>>(i&7))&1); //Show the interrupt status!
		}
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-18,debuggerrow++); //Interrupt status!
		GPU_textprintf(frameratesurface,fontcolor,backcolor,"M:"); //Show the interrupt mask!
		for (i = 0xF;i >= 0;i--) //All 16 interrupt flags!
		{
			GPU_textprintf(frameratesurface,fontcolor,backcolor,"%u",(i8259.imr[(i&8)>>3]>>(i&7))&1); //Show the interrupt status!
		}

		if (memprotect(getActiveVGA(),sizeof(VGA_Type),"VGA_Struct")) //Gotten an active VGA?
		{
			GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH-52,debuggerrow++); //CRT status!
			GPU_textprintf(frameratesurface,fontcolor,backcolor,"VGA@%05i,%05i(CRT:%05i,%05i) Display=%05i,%05i",((SEQ_DATA *)getActiveVGA()->Sequencer)->x,((SEQ_DATA *)getActiveVGA()->Sequencer)->Scanline,getActiveVGA()->CRTC.x,getActiveVGA()->CRTC.y,GPU.xres, GPU.yres);
		}
		GPU_text_releasesurface(frameratesurface); //Unlock!
	}
}

extern byte Settings_request; //Settings requested to be executed?

void debuggerThread()
{
	byte openBIOS = 0;
	int i;
	int done = 0;
	byte displayed = 0; //Are we displayed?
	pauseEMU(); //Pause it!

	restartdebugger: //Restart the debugger during debugging!
	done = 0; //Init: not done yet!

	if (!(done || skipopcodes || (skipstep&&CPU[activeCPU].repeating))) //Are we to show the (new) debugger screen?
	{
		displayed = 1; //We're displayed!
		lock(LOCK_MAINTHREAD); //Lock the main thread!
		debugger_screen(); //Show debugger info on-screen!
		unlock(LOCK_MAINTHREAD); //Finished with the main thread!
	}

	for (;!(done || skipopcodes || (skipstep&&CPU[activeCPU].repeating));) //Still not done or skipping?
	{
		if (DEBUGGER_ALWAYS_STEP || (singlestep==1)) //Always step?
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
			singlestep = 0; //If single stepping, stop doing so!
			break;
		}
		if (psp_keypressed(BUTTON_TRIANGLE)) //Skip 10 commands?
		{
			while (psp_keypressed(BUTTON_TRIANGLE)) //Wait for release!
			{
			}
			skipopcodes = 9; //Skip 9 additional opcodes!
			singlestep = 0; //If single stepping, stop doing so!
			break;
		}
		if (psp_keypressed(BUTTON_SQUARE)) //Skip until finished command?
		{
			while (psp_keypressed(BUTTON_SQUARE)) //Wait for release!
			{
			}
			skipopcodes_destEIP = debuggerregisters.EIP+OPlength; //Destination instruction position!
			if (getcpumode() != CPU_MODE_PROTECTED) //Not protected mode?
			{
				skipopcodes_destEIP &= 0xFFFF; //Wrap around, like we need to!
			}
			if (psp_keypressed(BUTTON_CIRCLE) && (CPU[activeCPU].repeating==0)) //Wait for the jump to be taken from the current address?
			{
				skipopcodes_destEIP = debuggerregisters.EIP; //We're jumping from this address!
				skipstep = 4;
			}
			else //Normal behaviour?
			{
				if (CPU[activeCPU].repeating) //Are we repeating?
				{
					skipstep = 1; //Skip all REP additional opcodes!
				}
				else //Use the supplied EIP!
				{
					skipstep = 2; //Simply skip until the next instruction is reached after this address!
				}
			}
			break;
		}
		if (psp_keypressed(BUTTON_CIRCLE)) //Dump memory?
		{
			while (psp_keypressed(BUTTON_CIRCLE)) //Wait for release!
			{
			}
			if (psp_keypressed(BUTTON_TRIANGLE)) //Memory dump?
			{
				MMU_dumpmemory("memory.dat"); //Dump the MMU memory!
			}
		}
		openBIOS = 0; //Init!
		lock(LOCK_MAINTHREAD); //We're checking some input!
		openBIOS = Settings_request;
		Settings_request = 0; //We're handling it if needed!
		unlock(LOCK_MAINTHREAD); //We've finished checking for input!
		openBIOS |= psp_keypressed(BUTTON_SELECT); //Are we to open the BIOS menu?
		if (openBIOS && !is_gamingmode()) //Goto BIOS?
		{
			while (psp_keypressed(BUTTON_SELECT)) //Wait for release when pressed!
			{
			}
			//Start the BIOS
			if (runBIOS(0)) //Run the BIOS, reboot needed?
			{
				skipopcodes = 0; //Nothing to be skipped!
				goto singlestepenabled; //We're rebooting, abort!
			}
			//Check the current state to continue at!
			if (debugging()) //Recheck the debugger!
			{
				goto restartdebugger; //Restart the debugger!
			}
			else //Not debugging anymore?
			{
				goto singlestepenabled; //Stop debugging!
			}
		}
		if (shuttingdown()) break; //Stop debugging when shutting down!
		delay(0); //Wait a bit!
	} //While not done
	singlestepenabled: //Single step has been enabled just now?
	if (displayed) //Are we to clean up?
	{
		lock(LOCK_MAINTHREAD); //Make sure we aren't cleaning up!
		GPU_text_locksurface(frameratesurface); //Lock!
		for (i = GPU_TEXT_DEBUGGERROW;i < debuggerrow;i++) GPU_textclearrow(frameratesurface, i); //Clear our debugger rows!
		GPU_text_releasesurface(frameratesurface); //Unlock!
		unlock(LOCK_MAINTHREAD); //Finished!
	}
	resumeEMU(1); //Resume it!
}

ThreadParams_p debugger_thread = NULL; //The debugger thread, if any!

extern byte didJump; //Did we jump this instruction?

void debugger_step() //Processes the debugging step!
{
	if (debugger_thread) //Debugger not running yet?
	{
		if (threadRunning(debugger_thread)) //Still running?
		{
			return; //We're still running, so start nothing!
		}
	}
	debugger_simplifiedlog = ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_SINGLELINE_SIMPLIFIED) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_SINGLELINE_SIMPLIFIED) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)); //Simplified log?
	debugger_loggingtimestamp = ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT))?0:1; //Are we to log time too? Not with common log format!
	debugger_thread = NULL; //Not a running thread!
	debugger_autolog(); //Log when enabled!
	if (CPU[activeCPU].executed) //Are we executed?
	{
		if (debugging()) //Debugging step or single step enforced?
		{
			if (shuttingdown()) return; //Don't when shutting down!
			if (skipstep) //Finished?
			{
				if (!CPU[activeCPU].repeating && (skipstep==1)) //Finished repeating?
				{
					skipstep = 0; //Disable skip step!
				}
				else if (debuggerregisters.EIP == skipopcodes_destEIP) //We've reached the destination address?
				{
					if ((skipstep==4) && didJump) //Jumped at our specified step?
					{
						skipstep = 0; //We're finished!
					}
					else if (skipstep!=4) //Normal finish not requiring Jump to be executed?
					{
						skipstep = 0; //We're finished!
					}
				}
				else if (skipstep==3) //Stop immediately?
				{
					skipstep = 0; //We're finished!
				}
			}
			if (skipopcodes) //Skipping?
			{
				--skipopcodes; //Skipped one opcode!
			}
			if (!(skipopcodes || ((skipstep==1)&&CPU[activeCPU].repeating) || (skipstep==2))) //To debug when not skipping repeating or skipping opcodes?
			{
				debugger_thread = startThread(debuggerThread,"UniPCemu_debugger",NULL); //Start the debugger!
			}
		} //Step mode?
		if (singlestep>1) //Start single-stepping from the next instruction?
		{
			--singlestep; //Start single-stepping the next X instruction!
		}
	}
	#ifdef DEBUG_PROTECTEDMODE
	harddebugging = (getcpumode()!=CPU_MODE_REAL); //Protected/V86 mode forced debugging log to start/stop? Don't include the real mode this way(as it's already disabled after execution), do include the final instruction, leaving protected mode this way(as it's already handled).
	#endif
}

extern byte cpudebugger;

void debugger_setcommand(char *text, ...)
{
	char *c;
	if (cpudebugger) //Are we debugging?
	{
		va_list args; //Going to contain the list!
		va_start (args, text); //Start list!
		vsprintf (debugger_command_text, text, args); //Compile list!
		va_end (args); //Destroy list!
		debugger_set = 1; //We've set the debugger!
		if ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Common log format?
		{
			c = &debugger_command_text[0]; //Process it first!
			for (;*c;)
			{
				if ((*c>='A') && (*c<='Z')) //Capital?
				{
					*c -= (int)'A'; //Decrease!
					*c += (int)'a'; //Convert to lower case!
				}
				++c; //Next character!
			}
		}
	}
}

void debugger_setprefix(char *text)
{
	char *c;
	if ((debugger_prefix[0]=='\0') || (*text=='\0')) //No prefix yet or reset?
	{
		strcpy(debugger_prefix,text); //Set prefix!
		if (*text!='\0') //Not reset?
		{
			strcat(debugger_prefix, " "); //Prefix seperator!
		}
	}
	else
	{
		strcat(debugger_prefix, text); //Add prefix!
		strcat(debugger_prefix, " "); //Prefix seperator!
	}
	if ((DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_ALWAYS_DURINGSKIPSTEP_COMMONLOGFORMAT) || (DEBUGGER_LOG==DEBUGGERLOG_DEBUGGING_COMMONLOGFORMAT)) //Common log format?
	{
		c = &debugger_prefix[0]; //Process it first!
		for (;*c;)
		{
			if ((*c>='A') && (*c<='Z')) //Capital?
			{
				*c -= (int)'A'; //Decrease!
				*c += (int)'a'; //Convert to lower case!
			}
			++c; //Next character!
		}
	}
}

void initDebugger() //Initialize the debugger if needed!
{
	verifyfile = file_exists("debuggerverify16.dat"); //To perform verification checks at all?
	memset(&flags,0,sizeof(flags)); //Clear/init flags!
	memset(&executedinstruction,0,sizeof(executedinstruction)); //Init instruction!
	memset(&statelog,0,sizeof(statelog));
	memset(&debugger_memoryaccess_text,0,sizeof(debugger_memoryaccess_text));
	memset(&debugger_memoryaccess_line,0,sizeof(debugger_memoryaccess_line));
	memset(&executedinstructionstatelog,0,sizeof(executedinstructionstatelog));
}
