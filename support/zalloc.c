#include "headers/types.h" //Basic types!
#include "headers/support/zalloc.h" //Our own definitions!
#include "headers/support/log.h" //Logging support!

#include <malloc.h> //Specific to us only!

typedef struct
{
void *pointer; //Pointer to the start of the allocated data!
uint_32 size; //Size of the data!
char name[256]; //The name of the allocated entry!
DEALLOCFUNC dealloc; //Deallocation function!
} POINTERENTRY;

POINTERENTRY registeredpointers[1024]; //All registered pointers!
byte pointersinitialised = 0; //Are the pointers already initialised?

//Our maximum memory that's supported: 3GB!
//#define MEM_MAX_10 2000000000
//For debugging, limit to 10MB, thus a factor of 1MB to start with (maximum block size: 9.999... times this, eg 10MB=99,9...MB allocated max)!
#define MEM_MAX_10 1000000
//Limit each block allocated to this number when defined! Limit us to ~10MB for testing!
#define MEM_BLOCK_LIMIT 10000000

//Debug undefined deallocations?
#define DEBUG_WRONGDEALLOCATION
//Debug allocation and deallocation?
//#define DEBUG_ALLOCDEALLOC

//Pointer registration/unregistration

byte allow_zallocfaillog = 1; //Allow zalloc fail log?

//Initialisation.
void initZalloc() //Initialises the zalloc subsystem!
{
	if (pointersinitialised) return; //Don't do anything when we're ready already!
	//memset(&registeredpointers,0,sizeof(registeredpointers)); //Initialise all registered pointers!
	atexit(&freezall); //Our cleanup function registered!
	pointersinitialised = 1; //We're ready to run!
}

//Log all pointers to zalloc.
void logpointers(char *cause) //Logs any changes in memory usage!
{
	int current;
	uint_32 total_memory = 0; //For checking total memory count!
	initZalloc(); //Make sure we're started!
	dolog("zalloc","Starting dump of allocated pointers (cause: %s)...",cause);
	for (current=0;current<NUMITEMS(registeredpointers);current++)
	{
		if (registeredpointers[current].pointer && registeredpointers[current].size) //Registered?
		{
			if (strlen(registeredpointers[current].name)>0) //Valid name?
			{
				dolog("zalloc","- %s with %i bytes@%p",registeredpointers[current].name,registeredpointers[current].size,registeredpointers[current].pointer); //Add the name!
				total_memory += registeredpointers[current].size; //Add to total memory!
			}
		}
	}
	dolog("zalloc","End dump of allocated pointers.");
	dolog("zalloc","Total memory allocated: %i bytes",total_memory); //We're a full log!
}

//(un)Registration and lookup of pointers.

/*
Matchpointer: matches an pointer to an entry?
parameters:
	ptr: The pointer!
	index: The start index (in bytes)
	size: The size of the data we're going to dereference!
Result:
	-2 when not matched, -1 when matched within another pointer, 0+: the index in the registeredpointers table.
	
*/

sword matchptr(void *ptr, uint_32 index, uint_32 size, char *name) //Are we already in our list? Give the position!
{
	int current;
	uint_32 address_start, address_end;
	initZalloc(); //Make sure we're started!
	if (!ptr) return -2; //Not matched when NULL!
	if (!size) return -2; //Not matched when no size (should be impossible)!
	address_start = (uint_32)ptr;
	address_start += index; //Start of data!
	address_end = address_start+size-1; //End of data!

	for (current=0;current<NUMITEMS(registeredpointers);current++) //Process matched options!
	{
		if (registeredpointers[current].pointer && registeredpointers[current].size) //An registered pointer?
		{
			uint_32 currentstart = (uint_32)registeredpointers[current].pointer; //Start of addressing!
			uint_32 currentend = currentstart+registeredpointers[current].size-1; //End of addressing!
			if (name) //Name specified?
			{
				if (!!strcmp(registeredpointers[current].name,name)) //Invalid name?
				{
					continue; //Skip after all: we're name specific!
				}
			}
			if ((currentstart<=address_start) && (currentend>=address_end)) //Within range?
			{
				if (currentstart==address_start && currentend==address_end) //Full match?
				{
					return current; //Found at this index!
				}
				else //Partly match?
				{
					return -1; //Partly match!
				}
			}
		}
	}
	return -2; //Not found!
}

byte registerptr(void *ptr,uint_32 size, char *name,DEALLOCFUNC dealloc) //Register a pointer!
{
	uint_32 current; //Current!
	initZalloc(); //Make sure we're started!
	if (!ptr)
	{
		#ifdef DEBUG_ALLOCDEALLOC
		if (allow_zallocfaillog) dolog("zalloc","WARNING: RegisterPointer %s with size %i has invalid pointer!",name,size);
		#endif
		return 0; //Not a pointer?
	}
	if (!size)
	{
		#ifdef DEBUG_ALLOCDEALLOC
		if (allow_zallocfaillog) dolog("zalloc","WARNING: RegisterPointer %s with no size!",name,size);
		#endif
		return 0; //Not a size, so can't be a pointer!
	}
	if (matchptr(ptr,0,size,NULL)>-2) return 0; //Already gotten (prevent subs to register after parents)?
	
	for (current=0;current<NUMITEMS(registeredpointers);current++) //Process valid!
	{
		if (!registeredpointers[current].pointer || !registeredpointers[current].size) //Unused?
		{
			registeredpointers[current].pointer = ptr; //The pointer!
			registeredpointers[current].size = size; //The size!
			registeredpointers[current].dealloc = dealloc; //The deallocation function to call, if any to use!
			bzero(&registeredpointers[current].name,sizeof(registeredpointers[current].name)); //Initialise the name!
			strcpy(registeredpointers[current].name,name); //Set the name!
			#ifdef DEBUG_ALLOCDEALLOC
			if (allow_zallocfaillog) dolog("zalloc","Memory has been allocated. Size: %i. name: %s, location: %p",size,name,ptr); //Log our allocated memory!
			#endif
			return 1; //Registered!
		}
	}
	dolog("zalloc","Registration buffer full@%s@%p!",name,ptr);
	return 0; //Give error!
}

byte unregisterptr(void *ptr, uint_32 size) //Remove pointer from registration (only if original pointer)?
{
	int index;
	initZalloc(); //Make sure we're started!
	if ((index = matchptr(ptr,0,size,NULL))>-1) //We've been found fully?
	{
		if (registeredpointers[index].pointer==ptr && registeredpointers[index].size==size) //Fully matched (parents only)?
		{
			registeredpointers[index].pointer = NULL; //Not a pointer!
			registeredpointers[index].size = 0; //No size: we're not a pointer!
			#ifdef DEBUG_ALLOCDEALLOC
			if (allow_zallocfaillog) dolog("zalloc","Freeing pointer %s with size %i bytes...",registeredpointers[index].name,size); //Show we're freeing this!
			#endif
			memset(&registeredpointers[index].name,0,sizeof(registeredpointers[index].name)); //Clear the name!
			return 1; //Safely unregistered!
		}
	}
	return 0; //We could't find the pointer to unregister!
}

//Core allocation/deallocation functions.
void zalloc_free(void **ptr, uint_32 size) //Free a pointer (used internally only) allocated with nzalloc/zalloc!
{
	void *ptrdata = NULL;
	initZalloc(); //Make sure we're started!
	if (ptr) //Valid pointer to our pointer?
	{
		ptrdata = *ptr; //Read the current pointer!
		if (unregisterptr(ptrdata,size)) //Safe unregister, keeping parents alive, use the copy: the original pointer is destroyed by free in Visual C++?!
		{
			free(ptrdata); //Release the valid allocated pointer!
		}
		*ptr = NULL; //Release the pointer given!
	}
}

void *nzalloc(uint_32 size, char *name) //Allocates memory, NULL on failure (ran out of memory), protected malloc!
{
	void *ptr = NULL;
	int times=10; //Try 10 times till giving up!
	initZalloc(); //Make sure we're started!
	if (!size) return NULL; //Can't allocate nothing!
	/*for (;(!ptr && times);) //Try for some times!
	{
		ptr = malloc(size); //Try to allocate!
		--times; //Next try!
	}*/
	ptr = malloc(size); //Try to allocate once!

	if (ptr!=NULL) //Allocated and a valid size?
	{
		if (registerptr(ptr,size,name,&zalloc_free)) //Register the pointer with the detection system!
		{
			return ptr; //Give the original pointer, cleared to 0!
		}
		#ifdef DEBUG_ALLOCDEALLOC
		if (allow_zallocfaillog) dolog("zalloc","Ran out of registrations while allocating %i bytes of data for block %s.",size,name);
		#endif
		free(ptr); //Free it, can't generate any more!
	}
	#ifdef DEBUG_ALLOCDEALLOC
	else if (allow_zallocfaillog)
	{
		if (freemem()>=size) //Enough memory after all?
		{
			dolog("zalloc","Error while allocating %i bytes of data for block \"%s\" with enough free memory(%i bytes).",size,name,freemem());
		}
		else
		{
			dolog("zalloc","Ran out of memory while allocating %i bytes of data for block \"%s\".",size,name);
		}
	}
	#endif
	return NULL; //Not allocated!
}

//Deallocation core function.
void freez(void **ptr, uint_32 size, char *name)
{
	int ptrn=-1;
	initZalloc(); //Make sure we're started!
	if (!ptr) return; //Invalid pointer to deref!
	if ((ptrn = matchptr(*ptr,0,size,NULL))>-1) //Found fully existant?
	{
		if (!registeredpointers[ptrn].dealloc) //Deallocation not registered?
		{
			return; //We can't be freed using this function! We're still allocated!
		}
		registeredpointers[ptrn].dealloc(ptr,size); //Release the memory tied to it using the registered deallocation function, if any!
	}
	#ifdef DEBUG_ALLOCDEALLOC
	else if (allow_zallocfaillog && ptr!=NULL) //An pointer pointing to nothing?
	{
		dolog("zalloc","Warning: freeing pointer which isn't an allocated reference: %s=%p",name,*ptr); //Log it!
	}
	#endif
	//Still allocated, we might be a pointer which is a subset, so we can't deallocate!
}

//Allocation support: add initialization to zero.
void *zalloc(uint_32 size, char *name) //Same as nzalloc, but clears the allocated memory!
{
	void *ptr;
	ptr = nzalloc(size,name); //Try to allocate!
	if (ptr) //Allocated?
	{
		if (memset(ptr, 0, size)) //Give the original pointer, cleared to 0!
		{
			return ptr; //Give the pointer allocated!
		}
		freez((void **)&ptr, size, NULL); //Release the pointer: we can't be cleared!
	}
	return NULL; //Not allocated!
}

//Deallocation support: release all registered pointers! This used to be unregisterptrall.
void freezall(void) //Free all allocated memory still allocated (on shutdown only, garbage collector)!
{
	int i;
	initZalloc(); //Make sure we're started!
	for (i=0;i<NUMITEMS(registeredpointers);i++)
	{
		freez(&registeredpointers[i].pointer,registeredpointers[i].size,"Unregisterptrall"); //Unregister a pointer when allowed!
	}
}

//Memory protection/verification function. Returns the pointer when valid, NULL on invalid.
void *memprotect(void *ptr, uint_32 size, char *name) //Checks address of pointer!
{
	if (!ptr || ptr==NULL) //Invalid?
	{
		return NULL; //Invalid!
	}
	if (matchptr(ptr,0,size,name)>-2) //Pointer matched (partly or fully)?
	{
		return ptr; //Give the pointer!
	}
	return NULL; //Invalid!
}

//Detect free memory.
uint_32 freemem() //Free memory left! We work!
{
	uint_32 curalloc; //Current allocated memory!
	char *buffer;
	uint_32 multiplier; //The multiplier!
	byte times = 9; //Times!
	uint_32 lastzalloc = 0;
	byte allocated = 0; //Allocated?
	curalloc = 0; //Reset at 1 bytes!
	multiplier = MEM_MAX_10; //Start at max multiplier (~100MB)!
	times = 9; //Times 9 to start with!

	allow_zallocfaillog = 0; //Don't allow!
	while (1) //While not done...
	{
		lastzalloc = (curalloc+(multiplier*times)); //Last zalloc!
		allocated = 0; //Default: not allocated!
		buffer = (char *)zalloc(lastzalloc,"freememdetect"); //Try allocating, don't have to be cleared!
		if (buffer) //Allocated?
		{
			freez((void **)&buffer,lastzalloc,"freememdetect"); //Release memory for next try!
			buffer = NULL; //Not allocated anymore!
			curalloc = lastzalloc; //Set detected memory!
			//dolog("zalloc","Free memory step: %i",curalloc); //Show our step! WE WORK!
			allocated = 1; //We're allocated!
		}
		if (!times || allocated) //Either nothing left or allocated?
		{
			multiplier /= 10; //Next digit!
			times = 9; //Reset times for next try!
		}
		else //Calculate next digit!
		{
			--times; //Next digit!
		}
		if (!multiplier) //Gotten an allocation and/or done?
		{
			break; //Stop searching: we're done!
		}
		//We're to continue!
	} //We have success!
	
	
	if (buffer) //Still allocated?
	{
		freez((void **)&buffer,lastzalloc,"Freemem@FinalCleanup"); //Still allocated=>release?
	}

	allow_zallocfaillog = 1; //Allow again!
	#ifdef MEM_BLOCK_LIMIT
		if (curalloc > MEM_BLOCK_LIMIT) //More than the limit?
		{
			curalloc = MEM_BLOCK_LIMIT; //Limit to this much!
		}
	#endif
	return curalloc; //Give free allocatable memory size!
}