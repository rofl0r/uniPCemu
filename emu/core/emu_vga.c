#include "headers/types.h" //Basic types!

#include "headers/hardware/vga.h" //VGA support!

#include "headers/emu/gpu/gpu.h" //Basic GPU!
#include "headers/emu/gpu/gpu_renderer.h" //Renderer support!
#include "headers/emu/gpu/gpu_emu.h" //Emulator support!
#include "headers/emu/gpu/gpu_text.h" //Emulator support!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate support!

//How many steps (function calls) for every full screen frame! Min=1, was originally 10!
#define SCREEN_BLOCKS 1

#define __HW_DISABLED 0

/*

Renderer mini-optimizations.

*/

word VGA_LINESTEP = 0; //How many lines to draw in one swoop!
static void VGA_generateScreenLineStep() //Generate one step in VGA Screen Lines!
{
	if (__HW_DISABLED) return; //Disabled?
	VGA_Sequencer(getActiveVGA()); //Generate one line only!
}

word oldlines = 0; //Old line step!
void changeRowTimer(VGA_Type *VGA, word lines) //Change the VGA row processing timer the ammount of lines on display!
{
	if (__HW_DISABLED) return; //Disabled?
	addtimer(VGA_VerticalRefreshRate(VGA),&VGA_generateScreenLineStep,"ScanLineBlock"); //Re-add the Scanline to the timers!
	return; //Old way disabled!
	//dolog("vgagenerator","Given row timer lines: %i",lines);
	if (VGA) //We've got a VGA to handle?
	{
		if (lines) //Got lines (can't have no lines!!!)?
		{
			VGA_LINESTEP = lines/SCREEN_BLOCKS; //Draw whole screen parts at once for max speed!
			oldlines = lines; //Set old lines for backup!
		}
		else if (oldlines) //Invalid lines (keep old)
		{
			VGA_LINESTEP = oldlines/SCREEN_BLOCKS; //Draw whole screen parts at once for max speed!
		}
		else
		{
			VGA_LINESTEP = 1; //Default: 1 line at a time!
		}

		if (!VGA_LINESTEP) //No lines?
		{
			VGA_LINESTEP = 1; //Step 1 line!
		}
		//dolog("vgagenerator","Linestep=%i",VGA_LINESTEP);
		//Only with valid scanlines we can set a framerate!
		addtimer(VGA_VerticalRefreshRate(VGA)/VGA_LINESTEP,&VGA_generateScreenLineStep,"ScanLineBlock"); //Re-add the Scanline to the timers!
	}
}
