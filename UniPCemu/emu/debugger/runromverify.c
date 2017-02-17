#include "headers/types.h"
#include "headers/cpu/cpu.h"
#include "headers/emu/debugger/debugger.h"
#include "headers/emu/gpu/gpu.h"
#include "headers/emu/timers.h"
#include "headers/hardware/pic.h" //For interrupts!
#include "headers/cpu/cpu_OP8086.h" //hardware interrupt function!
#include "headers/support/log.h" //Log support!
#include "headers/emu/gpu/gpu_text.h" //Text support!
#include "headers/cpu/cb_manager.h" //CPU callback support!
#include "headers/bios/biosrom.h" //BIOS ROM support!
#include "headers/emu/emucore.h" //Emulation core!
#include "headers/cpu/protection.h" //PMode support!
#include "headers/support/locks.h" //Lock support!
#include "headers/mmu/mmuhandler.h" //hasmemory support!
#include "headers/emu/threads.h" //Multithreading support!
#include "headers/cpu/easyregs.h" //Flag support!

extern byte reset; //To reset?
extern byte dosoftreset; //To soft-reset?
extern PIC i8259; //PIC processor!
extern byte cpudebugger; //To debug the CPU?
extern byte allow_debuggerstep; //Allow debugger stepping?

extern GPU_TEXTSURFACE *frameratesurface;

extern byte allow_RETHalt; //Allow RET(level<0)=HALT?
extern byte LOG_MMU_WRITES; //Log MMU writes?

extern byte HWINT_nr, HWINT_saved; //HW interrupt saved?

extern byte MMU_logging; //Are we logging MMU accesses?

extern byte REPPending; //REP pending reset?

extern uint_32 CPU_InterruptReturn, CPU_exec_EIP; //Interrupt return address!

extern ThreadParams_p debugger_thread; //Debugger menu thread!

int runromverify(char *filename, char *resultfile) //Run&verify ROM!
{
	byte useHWInterrupts = 0; //Default: disable hardware interrupts!
	char filename2[256];
	bzero(filename2,sizeof(filename2)); //Clear the filename!
	strcpy(filename2,filename); //Set the filename to use!
	strcat(filename2,".hwint.txt"); //Use HW interrupts? Simple text file will do!
	useHWInterrupts = file_exists(filename2); //Use hardware interrupts when specified!
	dolog("debugger","RunROMVerify...");
	FILE *f;
	int memloc = 0;
	f = fopen(filename,"rb"); //First, load file!

	dolog("debugger","RUNROMVERIFY: initEMU...");

	CPU[activeCPU].halt &= ~0x12; //Make sure to stop the CPU again!
	unlock(LOCK_CPU);
	lock(LOCK_MAINTHREAD); //Lock the main thread(our other user)!
	initEMU(1); //Init EMU first, enable video!
	unlock(LOCK_MAINTHREAD); //Unlock us!
	lock(LOCK_CPU);
	dolog("debugger","RUNROMVERIFY: ready to go.");

	if (!hasmemory()) //No memory present?
	{
		dolog("ROM_log","Error: no memory loaded!");
		return 0; //Error!
	}

	memloc = 0; //Init location!

	word datastart = 0; //Start of data segment!
	//resetCPU(); //Make sure we're loading the ROM correctly (start at 0xF000:FFFF) Don't reset the CPU, as this is already done by initEMU!

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
		unlock(LOCK_CPU); //Finished with the CPU!
		doneEMU();
		dolog("ROM_log","Invalid file size!");
		return 1; //OK!
	}
	
	fclose(f); //Close the ROM!
	
	if (!BIOS_load_custom("",filename)) //Failed to load the BIOS ROM?
	{
		unlock(LOCK_CPU); //Finished with the CPU!
		doneEMU(); //Finish the emulator!
		dolog("ROM_log","Failed loading the verification ROM as a BIOS!");
		return 0; //Failed!
	}

	CPU[activeCPU].halt = 0; //Start without halt!
	dolog("ROM_log","Starting verification ROM emulator...");
	uint_32 erroraddr = 0xFFFFFFFF; //Error address (undefined)
	uint_32 lastaddr = 0xFFFFFFFF; //Last address causing the error!
	uint_32 erroraddr16 = 0x00000000; //16-bit segment:offset pair.
	BIOS_registerROM(); //Register the BIOS ROM!
	dolog("debugger","Starting debugging file %s",filename); //Log the file we're going to test!
	LOG_MMU_WRITES = debugger_logging(); //Enable logging!
	allow_debuggerstep = 1; //Allow stepping of the debugger!
	unlock(LOCK_CPU);
	resetCPU(); //Make sure we start correctly!
	lock(LOCK_CPU);
	CPU[activeCPU].registers->CS = 0xF000;
	CPU[activeCPU].registers->EIP = 0xFFF0; //Our reset vector instead for the test ROMs!
	CPU_flushPIQ(); //Clear the PIQ from any unused instructions!
	for (;!CPU[activeCPU].halt;) //Still running?
	{
		if (debugger_thread)
		{
			if (threadRunning(debugger_thread)) //Are we running the debugger?
			{
				unlock(LOCK_CPU);
				delay(0); //OK, but skipped!
				lock(LOCK_CPU);
				continue; //Continue execution until not running anymore!
			}
		}

		uint_32 curaddr = (CPU[activeCPU].registers->CS<<4)+CPU[activeCPU].registers->IP; //Calculate current memory address!
		if (curaddr<0xF0000) //Out of executable range?
		{
			erroraddr = curaddr; //Set error address!
			erroraddr16 = (CPU[activeCPU].registers->CS<<16)|CPU[activeCPU].registers->IP; //Set error address segment:offset!
			break; //Continue, but keep our warning!
		}
		lastaddr = curaddr; //Save the current address for reference of the error address!
		cpudebugger = needdebugger(); //Debugging?
		HWINT_saved = 0; //No HW interrupt by default!
		CPU_beforeexec(); //Everything before the execution!
		if (shuttingdown()) goto doshutdown;
		if (useHWInterrupts) //HW interrupts enabled for this ROM?
		{
			if (!CPU[activeCPU].trapped && CPU[activeCPU].registers) //Only check for hardware interrupts when not trapped!
			{
				if (FLAG_IF) //Interrupts available?
				{
					if (PICInterrupt()) //We have a hardware interrupt ready?
					{
						HWINT_nr = nextintr(); //Get the HW interrupt nr!
						HWINT_saved = 2; //We're executing a HW(PIC) interrupt!
						if (!((EMULATED_CPU <= CPU_80286) && REPPending)) //Not 80386+, REP pending and segment override?
						{
							CPU_8086REPPending(); //Process pending REPs normally as documented!
						}
						else //Execute the CPU bug!
						{
							CPU_8086REPPending(); //Process pending REPs normally as documented!
							CPU[activeCPU].registers->EIP = CPU_InterruptReturn; //Use the special interrupt return address to return to the last prefix instead of the start!
						}
						call_hard_inthandler(HWINT_nr); //get next interrupt from the i8259, if any!
					}
				}
			}
		}
		cpudebugger = needdebugger(); //Debugging?
		MMU_logging = debugger_logging(); //Are we logging?
		CPU_exec(); //Run CPU!
		debugger_step(); //Step debugger if needed!
		CB_handleCallbacks(); //Handle callbacks after CPU/debugger usage!
	} //Main debug CPU loop!
	LOG_MMU_WRITES = 0; //Disable logging!

doshutdown:
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
		unlock(LOCK_CPU); //Finished with the CPU!
		doneEMU(); //Clean up!
		dolog("ROM_log","Error: Failed opening result file!");
		return 0; //Result file doesn't exist!
	}


	dolog("ROM_log","Verifying output...");	
	
	memloc = 0; //Initialise memory location!
	verified = 1; //Default: OK!
	byte data; //Data to verify!
	byte last; //Last data read in memory!
	for (;!feof(f);) //Data left?
	{
		if (fread(&data, 1, sizeof(data), f) != sizeof(data)) break; //Read data to verify!
		last = MMU_rb(-1,datastart,memloc,0,0); //One byte to compare from memory!
		byte verified2;
		verified2 = (data==last); //Verify with memory!
		verified &= verified2; //Check for verified!
		if (!verified2) //Error in verification byte?
		{
			dolog("ROM_log","Error address: %08X, expected: %02X, in memory: %02X",memloc,data,last); //Give the verification point that went wrong!
			//Continue checking for listing all errors!
		}
		++memloc; //Increase the location!
	}
	fclose(f); //Close the file!
	//dolog("ROM_log","Finishing emulator...");	
	unlock(LOCK_CPU); //Finished with the CPU!
	lock(LOCK_MAINTHREAD); //Lock the main thread(our other user)!
	BIOS_free_custom(filename); //Free the custom BIOS ROM!
	unlock(LOCK_MAINTHREAD); //Unlock us!
	lock(LOCK_CPU);
	dolog("ROM_log","ROM Success: %i...",verified);
	if (!verified && erroraddr!=0xFFFFFFFF) //Error address specified?
	{
		dolog("ROM_log","Error address: %08X, Possible cause: %08X; Real mode address: %04X:%04X",erroraddr,lastaddr,((erroraddr16>>16)&0xFFFF),(erroraddr&0xFFFF)); //Log the error address!
	}
	return verified; //Verified?
}