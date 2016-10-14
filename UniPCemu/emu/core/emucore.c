#include "headers/types.h" //For global stuff etc!
#include "headers/cpu/mmu.h" //For MMU
#include "headers/cpu/cpu.h" //For CPU
#include "headers/emu/debugger/debugger.h" //Debugger support!
#include "headers/hardware/vga/vga.h" //For savestate support!
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
#include "headers/mmu/mmuhandler.h" //MMU handler support!
#include "headers/bios/biosmenu.h" //For running the BIOS!

//All graphics now!
#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!
#include "headers/emu/gpu/gpu_renderer.h" //Renderer support!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate support!
#include "headers/emu/emucore.h" //Emulation core!
#include "headers/interrupts/interrupt19.h" //INT19 support!
#include "headers/hardware/softdebugger.h" //Software debugger and Port E9 Hack.
#include "headers/hardware/8237A.h" //DMA Controller!
#include "headers/hardware/midi/midi.h" //MIDI/MPU support!
#include "headers/bios/biosrom.h" //BIOS ROM support!
#include "headers/emu/threads.h" //Multithreading support!
#include "headers/hardware/vga/vga_renderer.h" //VGA renderer for direct MAX speed dump!
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
#include "headers/hardware/vga/vga_cga_mda.h" //CGA/MDA compatibility layer support!
#include "headers/support/dro.h" //DRO player playback support!
#include "headers/hardware/midi/midi.h" //MPU support!
#include "headers/hardware/dram.h" //DRAM support!
#include "headers/hardware/vga/svga/tseng.h" //Tseng ET3000/ET4000 SVGA card!
#include "headers/hardware/joystick.h" //Joystick support!
#include "headers/hardware/xtexpansionunit.h" //XT expansion unit!
#include "headers/cpu/cb_manager.h" //For handling callbacks!
#include "headers/hardware/gameblaster.h" //Game blaster support!
#include "headers/hardware/soundblaster.h" //Sound blaster support!

//Emulator single step address, when enabled.
byte doEMUsinglestep = 0; //CPU mode plus 1
uint_64 singlestepaddress = 0x00007C00; //The segment:offset address!

/*

It's the row:
0xF0001671 CALL DDS (AT BIOS)
0xF000E1CC Allow NMI interrupts

which is at the first row of the IBM AT POST3 function.
*/

//CPU default clock speeds (in Hz)!

//The clock speed of the 8086 (~14.31818MHz divided by 3)!
#define CPU808X_CLOCK (MHZ14/3.0f)
#define CPU808X_TURBO_CLOCK (MHZ14/3.0f)*2.1f

//80286 clock is set so that the DRAM refresh ends up with a count of F952h in CX.
#define CPU80286_CLOCK 7280500.0

//Timeout CPU time and instruction interval! 44100Hz or 1ms!
#define TIMEOUT_INTERVAL 10
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

double MHZ14tick = (1000000000/(double)MHZ14); //Time of a 14 MHZ tick!
double MHZ14_ticktiming = 0.0; //Timing of the 14MHz clock!

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
	if (DEBUG_EMU) dolog("emu",text); //Log it when enabled!
}

/*

Emulator initialisation and destruction!

*/

VGA_Type *MainVGA; //The main VGA chipset!

uint_32 initEMUmemory = 0;

TicksHolder CPU_timing; //CPU timing counter!

extern byte CPU_databussize; //0=16/32-bit bus! 1=8-bit bus when possible (8088/80188)!

extern byte allcleared;

extern char soundfontpath[256]; //The soundfont path!

byte useGameBlaster; //Using the Game blaster?
byte useAdlib; //Using the Adlib?
byte useLPTDAC; //Using the LPT DAC?
byte useSoundBlaster; //Using the Sound Blaster?

byte is_XT = 0; //Are we emulating an XT architecture?

void initEMU(int full) //Init!
{
	char soundfont[256];
	doneEMU(); //Make sure we're finished too!

	MHZ14tick = (1000000000/(double)MHZ14); //Initialize the 14 MHZ tick timing!
	MHZ14_ticktiming = 0.0; //Default to no time passed yet!

	allcleared = 0; //Not cleared anymore!

	MMU_resetHandlers(NULL); //Reset all memory handlers before starting!

	initTicksHolder(&CPU_timing); //Initialise the ticks holder!

	debugrow("Initializing user input...");
	psp_input_init(); //Make sure input is set up!

	initEMUmemory = freemem(); //Log free mem!
	
	debugrow("Loading basic BIOS I/O...");
	BIOS_LoadIO(0); //Load basic BIOS I/O, also VGA information, don't show checksum errors!

	//Check for memory requirements of the system!
	if ((BIOS_Settings.memory & 0xFFFF) && (EMULATED_CPU >= CPU_80286)) //IBM PC/AT has specific memory requirements? Needs to be 64K aligned!
	{
		BIOS_Settings.memory &= ~0xFFFF; //We're forcing a redetection of available memory, if it's needed! Else, just round down memory to the nearest compatible 64K memory!
		forceBIOSSave(); //Force-save the BIOS settings!
	}

	debugrow("Initializing I/O port handling...");
	Ports_Init(); //Initialise I/O port support!
	
	debugrow("Initialising PCI...");
	initPCI(); //Initialise PCI support!

	debugrow("Initializing timer service...");
	resetTimers(); //Reset all timers/disable them all!
	
	debugrow("Initialising audio subsystem...");
	resetchannels(); //Reset all channels!

	autoDetectArchitecture(); //Detect the architecture to use!

	debugrow("Initialising PC Speaker...");
	initSpeakers(BIOS_Settings.usePCSpeaker); //Initialise the speaker. Enable/disable sound according to the setting!

	debugrow("Initializing 8259...");
	init8259(); //Initialise the 8259 (PIC)!

	useAdlib = BIOS_Settings.useAdlib|BIOS_Settings.useSoundBlaster; //Adlib used?
	if (useAdlib)
	{
		debugrow("Initialising Adlib...");
		initAdlib(); //Initialise adlib!
	}

	useGameBlaster = BIOS_Settings.useGameBlaster; //Game blaster used (optional in the Sound Blaster)?
	if (useGameBlaster)
	{
		debugrow("Initialising Game Blaster...");
		initGameBlaster(0x220); //Initialise game blaster!
		GameBlaster_setVolume((float)BIOS_Settings.GameBlaster_Volume); //Set the sound source volume!
		setGameBlaster_SoundBlaster(BIOS_Settings.useSoundBlaster?2:0); //Fully Sound Blaster compatible?
	}

	debugrow("Initialising Parallel ports...");
	initParallelPorts(1); //Initialise the Parallel ports (LPT ports)!

	useLPTDAC = BIOS_Settings.useLPTDAC; //LPT DAC used?
	if (useLPTDAC)
	{
		debugrow("Initialising Disney Sound Source...");
		initSoundsource(); //Initialise Disney Sound Source!
		ssource_setVolume((float)BIOS_Settings.SoundSource_Volume); //Set the sound source volume!
	}

	debugrow("Initialising MPU...");
	if (strcmp(BIOS_Settings.SoundFont,"")!=0) //Gotten a soundfont?
	{
		memset(&soundfont,0,sizeof(soundfont)); //Init!
		strcpy(soundfont,soundfontpath); //The path to the soundfont!
		strcat(soundfont,"/");
		strcat(soundfont,BIOS_Settings.SoundFont); //The full path to the soundfont!
		if (!initMPU(&soundfont[0])) //Initialise our MPU! Use the selected soundfont!
		{
			//We've failed loading!
			memset(&BIOS_Settings.SoundFont, 0, sizeof(BIOS_Settings.SoundFont));
			forceBIOSSave(); //Save the new BIOS!
		}
	}

	debugrow("Initialising DMA Controllers...");
	initDMA(); //Initialise the DMA Controller!

	//Check if we're allowed to use full Sound Blaster emulation!
	useSoundBlaster = BIOS_Settings.useSoundBlaster; //Sound blaster used?
	if ((strcmp(BIOS_Settings.SoundFont,"")==0) && useSoundBlaster) //Sound Blaster without soundfont?
	{
		useSoundBlaster = 0; //No sound blaster to emulate! We can't run without a soundfont!
	}

	if (useSoundBlaster) //Sound Blaster used?
	{
		initSoundBlaster(0x220,useSoundBlaster-1); //Initialise sound blaster with the specified version!
	}
	else //Sound Blaster not used and allowed?
	{
		if (BIOS_Settings.useSoundBlaster) //Sound Blaster specified?
		{
			BIOS_Settings.useSoundBlaster = 0; //Don't allow Sound Blaster emulation!
			forceBIOSSave(); //Save the new BIOS!
		}
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

	uint_32 VRAMSizeBackup;
	VRAMSizeBackup = BIOS_Settings.VRAM_size; //Save the original VRAM size for extensions!

	MainVGA = VGAalloc(0,1,(BIOS_Settings.VGA_Mode==6)?1:(BIOS_Settings.VGA_Mode==7?2:0)); //Allocate a main VGA, automatically by BIOS!
	debugrow("Activating main VGA engine...");
	setActiveVGA(MainVGA); //Initialise primary VGA using the BIOS settings, for the system itself!
	VGA_initTimer(); //Initialise the VGA timer for usage!
	initCGA_MDA(); //Add CGA/MDA support to the VGA as an extension!
	if ((BIOS_Settings.VGA_Mode==6) || (BIOS_Settings.VGA_Mode==7)) SVGA_Setup_TsengET4K(VRAMSizeBackup); //Start the Super VGA card instead if enabled!

	debugrow("Starting video subsystem...");
	if (full) startVideo(); //Start the video functioning!
	
	debugrow("Initializing 8042...");
	BIOS_init8042(); //Init 8042 PS/2 controller!

	debugrow("Initialising keyboard...");
	BIOS_initKeyboard(); //Start up the keyboard!

	debugrow("Initialising mouse...");
	PS2_initMouse(BIOS_Settings.architecture>=ARCHITECTURE_PS2); //Start up the mouse!

	//Load all BIOS presets!
	debugrow("Initializing 8253...");
	init8253(); //Init Timer&PC Speaker!
	
	if (BIOS_Settings.architecture==ARCHITECTURE_XT) //XT architecture?
	{
		initEMS(2 * MBMEMORY); //2MB EMS memory!
	}

	debugrow("Initialising MMU...");
	resetMMU(); //Initialise MMU (we need the BDA from now on!)!

	EMU_update_VGA_Settings(); //Update the VGA Settings to it's default value!
	setupVGA(); //Set the VGA up for int10&CPU usage!

	//PPI after VGA because we're dependant on the CGA/MDA only mode!
	debugrow("Initialising PPI...");
	initPPI(BIOS_Settings.diagnosticsportoutput_breakpoint,BIOS_Settings.diagnosticsportoutput_timeout); //Start PPI with our breakpoint settings!

	debugrow("Initializing CPU...");
	CPU_databussize = BIOS_Settings.DataBusSize; //Apply the bus to use for our emulation!
	initCPU(); //Initialise CPU for emulation!
	
	debugrow("Initialising CMOS...");
	initCMOS(); //Initialise the CMOS!
	
	debugrow("Initializing DRAM refresh...");
	initDRAM(); //Initialise the DRAM Refresh!

	debugrow("Initialising UART...");
	initUART(1); //Initialise the UART (COM ports)!

	debugrow("Initialising serial mouse...");
	initSERMouse(BIOS_Settings.architecture<=ARCHITECTURE_AT); //Initilialise the serial mouse for XT and AT!

	debugrow("Initialising Floppy Disk Controller...");
	initFDC(); //Initialise the Floppy Disk Controller!

	debugrow("Initialising ATA...");
	initATA();

	debugrow("Initialising port E9 hack and emulator support functionality...");
	BIOS_initDebugger(); //Initialise the port E9 hack and emulator support functionality!

	debugrow("Initialising joystick...");
	joystickInit();

	if (is_XT) //XT?
	{
		initXTexpansionunit(); //Initialize the expansion unit!
	}

	//Initialize the normal debugger!
	debugrow("Initialising debugger...");
	initDebugger(); //Initialize the debugger if needed!
	
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

	//Finally: signal we're ready!
	emu_started = 1; //We've started!

	updateSpeedLimit(); //Update the speed limit!

	debugrow("Starting VGA...");
	startVGA(); //Start the current VGA!

	debugrow("EMU Ready to run.");
}

void doneEMU()
{
	if (emu_started) //Started?
	{
		debugrow("doneEMU: resetTimers");
		resetTimers(); //Stop the timers!
		debugrow("doneEMU: Finishing joystick...");
		joystickDone();
		debugrow("doneEMU: Finishing port E9 hack and emulator support functionality...");
		BIOS_doneDebugger(); //Finish the port E9 hack and emulator support functionality!
		debugrow("doneEMU: Finishing serial mouse...");
		doneSERMouse(); //Finish the serial mouse, if present!
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
		debugrow("doneEMU: Finishing Sound Blaster...");
		doneSoundBlaster(); //Finish Sound Blaster!
		debugrow("doneEMU: Finish DMA Controller...");
		doneDMA(); //Initialise the DMA Controller!
		debugrow("doneEMU: Finishing Game Blaster...");
		doneGameBlaster(); //Finish Game Blaster!
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
		debugrow("doneEMU: Finishing Video...");
		doneVideo(); //Cleanup screen buffers!
		debugrow("doneEMU: Finishing user input...");
		psp_input_done(); //Make sure input is set up!
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
		stopEMUTimers(); //Stop the timers!
		EMU_RUNNING = 3; //We've stopped, but still active (paused)!
	}
}

void resumeEMU(byte startinput)
{
	if (emu_started) //Started?
	{
		startEMUTimers(); //Start the timers!
		if (startinput) EMU_startInput(); //Start the input when allowed to!
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
	#ifdef IS_PSP
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
extern byte interruptsaved; //Primary interrupt saved?

byte HWINT_nr = 0, HWINT_saved = 0; //HW interrupt saved?

extern byte REPPending; //REP pending reset?

extern byte MMU_logging; //Are we logging from the MMU?

extern byte Direct_Input; //Are we in direct input mode?

double last_timing = 0.0, timeemulated = 0.0; //Last timing!

double CPU_speed_cycle = 0.0; //808X signal cycles by default!
byte DosboxClock = 1; //We're executing using the Dosbox clock cycles?

ThreadParams_p BIOSMenuThread; //BIOS pause menu thread!
extern ThreadParams_p debugger_thread; //Debugger menu thread!

void BIOSMenuResumeEMU()
{
	getnspassed(&CPU_timing); //We start off at this point with no time running! We start counting the last timing from now!
	updateSpeedLimit(); //Update the speed limit!
}

void BIOSMenuExecution()
{
	pauseEMU(); //Stop timers!
	//Special Android support!
	#ifdef ANDROID
	lock(LOCK_INPUT);
	toggleDirectInput(1);
	unlock(LOCK_INPUT);
	#endif
	if (runBIOS(0)) //Run the emulator BIOS!
	{
		lock(LOCK_CPU); //We're updating the CPU!
		reset = 1; //We're to reset!
		unlock(LOCK_CPU);
	}
	#ifdef ANDROID
	lock(LOCK_INPUT);
	toggleDirectInput(1);
	unlock(LOCK_INPUT);
	#endif
	resumeEMU(1); //Resume!
	//Update CPU speed!
	lock(LOCK_CPU); //We're updating the CPU!
	BIOSMenuResumeEMU(); //Resume the emulator from the BIOS menu thread!
	unlock(LOCK_CPU);
}

extern byte TurboMode; //Are we in Turbo mode?

void setDosboxCycles(byte useDosboxClock, uint_32 cycles)
{
	DosboxClock = useDosboxClock; //Use Dosbox-clock style?
	if (DosboxClock) //Dosbox clock cycles?
	{
		CPU_speed_cycle = 1000000.0 / (float)cycles; //Cycles per ms is used!
	}
	else //Actual clock cycles?
	{
		CPU_speed_cycle = 1000000000.0 / (float)cycles; //8086 CPU cycle length in us, since no other CPUs are known yet!	
	}
}

void updateSpeedLimit()
{
	DosboxClock = 1; //We're executing using Dosbox clocks!
	if (BIOS_Settings.CPUSpeed) //Gotten speed cycles set?
	{
		setDosboxCycles(1,BIOS_Settings.CPUSpeed); //Dosbox-style cycles!
		if (TurboMode && BIOS_Settings.TurboCPUSpeed && BIOS_Settings.useTurboSpeed) //Turbo enabled and specified?
		{
			setDosboxCycles(1, BIOS_Settings.TurboCPUSpeed); //Dosbox-style Turbo cycles!
		}
	}
	else //CPU speed cycles not set? No Dosbox cycles here normally (until implemented)!
	{
		DosboxClock = 0; //We're executing using actual clocks!
		CPU_speed_cycle = 1000000000.0/CPU808X_CLOCK; //8086 CPU cycle length in us, since no other CPUs are known yet!	
		if (EMULATED_CPU >= CPU_80286) //Faster clocks?
		{
			CPU_speed_cycle = 1000000000.0 / CPU80286_CLOCK; //80286 ~6MHz for DMA speed check compatibility!
		}
		if (TurboMode && BIOS_Settings.useTurboSpeed) //Turbo mode enabled?
		{
			if (BIOS_Settings.TurboCPUSpeed) //Turbo speed specified?
			{
				setDosboxCycles(1, BIOS_Settings.TurboCPUSpeed); //Dosbox-style Turbo cycles!
			}
			else
			{
				CPU_speed_cycle = 1000000000.0 / CPU808X_TURBO_CLOCK; //8086 CPU cycle length in us, since no other CPUs are known yet! Use the 10MHz Turbo version by default!	
			}
		}
	}
}

extern uint_32 CPU_InterruptReturn, CPU_exec_EIP; //Interrupt return address!

extern byte allcleared;

double currenttiming = 0.0; //Current timing spent to emulate!

extern byte Settings_request; //Settings requested to be executed?

OPTINLINE byte coreHandler()
{
	uint_32 MHZ14passed; //14 MHZ clock passed?
	byte BIOSMenuAllowed = 1; //Are we allowed to open the BIOS menu?
	//CPU execution, needs to be before the debugger!
	currenttiming += getnspassed(&CPU_timing); //Check for any time that has passed to emulate!
	uint_64 currentCPUtime = (uint_64)currenttiming; //Current CPU time to update to!
	uint_64 timeoutCPUtime = currentCPUtime+TIMEOUT_TIME; //We're timed out this far in the future (1ms)!

	double instructiontime,timeexecuted=0.0f; //How much time did the instruction last?
	byte timeout = TIMEOUT_INTERVAL; //Check every 10 instructions for timeout!
	for (;last_timing<currentCPUtime;) //CPU cycle loop for as many cycles as needed to get up-to-date!
	{
		if (debugger_thread)
		{
			if (threadRunning(debugger_thread)) //Are we running the debugger?
			{
				instructiontime = currentCPUtime - last_timing; //The instruction time is the total time passed!
				updateAudio(instructiontime); //Discard the time passed!
				timeexecuted += instructiontime; //Increase CPU executed time executed this block!
				last_timing += instructiontime; //Increase the last timepoint!
				goto skipCPUtiming; //OK, but skipped!
			}
		}
		if (BIOSMenuThread)
		{
			if (threadRunning(BIOSMenuThread)) //Are we running the BIOS menu and not permanently halted? Block our execution!
			{
				if ((CPU[activeCPU].halt&2)==0) //Are we allowed to be halted entirely?
				{
					instructiontime = currentCPUtime - last_timing; //The instruction time is the total time passed!
					updateAudio(instructiontime); //Discard the time passed!
					timeexecuted += instructiontime; //Increase CPU executed time executed this block!
					last_timing += instructiontime; //Increase the last timepoint!
					goto skipCPUtiming; //OK, but skipped!
				}
				BIOSMenuAllowed = 0; //We're running the BIOS menu! Don't open it again!
			}
		}
		if ((CPU[activeCPU].halt&2)==0) //Are we running normally(not partly ran without CPU from the BIOS menu)?
		{
			BIOSMenuThread = NULL; //We don't run the BIOS menu anymore!
		}

		if (allcleared) return 0; //Abort: invalid buffer!

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

			if (CPU[activeCPU].halt & 0xC) //CGA wait state is active?
			{
				if ((CPU[activeCPU].halt&0xC) == 8) //Are we to resume execution now?
				{
					CPU[activeCPU].halt &= ~0xC; //We're resuming execution!
					goto resumeFromHLT; //We're resuming from HLT state!
				}
				goto skipHaltRestart; //Count cycles normally!
			}
			else if (CPU[activeCPU].registers->SFLAGS.IF && PICInterrupt() && ((CPU[activeCPU].halt&2)==0)) //We have an interrupt? Clear Halt State when allowed to!
			{
				CPU[activeCPU].halt = 0; //Interrupt->Resume from HLT
				goto resumeFromHLT; //We're resuming from HLT state!
			}
			else
			{
				skipHaltRestart:
				if (DosboxClock) //Execute using Dosbox clocks?
				{
					CPU[activeCPU].cycles = 1; //HLT takes 1 cycle for now!
				}
				else //Execute using actual CPU clocks!
				{
					CPU[activeCPU].cycles = 1; //HLT takes 1 cycle for now, since it's unknown!
				}
			}
			if (CPU[activeCPU].halt==1) //Normal halt?
			{
				//Increase the instruction counter every instruction/HLT time!
				cpudebugger = needdebugger(); //Debugging information required? Refresh in case of external activation!
				if (cpudebugger) //Debugging?
				{
					debugger_beforeCPU(); //Make sure the debugger is prepared when needed!
					debugger_setcommand("<HLT>"); //We're a HLT state, so give the HLT command!
				}
				debugger_step(); //Step debugger if needed, even during HLT state!
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
			if ((!CPU[activeCPU].trapped) && CPU[activeCPU].registers && CPU[activeCPU].allowInterrupts && (CPU[activeCPU].permanentreset==0)) //Only check for hardware interrupts when not trapped and allowed to execute interrupts(not permanently reset)!
			{
				if (CPU[activeCPU].registers->SFLAGS.IF) //Interrupts available?
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

		//Tick 14MHz master clock, for basic hardware using it!
		MHZ14_ticktiming += instructiontime; //Add time to the 14MHz master clock!
		if (MHZ14_ticktiming>=MHZ14tick) //To tick some 14MHz clocks?
		{
			MHZ14passed = (uint_32)(MHZ14_ticktiming/MHZ14tick); //Tick as many as possible!
			MHZ14_ticktiming -= MHZ14tick*(float)MHZ14passed; //Rest the time passed!
		}
		else
		{
			MHZ14passed = 0; //No time has passed on the 14MHz Master clock!
		}

		tickPIT(instructiontime,MHZ14passed); //Tick the PIT as much as we need to keep us in sync!
		updateDMA(MHZ14passed); //Update the DMA timer!
		updateMouse(instructiontime); //Tick the mouse timer if needed!
		stepDROPlayer(instructiontime); //DRO player playback, if any!
		updatePS2Keyboard(instructiontime); //Tick the PS/2 keyboard timer, if needed!
		updatePS2Mouse(instructiontime); //Tick the PS/2 mouse timer, if needed!
		update8042(instructiontime); //Tick the PS/2 mouse timer, if needed!
		updateCMOS(instructiontime); //Tick the CMOS, if needed!
		if (useAdlib) updateAdlib(MHZ14passed); //Tick the adlib timer if needed!
		if (useGameBlaster) updateGameBlaster(MHZ14passed); //Tick the Game Blaster timer if needed!
		if (useSoundBlaster) updateSoundBlaster(instructiontime,MHZ14passed); //Tick the Sound Blaster timer if needed!
		//updateATA(instructiontime); //Update the ATA timer! This is currently not used, so ignore it!
		tickParallel(instructiontime); //Update the Parallel timer!
		updateUART(instructiontime); //Update the UART timer!
		if (useLPTDAC) tickssourcecovox(instructiontime); //Update the Sound Source / Covox Speech Thing if needed!
		if ((CPU[activeCPU].halt&2)==0) updateVGA(instructiontime); //Update the VGA timer when running!
		updateJoystick(instructiontime); //Update the Joystick!
		updateAudio(instructiontime); //Update the general audio processing!
		if (--timeout==0) //Timed out?
		{
			timeout = TIMEOUT_INTERVAL; //Reset the timeout to check the next time!
			currenttiming += getnspassed(&CPU_timing); //Check for passed time!
			if (currenttiming >= timeoutCPUtime) break; //Timeout? We're not fast enough to run at full speed!
		}
	} //CPU cycle loop!

	skipCPUtiming: //Audio emulation only?
	//Slowdown to requested speed if needed!
	currenttiming += getnspassed(&CPU_timing); //Add real time!
	for (;currenttiming < last_timing;) //Not enough time spent on instructions?
	{
		currenttiming += getnspassed(&CPU_timing); //Add to the time to wait!
		delay(0); //Update to current time every instruction according to cycles passed!
	}

	float temp;
	temp = (float)MAX(last_timing,currenttiming); //Save for substraction(time executed in real time)!
	last_timing -= temp; //Keep the CPU timing within limits!
	currenttiming -= temp; //Keep the current timing within limits!

	timeemulated += timeexecuted; //Add timing for the CPU percentage to update!

	updateKeyboard(timeexecuted); //Tick the keyboard timer if needed!

	//Check for BIOS menu!
	if ((psp_keypressed(BUTTON_SELECT) || Settings_request) && (BIOSMenuThread==NULL)) //Run in-emulator BIOS menu requested?
	{
		if ((!is_gamingmode() && !Direct_Input && BIOSMenuAllowed) || Settings_request) //Not gaming/direct input mode and allowed to open it(not already started)?
		{
			lock(LOCK_INPUT);
			Settings_request = 0; //We're handling the request!
			unlock(LOCK_INPUT);
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

	if (++shutdowncounter >= 50) //Check for shutdown every X opcodes?
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
				"AdlibAttackDecay",
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

extern Controller8042_t Controller8042;
extern byte SystemControlPortA;
void EMU_onCPUReset()
{
	SystemControlPortA &= ~2; //Clear A20 here!
	if (is_XT==0) //AT CPU?
	{
		Controller8042.outputport |= 2; //Set A20 here!
	}
	else
	{
		Controller8042.outputport &= ~2; //Clear A20 here!
	}
	Controller8042.outputport |= 1; //Prevent us from deadlocking(calling this function over and over fininitely within itself)!
	refresh_outputport(); //Refresh from 8042!
	checkPPIA20(); //Refresh from Fast A20!
}
