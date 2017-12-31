#include "headers/types.h" //Basic types!

SDL_sem *locks[100]; //All allocated locks!
SDL_sem *LockLock; //Our own lock!

void exitLocks(void)
{
	int i;
	for (i = 0; i < (int)NUMITEMS(locks); i++)
	{
		if (locks[i]) //Gotten a lock?
		{
			SDL_DestroySemaphore(locks[i]);
			locks[i] = NULL; //Destroy the createn item to make it unusable!
		}
	}
	SDL_DestroySemaphore(LockLock); //Finally: destroy our own lock: we're finished!
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

SDL_sem *getLock(byte id)
{
	if (locks[id]) //Used lock?
	{
		return locks[id]; //Give the lock!
	}
	//Not found? Allocate the lock!
	WaitSem(LockLock)
	locks[id] = SDL_CreateSemaphore(1); //Create the lock!
	PostSem(LockLock)
	return locks[id]; //Give the createn lock!
}

byte lock(byte id)
{
	SDL_sem *lock;
	lock = getLock(id); //Get the lock!
	if (lock) //Gotten the lock?
	{
		WaitSem(lock); //Wait for it!
		return 1; //OK!
	}
	return 0; //Error!
}

void unlock(byte id)
{
	SDL_sem *lock;
	lock = getLock(id); //Try and get the lock!
	if (lock) //Gotten the lock?
	{
		PostSem(lock)
	}
}