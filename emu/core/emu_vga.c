#include "headers/types.h" //Basic types!

#include "headers/hardware/vga.h" //VGA support!

#include "headers/emu/gpu/gpu.h" //Basic GPU!
#include "headers/emu/gpu/gpu_renderer.h" //Renderer support!
#include "headers/emu/gpu/gpu_emu.h" //Emulator support!
#include "headers/emu/gpu/gpu_text.h" //Emulator support!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate support!
#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer itself!
#include "headers/emu/timers.h" //Timer support!

//How many lines to render at a time when limited.
#define __SCREEN_LINES_LIMIT 500

//#define __HW_DISABLED

/*

Renderer mini-optimizations.

*/

void changeRowTimer(VGA_Type *VGA, word lines) //Change the VGA row processing timer the ammount of lines on display!
{
	#ifdef __HW_DISABLED
	return; //Disabled?
	#endif
	addtimer(VGA_VerticalRefreshRate(VGA),&VGA_Sequencer,"VGA_ScanLine",__SCREEN_LINES_LIMIT,0,NULL); //Re-add the Scanline to the timers!
}