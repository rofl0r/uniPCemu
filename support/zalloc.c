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
#define MEM_MAX_10 2000000000

//Debug undefined deallocations?
#define DEBUG_WRONGDEALLOCATION 1
//Debug allocation and deallocation?
#define DEBUG_ALLOCDEALLOC 0

//Pointer registration/unregistration

byte allow_zallocfaillog = 1; //Allow zalloc fail log?
uint_32 totalmemory_real; //Total memory present?

void initZalloc() //Initialises the zalloc subsystem!
{
	if (pointersinitialised) return; //Don't do anything when we're ready already!
	memset(&registeredpointers,0,sizeof(registeredpointers)); //Initialise all registered pointers!
	pointersinitialised = 1; //We're ready to run!
	totalmemory_real = freemem(); //Load total memory present!
}

void logpointers() //Logs any changes in memory usage!
{
	initZalloc(); //Make sure we're started!
	int current;
	dolog("zalloc","Starting dump of allocated pointers...");
	uint_32 total_memory = 0; //For checking total memory count!
	uint_32 free_memory = freemem(); //Free memory present!
	total_memory = free_memory; //Total memory present, which is free!
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
	dolog("zalloc","Total memory detected: %i bytes; From which Free memory detected: %i bytes",total_memory,free_memory); //We're a full log!
	dolog("zalloc","Actual memory registered during initialisation of memory module: %i bytes; Missing difference (detected compared to total): %i bytes",totalmemory_real,(int_32)total_memory-(int_32)totalmemory_real); //We're the difference info!
}

static void zalloc_free(void **ptr, uint_32 size) //Free a pointer (used internally only) allocated with nzalloc/zalloc!
{
	initZalloc(); //Make sure we're started!
	free(*ptr); //Release the pointer!
	unregisterptr(*ptr,size); //Safe unregister, keeping parents alive!
	*ptr = NULL; //Release the memory for the user!
}

/*
Matchpointer: matches an pointer to an entry?
parameters:
	ptr: The pointer!
	index: The start index (in bytes)
	size: The size of the data we're going to dereference!
Result:
	-1 when not matched, else the index in the validpointers table.
	
*/
int matchptr(void *ptr, uint_32 index, uint_32 size, char *name) //Are we already in our list? Give the position!
{
	initZalloc(); //Make sure we're started!
	if (!ptr) return -1; //Not matched when NULL!
	if (!size) return -1; //Not matched when no size (should be impossible)!
	uint_32 address_start = (uint_32)ptr+index; //Start of data!
	uint_32 address_end = address_start+size-1; //End of data!

	int current;
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

int registerptr(void *ptr,uint_32 size, char *name,DEALLOCFUNC dealloc) //Register a pointer!
{
	initZalloc(); //Make sure we're started!
	if (!ptr)
	{
		if (allow_zallocfaillog && DEBUG_ALLOCDEALLOC) dolog("zalloc","WARNING: RegisterPointer %s with size %i has invalid pointer!",name,size);
		return 0; //Not a pointer?
	}
	if (!size)
	{
		if (allow_zallocfaillog && DEBUG_ALLOCDEALLOC) dolog("zalloc","WARNING: RegisterPointer %s with no size!",name,size);
		return 0; //Not a size, so can't be a pointer!
	}
	if (matchptr(ptr,0,size,NULL)>-2) return 0; //Already gotten (prevent subs to register after parents)?
	
	uint_32 current; //Current!
	for (current=0;current<NUMITEMS(registeredpointers);current++) //Process valid!
	{
		if (!registeredpointers[current].pointer || !registeredpointers[current].size) //Unused?
		{
			registeredpointers[current].pointer = ptr; //The pointer!
			registeredpointers[current].size = size; //The size!
			registeredpointers[current].dealloc = dealloc; //The deallocation function to call, if any to use!
			bzero(&registeredpointers[current].name,sizeof(registeredpointers[current].name)); //Initialise the name!
			strcpy(registeredpointers[current].name,name); //Set the name!
			if (allow_zallocfaillog && DEBUG_ALLOCDEALLOC) dolog("zalloc","Memory has been allocated. Size: %i. name: %s, location: %p",size,name,ptr); //Log our allocated memory!
			return 1; //Registered!
		}
	}
	dolog("zalloc","Pointer already registered or registration buffer full@%s@%p!",name,ptr);
	return 0; //Give error!
}

void unregisterptr(void *ptr, uint_32 size) //Remove pointer from registration (only if original pointer)?
{
	initZalloc(); //Make sure we're started!
	int index;
	if ((index = matchptr(ptr,0,size,NULL))>-1) //We've been found fully?
	{
		if (registeredpointers[index].pointer==ptr && registeredpointers[index].size==size) //Fully matched (parents only)?
		{
			registeredpointers[index].pointer = NULL; //Not a pointer!
			registeredpointers[index].size = 0; //No size: we're not a pointer!
			if (allow_zallocfaillog && DEBUG_ALLOCDEALLOC) dolog("zalloc","Freeing pointer %s with size %i bytes...",registeredpointers[index].name,size); //Show we're freeing this!
			memset(registeredpointers[index].name,0,sizeof(registeredpointers[index].name)); //Clear the name!
		}
	}
}

OPTINLINE void *nzalloc(uint_32 size, char *name) //Allocates memory, NULL on failure (ran out of memory), protected malloc!
{
	initZalloc(); //Make sure we're started!
	if (!size) return NULL; //Can't allocate nothing!
	void *ptr = NULL;
	int times=10; //Try 10 times till giving up!
	for (;(!ptr && times);) //Try for some times!
	{
		ptr = malloc(size); //Try to allocate!
		--times; //Next try!
	}
	if (ptr!=NULL) //Allocated and a valid size?
	{
		if (registerptr(ptr,size,name,&zalloc_free)) //Register the pointer with the detection system!
		{
			return ptr; //Give the original pointer, cleared to 0!
		}
		if (allow_zallocfaillog && DEBUG_ALLOCDEALLOC) dolog("zalloc","Ran out of registrations while allocating %i bytes of data for block %s.",size,name);
		free(ptr); //Free it, can't generate any more!
	}
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
		logpointers(); //Log a dump of pointers!
	}
	return NULL; //Not allocated!
}

OPTINLINE void *zalloc(uint_32 size, char *name) //Same as nzalloc, but clears the allocated memory!
{
	void *ptr;
	ptr = nzalloc(size,name); //Try to allocate!
	if (ptr) //Allocated?
	{
		return memset(ptr,0,size); //Give the original pointer, cleared to 0!
	}
	return NULL; //Not allocated!
}


void freez(void **ptr, uint_32 size, char *name)
{
	initZalloc(); //Make sure we're started!
	if (!ptr) return; //Invalid pointer to deref!
	int ptrn=-1;
	if ((ptrn = matchptr(*ptr,0,size,NULL))>-1) //Found fully existant?
	{
		if (!registeredpointers[ptrn].dealloc) //Deallocation not registered?
		{
			return; //We can't be freed using this function! We're still allocated!
		}
		registeredpointers[ptrn].dealloc(ptr,size); //Release the memory tied to it using the registered deallocation function, if any!
	}
	else if (allow_zallocfaillog && DEBUG_ALLOCDEALLOC && ptr!=NULL) //An pointer pointing to nothing?
	{
		dolog("zalloc","Warning: freeing pointer which isn't an allocated reference: %s=%p",name,*ptr); //Log it!
	}
	//Still allocated, we might be a pointer which is a subset, so we can't deallocate!
}

void unregisterptrall() //Free all allocated memory still allocated (on shutdown only, garbage collector)!
{
	initZalloc(); //Make sure we're started!
	int i;
	for (i=0;i<NUMITEMS(registeredpointers);i++)
	{
		void *ptr;
		ptr = registeredpointers[i].pointer; //A pointer!
		freez(&ptr,registeredpointers[i].size,"Unregisterptrall"); //Unregister a pointer when allowed!
	}
}


void freezall() //Same as unregisterptrall
{
	unregisterptrall(); //Unregister all!
}

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

OPTINLINE uint_32 freemem() //Free memory left! We work!
{
	//dolog("zalloc","Detecting free memory...");
	uint_32 curalloc; //Current allocated memory!
	char *buffer;
	uint_32 multiplier; //The multiplier!
	curalloc = 0; //Reset at 1 bytes!
	multiplier = MEM_MAX_10; //Start at max multiplier (~100MB)!
	byte times = 9; //Times!
	times = 9; //Times 9 to start with!

	uint_32 lastzalloc = 0;
	allow_zallocfaillog = 0; //Don't allow!
	byte allocated = 0; //Allocated?
	while (1) //While not done...
	{
		lastzalloc = (curalloc+(multiplier*times)); //Last zalloc!
		allocated = 0; //Default: not allocated!
		buffer = (char *)zalloc(lastzalloc,"freememdetect"); //Try allocating, don't have to be cleared!
		if (buffer) //Allocated?
		{
			freez((void **)&buffer,lastzalloc,"Freemem@Cleanup"); //Release memory for next try!
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

	//dolog("zalloc","Free memory detected: %i bytes",curalloc);
	allow_zallocfaillog = 1; //Allow again!
	return curalloc; //Give free allocatable memory size!
}