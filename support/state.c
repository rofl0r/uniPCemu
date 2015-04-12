#include "headers/hardware/vga.h" //VGA support!
#include "headers/emu/state.h" //Our own data and support etc.
#include "headers/support/crc32.h" //CRC32 support!

extern CPU_type CPU; //CPU!
extern GPU_type GPU; //GPU!
extern Handler CBHandlers[CB_MAX]; //Handlers!

SAVED_CPU_STATE_HEADER SaveStatus_Header; //SaveStatus structure!


//Version of save state!
#define SAVESTATE_MAIN_VER 1
#define SAVESTATE_SUB_VER 0

void EMU_SaveStatus(char *filename) //Save the status to file or memory
{
	return; //Not working ATM!
}

int EMU_LoadStatus(char *filename) //Load the status from file or memory (TRUE for success, FALSE for error)
{
	return FALSE; //Cannot load: not compatible yet!
}