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
#include "headers/hardware/vga/vga_crtcontroller.h" //CRT Controller for finishing up!
#include "headers/hardware/pic.h" //IRQ support!
#include "headers/support/highrestimer.h" //Automatic timer support!

//Are we disabled?
//#define __HW_DISABLED
//Are we to limit the VGA emulation based on the host CPU speed?
#define VGA_LIMITER

/*

Renderer mini-optimizations.

*/

float oldrate = 0.0f; //The old rate we're using!

double VGA_timing = 0.0f; //No timing yet!
double VGA_rendertiming = 0.0f; //Time for the renderer to tick!

TicksHolder VGA_test;
uint_64 VGA_limit = 0.0f; //Our speed factor!

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
	initTicksHolder(&VGA_test);
}

extern GPU_type GPU;

OPTINLINE byte doVGA_Sequencer() //Do we even execute?
{
	if (!memprotect(getActiveVGA(), sizeof(VGA_Type), "VGA_Struct")) //Invalid VGA? Don't do anything!
	{
		//unlockVGA();
		return 0; //Abort: we're disabled without a invalid VGA!
	}
	if (!(getActiveVGA()->registers->SequencerRegisters.REGISTERS.RESETREGISTER.SR && getActiveVGA()->registers->SequencerRegisters.REGISTERS.RESETREGISTER.AR)) //Reset sequencer?
	{
		return 0; //Abort: we're disabled!
	}
	if (!memprotect(GPU.emu_screenbuffer, 4, "EMU_ScreenBuffer")) //Invalid framebuffer? Don't do anything!
	{
		//unlockVGA();
		//unlockGPU(); //Unlock the VGA&GPU for Software access!
		return 0; //Abort: we're disabled!
	}
	return 1; //We can render something!
}

extern word signal_x, signal_scanline; //Signal location!
byte Sequencer_run; //Sequencer breaked (loop exit)?

//Special states!
extern byte blanking; //Are we blanking!
extern byte retracing; //Allow rendering by retrace!
extern byte totalling; //Allow rendering by total!
extern byte totalretracing; //Combined flags of retracing/totalling!

extern byte hblank, hretrace; //Horizontal blanking/retrace?
extern byte vblank, vretrace; //Vertical blanking/retrace?
extern word blankretraceendpending; //Ending blank/retrace pending? bits set for any of them!

OPTINLINE void VGA_SIGNAL_HANDLER(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal)
{
	//Blankings
	if (signal&VGA_SIGNAL_HBLANKSTART) //HBlank start?
	{
		hblank = 1; //We're blanking!
	}
	else if (hblank)
	{
		if (signal&VGA_SIGNAL_HBLANKEND) //HBlank end?
		{
			blankretraceendpending |= VGA_SIGNAL_HBLANKEND;
		}
		else if (blankretraceendpending&VGA_SIGNAL_HBLANKEND) //End pending HBlank!
		{
			hblank = 0; //We're not blanking anymore!
			blankretraceendpending &= ~VGA_SIGNAL_HBLANKEND; //Remove from flags pending!
		}
	}

	if (signal&VGA_SIGNAL_VBLANKSTART) //VBlank start?
	{
		vblank = 1; //We're blanking!
	}
	else if (vblank)
	{
		if (signal&VGA_SIGNAL_VBLANKEND) //VBlank end?
		{
			blankretraceendpending |= VGA_SIGNAL_VBLANKEND;
		}
		else if (blankretraceendpending&VGA_SIGNAL_VBLANKEND) //End pending HBlank!
		{
			vblank = 0; //We're not blanking anymore!
			blankretraceendpending &= ~VGA_SIGNAL_VBLANKEND; //Remove from flags pending!
		}
	}
	
	//Both H&VBlank count!
	blanking = hblank;
	blanking |= vblank; //Process blank!
	
	//Retraces
	if (signal&VGA_SIGNAL_HRETRACESTART) //HRetrace start?
	{
		if (!hretrace) //Not running yet?
		{
			VGA_HRetrace(Sequencer, VGA); //Execute the handler!
		}
		hretrace = 1; //We're retracing!
	}
	else if (hretrace)
	{
		if (signal&VGA_SIGNAL_HRETRACEEND) //HRetrace end?
		{
			hretrace = 0; //We're not retraing anymore!
		}
	}

	if (signal&VGA_SIGNAL_VRETRACESTART) //VRetrace start?
	{
		if (!vretrace) //Not running yet?
		{
			VGA_VRetrace(Sequencer, VGA); //Execute the handler!

			//VGA/EGA vertical retrace interrupt support!
			if (VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalInterrupt_NotCleared) //Enabled vertical retrace interrupt?
			{
				if (!VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalInterrupt_Disabled) //Generate vertical retrace interrupts?
				{
					doirq(VGA_IRQ); //Execute the CRT interrupt when possible!
				}
				VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.CRTInterruptPending = 1; //We're pending an CRT interrupt!
			}
		}
		vretrace = 1; //We're retracing!
	}
	else if (vretrace)
	{
		if (signal&VGA_SIGNAL_VRETRACEEND) //VRetrace end?
		{
			vretrace = 0; //We're not retracing anymore!
		}
	}

	VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.VRetrace = vretrace; //Vertical retrace?

	register byte isretrace;
	isretrace = hretrace;
	isretrace |= vretrace; //We're retracing?
	//Retracing disables output!

	VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.DisplayDisabled = retracing = isretrace; //Vertical or horizontal retrace?

	totalling = 0; //Default: Not totalling!
	//Totals
	if (signal&VGA_SIGNAL_HTOTAL) //HTotal?
	{
		VGA_HTotal(Sequencer,VGA); //Process HTotal!
		totalling = 1; //Total reached!
	}
	if (signal&VGA_SIGNAL_VTOTAL) //VTotal?
	{
		VGA_VTotal(Sequencer,VGA); //Process VTotal!
		totalling = 1; //Total reached!
	}
	
	totalretracing = totalling;
	totalretracing <<= 1; //1 bit needed more!
	totalretracing |= retracing; //Are we retracing?
}

extern DisplayRenderHandler displayrenderhandler[4][0x10000]; //Our handlers for all pixels!

OPTINLINE static void VGA_Sequencer(SEQ_DATA *Sequencer)
{
	//if (!lockVGA()) return; //Lock ourselves!
	static word displaystate = 0; //Last display state!
	
	//All possible states!
	if (!displayrenderhandler[0][0]) initStateHandlers(); //Init our display states for usage when needed!

	/*if (!lockGPU()) //Lock the GPU for our access!
	{
		//unlockVGA();
		return;
	}*/

	Sequencer_run = 1; //We're running!

	//Process one pixel only!
	signal_x = Sequencer->x;
	signal_scanline = Sequencer->Scanline;
	displaystate = get_display(getActiveVGA(), Sequencer->Scanline, Sequencer->x++); //Current display state!
	VGA_SIGNAL_HANDLER(Sequencer, getActiveVGA(), displaystate); //Handle any change in display state first!
	displayrenderhandler[totalretracing][displaystate](Sequencer, getActiveVGA()); //Execute our signal!

	//unlockVGA(); //Unlock the VGA for Software access!
	//unlockGPU(); //Unlock the GPU for Software access!
}

//CPU cycle locked version of VGA rendering!
void updateVGA(double timepassed)
{
	#ifdef VGA_LIMITER
	uint_64 limitcalc,renderingsbackup;
	double timeprocessed;
	#endif
	VGA_timing += timepassed; //Time has passed!
	if ((VGA_timing >= VGA_rendertiming) && VGA_rendertiming) //Might have passed?
	{
		uint_64 renderings;
		renderings = (uint_64)(VGA_timing/VGA_rendertiming); //Ammount of times to render!
		VGA_timing -= (renderings*VGA_rendertiming); //Rest the amount we can process!

		#ifdef VGA_LIMITER
		if ((renderings>VGA_limit) && VGA_limit) //Limit broken?
		{
			renderings = VGA_limit; //Limit the processing to the amount of time specified!
		}
		if (!renderings) return; //Nothing to render!
		timeprocessed = (renderings*VGA_rendertiming); //How much are we processing?
		renderingsbackup = renderings; //Save the backup for comparision!
		#endif

		if (!doVGA_Sequencer()) return; //Don't execute the sequencer if requested to!

		SEQ_DATA *Sequencer;
		Sequencer = GETSEQUENCER(getActiveVGA()); //Our sequencer!

		#ifdef VGA_LIMITER
		getnspassed(&VGA_test);
		#endif
		do
		{
			if (renderings>=10) //10+ optimization?
			{
				VGA_Sequencer(Sequencer); //Tick the VGA once!
				VGA_Sequencer(Sequencer); //Tick the VGA once!
				VGA_Sequencer(Sequencer); //Tick the VGA once!
				VGA_Sequencer(Sequencer); //Tick the VGA once!
				VGA_Sequencer(Sequencer); //Tick the VGA once!
				VGA_Sequencer(Sequencer); //Tick the VGA once!
				VGA_Sequencer(Sequencer); //Tick the VGA once!
				VGA_Sequencer(Sequencer); //Tick the VGA once!
				VGA_Sequencer(Sequencer); //Tick the VGA once!
				VGA_Sequencer(Sequencer); //Tick the VGA once!
				renderings -= 19; //We've processed 19 more!
			}
			else //Only one?
			{
				VGA_Sequencer(Sequencer); //Tick the VGA once!
			}
		} while (--renderings); //Ticks left to tick?
		#ifdef VGA_LIMITER
		limitcalc = getnspassed(&VGA_test); //How long have we taken?

		//timeprocessed=how much time to use, limitcalc=how much time we have taken, renderingsbackup=How many pixels have we processed.
		if (limitcalc<=timeprocessed) VGA_limit = 0; //Don't limit if we're running at full speed (we're below time we are allowed to process)!
		else VGA_limit = (uint_64)(((float)renderingsbackup/(float)limitcalc)*timeprocessed); //Don't process any more than we're allowed to (timepassed) otherwise.
		#endif
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