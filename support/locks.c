#include "headers/types.h" //Basic types!

struct
{
	byte used; //Used lock?
	SDL_sem *lock;
	char name[256];
} locks[100];

SDL_sem *LockLock; //Our own lock!

void exitLocks(void)
{
	int i;
	for (i = 0; i < NUMITEMS(locks); i++)
	{
		if (locks[i].lock) //Gotten a lock?
		{
			SDL_DestroySemaphore(locks[i].lock);
			memset(&locks[i], 0, sizeof(locks[i])); //Destroy the createn item to make it unusable!
		}
	}
}

void initLocks()
{
	static byte toinitialise = 1;
	if (toinitialise) //Not initialised yet?
	{
		memset(locks, 0, sizeof(locks)); //Initialise locks!
		atexit(&exitLocks); //Register the lock cleanup function!
		LockLock = SDL_CreateSemaphore(1); //Create our own lock!
		toinitialise = 0; //Initialised!
	}
}

SDL_sem *getLock(char *name)
{
	int i;
	WaitSem(LockLock)
	for (i = 0; i < NUMITEMS(locks); i++)
	{
		if (locks[i].used) //Used lock?
		{
			if (strcmp(name, locks[i].name)==0) //Found?
			{
				PostSem(LockLock)
				return locks[i].lock; //Give the lock!
			}
		}
	}
	//Not found? Allocate the lock!
	for (i = 0; i < NUMITEMS(locks); i++)
	{
		if (!locks[i].used) //Unused lock?
		{
			strcpy(locks[i].name, name); //Set the lock to used!
			if (!locks[i].lock) //Not used yet?
			{
				locks[i].lock = SDL_CreateSemaphore(1); //Create the lock!
			}
			locks[i].used = 1; //We're used!
			PostSem(LockLock)
			return locks[i].lock; //Give the createn lock!
		}
	}
	PostSem(LockLock)
	return NULL; //Unable to allocate: invalid lock!
}

byte lock(char *name)
{
	SDL_sem *lock;
	lock = getLock(name); //Get the lock!
	if (lock) //Gotten the lock?
	{
		WaitSem(lock) //Wait for it!
		return 1; //OK!
	}
	return 0; //Error!
}

void unlock(char *name)
{
	SDL_sem *lock;
	lock = getLock(name); //Try and get the lock!
	if (lock) //Gotten the lock?
	{
		PostSem(lock)
	}
}