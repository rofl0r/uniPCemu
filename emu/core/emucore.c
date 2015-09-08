#include "headers/types.h" //For global stuff etc!

#include "headers/mmu/mmu.h" //For MMU
#include "headers/cpu/cpu.h" //For CPU
#include "headers/debugger/debugger.h" //Debugger support!
#include "headers/hardware/vga.h" //For savestate support!

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
#include "headers/hardware/pcspeaker.h" //PC Speakers support!

#include "headers/hardware/vga_screen/vga_sequencer.h" //VGA sequencer for direct MAX speed dump!
#include "headers/hardware/vga_screen/vga_dacrenderer.h" //DAC support!

#include "headers/hardware/uart.h" //UART support!

#include "headers/emu/emu_vga.h" //VGA update support!

#include "headers/support/highrestimer.h" //High resolution timer!

#include "headers/hardware/ide.h" //IDE/ATA support!
#include "headers/hardware/pci.h" //PCI support!

#include "headers/hardware/sermouse.h" //Serial mouse support!

#include "headers/emu/gpu/gpu_text.h" //GPU text surface support!
#include "headers/bios/io.h" //I/O support!

//Allow GPU rendering (to show graphics)?
#define ALLOW_GRAPHICS 1
//To debug VGA at MAX speed?
#define DEBUG_VGA_SPEED 0
//To show the framerate?
#define DEBUG_FRAMERATE 1
//Debug any sound devices? (PC Speaker, Adlib, MPU(to be tested in software), SB16?)
#define DEBUG_SOUND 0
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

void EMU_setDiskBusy(byte disk, byte busy) //Are we busy?
{
	uint_32 busycolor;
	busycolor = (busy == 1) ? RGB(0x00, 0xFF, 0x00) : RGB(0xFF, 0x66, 0x00); //Busy color Read/Write!
	switch (disk) //What disk?
	{
	case FLOPPY0:
		GPU_text_locksurface(frameratesurface);
		GPU_textgotoxy(frameratesurface,GPU_TEXTSURFACE_WIDTH - 6, 1); //Goto second row!
		if (busy) //Busy?
		{
			GPU_textprintf(frameratesurface, busycolor, RGB(00,00,00), "A");
		}
		else
		{
			GPU_textprintf(frameratesurface, RGB(0x00,0x00,0x00), RGB(0x00,0x00,0x00), " ");
		}
		GPU_text_releasesurface(frameratesurface);
		break;
	case FLOPPY1:
		GPU_text_locksurface(frameratesurface);
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 5, 1); //Goto second row!
		if (busy) //Busy?
		{
			GPU_textprintf(frameratesurface, busycolor, RGB(00, 00, 00), "B");
		}
		else
		{
			GPU_textprintf(frameratesurface, RGB(0x00, 0x00, 0x00), RGB(0x00, 0x00, 0x00), " ");
		}
		GPU_text_releasesurface(frameratesurface);
		break;
	case HDD0:
		GPU_text_locksurface(frameratesurface);
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 4, 1); //Goto second row!
		if (busy) //Busy?
		{
			GPU_textprintf(frameratesurface, busycolor, RGB(00, 00, 00), "C");
		}
		else
		{
			GPU_textprintf(frameratesurface, RGB(0x00, 0x00, 0x00), RGB(0x00, 0x00, 0x00), " ");
		}
		GPU_text_releasesurface(frameratesurface);
		break;
	case HDD1:
		GPU_text_locksurface(frameratesurface);
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 3, 1); //Goto second row!
		if (busy) //Busy?
		{
			GPU_textprintf(frameratesurface, busycolor, RGB(00, 00, 00), "D");
		}
		else
		{
			GPU_textprintf(frameratesurface, RGB(0x00, 0x00, 0x00), RGB(0x00, 0x00, 0x00), " ");
		}
		GPU_text_releasesurface(frameratesurface);
		break;
	case CDROM0:
		GPU_text_locksurface(frameratesurface);
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 2, 1); //Goto second row!
		if (busy) //Busy?
		{
			GPU_textprintf(frameratesurface, busycolor, RGB(00, 00, 00), "E");
		}
		else
		{
			GPU_textprintf(frameratesurface, RGB(0x00, 0x00, 0x00), RGB(0x00, 0x00, 0x00), " ");
		}
		GPU_text_releasesurface(frameratesurface);
		break;
	case CDROM1:
		GPU_text_locksurface(frameratesurface);
		GPU_textgotoxy(frameratesurface, GPU_TEXTSURFACE_WIDTH - 1, 1); //Goto second row!
		if (busy) //Busy?
		{
			GPU_textprintf(frameratesurface, busycolor, RGB(00, 00, 00), "F");
		}
		else
		{
			GPU_textprintf(frameratesurface, RGB(0x00, 0x00, 0x00), RGB(0x00, 0x00, 0x00), " ");
		}
		GPU_text_releasesurface(frameratesurface);
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

void initEMU(int full) //Init!
{
	doneEMU(); //Make sure we're finished too!

	initTicksHolder(&CPU_timing); //Initialise the ticks holder!
	getuspassed(&CPU_timing); //Initialise our timing!

	psp_input_init(); //Make sure input is set up!

	initEMUmemory = freemem(); //Log free mem!
	
	debugrow("Loading basic BIOS I/O...");
	BIOS_LoadIO(0); //Load basic BIOS I/O, also VGA information, don't show checksum errors!

	debugrow("Initializing I/O port handling...");
	Ports_Init(); //Initialise I/O port support!
	
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
	
	debugrow("Initialising MPU...");
	if (!initMPU(&BIOS_Settings.SoundFont[0])) //Initialise our MPU! Use the selected soundfont!
	{
		//We've failed loading!
		memset(&BIOS_Settings.SoundFont, 0, sizeof(BIOS_Settings.SoundFont));
		forceBIOSSave(); //Save the new BIOS!
	}

	if (!DEBUG_SOUND) //Not sound only?
	{
	debugrow("Initializing PSP OSK...");
	psp_keyboard_init(); //Initialise the PSP's on-screen keyboard for the emulator!

	debugrow("Initializing video...");
	initVideo(DEBUG_FRAMERATE); //Reset video!

	debugrow("Initializing VGA...");	
	//First, VGA allocations for seperate systems!
	MainVGA = VGAalloc(0,1); //Allocate a main VGA, automatically by BIOS!
	debugrow("Activating main VGA engine...");
	setActiveVGA(MainVGA); //Initialise primary VGA using the BIOS settings, for the system itself!

	debugrow("Initializing 8259...");
	init8259(); //Initialise the 8259 (PIC)!

	debugrow("Starting video subsystem...");
	if (full) //Enable video?
	{
		if (!DEBUG_VGA_SPEED) //Not to debug speed only?
		{
			startVideo(); //Start the video functioning!
		}
	}
	
	debugrow("Initializing 8042...");
	BIOS_init8042(); //Init 8042 PS/2 controller!

	debugrow("Initialising keyboard...");
	BIOS_initKeyboard(); //Start up the keyboard!

	debugrow("Initialising mouse...");
	PS2_initMouse(BIOS_Settings.PS2Mouse); //Start up the mouse!
	}

	//Load all BIOS presets!
	debugrow("Initializing 8253...");
	init8253(); //Init Timer&PC Speaker!
	
	if (!DEBUG_SOUND) //Not sound only?
	{
	debugrow("Starting VGA...");
	startVGA(); //Start the current VGA!
	if (DEBUG_VGA_SPEED) //To debug the VGA MAX speed?
	{
		debugrow("Debugging Maximum VGA speed...");
		startTimers(1); //Start the timers!
		while (1)
		{
			VGA_Sequencer(getActiveVGA()); //Generate one line!
			delay(1); //Allow other threads!
			logVGASpeed(); //Log any VGA speeds!
		}
		sleep(); //Stop running: give the VGA maximum priority!
	}

	debugrow("Initialising MMU...");
	resetMMU(); //Initialise MMU (we need the BDA from now on!)!
	setupVGA(); //Set the VGA up for int10&CPU usage!

	EMU_update_VGA_Settings(); //Update the VGA Settings to it's default value!

	debugrow("Initializing CPU...");
	resetCPU(); //Initialise CPU!
	
	debugrow("Initialising CMOS...");
	initCMOS(); //Initialise the CMOS!
	
	debugrow("Initialising DMA Controllers...");
	initDMA(); //Initialise the DMA Controller!

	debugrow("Initialising UART...");
	initUART(); //Initialise the UART (COM ports)!
	
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
	} //Not debugging sound only!

	EMU_update_VGA_Settings(); //Update the VGA Settings to it's default value!

	//Finally: signal we're ready!
	emu_started = 1; //We've started!

	debugrow("EMU Ready to run.");
	if (DEBUG_SOUND) //Debugging sound only?
	{
		dosoundtest();
		termThreads(); //Terminate our thread for max priority, if possible!
	}
}

void doneEMU()
{
	if (emu_started) //Started?
	{
		debugrow("doneEMU: resetTimers");
		resetTimers(); //Stop the timers!
		if (!DEBUG_SOUND) //Not sound only?
		{
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
			debugrow("doneEMU: finish Adlib...");
		}
		debugrow("doneEMU: Finishing MPU...");
		doneMPU(); //Finish our MPU!
		debugrow("doneEMU: Finishing Adlib...");
		doneAdlib(); //Finish adlib!
		debugrow("doneEMU: Finishing PC Speaker...");
		doneSpeakers();
		if (!DEBUG_SOUND)
		{
			debugrow("doneEMU: finish Keyboard chip...");
			BIOS_doneKeyboard(); //Done with the keyboard!
			debugrow("doneEMU: finish Mouse chip...");
			BIOS_doneMouse(); //Done with the mouse!
			debugrow("doneEMU: finish 8042...");
			BIOS_done8042(); //Done with PS/2 communications!~
			debugrow("doneEMU: reset audio channels...");
		}
		resetchannels(); //Release audio!
		if (!DEBUG_SOUND)
		{
			debugrow("doneEMU: finish Video...");
			doneVideo(); //Cleanup screen buffers!
		}
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
		halt(0); //Shut down!
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

uint_64 last_timing = 0; //Last timing!

extern SDL_sem *IPS_Lock;
uint_64 instructioncounter; //Instruction counter!

void CPU_Speed_Unlimited()
{
	static uint_32 numopcodes = 0; //Delay counter!
	if (++numopcodes == 1000000)//Every 10000 opcodes(to allow for more timers/input to update)
	{
		numopcodes = 0; //Reset!
		delay(0); //Wait minimal time for other threads to process data!
	}
}

void CPU_Speed_Limited()
{
	//Delay some time to get accurate timing!
	last_timing += 3; //Increase last timing!
	for (;getuspassed_k(&CPU_timing) < last_timing;) delay(0); //Update to current time!
}

byte coreHandler()
{
	static Handler SpeedLimit[2] = {CPU_Speed_Unlimited,CPU_Speed_Limited}; //CPU speed settings!
	if ((romsize!=0) && (CPU[activeCPU].halt)) //Debug HLT?
	{
		MMU_dumpmemory("bootrom.dmp"); //Dump the memory to file!
		return 0; //Stop!
	}

	if (!CPU[activeCPU].registers) return 0; //Invalid registers!

	updateKeyboard(); //Tick the keyboard timer if needed!
	updateAdlib(); //Tick the adlib timer if needed!
	updatePIT0(); //Tick the PIT timer if needed!
	updateATA(); //Update the ATA timer!

	//CPU execution, needs to be before the debugger!
	interruptsaved = 0; //Reset PIC interrupt to not used!
	if (!CPU[activeCPU].halt) //Not halted?
	{
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
	}
	else if (CPU[activeCPU].registers->SFLAGS.IF && PICInterrupt()) //We have an interrupt? Clear Halt State!
	{
		CPU[activeCPU].halt = 0; //Interrupt->Resume from HLT
	}

	//Increase the instruction counter every instruction/HLT time!
	WaitSem(IPS_Lock); //Wait for the IPS!
	++instructioncounter; //Increase the instruction counter!
	PostSem(IPS_Lock); //Finished with the IPS!

	debugger_step(); //Step debugger if needed!

	CB_handleCallbacks(); //Handle callbacks after CPU/debugger usage!

	SpeedLimit[BIOS_Settings.CPUSpeed](); //Slowdown the CPU to the requested speed?

	if (psp_keypressed(BUTTON_SELECT)) //Run in-emulator BIOS menu and not gaming mode?
	{
		if (!is_gamingmode() && !Direct_Input) //Not gaming/direct input mode?
		{
			pauseEMU(); //Stop timers!
			if (runBIOS(0)) //Run the emulator BIOS!
			{
				reset = 1; //We're to reset!
			}
			resumeEMU(); //Resume!
			if (BIOS_Settings.CPUSpeed) //Needs slowdown here?
			{
				last_timing = getuspassed_k(&CPU_timing); //We start off at this point!
			}
		}
	}
	return 1; //OK!
}

//DoEmulator results:
//0:Shutdown
//1:Softreset
//2:Reset emu
int DoEmulator() //Run the emulator (starting with the BIOS always)!
{
	EMU_RUNNING = 1; //The emulator is running now!

	EMU_enablemouse(1); //Enable all mouse input packets!
	enableKeyboard(0); //Enable standard keyboard!
	
//Start normal emulation!
	lock(LOCK_CPU); //Start by locking the CPU: we're busy!
	for (;;)
	{
		if (!CPU[activeCPU].running || !hasmemory()) //Not running anymore or no memory present to use?
		{
			break; //Stop running!
		}
		
		if (shuttingdown() || reset)
		{
			debugrow("Reset/shutdown detected!");
			break;    //Shutdown or reset?
		}

		if (!coreHandler()) //Run the core CPU+related handler, gotten abort?
		{
			break; //Abort!
		}
	}
	unlock(LOCK_CPU); //We're finished with the CPU!

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
	for (i=0;i<NUMITEMS(EMU_TIMERS);i++) //Process all emulator timers!
	{
		useTimer(EMU_TIMERS[i],0); //Disable it, if there!
	}
}

void startEMUTimers()
{
	int i;
	for (i=0;i<NUMITEMS(EMU_TIMERS);i++) //Process all emulator timers!
	{
		useTimer(EMU_TIMERS[i],1); //Enable it, if there!
	}
}
