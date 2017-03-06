#include "headers/cpu/protecteddebugging.h" //Our typedefs!
#include "headers/cpu/cpu.h" //CPU support!

byte checkProtectedModeDebuggerBreakpoint(uint_32 linearaddress, byte type, byte DR) //Check a single breakpoint. Return 0 for not triggered!
{
	return 0; //Not triggered!
}

byte checkProtectedModeDebugger(uint_32 linearaddress, byte type) //Access at memory/IO port?
{
	if (checkProtectedModeDebuggerBreakpoint(linearaddress,type,0)) return 1; //Break into the debugger on Breakpoint #0!
	if (checkProtectedModeDebuggerBreakpoint(linearaddress,type,1)) return 1; //Break into the debugger on Breakpoint #1!
	if (checkProtectedModeDebuggerBreakpoint(linearaddress,type,2)) return 1; //Break into the debugger on Breakpoint #2!
	if (checkProtectedModeDebuggerBreakpoint(linearaddress,type,3)) return 1; //Break into the debugger on Breakpoint #3!
	return 0; //Not supported yet!
}

void protectedModeDebugger_taskswitch() //Task switched?
{
	//Clear the local debugger breakpoints(bits 0,2,4,6 of DR7)
	CPU[activeCPU].registers->DR7 &= ~0x55; //Clear bits 0,2,4,6 on any task switch!
}