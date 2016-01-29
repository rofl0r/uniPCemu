#include "headers/types.h" //Basic types!

#include "headers/hardware/vga/vga.h" //VGA support!

#include "headers/emu/gpu/gpu.h" //Basic GPU!
#include "headers/emu/gpu/gpu_renderer.h" //Renderer support!
#include "headers/emu/gpu/gpu_emu.h" //Emulator support!
#include "headers/emu/gpu/gpu_text.h" //Emulator support!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate support!
#include "headers/hardware/vga/vga_sequencer.h" //Sequencer itself!
#include "headers/emu/timers.h" //Timer support!
#include "headers/hardware/vga/vga_dacrenderer.h" //DAC renderer support!

//How many lines to render at a time when limited.
#define __SCREEN_LINES_LIMIT 1000

//#define __HW_DISABLED

/*

Renderer mini-optimizations.

*/

float oldrate = 0.0f; //The old rate we're using!

double VGA_timing = 0.0f; //No timing yet!
double VGA_rendertiming = 0.0f; //Time for the renderer to tick!

void changeRowTimer(VGA_Type *VGA, word lines) //Change the VGA row processing timer the ammount of lines on display!
{
	#ifdef __HW_DISABLED
	return; //Disabled?
	#endif
	float rate;
	rate = VGA_VerticalRefreshRate(VGA); //Get our rate first!
	if (rate!=oldrate) //New rate has been specified?
	{
		oldrate = rate; //We've updated to this rate!
		VGA_rendertiming = 1000000000.0f/rate; //Handle this rate from now on! Keep us locked though to prevent screen updates messing with this!
	}
}

void VGA_initTimer()
{
	VGA_timing = 0.0f; //We're starting to run now!
	oldrate = VGA_VerticalRefreshRate(getActiveVGA()); //Initialise the default rate!
	VGA_rendertiming = 1000000000.0f/oldrate; //Handle this rate from now on!
}

//CPU cycle locked version of VGA rendering!
void updateVGA(double timepassed)
{
	VGA_timing += timepassed; //Time has passed!
	if (VGA_timing >= VGA_rendertiming) //Might have passed?
	{
		if ((VGA_timing >= VGA_rendertiming) && VGA_rendertiming) //Thread safe solution to verify!
		{
			for (;VGA_timing >= VGA_rendertiming;) //Ticks left to tick?
			{
				VGA_Sequencer(); //Tick the VGA once!
				VGA_timing -= VGA_rendertiming; //Decrease the time left to render!
			}
		}
	}
}

extern BIOS_Settings_TYPE BIOS_Settings; //Our settings!

void EMU_update_VGA_Settings() //Update the VGA settings!
{
	DAC_Use_BWMonitor((BIOS_Settings.bwmonitor>0) ? 1 : 0); //Select color/bw monitor!
	if (DAC_Use_BWMonitor(0xFF)) //Using a b/w monitor?
	{
		DAC_BWColor(BIOS_Settings.bwmonitor); //Set the color to use!
	}
	setVGA_NMIonPrecursors(BIOS_Settings.VGA_NMIonPrecursors); //Set NMI on precursors!
}