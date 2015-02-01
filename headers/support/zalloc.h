#ifndef ZALLOC_H
#define ZALLOC_H

typedef void (*DEALLOCFUNC)(void **ptr, uint_32 size); //Deallocation functionality!

//Functionality for dynamic memory!
void *nzalloc(uint_32 size, char *name); //Allocates memory, NULL on failure (ran out of memory), protected allocation!
void *zalloc(uint_32 size, char *name); //Same as nzalloc, but clears the allocated memory!
void freez(void **ptr, uint_32 size, char *name); //Release memory allocated with zalloc!
void freezall(void); //Free all allocated memory still allocated (on shutdown only, garbage collector)!

//Pointer derefenence protection!
void *memprotect(void *ptr, uint_32 size, char *name); //Checks address (with name optionally) of pointer!

//Free memory available!
uint_32 freemem(); //Free memory left!

//Debugging functionality for finding memory leaks!
void logpointers(char *cause); //Logs any changes in memory usage!

//For stuff using external allocations. Result: 1 on success, 0 on failure.
byte registerptr(void *ptr,uint_32 size, char *name, DEALLOCFUNC dealloc); //Register a pointer!
byte unregisterptr(void *ptr, uint_32 size); //Remove pointer from registration (only if original pointer)?
#endif