#include "headers/types.h" //For global stuff etc!

#include "headers/mmu/mmu.h" //For MMU
#include "headers/cpu/cpu.h" //For CPU
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/hardware/vga/vga.h" //For savestate support!

#include "headers/emu/state.h" //SaveState holder!
#include "headers/hardware/pic.h" //Interrupt controller support!
#include "headers/emu/timers.h" //Timers!
#include "headers/hardware/8042.h" //For void BIOS_init8042()
#include "headers/emu/sound.h" //PC speaker support!

#include "headers/hardware/8253.h" //82C54 support!

#include "headers/hardware/ports.h" //Port support!
#include "headers/support/log.h" //Log support!
#include "headers/support/zalloc.h" //For final freezall functionality!

#include "headers/hardware/adlib.h" //Adlib!
#include "headers/hardware/ps2_keyboard.h" //PS/2 keyboard support!
#include "headers/hardware/ps2_mouse.h" //PS/2 mouse support!
#include "headers/hardware/cmos.h" //CMOS support!

#include "headers/emu/emu_bios_sound.h" //BIOS sound support!

#include "headers/hardware/sermouse.h" //Serial mouse support!

#include "headers/mmu/mmuhandler.h" //MMu handler support!

//All graphics now!

#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!
#include "headers/emu/gpu/gpu_renderer.h" //Renderer support!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate support!
#include "headers/emu/emucore.h" //Emulation core!
#include "headers/emu/soundtest.h" //Sound test utility!

#include "headers/interrupts/interrupt19.h" //INT19 support!

#include "headers/hardware/softdebugger.h" //Software debugger and Port E9 Hack.

#include "headers/hardware/8237A.h" //DMA Controller!
#include "headers/hardware/midi/midi.h" //MIDI/MPU support!

#include "headers/bios/biosrom.h" //BIOS ROM support!
#include "headers/emu/threads.h" //Multithreading support!

#include "headers/hardware/vga/vga_sequencer.h" //VGA sequencer for direct MAX speed dump!
#include "headers/hardware/vga/vga_dacrenderer.h" //DAC support!

#include "headers/hardware/uart.h" //UART support!

#include "headers/emu/emu_vga.h" //VGA update support!

#include "headers/support/highrestimer.h" //High resolution timer!

#include "headers/hardware/ide.h" //IDE/ATA support!
#include "headers/hardware/pci.h" //PCI support!

#include "headers/hardware/sermouse.h" //Serial mouse support!

#include "headers/emu/gpu/gpu_text.h" //GPU text surface support!
#include "headers/basicio/io.h" //I/O support!

#include "headers/hardware/floppy.h" //Floppy disk controller!

#include "headers/hardware/ppi.h" //PPI support!

#include "headers/hardware/ems.h" //EMS support!

#include "headers/hardware/ssource.h" //Disney Sound Source support!

#include "headers/hardware/parallel.h" //Parallel port support!

//CPU default clock speeds (in Hz)!

//The clock speed of the 8086 (14.31818MHz divided by 3)!
#define CPU808X_CLOCK (14318180.0f/3.0f)

//Timeout CPU time! 44100Hz or 1ms!
#define TIMEOUT_TIME 1000000

//Allow GPU rendering (to show graphics)?
#define ALLOW_GRAPHICS 1
//To show the framerate?
#define DEBUG_FRAMERATE 1
//All external variables!
extern byte EMU_RUNNING; //Emulator running? 0=Not running, 1=Running, Active CPU, 2=Running, Inactive CPU (BIOS etc.)
extern byte reset; //To fully reset emu?
extern uint_32 romsize; //For checking if we're running a ROM!
extern byte cpudebugger; //To debug the CPU?
extern PIC i8259; //PIC processor!

int emu_started = 0; //Emulator started (initEMU called)?

//To debug init/doneemu?
#define DEBUG_EMU 0

//Report a memory leak has occurred?
//#define REPORT_MEMORYLEAK

/*

debugging for us!

*/

extern GPU_TEXTSURFACE *frameratesurface;

byte currentbusy[6] = {0,0,0,0,0,0}; //Current busy status; default none!

void EMU_drawBusy(byte disk) //Draw busy on-screen!
{
	char text[2] = {' ','\0'};
	text[0] = 'A'; //Start with A and increase!
	text[0] += disk; //Increasing disk letter!
	uint_32 busycolor;
	busycolor = (currentbusy[disk] == 1) ? RGB(0x00, 0xFF, 0x00) : RGB(0xFF, 0x66, 0x00); //Busy color Read/Write!
	GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 6 + disk, 1); //Goto second row column!
	if (currentbusy[disk]) //Busy?
	{
		GPU_textprintf(frameratesurface, busycolor, RGB(00, 00, 00), text);
	}
	else
	{
		GPU_textprintf(frameratesurface, RGB(0x00, 0x00, 0x00), RGB(0x00, 0x00, 0x00), " ");
	}
}

void EMU_setDiskBusy(byte disk, byte busy) //Are we busy?
{
	switch (disk) //What disk?
	{
	case FLOPPY0:
		if (currentbusy[0]!=busy) //Changed?
		{
			GPU_text_locksurface(frameratesurface);
			currentbusy[0] = busy; //New busy status!
			EMU_drawBusy(0); //Draw the current busy!
			GPU_text_releasesurface(frameratesurface);
		}
		break;
	case FLOPPY1:
		if (currentbusy[1] != busy) //Changed?
		{
			GPU_text_locksurface(frameratesurface);
			currentbusy[1] = busy; //New busy status!
			EMU_drawBusy(1); //Draw the current busy!
			GPU_text_releasesurface(frameratesurface);
		}
		break;
	case HDD0:
		if (currentbusy[2] != busy) //Changed?
		{
			GPU_text_locksurface(frameratesurface);
			currentbusy[2] = busy; //New busy status!
			EMU_drawBusy(2); //Draw the current busy!
			GPU_text_releasesurface(frameratesurface);
		}
		break;
	case HDD1:
		if (currentbusy[3] != busy) //Changed?
		{
			GPU_text_locksurface(frameratesurface);
			currentbusy[3] = busy; //New busy status!
			EMU_drawBusy(3); //Draw the current busy!
			GPU_text_releasesurface(frameratesurface);
		}
		break;
	case CDROM0:
		if (currentbusy[4] != busy) //Changed?
		{
			GPU_text_locksurface(frameratesurface);
			currentbusy[4] = busy; //New busy status!
			EMU_drawBusy(4); //Draw the current busy!
			GPU_text_releasesurface(frameratesurface);
		}
		break;
	case CDROM1:
		if (currentbusy[5] != busy) //Changed?
		{
			GPU_text_locksurface(frameratesurface);
			currentbusy[5] = busy; //New busy status!
			EMU_drawBusy(5); //Draw the current busy!
			GPU_text_releasesurface(frameratesurface);
		}
		break;
	default:
		break;
	}
}

void debugrow(char *text)
{
	if (DEBUG_EMU) dolog("zalloc",text); //Log it when enabled!
}

/*

Emulator initialisation and destruction!

*/

VGA_Type *MainVGA; //The main VGA chipset!

uint_32 initEMUmemory = 0;

TicksHolder CPU_timing; //CPU timing counter!

void updateSpeedLimit(); //Prototype!

extern byte CPU_databussize; //0=16/32-bit bus! 1=8-bit bus when possible (8088/80188)!

void initEMU(int full) //Init!
{
	doneEMU(); //Make sure we're finished too!

	MMU_resetHandlers(NULL); //Reset all memory handlers before starting!

	initTicksHolder(&CPU_timing); //Initialise the ticks holder!
	getuspassed(&CPU_timing); //Initialise our timing!

	psp_input_init(); //Make sure input is set up!

	initEMUmemory = freemem(); //Log free mem!
	
	debugrow("Loading basic BIOS I/O...");
	BIOS_LoadIO(0); //Load basic BIOS I/O, also VGA information, don't show checksum errors!

	debugrow("Initializing I/O port handling...");
	Ports_Init(); //Initialise I/O port support!
	
	debugrow("Initialising PPI...");
	initPPI();

	debugrow("Initialising PCI...");
	initPCI(); //Initialise PCI support!

	debugrow("Initializing timer service...");
	resetTimers(); //Reset all timers/disable them all!
	
	debugrow("Initialising audio subsystem...");
	resetchannels(); //Reset all channels!
	
	debugrow("Initialising PC Speaker...");
	initSpeakers(); //Initialise the speaker(s)!

	debugrow("Initialising Adlib...");
	initAdlib(); //Initialise adlib!

	debugrow("Initialising Parallel ports...");
	initParallelPorts(1); //Initialise the Parallel ports (LPT ports)!

	debugrow("Initialising Disney Sound Source...");
	initSoundsource(); //Initialise Disney Sound Source!
	ssource_setVolume(BIOS_Settings.SoundSource_Volume); //Set the sound source volume!

	debugrow("Initialising MPU...");
	if (!initMPU(&BIOS_Settings.SoundFont[0])) //Initialise our MPU! Use the selected soundfont!
	{
		//We've failed loading!
		memset(&BIOS_Settings.SoundFont, 0, sizeof(BIOS_Settings.SoundFont));
		forceBIOSSave(); //Save the new BIOS!
	}

	debugrow("Initializing PSP OSK...");
	psp_keyboard_init(); //Initialise the PSP's on-screen keyboard for the emulator!

	debugrow("Initializing video...");
	initVideo(DEBUG_FRAMERATE); //Reset video!
	GPU_AspectRatio(BIOS_Settings.aspectratio); //Keep the aspect ratio is cleared by default by the GPU initialisation?
	setGPUFramerate(BIOS_Settings.ShowFramerate); //Show the framerate?

	memset(&currentbusy,0,sizeof(currentbusy)); //Initialise busy status!

	debugrow("Initializing VGA...");	
	//First, VGA allocations for seperate systems!

	VGA_initTimer(); //Initialise the VGA timer for usage!

	MainVGA = VGAalloc(0,1); //Allocate a main VGA, automatically by BIOS!
	debugrow("Activating main VGA engine...");
	setActiveVGA(MainVGA); //Initialise primary VGA using the BIOS settings, for the system itself!

	debugrow("Initializing 8259...");
	init8259(); //Initialise the 8259 (PIC)!

	debugrow("Starting video subsystem...");
	if (full) startVideo(); //Start the video functioning!
	
	debugrow("Initializing 8042...");
	BIOS_init8042(); //Init 8042 PS/2 controller!

	debugrow("Initialising keyboard...");
	BIOS_initKeyboard(); //Start up the keyboard!

	debugrow("Initialising mouse...");
	PS2_initMouse(BIOS_Settings.PS2Mouse); //Start up the mouse!

	//Load all BIOS presets!
	debugrow("Initializing 8253...");
	init8253(); //Init Timer&PC Speaker!
	
	debugrow("Starting VGA...");
	startVGA(); //Start the current VGA!

	if (EMULATED_CPU <= CPU_80186) //-186 CPU?
	{
		initEMS(4 * MBMEMORY); //4MB EMS memory!
	}

	debugrow("Initialising MMU...");
	resetMMU(); //Initialise MMU (we need the BDA from now on!)!
	setupVGA(); //Set the VGA up for int10&CPU usage!

	EMU_update_VGA_Settings(); //Update the VGA Settings to it's default value!

	debugrow("Initializing CPU...");
	CPU_databussize = BIOS_Settings.DataBusSize; //Apply the bus to use for our emulation!
	resetCPU(); //Initialise CPU!
	
	debugrow("Initialising CMOS...");
	initCMOS(); //Initialise the CMOS!
	
	debugrow("Initialising DMA Controllers...");
	initDMA(); //Initialise the DMA Controller!

	debugrow("Initialising UART...");
	initUART(1); //Initialise the UART (COM ports)!

	debugrow("Initialising serial mouse...");
	initSERMouse(!BIOS_Settings.PS2Mouse); //Initilialise the serial mouse!

	debugrow("Initialising Floppy Disk Controller...");
	initFDC(); //Initialise the Floppy Disk Controller!

	debugrow("Initialising ATA...");
	initATA();

	debugrow("Initialising port E9 hack and emulator support functionality...");
	BIOS_initDebugger(); //Initialise the port E9 hack and emulator support functionality!
	
	if (full) //Full start?
	{
		debugrow("Starting timers...");
		startTimers(0); //Start the timers!
		debugrow("Loading system BIOS ROM...");
		BIOS_registerROM(); //Register the ROMs for usage!
		BIOS_load_systemROM(); //Load custom ROM from emulator itself, we don't know about any system ROM!
		clearCBHandlers(); //Reset all callbacks!
		BIOS_initStart(); //Get us ready to load our own BIOS boot sequence!
	}
	else
	{
		debugrow("No timers enabled.");
	}

	EMU_update_VGA_Settings(); //Update the VGA Settings to it's default value!

	//Finally: signal we're ready!
	emu_started = 1; //We've started!

	updateSpeedLimit(); //Update the speed limit!

	debugrow("EMU Ready to run.");
}

void doneEMU()
{
	if (emu_started) //Started?
	{
		debugrow("doneEMU: resetTimers");
		resetTimers(); //Stop the timers!
		debugrow("doneEMU: Finishing port E9 hack and emulator support functionality...");
		BIOS_doneDebugger(); //Finish the port E9 hack and emulator support functionality!
		debugrow("doneEMU: Finish DMA Controller...");
		doneDMA(); //Initialise the DMA Controller!
		debugrow("doneEMU: Saving CMOS...");
		saveCMOS(); //Save the CMOS!
		debugrow("doneEMU: stopVideo...");
		stopVideo(); //Video can't process without MMU!
		debugrow("doneEMU: Finish keyboard PSP...");
		psp_keyboard_done(); //We're done with the keyboard!
		debugrow("doneEMU: finish active VGA...");
		doneVGA(&MainVGA); //We're done with the VGA!
		debugrow("doneEMU: finish CPU.");
		doneCPU(); //Finish the CPU!
		debugrow("doneEMU: finish MMU...");
		doneMMU(); //Release memory!
		debugrow("doneEMU: finish EMS if enabled...");
		doneEMS(); //Finish EMS!
		debugrow("doneEMU: Finishing MPU...");
		doneMPU(); //Finish our MPU!
		debugrow("doneEMU: Finishing Disney Sound Source...");
		doneSoundsource(); //Finish Disney Sound Source!
		debugrow("doneEMU: Finishing Adlib...");
		doneAdlib(); //Finish adlib!
		debugrow("doneEMU: Finishing PC Speaker...");
		doneSpeakers();
		debugrow("doneEMU: finish Keyboard chip...");
		BIOS_doneKeyboard(); //Done with the keyboard!
		debugrow("doneEMU: finish Mouse chip...");
		BIOS_doneMouse(); //Done with the mouse!
		debugrow("doneEMU: finish 8042...");
		BIOS_done8042(); //Done with PS/2 communications!~
		debugrow("doneEMU: reset audio channels...");
		resetchannels(); //Release audio!
		debugrow("doneEMU: finish Video...");
		doneVideo(); //Cleanup screen buffers!
		debugrow("doneEMU: EMU finished!");
		emu_started = 0; //Not started anymore!
		EMU_RUNNING = 0; //We aren't running anymore!
		#ifdef REPORT_MEMORYLEAK
		if (freemem()!=initEMUmemory && initEMUmemory) //Difference?
		{
			logpointers("doneEMU: warning: memory difference before and after allocating EMU services!"); //Log all pointers!
		}
		#endif
	}
	
}

void EMU_stopInput()
{
	save_keyboard_status(); //Save keyboard status to memory!
	disableKeyboard(); //Disable it!
	EMU_enablemouse(0); //Disable all mouse input packets!
}

void EMU_startInput()
{
	load_keyboard_status(); //Load keyboard status from memory!
	enableKeyboard(0); //Enable the keyboard, don't buffer!
	EMU_enablemouse(1); //Enable all mouse input packets!
}

void pauseEMU()
{
	if (emu_started) //Started?
	{
		EMU_stopInput(); //Stop all input!
		EMU_SaveStatus(""); //Save status (temp)
		stopEMUTimers(); //Stop the timers!
		EMU_RUNNING = 3; //We've stopped, but still active (paused)!
	}
}

void resumeEMU()
{
	if (emu_started) //Started?
	{
		EMU_LoadStatus(""); //Load status (temp)
		startEMUTimers(); //Start the timers!
		EMU_startInput(); //Start the input!
		EMU_RUNNING = 1; //We've restarted!
		cleanKeyboard(); //Clean the keyboard timer!
		cleanMouse(); //Clean the mouse timer!
		cleanAdlib(); //Clean the adlib timer!
		cleanPIT(); //Clean the PIT timers!
		cleanATA(); //Update the ATA timer!
		cleanDMA(); //Update the DMA timer!
	}
}

void initEMUreset() //Simple reset emulator!
{
	debugrow("initEMUreset!");
	debugrow("immediatelyafter");
	#ifdef __psp__
		pspDebugScreenClear(); //Clear the debug screen!
	#endif
	EMU_RUNNING = 0; //Emulator isn't running anymore!

	reset = 0; //Not resetting!
//Shutdown check as fast as we can!
	if (shuttingdown()) //Shut down?
	{
		debugrow("shutdown!");
		doneEMU(); //Clean up if needed!
		EMU_Shutdown(0); //Done shutting down!
		quitemu(0); //Shut down!
	}

	debugrow("initemu!");
	initEMU(ALLOW_GRAPHICS); //Initialise the emulator fully!
	debugrow("Ready to run!"); //Ready to run!
}

/* coreHandler: The core emulation handler (running CPU and external hardware required to run it.) */

extern byte singlestep; //Enable EMU-driven single step!
byte doEMUsinglestep = 0; //CPU mode plus 1
uint_64 singlestepaddress = 0x00007C51; //The segment:offset address!

extern byte interruptsaved; //Primary interrupt saved?

byte HWINT_nr = 0, HWINT_saved = 0; //HW interrupt saved?

extern byte REPPending; //REP pending reset?

extern byte MMU_logging; //Are we logging from the MMU?

extern byte Direct_Input; //Are we in direct input mode?

double last_timing = 0.0, last_timing_start = 0.0; //Last timing!

double CPU_speed_cycle = 1000000000.0f/CPU808X_CLOCK; //808X signal cycles by default!
byte DosboxClock = 1; //We're executing using the Dosbox clock cycles?

ThreadParams_p BIOSMenuThread; //BIOS pause menu thread!
extern ThreadParams_p debugger_thread; //Debugger menu thread!

void BIOSMenuExecution()
{
	pauseEMU(); //Stop timers!
	if (runBIOS(0)) //Run the emulator BIOS!
	{
		reset = 1; //We're to reset!
	}
	resumeEMU(); //Resume!
	//Update CPU speed!
	lock(LOCK_CPU); //We're updating the CPU!
	last_timing = last_timing_start = (double)getnspassed_k(&CPU_timing); //We start off at this point with no time running! We start counting the last timing from now!
	updateSpeedLimit(); //Update the speed limit!
	unlock(LOCK_CPU);
}

void updateSpeedLimit()
{
	DosboxClock = 1; //We're executing using Dosbox clocks!
	if (BIOS_Settings.CPUSpeed) //Gotten speed cycles set?
	{
		if (DosboxClock) //Dosbox clock cycles?
		{
			CPU_speed_cycle = 1000000.0f/(float)BIOS_Settings.CPUSpeed; //Cycles per ms is used!
		}
		else //Actual clock cycles?
		{
			CPU_speed_cycle = 1000000000.0f / (float)BIOS_Settings.CPUSpeed; //8086 CPU cycle length in us, since no other CPUs are known yet!	
		}
	}
	else //CPU speed cycles not set? No Dosbox cycles here normally (until implemented)!
	{
		DosboxClock = 0; //We're executing using actual clocks!
		CPU_speed_cycle = 1000000000.0f/CPU808X_CLOCK; //8086 CPU cycle length in us, since no other CPUs are known yet!	
	}
}

OPTINLINE byte coreHandler()
{
	//CPU execution, needs to be before the debugger!
	uint_64 currentCPUtime = getnspassed_k(&CPU_timing); //Current CPU time to update to!
	uint_64 timeoutCPUtime = currentCPUtime+TIMEOUT_TIME; //We're timed out this far in the future (1ms)!

	double instructiontime,timeexecuted=0.0f; //How much time did the instruction last?
	for (;last_timing<currentCPUtime;) //CPU cycle loop for as many cycles as needed to get up-to-date!
	{
		if (debugger_thread)
		{
			if (threadRunning(debugger_thread, "debugger")) //Are we running the debugger?
			{
				return 1; //OK, but skipped!
			}
		}
		if (BIOSMenuThread)
		{
			if (threadRunning(BIOSMenuThread, "BIOSMenu")) //Are we running the BIOS menu?
			{
				return 1; //OK, but skipped!
			}
		}
		BIOSMenuThread = NULL; //We don't run the BIOS menu!

		interruptsaved = 0; //Reset PIC interrupt to not used!
		if (!CPU[activeCPU].registers) //We need registers at this point, but have none to use?
		{
			return 0; //Invalid registers: abort, since we're invalid!
		}
		if (CPU[activeCPU].halt) //Halted?
		{
			if (romsize) //Debug HLT?
			{
				MMU_dumpmemory("bootrom.dmp"); //Dump the memory to file!
				return 0; //Stop execution!
			}

			if (CPU[activeCPU].registers->SFLAGS.IF && PICInterrupt()) //We have an interrupt? Clear Halt State!
			{
				CPU[activeCPU].halt = 0; //Interrupt->Resume from HLT
				goto resumeFromHLT; //We're resuming from HLT state!
			}
			if (DosboxClock) //Execute using Dosbox clocks?
			{
				CPU[activeCPU].cycles = 1; //HLT takes 1 cycle for now!
			}
			else //Execute using actual CPU clocks!
			{
				CPU[activeCPU].cycles = 1; //HLT takes 1 cycle for now, since it's unknown!
			}
		}
		else //We're not halted? Execute the CPU routines!
		{
			resumeFromHLT:
			if (CPU[activeCPU].registers && doEMUsinglestep) //Single step enabled?
			{
				if (getcpumode() == (doEMUsinglestep - 1)) //Are we the selected CPU mode?
				{
					switch (getcpumode()) //What CPU mode are we to debug?
					{
					case CPU_MODE_REAL: //Real mode?
						singlestep |= ((CPU[activeCPU].registers->CS == (singlestepaddress >> 16)) && (CPU[activeCPU].registers->IP == (singlestepaddress & 0xFFFF))); //Single step enabled?
						break;
					case CPU_MODE_PROTECTED: //Protected mode?
					case CPU_MODE_8086: //Virtual 8086 mode?
						singlestep |= ((CPU[activeCPU].registers->CS == singlestepaddress >> 32) && (CPU[activeCPU].registers->EIP == (singlestepaddress & 0xFFFFFFFF))); //Single step enabled?
						break;
					default: //Invalid mode?
						break;
					}
				}
			}

			HWINT_saved = 0; //No HW interrupt by default!
			CPU_beforeexec(); //Everything before the execution!
			if (!CPU[activeCPU].trapped && CPU[activeCPU].registers) //Only check for hardware interrupts when not trapped!
			{
				if (CPU[activeCPU].registers->SFLAGS.IF) //Interrupts available?
				{
					if (PICInterrupt()) //We have a hardware interrupt ready?
					{
						HWINT_nr = nextintr(); //Get the HW interrupt nr!
						HWINT_saved = 2; //We're executing a HW(PIC) interrupt!
						if (!((EMULATED_CPU == CPU_8086) && (CPU_segmentOverridden(activeCPU)) && REPPending)) //Not 8086, REP pending and segment override?
						{
							CPU_8086REPPending(); //Process pending REPs!
						}
						else
						{
							REPPending = 0; //Clear the REP pending flag: this makes the bug in the 8086 not repeat anymore during interrupts in this case!
						}
						call_hard_inthandler(HWINT_nr); //get next interrupt from the i8259, if any!
					}
				}
			}
			cpudebugger = needdebugger(); //Debugging information required? Refresh in case of external activation!
			MMU_logging = debugger_logging(); //Are we logging?
			CPU_exec(); //Run CPU!

			//Increase the instruction counter every instruction/HLT time!
			debugger_step(); //Step debugger if needed!

			CB_handleCallbacks(); //Handle callbacks after CPU/debugger usage!
		}

		//Update current timing with calculated cycles we've executed!
		instructiontime = CPU[activeCPU].cycles*CPU_speed_cycle; //Increase timing with the instruction time!
		last_timing += instructiontime; //Increase CPU time executed!
		timeexecuted += instructiontime; //Increase CPU executed time executed this block!
		tickPIT(instructiontime); //Tick the PIT as much as we need to keep us in sync!
		updateMouse(instructiontime); //Tick the mouse timer if needed!
		updateAdlib(instructiontime); //Tick the adlib timer if needed!
		updateATA(instructiontime); //Update the ATA timer!
		updateDMA(instructiontime); //Update the DMA timer!
		tickParallel(instructiontime); //Update the Parallel timer!
		tickssourcecovox(instructiontime); //Update the Sound Source / Covox Speech Thing!
		updateVGA(instructiontime); //Update the VGA timer!
		if (getnspassed_k(&CPU_timing) >= timeoutCPUtime) break; //Timeout? We're not fast enough to run at full speed!
	} //CPU cycle loop!

	//Slowdown to requested speed if needed!
	for (;getnspassed_k(&CPU_timing) < last_timing;) delay(0); //Update to current time every instruction according to cycles passed!

	updateKeyboard(timeexecuted); //Tick the keyboard timer if needed!

	//Check for BIOS menu!
	if (psp_keypressed(BUTTON_SELECT)) //Run in-emulator BIOS menu and not gaming mode?
	{
		if (!is_gamingmode() && !Direct_Input) //Not gaming/direct input mode?
		{
			BIOSMenuThread = startThread(&BIOSMenuExecution,"BIOSMenu",NULL); //Start the BIOS menu thread!
			delay(0); //Wait a bit for the thread to start up!
		}
	}
	return 1; //OK!
}

word shutdowncounter = 0;

//DoEmulator results:
//-1: Execute next instruction!
//0:Shutdown
//1:Softreset
//2:Reset emu
int DoEmulator() //Run the emulator (starting with the BIOS always)!
{
	EMU_enablemouse(1); //Enable all mouse input packets!

//Start normal emulation!
	if (!CPU[activeCPU].running || !hasmemory()) //Not running anymore or no memory present to use?
	{
		goto skipcpu; //Stop running!
	}
		
	if (reset)
	{
		doshutdown:
		debugrow("Reset/shutdown detected!");
		goto skipcpu; //Shutdown or reset?
	}

	if (++shutdowncounter >= 1000) //Check for shutdown every X opcodes?
	{
		shutdowncounter = 0; //Reset counter!
		if (shuttingdown())
		{
			goto doshutdown; //Execute the shutdown procedure!
		}
	}

	if (!coreHandler()) //Run the core CPU+related handler, gotten abort?
	{
		goto skipcpu; //Abort!
	}
	goto runcpu;

skipcpu: //Finish the CPU loop!

	EMU_RUNNING = 0; //We're not running anymore!
	
	if (shuttingdown()) //Shut down?
	{
		debugrow("Shutdown requested");
		return 0; //Shut down!
	}

	if (reset) //To soft-reset?
	{
		debugrow("Reset requested!");
		return 1; //Full reset emu!
	}

	if (!hasmemory()) //Forced termination?
	{
		debugrow("No memory (anymore)! Reset requested!");
		return 1; //Full reset emu!
	}

	debugrow("Reset by default!");
	return 1; //Reset emu!

	runcpu: //Keep running the CPU?
	return -1; //Keep running!
}

//All emulated timers for the user.
char EMU_TIMERS[][256] = {
				"AddrPrint", //When debugging ROMs!
				"RTC", //Real-time clock!
				"PSP Mouse", //PS/2 mouse input!
				"VGA_ScanLine", //VGA rendering!
				"AdlibAttackDecay",
				//"Keyboard PSP Type",
				//"Keyboard PSP Swap", //Keyboard timers aren't EMU only: the admin can change these!
				"PSP Mouse",
				"DMA tick",
				"Framerate"
				}; //All emulator (used when running the emulator) timers, which aren't used outside the emulator itself!

void stopEMUTimers()
{
	int i;
	for (i=0;i<(int)NUMITEMS(EMU_TIMERS);i++) //Process all emulator timers!
	{
		useTimer(EMU_TIMERS[i],0); //Disable it, if there!
	}
}

void startEMUTimers()
{
	int i;
	for (i=0;i<(int)NUMITEMS(EMU_TIMERS);i++) //Process all emulator timers!
	{
		useTimer(EMU_TIMERS[i],1); //Enable it, if there!
	}
}
