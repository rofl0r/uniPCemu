#ifndef STATE_H
#define STATE_H

#include "headers/types.h" //Basic types!
#include "headers/mmu/mmu.h" //Types in memory!
#include "headers/cpu/cpu.h" //CPU functionality!
#include "headers/emu/gpu/gpu.h" //GPU functionality!
#include "headers/hardware/vga.h" //VGA functionality
#include "headers/mmu/bda.h" //BDA support!
#include "headers/cpu/callback.h" //Callbacks!

typedef struct
{
	uint_32 checksum; //Checksum of below for error checking!
	union
	{
		struct
		{
			uint_32 CPU; //Saved-state CPU!
			uint_32 GPU; //Saved-state GPU!
			uint_32 VGA; //Saved-state VGA (GPU subsystem)
			uint_32 MMU_size; //MMU size!
			Handler CBHandlers[CB_MAX]; //Handlers!
		}; //Contents!
		byte data[sizeof(uint_32)*4+(sizeof(Handler)*CB_MAX)]; //Data!
	}; //Contains data!
} SAVED_CPU_STATE_HEADER; //Saved/loaded status of CPU/MMU/etc. information!

//Finally: functions for loading and saving!

void EMU_SaveStatus(char *filename); //Save the status to file or memory
int EMU_LoadStatus(char *filename); //Load the status from file or memory (TRUE for success, FALSE for error)

#endif