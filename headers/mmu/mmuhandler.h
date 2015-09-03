#ifndef MMUHANDLER_H
#define MMUHANDLER_H

#include "headers/types.h" //Basic types!

/*

w/rhandler:
	offset: The offset to be read (full memory).
	value: The value to read into/from.
result:
	TRUE when successfull. FALSE when failed (continue searching for a viable solution).

*/

typedef byte (*MMU_WHANDLER)(uint_32 offset, byte value);    /* A pointer to a handler function */
typedef byte (*MMU_RHANDLER)(uint_32 offset, byte *value);    /* A pointer to a handler function */

void MMU_resetHandlers(char *module); //Initialise/reset handlers, no module (""/NULL) for all.
byte MMU_registerWriteHandler(MMU_WHANDLER handler, char *module); //Register a write handler!
byte MMU_registerReadHandler(MMU_RHANDLER handler, char *module); //Register a read handler!

//MMU specific handlers:
byte MMU_IO_writehandler(uint_32 offset, byte value); //Handle MMU-device I/O!
byte MMU_IO_readhandler(uint_32 offset, byte *value); //Handle MMU-device I/O!

#endif