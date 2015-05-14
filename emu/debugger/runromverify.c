#include "headers/types.h"
#include "headers/cpu/cpu.h"
#include "headers/debugger/debugger.h"
#include "headers/emu/gpu/gpu.h"
#include "headers/emu/timers.h"
#include "headers/hardware/pic.h" //For interrupts!
//#include "headers/cpu/interrupts.h" //Interrupts!
#include "headers/cpu/8086/cpu_OP8086.h" //hardware interrupt function!
#include "headers/support/log.h" //Log support!
#include "headers/emu/gpu/gpu_text.h" //Text support!
#include "headers/cpu/callback.h" //CPU callback support!
#include "headers/bios/biosrom.h" //BIOS ROM support!

#include "headers/emu/emucore.h" //Emulation core!

extern byte reset; //To reset?
extern byte dosoftreset; //To soft-reset?
extern PIC i8259; //PIC processor!
extern byte cpudebugger; //To debug the CPU?
extern byte allow_debuggerstep; //Allow debugger stepping?

extern GPU_TEXTSURFACE *frameratesurface;

extern byte allow_RETHalt; //Allow RET(level<0)=HALT?
extern byte LOG_MMU_WRITES; //Log MMU writes?

int runromverify(char *filename, char *resultfile) //Run&verify ROM!
{
	dolog("debugger","RunROMVerify...");
	FILE *f;
	int memloc = 0;
	f = fopen(filename,"rb"); //First, load file!

	stopVideo(); //Disable video emulation!
	dolog("debugger","RUNROMVERIFY: initEMU...");
	initEMU(0); //Init EMU first, no video!
	dolog("debugger","RUNROMVERIFY: ready to go.");

	if (!hasmemory()) //No memory present?
	{
		dolog("ROM_log","Error: no memory loaded!");
		return 0; //Error!
	}

	memloc = 0; //Init location!

	word datastart = 0; //Start of data segment!
	resetCPU(); //Make sure we're loading the ROM correctly (start at 0xF000:FFFF)

	if (!f)
	{
		return 0; //Error: file doesn't exist!
	}

	fseek(f,0,SEEK_END); //Goto EOF!
	int fsize;
	fsize = ftell(f); //Size!
	fseek(f,0,SEEK_SET); //Goto BOF!		

	if (!fsize) //Invalid?
	{
		if (f)
		{
			fclose(f);    //Close when needed!
		}
		doneEMU();
		dolog("ROM_log","Invalid file size!");
		return 1; //OK!
	}
	
	fclose(f); //Close the ROM!
	
	if (!BIOS_load_custom(filename)) //Failed to load the BIOS ROM?
	{
		doneEMU(); //Finish the emulator!
		dolog("ROM_log","Failed loading the verification ROM as a BIOS!");
		return 0; //Failed!
	}
	
	EMU_Shutdown(0);
	cleartimers(); //Clear all timers!
	
	dolog("ROM_log","Terminating VGA...");
	terminateVGA(); //Don't show VGA!
	CPU[activeCPU].halt = 0; //Start without halt!
	dolog("ROM_log","Starting verification ROM emulator...");
	uint_32 erroraddr = 0xFFFFFFFF; //Error address (undefined)
	uint_32 lastaddr = 0xFFFFFFFF; //Last address causing the error!
	uint_32 erroraddr16 = 0x00000000; //16-bit segment:offset pair.
	BIOS_registerROM(); //Register the BIOS ROM!
	dolog("debugger","Starting debugging file %s",filename); //Log the file we're going to test!
	LOG_MMU_WRITES = 1; //Enable logging!
	allow_debuggerstep = 1; //Allow stepping of the debugger!
	for (;!CPU[activeCPU].halt;) //Still running?
	{
		if (CPU[activeCPU].registers->SFLAGS.IF && PICInterrupt()) CPU8086_hardware_int(nextintr(),0,0); //get next interrupt from the i8259, if any
		uint_32 curaddr = (CPU[activeCPU].registers->CS<<4)+CPU[activeCPU].registers->IP; //Calculate current memory address!
		if (curaddr<0xF0000) //Out of executable range?
		{
			erroraddr = curaddr; //Set error address!
			erroraddr16 = (CPU[activeCPU].registers->CS<<16)|CPU[activeCPU].registers->IP; //Set error address segment:offset!
			break; //Continue, but keep our warning!
		}
		lastaddr = curaddr; //Save the current address for reference of the error address!
		cpudebugger = needdebugger(); //Debugging?
		debugger_beforeCPU(); //Everything before the CPU!
		CPU_beforeexec(); //Everything before the execution!
		CPU_exec(); //Run CPU!
		debugger_step(); //Step debugger if needed!
		CB_handleCallbacks(); //Handle callbacks after CPU/debugger usage!
	} //Main debug CPU loop!
	LOG_MMU_WRITES = 0; //Disable logging!

	if (CPU[activeCPU].halt) //HLT Received?
	{
		dolog("ROM_log","Emulator terminated OK."); //Log!
	}
	else
	{
		dolog("ROM_log","Emulator terminated wrong."); //Log!
	}

	EMU_Shutdown(0);

	int verified = 1; //Data verified!
	f = fopen(resultfile,"rb"); //Result file verification!
	memloc = 0; //Start location!
	if (!f)
	{
		BIOS_free_custom(filename); //Free the custom BIOS ROM!
		doneEMU(); //Clean up!
		dolog("ROM_log","Error: Failed opening result file!");
		return 0; //Result file doesn't exist!
	}


	dolog("ROM_log","Verifying output...");	
	
	memloc = 0; //Initialise memory location!
	verified = 1; //Default: OK!
	byte data; //Data to verify!
	byte last; //Last data read in memory!
	while (!feof(f)) //Data left?
	{
		fread(&data,1,sizeof(data),f); //Read data to verify!
		last = MMU_rb(-1,datastart,memloc++,0); //One byte to compare from memory!
		byte verified2;
		verified2 = (data==last); //Verify with memory!
		verified &= verified2; //Check for verified!
		if (!verified2) //Error in verification byte?
		{
			dolog("ROM_log","Error address: %08X, expected: %02X, in memory: %02X",memloc-1,data,last); //Give the verification point that went wrong!
			//Continue checking for listing all errors!
		}
	}
	fclose(f); //Close the file!
	//dolog("ROM_log","Finishing emulator...");	
	BIOS_free_custom(filename); //Free the custom BIOS ROM!
	doneEMU(); //Cleanup!
	dolog("ROM_log","ROM Success: %i...",verified);
	if (!verified && erroraddr!=0xFFFFFFFF) //Error address specified?
	{
		dolog("ROM_log","Error address: %08X, Possible cause: %08X; Real mode address: %04X:%04X",erroraddr,lastaddr,((erroraddr16>>16)&0xFFFF),(erroraddr&0xFFFF)); //Log the error address!
	}
	return verified; //Verified?
}