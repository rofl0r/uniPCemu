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

//Disable this hardware?
//#define __HW_DISABLED
//Limit the VGA to run slower on too slow PCs? Check at least this many pixels if defined before locking on the speed!
#define LIMITVGA 1000

/*

Renderer mini-optimizations.

*/

float oldrate = 0.0f; //The old rate we're using!

double VGA_timing = 0.0f; //No timing yet!
double VGA_rendertiming = 0.0f; //Time for the renderer to tick!

TicksHolder VGA_test;
uint_64 VGA_limit = 0.0f; //Our speed factor!

#ifdef LIMITVGA
uint_32 passedcounter = LIMITVGA; //Times to check for speed with LIMITVGA
#endif

byte VGA_vtotal = 0; //Are we detecting VTotal?

byte currentVGASpeed = 0; //Default: run at 100%!
byte SynchronizationMode = 0; //Synchronization mode when used: 0=Old style, 1=New style

//0=Automatic synchronization, 1=Tightly synchronized with the CPU emulation.
void setVGASpeed(byte setting)
{
	if (setting) //New style setting?
	{
		if (setting==1) //Modern Automatic synchronization and request to tightly synchronize?
		{
			if (currentVGASpeed) //Set?
			{
				currentVGASpeed = 0; //Start tight synchronization!
			}
		}
		else if ((!currentVGASpeed) && (setting==2)) //Tightly synchronized and request to use automatic synchronization?
		{
			passedcounter = LIMITVGA; //Start speed detection with this many items!
			currentVGASpeed = 1; //Start automatic synchronization!
			SynchronizationMode = 1; //New style synchronization!
		}
		//When there's no change, do nothing!
	}
	else //Old style synchronization method?
	{
		currentVGASpeed = 1; //Start automatic synchronization!
		passedcounter = 1; //Don't apply passed counter! As long as we're >0 to apply synchronization!
		SynchronizationMode = 0; //Old style synchronization!		
	}
}

void adjustVGASpeed()
{
	#ifdef LIMITVGA
	passedcounter = LIMITVGA; //Start counting this many times before locking to the speed!
	#endif
}

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
		adjustVGASpeed(); //Auto-adjust our speed!
	}
}

void VGA_initTimer()
{
	VGA_timing = 0.0f; //We're starting to run now!
	oldrate = VGA_VerticalRefreshRate(getActiveVGA()); //Initialise the default rate!
	VGA_rendertiming = 1000000000.0f/oldrate; //Handle this rate from now on!
	initTicksHolder(&VGA_test);
	adjustVGASpeed(); //Auto-adjust our speed!
}

extern GPU_type GPU;

OPTINLINE byte doVGA_Sequencer() //Do we even execute?
{
	if (!getActiveVGA()) //Invalid VGA? Don't do anything!
	{
		//unlockVGA();
		return 0; //Abort: we're disabled without a invalid VGA!
	}
	if (!(getActiveVGA()->registers->SequencerRegisters.REGISTERS.RESETREGISTER.SR && getActiveVGA()->registers->SequencerRegisters.REGISTERS.RESETREGISTER.AR)) //Reset sequencer?
	{
		return 0; //Abort: we're disabled!
	}
	//if (!memprotect(GPU.emu_screenbuffer, 4, "EMU_ScreenBuffer")) //Invalid framebuffer? Don't do anything!
	if (!GPU.emu_screenbuffer) //Invalid screen buffer?
	{
		//unlockVGA();
		//unlockGPU(); //Unlock the VGA&GPU for Software access!
		return 0; //Abort: we're disabled!
	}
	return 1; //We can render something!
}

extern word signal_x, signal_scanline; //Signal location!
byte Sequencer_run; //Sequencer breaked (loop exit)?
byte isoutputdisabled = 0; //Output disabled?

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
			if (VGA->registers->specialCGAflags&1) goto CGAendhblank;
			blankretraceendpending |= VGA_SIGNAL_HBLANKEND;
		}
		else if (blankretraceendpending&VGA_SIGNAL_HBLANKEND) //End pending HBlank!
		{
			CGAendhblank:
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
			if (VGA->registers->specialCGAflags&1) goto CGAendvblank;
			blankretraceendpending |= VGA_SIGNAL_VBLANKEND;
		}
		else if (blankretraceendpending&VGA_SIGNAL_VBLANKEND) //End pending HBlank!
		{
			CGAendvblank:
			vblank = 0; //We're not blanking anymore!
			blankretraceendpending &= ~VGA_SIGNAL_VBLANKEND; //Remove from flags pending!
		}
	}

	if (VGA->registers->specialCGAflags&1) //CGA compatibility mode?
	{
		isoutputdisabled = (((~VGA->registers->Compatibility_CGAModeControl)&8)>>3); //This bit disables input!
	}

	hblank |= isoutputdisabled; //We're enforcing blanking when output is disabled!
	
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
	VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.DisplayDisabled = (retracing = isretrace)&isoutputdisabled; //Vertical or horizontal retrace?

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

OPTINLINE word get_display(VGA_Type *VGA, word Scanline, word x) //Get/adjust the current display part for the next pixel (going from 0-total on both x and y)!
{
	register word stat; //The status of the pixel!
	//We are a maximum of 4096x1024 size!
	Scanline &= 0x3FF; //Range safety: 1024 scanlines!
	x &= 0xFFF; //Range safety: 4095 columns!
	stat = VGA->CRTC.rowstatus[Scanline]; //Get row status!
	stat |= VGA->CRTC.colstatus[x]; //Get column status!
	return stat; //Give the combined (OR'ed) status!
}

OPTINLINE static void VGA_Sequencer(SEQ_DATA *Sequencer)
{
	//if (!lockVGA()) return; //Lock ourselves!
	static word displaystate = 0; //Last display state!
	
	//All possible states!
	if (!displayrenderhandler[0][0]) initStateHandlers(); //Init our display states for usage when needed!
	if (!Sequencer->extrastatus) Sequencer->extrastatus = &getActiveVGA()->CRTC.extrahorizontalstatus[0]; //Start our extra status at the beginning of the row!

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
	#ifdef LIMITVGA
	uint_64 limitcalc=0,renderingsbackup=0;
	double timeprocessed=0.0;
	#endif
	VGA_timing += timepassed; //Time has passed!
	if ((VGA_timing >= VGA_rendertiming) && VGA_rendertiming) //Might have passed?
	{
		uint_64 renderings;
		renderings = (uint_64)(VGA_timing/VGA_rendertiming); //Ammount of times to render!
		VGA_timing -= (renderings*VGA_rendertiming); //Rest the amount we can process!

		#ifdef LIMITVGA
		if ((renderings>VGA_limit) && VGA_limit) //Limit broken?
		{
			renderings = VGA_limit; //Limit the processing to the amount of time specified!
		}
		#endif
		if (!renderings) return; //Nothing to render!
		#ifdef LIMITVGA
		if (passedcounter && currentVGASpeed) //Still counting?
		{
			timeprocessed = (renderings*VGA_rendertiming); //How much are we processing?
			renderingsbackup = renderings; //Save the backup for comparision!
			VGA_vtotal = 0; //Reset our flag to detect finish of a frame while measuring!
		}
		#endif

		if (!doVGA_Sequencer()) return; //Don't execute the sequencer if requested to!

		SEQ_DATA *Sequencer;
		Sequencer = GETSEQUENCER(getActiveVGA()); //Our sequencer!

		#ifdef LIMITVGA
		if (passedcounter && currentVGASpeed) getnspassed(&VGA_test); //Still counting? Then count our interval!
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
				renderings -= 9; //We've processed 9 more!
			}
			VGA_Sequencer(Sequencer); //Tick the VGA once!
		} while (--renderings); //Ticks left to tick?
		#ifdef LIMITVGA
		if (passedcounter && currentVGASpeed) //Still counting?
		{
			limitcalc = getnspassed(&VGA_test); //How long have we taken?

			//timeprocessed=how much time to use, limitcalc=how much time we have taken, renderingsbackup=How many pixels have we processed.
			VGA_limit = (uint_64)(((float)renderingsbackup/(float)limitcalc)*timeprocessed); //Don't process any more than we're allowed to (timepassed).
			if (limitcalc<=timeprocessed) VGA_limit = 0; //Don't limit if we're running at full speed (we're below time we are allowed to process)!
			if (SynchronizationMode) --passedcounter; //A part has been rendered! Only with
		}
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
	switch (BIOS_Settings.VGA_Mode) //What precursor compatibility mode?
	{
		default: //Pure VGA?
		case 0: //Pure VGA?
			setVGA_NMIonPrecursors(0); //No NMI on precursors!
			setVGA_CGA(0); //No CGA!
			setVGA_MDA(0); //No MDA!
			break;
		case 1: //VGA with NMI?
			setVGA_NMIonPrecursors(BIOS_Settings.VGA_Mode); //Set NMI on precursors!
			setVGA_CGA(0); //No CGA!
			setVGA_MDA(0); //No MDA!
			break;
		case 2: //VGA with CGA?
			setVGA_NMIonPrecursors(0); //Disable NMI on precursors!
			setVGA_CGA(2); //CGA enabled with VGA!
			setVGA_MDA(0); //No MDA!
			break;
		case 3: //VGA with MDA?
			setVGA_NMIonPrecursors(0); //Disable NMI on precursors!
			setVGA_CGA(0); //No CGA!
			setVGA_MDA(2); //MDA enabled with VGA!
			break;
		case 4: //Pure CGA?
			setVGA_NMIonPrecursors(0); //Disable NMI on precursors!
			setVGA_CGA(1); //Pure CGA!
			setVGA_MDA(0); //No MDA!
			break;
		case 5: //Pure MDA?
			setVGA_NMIonPrecursors(0); //Disable NMI on precursors!
			setVGA_CGA(0); //No CGA!
			setVGA_MDA(1); //Pure MDA!
			break;
	}
	setVGASpeed(BIOS_Settings.VGASynchronization); //Apply VGA synchronization setting!
	if (getActiveVGA()) //Gotten an active VGA?
	{
		DAC_updateEntries(getActiveVGA()); //Update all DAC entries according to the current/new color settings!
	}
}