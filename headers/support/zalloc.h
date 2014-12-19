#ifndef ZALLOC_H
#define ZALLOC_H

typedef void (*DEALLOCFUNC)(void **ptr, uint_32 size); //Deallocation functionality!

//Functionality for dynamic memory!
OPTINLINE void *nzalloc(uint_32 size, char *name); //Allocates memory, NULL on failure (ran out of memory), protected allocation!
OPTINLINE void *zalloc(uint_32 size, char *name); //Same as nzalloc, but clears the allocated memory!
void freez(void **ptr, uint_32 size, char *name); //Release memory allocated with zalloc!
void unregisterptrall(); //Free all allocated memory still allocated (on shutdown only, garbage collector)!
void freezall(); //Same as unregisterall

//Pointer derefenence protection!
void *memprotect(void *ptr, uint_32 size, char *name); //Checks address (with name optionally) of pointer!

//Free memory available!
OPTINLINE uint_32 freemem(); //Free memory left!

//Debugging functionality for finding memory leaks!
void logpointers(); //Logs any changes in memory usage!

//For stuff using external allocations:
int registerptr(void *ptr,uint_32 size, char *name, DEALLOCFUNC dealloc); //Register a pointer!
void unregisterptr(void *ptr, uint_32 size); //Remove pointer from registration (only if original pointer)?

#endif