#include "headers/types.h" //Basic type support etc.
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/emu/gpu/gpu.h" //Need GPU comp!
#include "headers/cpu/cpu_OP8086.h" //8086 interrupt instruction support!
#include "headers/bios/bios.h" //BIOS Support!
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/cpu/easyregs.h" //Easy register addressing!

#include "headers/emu/gpu/gpu_emu.h" //GPU EMU support!
#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!
#include "headers/emu/timers.h" //Timer support!

void halt_modrm(char *message, ...) //Unknown modr/m?
{
	stopVideo(); //Need no video!
	stopTimers(0); //Stop all normal timers!
	char buffer[256]; //Going to contain our output data!
	va_list args; //Going to contain the list!
	va_start (args, message); //Start list!
	vsprintf (buffer, message, args); //Compile list!
	va_end (args); //Destroy list!
	raiseError("modrm","Modr/m error: %s",buffer); //Shut the adress and opcode!
	debugger_screen(); //Show debugger info!
//EMU_Shutdown(1); //Shut down the emulator!
	sleep(); //Wait forever!
}