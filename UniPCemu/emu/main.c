#include "headers/types.h" //For global stuff etc!
#include "exception/exception.h" //For the exception handler!
#include "headers/emu/threads.h" //Threads!
#include "headers/emu/gpu/gpu_sdl.h" //SDL support!
#include "headers/support/zalloc.h" //For final freezall functionality!
#include "headers/emu/sound.h" //PC speaker support!

//Various stuff we use!
#include "headers/emu/emu_main.h" //Emulation core is required to run the CPU!
#include "headers/emu/emu_misc.h" //Misc. stuff!

#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/support/log.h" //Logging support!

#include "headers/emu/emucore.h" //Emulator core support for checking for memory leaks!
#include "headers/emu/timers.h" //Timer support!

#include "headers/fopen64.h" //fopen64 support!

#include "headers/support/locks.h" //Lock support!

#include "headers/hardware/vga/vga.h" //VGA support for locks!
#include "headers/support/highrestimer.h" //High resolution timer support!

#include "headers/cpu/mmu.h" //MMU support!

#include "headers/hardware/8253.h" //PIT support!

#include "headers/bios/bios.h" //Basic BIOS support!

#include "headers/mmu/mmuhandler.h" //hasmemory support!

#ifdef IS_PSP
#include <psppower.h> //PSP power support for clock speed!
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER); //Make sure we're user mode!
PSP_HEAP_SIZE_MAX(); //Free maximum for us: need this for the memory allocation (m/zalloc)!
#endif

//Debug zalloc allocations?
//#define DEBUG_ZALLOC

//Delete all logs on boot?
#define DELETE_LOGS_ONBOOT 1
//Delete all bitmaps captured on boot?
#define DELETE_BMP_ONBOOT 0

//Find emulator memory leaks during allocation/deallocation when defined?
//#define FIND_EMU_MEMORY_LEAKS

extern byte active_screen; //Active screen: 0=bottom, 1=Top, 2=Left/Right, 3=Right/Left!

//To debug VRAM writes?
#define DEBUG_VRAM_WRITES 0

//Automatically sleep on main thread close?
#define SLEEP_ON_MAIN_CLOSE 0

extern double last_timing, timeemulated; //Our timing variables!
extern TicksHolder CPU_timing; //Our timing holder!

byte EMU_IsShuttingDown = 0; //Shut down (default: NO)?

void EMU_Shutdown(byte execshutdown)
{
	lock(LOCK_SHUTDOWN); //Lock the CPU: we're running!
	EMU_IsShuttingDown = execshutdown; //Call shutdown or not!
	unlock(LOCK_SHUTDOWN); //Finished with the CPU: we're running!
}

byte shuttingdown() //Shutting down?
{
	lock(LOCK_SHUTDOWN); //Lock the CPU: we're running!
	if (EMU_IsShuttingDown)
	{
		unlock(LOCK_SHUTDOWN); //Finished with the CPU: we're running!
		return 1; //Shutting down!
	}
	unlock(LOCK_SHUTDOWN); //Finished with the CPU: we're running!
	return 0; //Not shutting down (anymore)!
}

/*

BASIC Exit Callbacks

*/

#ifdef IS_PSP
/* Exit callback */
int exit_callback(int arg1, int arg2, void *common)
{
	EMU_Shutdown(1); //Call for a shut down!
	int counter = 0; //Counter for timeout!
	for(;;) //Loop!
	{
		if (counter>=SHUTDOWN_TIMEOUT) //Waited more than timeout's seconds (emulator might not be responding?)?
		{
			break; //Timeout! Force shut down!
		}
		if (shuttingdown()) //Shut down?
		{
			break; //We've shut down!
		}
		else
		{
			delay(100000); //Wait a bit!
			counter += 100000; //For timeout!
		}
	}
	termThreads(); //Terminate all threads now!	

	finishEMU(); //Finish the emulator!
	freezall(); //Release all still allocated data when possible!
	
	if (SDL_WasInit(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_JOYSTICK)) //Video and/or audio and joystick loaded?
	{
		SDL_Quit(); //Quit SDL, releasing everything still left!
	}
	quitemu(0); //The emu has shut down!
	return 0; //Never arriving here!
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
	int cbid;

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);

	sceKernelSleepThreadCB();

	return 0;
}
#endif

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks()
{
	atexit(SDL_Quit); //Basic SDL safety!
#ifdef IS_PSP
	int thid = 0;

	thid = sceKernelCreateThread("UniPCemu_ExitThread", CallbackThread, EXIT_PRIORITY, 0xFA0, 0, 0); //Create thread at highest priority!
	if(thid >= 0)
	{
		sceKernelStartThread(thid, 0, 0);
	}

	return thid;
#endif
	return 0;
}

/*

Main emulation routine

*/

extern byte use_profiler; //To determine if the profiler is used!

double clockspeed; //Current clock speed, for affecting timers!

OPTINLINE double getCurrentClockSpeed()
{
	#ifdef __psp
		return scePowerGetCpuClockFrequencyFloat(); //Current clock speed!
	#else
		return 222.0f; //Not used yet, just assume 222Hz!
	#endif
}

extern byte EMU_RUNNING; //Are we running?

TicksHolder CPUUpdate;

uint_64 CPU_time = 0; //Total CPU time before delay!

void updateInputMain() //Frequency 1000Hz!
{
	SDL_Event event;
	if (SDL_PollEvent(&event)) //Gotten an event to process?
	{
		do //Gotten events to handle?
		{
			//Handle an event!
			updateInput(&event); //Update input status when needed!
		}
		while (SDL_PollEvent(&event)); //Keep polling while available!
	}
}

extern byte allcleared;
extern char logpath[256]; //Log path!
extern char capturepath[256]; //Capture path!
extern byte RDP = 0;

int main(int argc, char * argv[])
{
	int argn;
	char *argch;
	char *testparam;
	char nosoundparam[] = "nosound";
	char RDPparam[] = "rdp";
	byte usesoundmode = 1;
	uint_32 SDLsubsystemflags = 0; //Our default SDL subsystem flags of used functionality!
	int emu_status;

//Basic PSP stuff!
	SetupCallbacks();
	#ifdef IS_PSP
		scePowerSetClockFrequency(333, 333, 166); //Start high-speed CPU!
	#endif
	clockspeed = getCurrentClockSpeed(); //Save the current clock frequency for reference!

	#ifdef IS_PSP
	if (FILE_EXISTS("exception.prx")) //Enable exceptions?
	{
		initExceptionHandler(); //Start the exception handler!
	}
	#endif

	if (SDL_Init(0) < 0) //Error initialising SDL defaults?
	{
		SDL_Quit(); //Nothing to be done!
		exit(1); //Just to be sure
	}

	RDP = 0; //Default: normal!

	if (argc) //Gotten parameters?
	{
		for (argn=0;argn<argc;++argn) //Process all arguments!
		{
			argch = &argv[argn][0]; //First character of the parameter!
			if (*argch) //Specified?
			{
				argch = &argv[argn][0]; //First character of the parameter!
				testparam = &nosoundparam[0]; //Our parameter to check for!
				for (;*argch!='\0';) //Parse the string!
				{
					if ((char)tolower((int)*argch)!=*testparam) //Not matched?
					{
						goto nomatch;
					}
					if (*testparam=='\0') //No match? We're too long!
					{
						goto nomatch;
					}
					++argch;
					++testparam;
				}
			nomatch:
				if ((*argch==*testparam) && (*argch=='\0')) //End of string? Full match!
				{
					usesoundmode = 0; //Disable audio: we're disabled by the parameter!
				}
argch = &argv[argn][0]; //First character of the parameter!
				testparam = &RDPparam[0]; //Our parameter to check for!
				for (;*argch!='\0';) //Parse the string!
				{
					if ((char)tolower((int)*argch)!=*testparam) //Not matched?
					{
						goto nomatch;
					}
					if (*testparam=='\0') //No match? We're too long!
					{
						goto nomatch;
					}
					++argch;
					++testparam;
				}
			nomatch:
				if ((*argch==*testparam) && (*argch=='\0')) //End of string? Full match!
				{
					RDP = 1; //Enable RDP: we're enabled by the parameter!
				}
			}
		}
	}

	SDLsubsystemflags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK; //Our default subsystem flags!
	if (usesoundmode) //Using sound output?
	{
		SDLsubsystemflags |= SDL_INIT_AUDIO; //Use audio output!
	}

	initLocks(); //Initialise all locks before anything: we have the highest priority!

	//First, allocate all locks needed!
	getLock(LOCK_VGA);
	getLock(LOCK_GPU);
	getLock(LOCK_CPU);
	getLock(LOCK_VIDEO);
	getLock(LOCK_TIMERS);
	getLock(LOCK_INPUT);
	getLock(LOCK_SHUTDOWN);
	getLock(LOCK_FRAMERATE);
	getLock(LOCK_MAINTHREAD);
	
	BIOS_DetectStorage(); //Detect all storage devices and BIOS Settings file needed to run!

	initHighresTimer(); //Global init of the high resoltion timer!
	initTicksHolder(&CPUUpdate); //Initialise the Video Update timer!

	initlog(); //Initialise the logging system!

	//Normal operations!
	resetTimers(); //Make sure all timers are ready!
	
	#ifdef DEBUG_ZALLOC
	//Verify zalloc functionality?
	{
		//First ensure freemem is valid!
		uint_32 f1,f2;
		f1 = freemem(); //Get free memory!
		f2 = freemem(); //Second check!
		if (f1!=f2) //Memory changed on the second check?
		{
			dolog("zalloc_debug","Multiple freemem fail!");
			quitemu(0); //Quit
		}

		uint_32 f;
		f = freemem(); //Detect free memory final!
		
		int *p; //Pointer to int #1!
		int *p2; //Pointer to int #2!
		
		p = (int *)zalloc(sizeof(*p),"zalloc_debug_int",NULL);
		freez((void **)&p,sizeof(*p),"zalloc_debug_int"); //Release int #1!
		
		if (freemem()!=f) //Different free memory?
		{
			dolog("zalloc_debug","Allocation-deallocation failed.");
		}
		p = (int *)zalloc(sizeof(*p),"debug_int",NULL);
		p2 = (int *)zalloc(sizeof(*p),"debug_int_2",NULL);
		freez((void **)&p2,sizeof(*p),"debug_int_2"); //Release int #2!
		freez((void **)&p,sizeof(*p),"debug_int"); //Release int #1!
		
		if (freemem()!=f) //Different free memory?
		{
			dolog("zalloc_debug","Multiple deallocation failed.");
		}
		
		p = (int *)zalloc(sizeof(*p),"debug_int",NULL);
		p2 = (int *)zalloc(sizeof(*p),"debug_int_2",NULL);
		freez((void **)&p,sizeof(*p),"debug_int"); //Release int #1!
		freez((void **)&p2,sizeof(*p),"debug_int_2"); //Release int #2!
		
		if (freemem()!=f) //Different free memory?
		{
			dolog("zalloc_debug","Multiple deallocation (shuffled) failed.");
		}
		
		dolog("zalloc_debug","All checks passed. Free memory: %i bytes Total memory: %i bytes",freemem(),f1);
		quitemu(0); //Quit!
	}
	#endif
	
	if (DELETE_LOGS_ONBOOT)
	{
		delete_file(logpath,"*.log"); //Delete any logs still there!
		delete_file(logpath,"*.txt"); //Delete any logs still there!
	}
	if (DELETE_BMP_ONBOOT) delete_file(capturepath,"*.bmp"); //Delete any bitmaps still there!
	
	#ifdef IS_PSP
		if (FILE_EXISTS("logs/profiler.txt")) //Enable profiler: doesn't work in UniPCemu?
		{
			// Clear the existing profile regs
			pspDebugProfilerClear();
			// Enable profiling
			pspDebugProfilerEnable();
			use_profiler = 1; //Use the profiler!	
		}
	#endif

	if (SDL_InitSubSystem(SDLsubsystemflags)<0) //Error initialising video,audio&joystick?
	{
		raiseError("SDL Init error: %s",SDL_GetError()); //Raise an error!
		sleep(); //Wait forever!
	}

	initThreads(); //Initialise&reset thread subsystem!
	initVideoLayer(); //We're for allocating the main video layer, only deallocated using SDL_Quit (when quitting the application)!

	debugrow("Initialising main video service...");
	initVideoMain(); //All main video!
	debugrow("Initialising main audio service...");
	initAudio(); //Initialise main audio!

	resetmain: //Main reset!

	startTimers(1); //Start core timing!
	startTimers(0); //Disable normal timing!

//First, support for I/O on the PSP!

	#ifdef FIND_EMU_MEMORY_LEAKS
	//Find memory leaks?
		uint_32 freememstart;
		freememstart = freemem(); //Freemem at the start!
		logpointers("initEMU(simple test)..."); //Log pointers at the start!
		dolog("zalloc","");
		initEMU(0); //Start the EMU partially, not running video!
		logpointers("doneEMU..."); //Log pointers at work!
		doneEMU(); //Finish the EMU!
		
		logpointers("find_emu_memory_leaks"); //Log pointers after finishing!
		dolog("zalloc","Memory overflow at the end: %i bytes too much deallocated.",freemem()-freememstart); //Should be the ammount of data still allocated!
		termThreads(); //Terminate all running threads still running!

		//Extended test
		freememstart = freemem(); //Freemem at the start!
		logpointers("initEMU (extended test)..."); //Log pointers at the start!
		initEMU(1); //Start the EMU fully, running without CPU!
		delay(10000000); //Wait 10 seconds to allow the full emulator to run some (without CPU)!
		logpointers("doneEMU..."); //Log pointers at work!
		doneEMU(); //Finish the EMU!
		
		logpointers("find_emu_memory_leaks2"); //Log pointers after finishing!
		dolog("zalloc","Memory overflow at the end: %i bytes too much deallocated.",freemem()-freememstart); //Should be the ammount of data still allocated!
		termThreads(); //Terminate all running threads still running!

		debugrow("Terminating main audio service...");		
		doneAudio(); //Finish audio processing!
		debugrow("Terminating main video service...");		
		doneVideoMain(); //Finish video!
		quitemu(0); //Exit software!
		sleep(); //Wait forever if needed!
	#endif

	
	//Start of the visible part!

	initEMUreset(); //Reset initialisation!

	if (!hasmemory()) //No MMU?
	{
		dolog("BIOS", "EMU_BIOSLoader: we have no memory!");
		BIOS_LoadIO(1); //Load basic BIOS I/O (disks), don't show checksum errors!
		autoDetectMemorySize(0); //Check&Save memory size if needed!
		EMU_Shutdown(0); //No shutdown!
		goto skipcpu; //Reset!
	}

	//New SDL way!
	/* Check for events */
	getnspassed(&CPUUpdate);
	lock(LOCK_CPU); //Lock the CPU: we're running!
	lock(LOCK_MAINTHREAD); //Lock the main thread(us)!
	getnspassed(&CPU_timing); //Make sure we start at zero time!
	last_timing = 0.0; //Nothing spent yet!
	timeemulated = 0.0; //Nothing has been emulated yet!
	for (;;) //Still running?
	{
		updateInputMain(); //Update input!
		CPU_time += (uint_64)getuspassed(&CPUUpdate); //Update the CPU time passed!
		if (CPU_time>=10000) //Allow other threads to lock the CPU requirements once in a while!
		{
			CPU_time %= 10000; //Rest!
			unlock(LOCK_CPU); //Unlock the CPU: we're not running anymore!
			unlock(LOCK_MAINTHREAD); //Lock the main thread(us)!
			delay(0); //Wait minimum amount of time!
			lock(LOCK_MAINTHREAD); //Lock the main thread(us)!
			lock(LOCK_CPU); //Lock the CPU: we're running!
		}
		CPU_updateVideo(); //Update the video if needed from the CPU!
		GPU_tickVideo(); //Tick the video display to keep it up-to-date!
		//Now, run the CPU!
		emu_status = DoEmulator(); //Run the emulator!
		switch (emu_status) //What to do next?
		{
		case -1: //Continue running?
			emu_status = 0; //Continue running!
			break;
		case 0: //Shutdown
			debugrow("Shutdown...");
			EMU_Shutdown(1); //Execute shutdown!
		case 1: //Full reset emu
			debugrow("Reset..."); //Not supported yet!
		default: //Unknown status?
			debugrow("Invalid EMU return code OR full reset requested!");
			emu_status = 1; //Shut down our thread, returning to the main processor!
		}
		if (emu_status) break; //Stop running the CPU?
	}

	unlock(LOCK_CPU); //Unlock the CPU: we're not running anymore!
	doneEMU(); //Finish up the emulator, if still required!
	unlock(LOCK_MAINTHREAD); //Unlock us!
skipcpu: //No CPU to execute?
	stopTimers(1); //Stop all timers still running!

	doneEMU(); //Finish up the emulator, if still required!
	termThreads(); //Terminate all still running threads!

	if (shuttingdown()) //Shutdown requested or SDL termination requested?
	{
		debugrow("Terminating main audio service...");
		doneAudio(); //Finish audio processing!
		debugrow("Terminating main video service...");
		doneVideoMain(); //Finish video!
		exit(0); //Quit using SDL, terminating the pspsurface!
		return 0; //Finish to be safe!
	}

	//Prepare us for a full software/emu reset
	goto resetmain; //Reset?
	return 0; //Just here for, ehm.... nothing!
}
