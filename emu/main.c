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

#ifdef __psp__
#include <pspkernel.h>
PSP_MODULE_INFO("x86EMU", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER); //Make sure we're user mode!
PSP_HEAP_SIZE_MAX(); //Free maximum for us: need this for the memory allocation (m/zalloc)!
#endif

//Debug zalloc allocations?
#define DEBUG_ZALLOC 0

//Delete all logs on boot?
#define DELETE_LOGS_ONBOOT 1
//Delete all bitmaps on boot?
#define DELETE_BMP_ONBOOT 1

//Find emulator memory leaks during allocation/deallocation?
#define FIND_EMU_MEMORY_LEAKS 0

extern byte active_screen; //Active screen: 0=bottom, 1=Top, 2=Left/Right, 3=Right/Left!

//To debug VRAM writes?
#define DEBUG_VRAM_WRITES 0

//Automatically sleep on main thread close?
#define SLEEP_ON_MAIN_CLOSE 0

byte EMU_IsShuttingDown = 0; //Shut down (default: NO)?

void EMU_Shutdown(byte execshutdown)
{
	EMU_IsShuttingDown = execshutdown; //Call shutdown or not!
}

byte shuttingdown() //Shutting down?
{
	if (EMU_IsShuttingDown)
	{
		return 1; //Shutting down!
	}
	return 0; //Not shutting down (anymore)!
}

/*

BASIC Exit Callbacks

*/

#ifdef __psp__
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
	halt(); //The emu has shut down!
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
#ifdef __psp__
	int thid = 0;

	thid = sceKernelCreateThread("X86EMU_ExitThread", CallbackThread, EXIT_PRIORITY, 0xFA0, 0, 0); //Create thread at highest priority!
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

byte running = 0; //Are we running the main thread?

void updateInputMain() //Frequency 1000Hz!
{
	SDL_Event event;
	for (;SDL_PollEvent(&event);) //Gotten events to handle?
	{
		//Handle an event!
		updateInput(&event); //Update input status when needed!
		if (event.type == SDL_QUIT) //Quitting requested?
		{
			running = 0; //Terminate our app!
		}
	}

	keyboard_type_handler(); //Type handler at 1000Hz when EMU isn't running!
}

int main(int argc, char * argv[])
{
//Basic PSP stuff!
	SetupCallbacks();
	#ifdef __psp__
		scePowerSetClockFrequency(333, 333, 166); //Start high-speed CPU!
	#endif
	clockspeed = getCurrentClockSpeed(); //Save the current clock frequency for reference!

	if (FILE_EXISTS("exception.prx")) //Enable exceptions?
	{
		initExceptionHandler(); //Start the exception handler!
	}

	if (SDL_Init(0) < 0) //Error initialising SDL defaults?
	{
		SDL_Quit(); //Nothing to be done!
		exit(1); //Just to be sure
	}

	initLocks(); //Initialise all locks before anything: we have the highest priority!

	initlog(); //Initialise the logging system!

	//Normal operations!
	resetTimers(); //Make sure all timers are ready!
	
	if (DEBUG_ZALLOC) //Verify zalloc functionality?
	{
		//First ensure freemem is valid!
		uint_32 f1,f2;
		f1 = freemem(); //Get free memory!
		f2 = freemem(); //Second check!
		if (f1!=f2) //Memory changed on the second check?
		{
			dolog("zalloc_debug","Multiple freemem fail!");
			halt(); //Quit
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
		halt(); //Quit!
	}
	
	if (DELETE_LOGS_ONBOOT) delete_file("logs","*.log"); //Delete any logs still there!
	if (DELETE_BMP_ONBOOT) delete_file("captures","*.bmp"); //Delete any bitmaps still there!
	
	#ifdef __psp__
		if (FILE_EXISTS("profiler.txt")) //Enable profiler: doesn't work in EMU?
		{
			// Clear the existing profile regs
			pspDebugProfilerClear();
			// Enable profiling
			pspDebugProfilerEnable();
			use_profiler = 1; //Use the profiler!	
		}
	#endif

	if (SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_JOYSTICK)<0) //Error initialising video,audio&joystick?
	{
		raiseError("SDL Init error: %s",SDL_GetError()); //Raise an error!
		sleep(); //Wait forever!
	}
	psp_input_init(); //Make sure input is checked!
	initThreads(); //Initialise&reset thread subsystem!
	initVideoLayer(); //We're for allocating the main video layer, only deallocated using SDL_Quit (when quitting the application)!
	
	resetmain: //Main reset!

	debugrow("Initialising main video service...");	
	initVideoMain(); //All main video!
	debugrow("Initialising main audio service...");	
	initAudio(); //Initialise main audio!

	startTimers(1); //Start core timing!
	startTimers(0); //Disable normal timing!

//First, support for I/O on the PSP!

	if (FIND_EMU_MEMORY_LEAKS) //Find memory leaks?
	{
		uint_32 freememstart;
		freememstart = freemem(); //Freemem at the start!
		logpointers("initEMU(simple test)..."); //Log pointers at the start!
		dolog("zalloc","");
		initEMU(0); //Start the EMU partially, not running!
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
		halt(); //Exit software!
		sleep(); //Wait forever if needed!
	}

	
	ThreadParams_p rootthread; //The main thread we're going to use!
	//Start of the visible part!
	rootthread = startThread(&cputhread,"X86EMU_CPU",NULL, DEFAULT_PRIORITY); //Start the main thread (default priority)!

	//New SDL way!
	/* Check for events */
	running = 1; //Default: we're running!
	for (;running;) //Still running?
	{
		delay(0); //Give threads some time!
		updateInputMain(); //Update input!
		if (!threadRunning(rootthread, "X86EMU_CPU")) break; //Thread not running? Stop our running status!
	}

	stopTimers(1); //Make sure all timers are stopped!

	if (ThreadsRunning()) //Still running?
	{
		termThreads(); //Terminate our threads!
	}

	if (SLEEP_ON_MAIN_CLOSE) //Sleep on main thread close?
	{
		halt(); //Terminate the application!
		return 0; //Sleep: The main thread has been closed! Dont reset/quit!
	}

	termThreads(); //Terminate all still running threads (minimum threads included)!
	
	debugrow("Terminating main audio service...");		
	doneAudio(); //Finish audio processing!
	debugrow("Terminating main video service...");		
	doneVideoMain(); //Finish video!
	unlockVGA(); //Unlock the VGA if needed: it's not running anymore!
	unlockGPU(); //Unlock the GPU if needed: it's not running anymore!
	freezall(); //Finish up: free all used pointers!
	if (shuttingdown() || !running) //Shutdown requested or SDL termination requested?
	{
		exit(0); //Quit using SDL, terminating the pspsurface!
		return 0; //Finish to be safe!
	}

	//Prepare us for a full software/emu reset
	goto resetmain; //Reset?
	return 0; //Just here for, ehm.... nothing!
}