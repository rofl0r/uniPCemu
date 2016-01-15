#include "headers/types.h" //Basic types!
#include "headers/emu/threads.h" //Basic threads!
#include "headers/support/log.h" //Logging!
#include "headers/emu/gpu/gpu_text.h" //Text support!

#ifdef _WIN32
#include <sdl_thread.h> //Multithreading support!
#else
//PSP?
#include <SDL/SDL_thread.h> //Multithreading support!
#endif

#define MAX_THREAD 50
//Maximum ammount of threads:

#define THREADSTATUS_UNUSED 0
#define THREADSTATUS_ALLOCATED 1
#define THREADSTATUS_CREATEN 2
#define THREADSTATUS_RUNNING 4
//Terminated=Unused.
#define THREADSTATUS_TERMINATED THREADSTATUS_UNUSED


//To debug threads?
#define DEBUG_THREADS 0
//Allow running threads/callbacks?
#define CALLBACK_ENABLED 1

//Stuff for easy thread functions.

//Threads don't need stacks, but give it some to be sure!
#define THREAD_STACKSIZE 0x100000

ThreadParams threadpool[MAX_THREAD]; //Thread pool!

//Thread allocation/deallocation!

int getthreadpoolindex(uint_32 thid) //Get index of thread in thread pool!
{
	int i;
	for (i=0;i<(int)NUMITEMS(threadpool);i++) //Process all known indexes!
	{
		if (threadpool[i].used && threadpool[i].threadID==thid) //Used and found?
		{
			return i; //Give the index!
		}
	}
	return -1; //Not found!
}

/*#ifdef _WIN32
void SDL_KillThread(uint_32 thid) //Our custom version!
{
	int index = getthreadpoolindex(thid); //What index!
	threadpool[i].allow_running = 0; //Request quitting the thread!
	waitThreadEnd(&threadpool[i]); //Wait for the thread to end!
}
#endif*/

/*int allow_threadrunning() //Allow the current thread to continue running?
{
	int index = getthreadpoolindex(SDL_ThreadID()); //What index!
	return threadpool[i].allow_running; //Allow the thread to run?
}*/

ThreadParams_p allocateThread() //Allocate a new thread to run (waits if none to allocate)!
{
	uint_32 curindex;
	newallocate: //Try (again)!
	for (curindex=0;curindex<NUMITEMS(threadpool);curindex++) //Find an unused entry!
	{
		if (!threadpool[curindex].used) //Not used?
		{
			//dolog("threads","Allocating thread entry...");
			threadpool[curindex].used = 1; //Allocated!
			return &threadpool[curindex]; //Give the newly allocated thread params!
			//Failed to allocate, passthrough!
		}
	}
	delay(0); //Wait a bit for some space: allow other threads to!
	goto newallocate; //Try again till we work!
}

void releasePool(uint_32 threadid) //Release a pooled thread if it exists!
{
	int index;
	if ((index = getthreadpoolindex(threadid))!=-1) //Gotten index?
	{
		threadpool[index].used = 0; //Unused!
		threadpool[index].status = THREADSTATUS_TERMINATED; //We're terminated!
	}
}

void activeThread(uint_32 threadid, ThreadParams_p thread)
{
	thread->status |= THREADSTATUS_RUNNING; //Running!
	thread->threadID = threadid; //Our thread ID!
	//dolog("threads","activeThread_RET!");
}

void terminateThread(uint_32 thid) //Terminate the thread!
{
	//dolog("threads","terminateThread: Terminating thread: %x",thid);
	SDL_Thread *thread;
	int thnr;
	if ((thnr = getthreadpoolindex(thid))!=-1) //Found the thread?
	{
		thread = threadpool[thnr].thread; //Get the thread!
	}
	releasePool(thid); //Release from pool if available!
	if (thnr!=-1) //Valid thread to kill?
	{
		SDL_KillThread(thread); //Kill this thread!
	}
	//sceKernelTerminateDeleteThread(thid); //Exit and delete myself!
}

void deleteThread(uint_32 thid)
{
	terminateThread(thid); //Passthrough!
	/*if (sceKernelDeleteThread(thid)>=0) //Deleted?
	{
		//dolog("threads","Deletethread: freethread...");
		releasePool(thid); //Release from pool if available!
	}*/
}

void runcallback(uint_32 thid)
{
	//dolog("threads","Runcallback...");
	//Now run the requested thread!
	if (thid && CALLBACK_ENABLED) //Gotten params?
	{
		int index; //The index to be used!
		//dolog("threads","activeThread...");
		if ((index = getthreadpoolindex(thid))!=-1) //Gotten index?
		{
			if (threadpool[index].callback) //Found the callback?
			{
				//dolog("threads","RunRealCallback...");
				threadpool[index].callback(); //Execute the real callback!
			}
		}
	}
	//dolog("threads","Callback RET.");
}

//Running/stop etc. function for the thread!


int ThreadsRunning() //Are there any threads running or ready to run?
{
	int i;
	int numthreads = 0; //Number of running threads?
	for (i=0;i<(int)NUMITEMS(threadpool);i++) //Check all threads!
	{
		if (threadpool[i].used) //Allocated?
		{
			if (threadpool[i].status&THREADSTATUS_CREATEN) //Createn or running?
			{
				++numthreads; //We have some createn or running threads!
			}
		}
	}
	return numthreads; //How many threads are running?
}

int minthreadsrunning() //Minimum ammount of threads running when nothing's there!
{
	return DEBUG_THREADS; //1 or 0 minimal!
}

/*

threadhandler: The actual thread running over all other threads.

*/

int threadhandler(/*SceSize args, void *params*/ void *data)
{
	uint_32 thid = SDL_ThreadID(); //The thread ID!
	activeThread(thid,(ThreadParams_p)data); //Mark us as running!
	runcallback(thid); //Run the callback!
	releasePool(thid); //Terminate ourselves!
	return 0; //Shouldn't be here?
}

void termThreads() //Terminate all threads but our own!
{
	//dolog("threads","termThreads...");
	int i;
	uint_32 my_thid = SDL_ThreadID(); //My own thread ID!
	for (i=0;i<(int)NUMITEMS(threadpool);i++) //Process all of our threads!
	{
		if (threadpool[i].used && (threadpool[i].threadID!=my_thid)) //Used and not ourselves?
		{
			if (threadpool[i].status&THREADSTATUS_RUNNING) //Running?
			{
				//dolog("threads","termThreads: Running!");
				terminateThread(threadpool[i].threadID); //Terminate it!
			}
			else if (threadpool[i].status&THREADSTATUS_CREATEN) //Createn?
			{
				//dolog("threads","termThreads: Createn!");
				deleteThread(threadpool[i].threadID); //Delete it!
			}
			else if (threadpool[i].status&THREADSTATUS_ALLOCATED) //Allocated?
			{
				//dolog("threads","termThreads: Allocated!");
				threadpool[i].used = 0; //Deallocate it!
			}
		}
	}
}

extern GPU_TEXTSURFACE *frameratesurface;

void debug_threads()
{
	char thread_name[256];
	bzero(thread_name,sizeof(thread_name)); //Init!
	while (1)
	{
		int numthreads = 0; //Number of installed threads running!
		int i,i2;
		int totalthreads = ThreadsRunning(); //Ammount of threads running!
		GPU_text_locksurface(frameratesurface);
		for (i=0;i<(int)NUMITEMS(threadpool);i++)
		{
			if (threadpool[i].used) //Allocated?
			{
				if (threadpool[i].status>=THREADSTATUS_CREATEN) //Created or running?
				{
					++numthreads; //Count ammount of threads!
					GPU_textgotoxy(frameratesurface,0,29-totalthreads+numthreads); //Goto the debug row start!
					sprintf(thread_name,"Active thread: %s",threadpool[i].name); //Get data string!
					GPU_textprintf(frameratesurface,RGB(0xFF,0x00,0x00),RGB(0x00,0xFF,0x00),thread_name);
					for (i2=strlen(thread_name);i2<=50;i2++)
					{
						GPU_textprintf(frameratesurface,RGB(0xFF,0x00,0x00),RGB(0x00,0xFF,0x00)," "); //Filler to 50 chars!
					} 
				}
			}
		}
		GPU_textgotoxy(frameratesurface,0,30);
		GPU_textprintf(frameratesurface,RGB(0xFF,0x00,0x00),RGB(0x00,0xFF,0x00),"Number of threads: %i",numthreads); //Debug the ammount of threads used!
		GPU_text_releasesurface(frameratesurface);
		delay(100000); //Wait 100ms!
	}
}

void onThreadExit(void) //Exit handler for quitting the application! Used for cleanup!
{
	termThreads(); //Terminate all threads!
}

void initThreads() //Initialise&reset thread subsystem!
{
	//dolog("threads","initThreads...");
	//dolog("threads","initThreads: termThreads...");
	termThreads(); //Make sure all running threads are stopped!
	//dolog("threads","debugThreads?...");
	atexit(&onThreadExit); //Register out cleanup function!
	memset(threadpool,0,sizeof(threadpool)); //Clear thread pool!
	if (DEBUG_THREADS) startThread(&debug_threads,"X86EMU_Thread Debugger",NULL); //Plain debug threads!
	//dolog("threads","initThreads: RET...");
}

void *getthreadparams()
{
	uint_32 threadID = SDL_ThreadID(); //Get the current thread ID!
	int index;
	if ((index = getthreadpoolindex(threadID)) != -1) //Gotten?
	{
		return threadpool[index].params; //Give the params, if any!
	}
	return NULL; //No params given!
}

void threadCreaten(ThreadParams_p params, uint_32 threadID, char *name)
{
	//dolog("threads","threadCreaten...");
	if (params) //Gotten params?
	{
		//dolog("threads","threadCreaten set...");
		params->threadID = threadID; //The thread ID, just in case!
		bzero(params->name,sizeof(params->name));
		strcpy(params->name,name); //Save the name for usage!
	}
	//dolog("threads","threadCreaten: RET...");
}

byte threadRunning(ThreadParams_p thread, char *name)
{
	if (thread) //OK?
	{
		if (thread->used) //Are we used?
		{
			if (!strcmp(thread->name,name)) //Same name verification?
			{
				if (thread->status&(THREADSTATUS_CREATEN | THREADSTATUS_RUNNING)) //Createn or running?
				{
					return 1; //Active/ready to run!
				}
			}
		}
	}
	return 0; //Not running!
}

ThreadParams_p startThread(Handler thefunc, char *name, void *params) //Start a thread!
{
	//dolog("threads","startThread (%s)...",name);
	if (!thefunc || thefunc==NULL) //No func?
	{
		//dolog("threads","startThread: NULLfunc...");
		raiseError("thread manager","NULL thread: %s",name); //Show the thread as being NULL!
		return NULL; //Don't start: can't start no function!
	}


	//We create our handler in dynamic memory, because we need to keep it in the threadhandler!
	
	//dolog("threads","startThread: allocThread...");
	//First, allocate a thread position!
	ThreadParams_p threadparams = allocateThread(); //Allocate a thread for us, wait for any to come free in the meanwhile!
//Next, start the timer function!
	//SceUID thid; //The thread ID to allocate!
	threadparams->callback = thefunc; //The function to run!
	threadparams->status = THREADSTATUS_CREATEN; //Createn!
	threadparams->params = params; //The params to save!

	uint_32 thid; //The thread ID!
	//dolog("threads","startThread: createThread...");
	docreatethread: //Try to start a thread!
	threadparams->thread = SDL_CreateThread(threadhandler,threadparams); //Create the thread!
	//params->allowthreadrunning  = 1; //Allow the thread to run!
	
	if (!threadparams->thread) //Failed to create?
	{
		delay(0); //Wait a bit!
		goto docreatethread; //Try again!
	}
	thid = SDL_GetThreadID(threadparams->thread); //Get the thread ID!
	threadCreaten(threadparams,thid,name); //We've been createn!

	return threadparams; //Give the thread createn!

	//dolog("threads","startThread: threadCreaten...");
	
	//dolog("threads","startThread: checking params...");
	/*if (params) //Valid params?
	{
		//dolog("threads","startThread: Checking status...");
		if (params->status) //We're ready to go?
		{
			//dolog("threads","startThread: Ready! Setting callback...");
			//Set our parameters!
			params->callback = thefunc; //The function to run!
			
			//dolog("threads","startThread: Ready! StartThread...");
			//We have a thread we can use now!
			sceKernelStartThread(thid, 0, 0); //Start thread, if possible! Data is ready to be processed!
			return params; //Give the createn thread!
		}
		else //Remove the thread: we're done with it!
		{
			//dolog("threads","startThread: Status wrong. Calling deleteThread...");
			deleteThread(thid); //Cleanup the parameters!
		}
	}*/
	//dolog("threads","startThread: RET...");
	//return NULL; //Failed to allocate!
}

void waitThreadEnd(ThreadParams_p thread) //Wait for this thread to end!
{
	if (thread) //Valid thread?
	{
		if (thread->used) //Allocated?
		{
			if (thread->status>=THREADSTATUS_CREATEN) //Createn or running?
			{
				while (thread->status==THREADSTATUS_CREATEN) //Not running yet?
				{
					delay(100); //Wait for a bit for the thread to start!
				}
				int dummy;
				SDL_WaitThread(thread->thread,&dummy); //Wait for the thread to end, ignore the result from the thread!
			}
		}
	}
	//Done with running the thread!
}

OPTINLINE void quitThread() //Quit the current thread!
{
	uint_32 thid = SDL_ThreadID(); //Get the current thread ID!
	terminateThread(thid); //Terminate ourselves!
}

void termThread(){quitThread();} //Alias!