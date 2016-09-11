#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //I/O support!
#include "headers/mmu/mmu.h" //MMU support!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/hardware/vga/vga.h" //VGA/EGA/CGA/MDA support!
#include "headers/emu/emucore.h" //Speed change support!
#include "headers/emu/debugger/debugger.h" //Debugging support for logging POST codes!
#include "headers/support/log.h" //For logging POST codes!

byte SystemControlPortB=0x00; //System control port B!
byte SystemControlPortA=0x00; //System control port A!
byte PPI62, PPI63; //Default PPI switches!
byte TurboMode=0;
byte diagnosticsportoutput = 0x00;
extern byte singlestep; //Enable EMU-driven single step!
sword diagnosticsportoutput_breakpoint = -1; //Breakpoint set?
sword breakpoint_comparison = -1; //Breakpoint comparison value!
uint_32 breakpoint_timeout = 1; //Timeout for the breakpoint to become active, in instructions! Once it becomes 0(and was 1), it triggers the breakpoint!

byte readPPI62()
{
	byte result=0;
	//Setup PPI62 as defined by System Control Port B!
	if (EMULATED_CPU<=CPU_NECV30) //XT machine?
	{
		if (SystemControlPortB&8) //Read high switches?
		{
			if (((getActiveVGA()->registers->specialCGAflags&0x81)==1)) //Pure CGA mode?
			{
				result |= 2; //First bit set: 80x25 CGA!
			}
			else if (((getActiveVGA()->registers->specialMDAflags&0x81)==1)) //Pure MDA mode?
			{
				result |= 3; //Both bits set: 80x25 MDA!
			}
			else //VGA?
			{
				//Leave PPI62 at zero for VGA: we're in need of auto detection!
			}
			result |= 4; //Two floppy drives installed!
		}
		else //Read low switches?
		{
			result |= 1; //Two floppy drives installed!
		}
	}
	else
	{
		return PPI62; //Give the normal value!
	}
	return result; //Give the switches requested, if any!
}

byte PPI_readIO(word port, byte *result)
{
	switch (port) //Special register: System control port B!
	{
	case 0x61: //System control port B?
		*result = SystemControlPortB; //Read the value!
		return 1;
		break;
	case 0x62: //PPI62?
		*result = readPPI62(); //Read the value!
		return 1;
		break;
	case 0x63: //PPI63?
		*result = PPI63; //Read the value!
		return 1;
		break;
	case 0x92: //System control port A?
		*result = SystemControlPortA; //Read the value!
		return 1;
		break;
	default: //unknown port?
		break;
	}
	return 0; //No PPI!
}

byte PPI_writeIO(word port, byte value)
{
	switch (port)
	{
	case 0x61: //System control port B?
		SystemControlPortB = (value&0x7F); //Set the port, highest bit isn't ours!
		TurboMode = (value&4); //Turbo mode enabled?
		updateSpeedLimit(); //Update the speed used!
		return 1;
		break;
	case 0x62: //PPI62?
		PPI62 = value; //Set the value!
		return 1;
		break;
	case 0x63: //PPI63?
		PPI63 = value; //Set the value!
		return 1;
		break;
	case 0x80: //IBM AT Diagnostics!
		if (((sword)value!=breakpoint_comparison) && (diagnosticsportoutput_breakpoint == (sword)value)) //Have we reached a breakpoint?
		{
			if (breakpoint_timeout) //Breakpoint timing?
			{
				if (--breakpoint_timeout==0) //Timeout?
				{
					singlestep = 1; //Start single stepping from this breakpoint!
				}
			}
		}
		if (isDebuggingPOSTCodes() && ((sword)value!=breakpoint_comparison)) //Changed and debugging POST codes?
		{
			dolog("debugger", "POST Code: %02X", value); //Log the new value!
		}
		breakpoint_comparison = (sword)value; //Save into the comparison for new changes!
		diagnosticsportoutput = value; //Save it to the diagnostics display!
		break;
	case 0x92: //System control port A?
		MMU_setA20(1,value&2); //Fast A20!
		if (value&1) //Fast reset?
		{
			doneCPU();
			resetCPU(); //Reset the CPU!
		}
		SystemControlPortA = (value&(~1)); //Set the port!
		return 1;
		break;
	default: //unknown port?
		break;
	}
	return 0; //No PPI!
}

void initPPI(sword useDiagnosticsportoutput_breakpoint, uint_32 breakpointtimeout)
{
	SystemControlPortB = 0x7F; //Reset system control port B!
	PPI62 = 0x00; //Set the default switches!
	PPI63 = 0x00; //Set the default switches!
	diagnosticsportoutput = 0x00; //Clear diagnostics port output!
	diagnosticsportoutput_breakpoint = useDiagnosticsportoutput_breakpoint; //Breakpoint set?
	breakpoint_comparison = -1; //Default to no comparison set, so the first set will trigger a breakpoint if needed!
	breakpoint_timeout = breakpoint_timeout+1; //Time out after this many instructions(0=Very first instruction, 0xFFFFFFFF is 4G instructions)!
	TurboMode = 0; //Default to no turbo mode according to the switches!
	updateSpeedLimit(); //Update the speed used!
	register_PORTIN(&PPI_readIO);
	register_PORTOUT(&PPI_writeIO);
}
