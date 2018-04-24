#include "headers/cpu/protecteddebugging.h" //Our typedefs!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/cpu/easyregs.h" //Easy register addressing support!
#include "headers/cpu/cpu_execution.h" //Execution phase support!

OPTINLINE byte checkProtectedModeDebuggerBreakpoint(uint_32 linearaddress, byte type, byte DR) //Check a single breakpoint. Return 0 for not triggered!
{
	INLINEREGISTER uint_32 breakpointinfo;
	const uint_32 triggersizes[4] = {1,2,8,4}; //How many bytes to watch?
	uint_32 breakpointposition[2], endposition[2]; //Two breakpoint positions to support overflow locations!
	byte typematched=0; //Type matched?
	if (likely((CPU[activeCPU].registers->DR7&(3<<(DR<<1)))==0)) return 0; //Disabled? Both global and local are applied!
	{
		breakpointinfo = CPU[activeCPU].registers->DR7; //Get the info to process!
		breakpointinfo >>= (0x10|(DR<<2)); //Shift our information required to the low bits!
		switch (breakpointinfo&3) //Type matched? We're to handle this type of breakpoint!
		{
			case 0: //Execution?
				typematched = (type==PROTECTEDMODEDEBUGGER_TYPE_EXECUTION); //Matching type?
				break;
			case 1: //Data write?
				typematched = (type==PROTECTEDMODEDEBUGGER_TYPE_DATAWRITE); //Matching type?
				break;
			case 2: //Break on I/O read/write? Unsupported by all hardware! 64-bit mode makes it specify an 8-byte wide breakpoint area!
				typematched = (type==PROTECTEDMODEDEBUGGER_TYPE_IOREADWRITE); //Matching type?
				break;
			case 3: //Data read/write?
				typematched = ((type==PROTECTEDMODEDEBUGGER_TYPE_DATAREAD) || (type==PROTECTEDMODEDEBUGGER_TYPE_DATAWRITE)); //Matching type?
				break;
		}
		if (typematched) //Matching breakpoint type?
		{
			//Valid breakpoint type matched?
			breakpointinfo >>= 2; //Shift our size to watch to become available!
			breakpointinfo &= 3; //Only take the size to watch!
			breakpointinfo = triggersizes[breakpointinfo]; //Translate the size to watch info bytes to watch!
			breakpointposition[0] = breakpointposition[1] = CPU[activeCPU].registers->DR[DR]; //The debugger register to use for matching(start of linear block)!
			endposition[0] = endposition[1] = ((breakpointposition[0]+breakpointinfo)-1); //The end location of the breakpoint(final byte to watch)!
			if (endposition[0]<breakpointposition[0]) //Overflow in breakpoint address?
			{
				endposition[0] = ~0; //Maximum end location: first half of the breakpoint ends here!
				breakpointposition[1] = 0; //Second half of the breakpoint starts here!
			}
			if (((linearaddress>=breakpointposition[0]) && (linearaddress<=(endposition[0]))) || ((linearaddress>=breakpointposition[1]) && (linearaddress<=(endposition[1])))) //Breakpoint location matched?
			{
				if (type==PROTECTEDMODEDEBUGGER_TYPE_EXECUTION) //Executing fires immediately(fault)!
				{
					SETBITS(CPU[activeCPU].registers->DR6,DR,1,1); //Set this trap to fire!
					SETBITS(CPU[activeCPU].registers->DR6,15,1,0); //Clear bit 15, the new task's T-bit!
					CPU_executionphase_startinterrupt(EXCEPTION_DEBUG,0,-1); //Call the interrupt, no error code!
					return 1; //Triggered!
				}
				else //Data is a trap: report after executing!
				{
					SETBITS(CPU[activeCPU].debuggerFaultRaised,DR,1,1); //Set this trap to fire after the instruction(data breakpoint)!
				}
			}
		}
	}
	return 0; //Not triggered!
}

void checkProtectedModeDebuggerAfter() //Check after instruction for the protected mode debugger!
{
	byte DR;
	if (CPU[activeCPU].faultraised==0) //No fault raised yet?
	{
		if (CPU[activeCPU].debuggerFaultRaised && (FLAG_RF==0)) //Debugger fault raised?
		{
			for (DR=0;DR<4;++DR) //Check any exception that's occurred!
			{
				SETBITS(CPU[activeCPU].registers->DR6,GETBITS(CPU[activeCPU].debuggerFaultRaised,DR,1),1,1); //We're trapping this/these data breakpoint(s)!
			}
			CPU_executionphase_startinterrupt(EXCEPTION_DEBUG,0,-1); //Call the interrupt, no error code!
		}
		else //Successful completion of an instruction?
		{
			CPU[activeCPU].debuggerFaultRaised = 0; //Clear the fault raised information: no fault is raised for this instruction, prevent the same fault from bubbling into the next instruction!
			FLAGW_RF(0); //Successfull completion of an instruction clears the Resume Flag!
		}
	}
}

byte checkProtectedModeDebugger(uint_32 linearaddress, byte type) //Access at memory/IO port?
{
	if (likely(getcpumode()==CPU_MODE_REAL)) return 0; //Not supported in real mode!
	if (unlikely(FLAG_RF)) return 0; //Resume flag inhabits the exception!
	if (unlikely(checkProtectedModeDebuggerBreakpoint(linearaddress,type,0))) return 1; //Break into the debugger on Breakpoint #0!
	if (unlikely(checkProtectedModeDebuggerBreakpoint(linearaddress,type,1))) return 1; //Break into the debugger on Breakpoint #1!
	if (unlikely(checkProtectedModeDebuggerBreakpoint(linearaddress,type,2))) return 1; //Break into the debugger on Breakpoint #2!
	if (unlikely(checkProtectedModeDebuggerBreakpoint(linearaddress,type,3))) return 1; //Break into the debugger on Breakpoint #3!
	return 0; //Not supported yet!
}

void protectedModeDebugger_taskswitch() //Task switched?
{
	//Clear the local debugger breakpoints(bits 0,2,4,6 of DR7)
	CPU[activeCPU].registers->DR7 &= ~0x55; //Clear bits 0,2,4,6 on any task switch!
}
