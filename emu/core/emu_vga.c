#include "headers/types.h" //Basic types!

#include "headers/hardware/vga.h" //VGA support!

#include "headers/emu/gpu/gpu.h" //Basic GPU!
#include "headers/emu/gpu/gpu_renderer.h" //Renderer support!
#include "headers/emu/gpu/gpu_emu.h" //Emulator support!
#include "headers/emu/gpu/gpu_text.h" //Emulator support!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate support!

//How many steps (function calls) for every full screen frame! Min=1, was originally 10!
#define __SCREEN_LINES_LIMIT 10

#define __HW_DISABLED 0

/*

Renderer mini-optimizations.

*/

static void VGA_generateScreenLine() //Generate one step in VGA Screen Lines!
{
	if (__HW_DISABLED) return; //Disabled?
	VGA_Sequencer(getActiveVGA()); //Generate one line only!
}

void changeRowTimer(VGA_Type *VGA, word lines) //Change the VGA row processing timer the ammount of lines on display!
{
	if (__HW_DISABLED) return; //Disabled?
	addtimer(VGA_VerticalRefreshRate(VGA),&VGA_generateScreenLine,"ScanLineBlock",__SCREEN_LINES_LIMIT); //Re-add the Scanline to the timers!
}