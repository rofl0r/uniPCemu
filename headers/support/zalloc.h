#ifndef ZALLOC_H
#define ZALLOC_H

#include "headers/types.h" //Basic types!

void *memprotect(void *ptr, uint_32 size, char *name); //Checks address (with name optionally) of pointer!

typedef void (*DEALLOCFUNC)(void **ptr, uint_32 size, SDL_sem *lock); //Deallocation functionality!

//Functionality for dynamic memory!
void *nzalloc(uint_32 size, char *name, SDL_sem *lock); //Allocates memory, NULL on failure (ran out of memory), protected allocation!
void *zalloc(uint_32 size, char *name, SDL_sem *lock); //Same as nzalloc, but clears the allocated memory!
void freez(void **ptr, uint_32 size, char *name); //Release memory allocated with zalloc!
void freezall(void); //Free all allocated memory still allocated (on shutdown only, garbage collector)!

//Free memory available!
uint_32 freemem(); //Free memory left!

//Debugging functionality for finding memory leaks!
void logpointers(char *cause); //Logs any changes in memory usage!

//For stuff using external allocations. Result: 1 on success, 0 on failure.
byte registerptr(void *ptr,uint_32 size, char *name, DEALLOCFUNC dealloc, SDL_sem *lock); //Register a pointer!
byte unregisterptr(void *ptr, uint_32 size); //Remove pointer from registration (only if original pointer)?

DEALLOCFUNC getdefaultdealloc(); //The default dealloc function!
byte changedealloc(void *ptr, uint_32 size, DEALLOCFUNC dealloc); //Change the default dealloc func for an entry (used for external overrides)!
#endif