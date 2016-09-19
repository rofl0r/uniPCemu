#include "headers/types.h" //Basic types!

#include "headers/hardware/vga/vga.h" //VGA support!

#include "headers/emu/gpu/gpu.h" //Basic GPU!
#include "headers/emu/gpu/gpu_renderer.h" //Renderer support!
#include "headers/emu/gpu/gpu_text.h" //Emulator support!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate support!
#include "headers/hardware/vga/vga_renderer.h" //Renderer itself!
#include "headers/emu/timers.h" //Timer support!
#include "headers/hardware/vga/vga_dacrenderer.h" //DAC renderer support!
#include "headers/hardware/vga/vga_crtcontroller.h" //CRT Controller for finishing up!
#include "headers/hardware/vga/vga_cga_mda.h" //CGA/MDA support!
#include "headers/hardware/pic.h" //IRQ support!
#include "headers/support/highrestimer.h" //Automatic timer support!
#include "headers/cpu/cpu.h" //Currently emulated CPU for wait states!

//Disable this hardware?
//#define __HW_DISABLED
//Limit the VGA to run slower on too slow PCs? Check at least this many pixels if defined before locking on the speed!
#define LIMITVGA 1000

/*

Renderer mini-optimizations.

*/

double oldrate = 0.0f; //The old rate we're using!

double VGA_timing = 0.0; //No timing yet!
double VGA_debugtiming = 0.0; //Debug countdown if applyable!
byte VGA_debugtiming_enabled = 0; //Are we applying right now?
float VGA_rendertiming = 0.0f; //Time for the renderer to tick!

TicksHolder VGA_test;
float VGA_limit = 0.0f; //Our speed factor!

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

void changeRowTimer(VGA_Type *VGA) //Change the VGA row processing timer the ammount of lines on display!
{
	#ifdef __HW_DISABLED
	return; //Disabled?
	#endif
	double rate;
	rate = VGA_VerticalRefreshRate(VGA); //Get our rate first!
	if (rate!=oldrate) //New rate has been specified?
	{
		oldrate = rate; //We've updated to this rate!
		VGA_rendertiming = (float)(1000000000.0/rate); //Handle this rate from now on! Keep us locked though to prevent screen updates messing with this!
		adjustVGASpeed(); //Auto-adjust our speed!
	}
}

void VGA_initTimer()
{
	VGA_timing = 0.0f; //We're starting to run now!
	oldrate = VGA_VerticalRefreshRate(getActiveVGA()); //Initialise the default rate!
	VGA_rendertiming = (float)(1000000000.0/oldrate); //Handle this rate from now on!
	initTicksHolder(&VGA_test);
	adjustVGASpeed(); //Auto-adjust our speed!
}

extern GPU_type GPU;

extern byte allcleared; //Are all pointers cleared?

OPTINLINE byte doVGA_Sequencer() //Do we even execute?
{
	if (!getActiveVGA() || allcleared) //Invalid VGA? Don't do anything!
	{
		return 0; //Abort: we're disabled without a invalid VGA!
	}
	if (!GPU.emu_screenbuffer) //Invalid screen buffer?
	{
		return 0; //Abort: we're disabled!
	}
	return 1; //We can render something!
}

byte isoutputdisabled = 0; //Output disabled?

//Special states!
extern byte blanking; //Are we blanking!
extern byte retracing; //Allow rendering by retrace!
extern byte totalling; //Allow rendering by total!
byte totalretracing; //Combined flags of retracing/totalling!

extern byte hblank, hretrace; //Horizontal blanking/retrace?
extern byte vblank, vretrace; //Vertical blanking/retrace?
byte hblankendpending = 0; //Ending blank/retrace pending? bits set for any of them!
byte vblankendpending = 0; //Ending blank/retrace pending? bits set for any of them!

byte vtotal = 0; //VTotal busy?

byte VGA_hblankstart = 0; //HBlank started?
extern byte CGAMDARenderer; //CGA/MDA renderer?

OPTINLINE uint_32 get_display(VGA_Type *VGA, word Scanline, word x) //Get/adjust the current display part for the next pixel (going from 0-total on both x and y)!
{
	INLINEREGISTER uint_32 stat; //The status of the pixel!
	//We are a maximum of 4096x1024 size!
	Scanline &= 0x3FF; //Range safety: 1024 scanlines!
	x &= 0xFFF; //Range safety: 4095 columns!
	stat = VGA->CRTC.rowstatus[Scanline]; //Get row status!
	stat |= VGA->CRTC.colstatus[x]; //Get column status!
	stat |= VGA->precalcs.extrasignal; //Graphics mode etc. display status affects the signal too!
	stat |= (blanking<<VGA_SIGNAL_BLANKINGSHIFT); //Apply the current blanking signal as well!
	VGA_hblankstart = stat; //Save directly! Ignore the overflow!
	VGA_hblankstart >>= 7; //Shift into bit 1 to get the hblank status(small hack)!
	return stat; //Give the combined (OR'ed) status!
}

word displaystate; //Last display state!

//All possible wait-states for the video adapter!
typedef void (*WaitStateHandler)(VGA_Type *VGA);

void updateVGAWaitState(); //Prototype!

void WaitState_None(VGA_Type *VGA) {} //No waitstate: NOP!
void WaitState_WaitDots(VGA_Type *VGA)
{
	//Wait 8 hdots!
	if (--VGA->WaitStateCounter == 0) //First wait state done?
	{
		VGA->WaitState = 2; //Enter the next phase: Wait for the next lchar(16 dots period)!
		updateVGAWaitState(); //Update the waitstate!
	}
}
void WaitState_NextlChar(VGA_Type *VGA)
{
	//Wait for the next lchar?
	if ((VGA->PixelCounter & 0xF) == 0) //Second wait state done?
	{
		VGA->WaitState = 0; //Enter the next phase: Wait for the next ccycle(3 hdots)
		CPU[activeCPU].halt |= 8; //Start again when the next CPU clock arrives!
		CPU[activeCPU].halt &= ~4; //We're done waiting!
		updateVGAWaitState(); //Update the waitstate!
	}
}

WaitStateHandler CurrentWaitState = NULL; //Current waitstate!

WaitStateHandler WaitStates[8] = { NULL,
WaitState_WaitDots, //Wait 8 hdots!
WaitState_NextlChar, //Wait for the next lchar?
WaitState_None, //Wait for the next ccycle(3 hdots)?
WaitState_None,WaitState_None,WaitState_None,WaitState_None }; //All possible waitstates!

void updateVGAWaitState()
{
	CurrentWaitState = WaitStates[getActiveVGA()->WaitState]; //Load the new waitstate!
}

//HBlank/Retrace handling!

typedef void (*hblankretraceHandler)(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal);

void exechblankretrace(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal)
{
	if (VGA_hblankstart) //HBlank start?
	{
		hblank = 1; //We're blanking!
	}
	else if (hblank)
	{
		if (signal&VGA_SIGNAL_HBLANKEND) //HBlank end?
		{
			if ((VGA->registers->specialCGAMDAflags) & 1)
			{
				//End pending HBlank!
				hblank = 0; //We're not blanking anymore!
				hblankendpending = 0; //Remove from flags pending!
			}
			else
			{
				hblankendpending = 1;
			}
		}
	}

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
}

void nohblankretrace(SEQ_DATA *Sequencer, VGA_Type *VGA, word signal)
{
	if (hblankendpending==0) return; //End pending HBlank!
	{
		hblank = 0; //We're not blanking anymore!
		hblankendpending = 0; //Remove from flags pending!
	}
}

OPTINLINE void VGA_SIGNAL_HANDLER(SEQ_DATA *Sequencer, VGA_Type *VGA, byte *totalretracing, byte hblankretrace)
{
	const static byte retracemasks[4] = { 0xFF,0x00,0x00,0x00 }; //Disable display when retracing!
	const static hblankretraceHandler hblankretracehandlers[2] = { nohblankretrace,exechblankretrace }; //The handlers!

	INLINEREGISTER word tempsignalbackup, tempsignal; //Our signal backup and signal itself!
recalcsignal: //Recalculate the signal to process!
	tempsignal = tempsignalbackup = displaystate; //The back-up of the signal!
	//Blankings

	hblankretracehandlers[hblankretrace](Sequencer,VGA,tempsignal); //Horizontal timing?

	tempsignal = tempsignalbackup; //Restore the original backup signal!
	tempsignal &= VGA_VBLANKRETRACEMASK; //Check for blanking/tretracing!
	if (tempsignal) //VBlank?
	{
		if (tempsignal&VGA_SIGNAL_VBLANKSTART) //VBlank start?
		{
			vblank = 1; //We're blanking!
		}
		else if (vblank)
		{
			if (tempsignal&VGA_SIGNAL_VBLANKEND) //VBlank end?
			{
				if (VGA->registers->specialCGAMDAflags & 1) //CGA special?
				{
					vblank = 0; //We're not blanking anymore!
					vblankendpending = 0; //Remove from flags pending!
					VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.VRetrace = 0; //No vertical retrace?
				}
				else
				{
					vblankendpending = 1; //Start pending vblank end!
				}
			}
		}

		if (tempsignal&VGA_SIGNAL_VRETRACESTART) //VRetrace start?
		{
			if (!vretrace) //Not running yet?
			{
				VGA_VRetrace(Sequencer, VGA); //Execute the handler!

				//VGA/EGA vertical retrace interrupt support!
				if (VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalInterrupt_NotCleared) //Enabled vertical retrace interrupt?
				{
					if (!VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalInterrupt_Disabled) //Generate vertical retrace interrupts?
					{
						raiseirq(VGA_IRQ); //Execute the CRT interrupt when possible!
					}
					VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.CRTInterruptPending = 1; //We're pending an CRT interrupt!
				}
			}
			VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.VRetrace = vretrace = 1; //We're retracing!
		}
		else if (vretrace)
		{
			if (tempsignal&VGA_SIGNAL_VRETRACEEND) //VRetrace end?
			{
				vretrace = 0; //We're not retracing anymore!
				VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.VRetrace = 0; //Vertical retrace?
			}
			else
			{
				VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.VRetrace = 1; //Vertical retrace?
			}
		}
		else //No vretrace?
		{
			VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.VRetrace = 0; //Vertical retrace?
		}
	}
	else
	{
		if (vblankendpending) //End pending HBlank!
		{
			vblank = 0; //We're not blanking anymore!
			vblankendpending = 0; //Remove from flags pending!
		}
		VGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.VRetrace = 0; //No vertical retrace?
	}

	//Both H&VBlank count!
	blanking = hblank;
	blanking |= vblank; //Process blank!
	//Screen disable applies blanking permanently!
	blanking |= VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.ScreenDisable; //Use disabled output when asked to!

	//Process resetting the HSync/VSync counters!

	INLINEREGISTER byte isretrace; //Vertical or horizontal retrace?
	*totalretracing = isretrace = hretrace;
	retracing = (isretrace |= vretrace); //We're retracing?

	//Process HTotal/VTotal
	//INLINEREGISTER byte currenttotalling; //Current totalling state!
	//currenttotalling = 0; //Default: Not totalling!
	tempsignal = tempsignalbackup; //Restore the original backup signal!
	if (tempsignal&VGA_SIGNAL_HTOTAL) //HTotal?
	{
		VGA_HTotal(Sequencer,VGA); //Process HTotal!
		//currenttotalling = 1; //Total reached!
		displaystate = get_display(getActiveVGA(), Sequencer->Scanline, Sequencer->x++); //Current display state!
		if ((displaystate&VGA_SIGNAL_HTOTAL)==0) //Not infinitely looping?
		{
			hblankretrace = (displaystate&VGA_HBLANKRETRACEMASK)?1:0; //Check for blanking/retracing!
			goto recalcsignal; //Execute immediately!
		}
	}
	if (tempsignal&VGA_SIGNAL_VTOTAL) //VTotal?
	{
		VGA_VTotal(Sequencer,VGA); //Process VTotal!
		/*currenttotalling =*/ vtotal = 1; //Total reached!
		displaystate = get_display(getActiveVGA(), Sequencer->Scanline, Sequencer->x++); //Current display state!
		if ((displaystate&VGA_SIGNAL_VTOTAL)==0) //Not infinitely looping(VTotal ended)?
		{
			VGA_VTotalEnd(Sequencer, VGA); //Signal end of vertical total!
			vtotal = 0; //Not vertical total anymore!
			hblankretrace = (displaystate&VGA_HBLANKRETRACEMASK)?1:0; //Check for blanking/retracing!
			goto recalcsignal; //Execute immediately!
		}
	}
	else if (vtotal) //VTotal ended?
	{
		VGA_VTotalEnd(Sequencer,VGA); //Signal end of vertical total!
		vtotal = 0; //Not vertical total anymore!
	}

	tempsignal &= VGA_DISPLAYMASK; //Check the display now!

	INLINEREGISTER byte currenttotalretracing;
	//isretrace |= (currenttotalretracing = currenttotalling); //Apply totalling to retrace checks, also load totalling for totalretracing information!
	//currenttotalretracing <<= 1; //1 bit needed more!
	//currenttotalretracing = isretrace; //Are we retracing?
	//*totalretracing = currenttotalretracing; //Save retrace info!

	currenttotalretracing = (tempsignal==VGA_DISPLAYACTIVE); //We're active display when not retracing/totalling and active display area!
	currenttotalretracing &= retracemasks[isretrace]; //Apply the retrace mask: we're not using the displayenabled when retracing!
	VGA->CRTC.DisplayEnabled = currenttotalretracing; //The Display Enable signal, which depends on the active video adapter how to use it!
	++VGA->PixelCounter; //Simply blindly increase the pixel counter!

	if (CurrentWaitState) CurrentWaitState(VGA); //Execute the current waitstate, when used!
}

extern DisplayRenderHandler displayrenderhandler[4][VGA_DISPLAYRENDERSIZE]; //Our handlers for all pixels!

OPTINLINE static void VGA_Renderer(SEQ_DATA *Sequencer)
{
	static byte totalretracing = 0;
	//Process one pixel only!
	displaystate = get_display(getActiveVGA(), Sequencer->Scanline, Sequencer->x++); //Current display state!
	VGA_SIGNAL_HANDLER(Sequencer, getActiveVGA(),&totalretracing,(displaystate&VGA_HBLANKRETRACEMASK)?1:0); //Handle any change in display state first!
	displayrenderhandler[totalretracing][displaystate](Sequencer, getActiveVGA()); //Execute our signal!
}

//CPU cycle locked version of VGA rendering!
void updateVGA(double timepassed)
{
	#ifdef LIMITVGA
	float limitcalc=0,renderingsbackup=0;
	float timeprocessed=0.0;
	#endif
	VGA_timing += timepassed; //Time has passed!
	
	if (VGA_debugtiming_enabled) //Valid debug timing to apply?
	{
		VGA_debugtiming += timepassed; //Time has passed!
	}

	if ((VGA_timing >= VGA_rendertiming) && VGA_rendertiming) //Might have passed?
	{
		float renderings;
		renderings = floorf((float)(VGA_timing/VGA_rendertiming)); //Ammount of times to render!
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

		//All possible states!
		if (!displayrenderhandler[0][0]) initStateHandlers(); //Init our display states for usage when needed!
		if (!Sequencer->extrastatus) Sequencer->extrastatus = &getActiveVGA()->CRTC.extrahorizontalstatus[0]; //Start our extra status at the beginning of the row!

		#ifdef LIMITVGA
		if (passedcounter && currentVGASpeed) getnspassed(&VGA_test); //Still counting? Then count our interval!
		#endif
		do
		{
			if (renderings<5) VGA_Renderer(Sequencer); //5+ optimization? Not usable? Execute only once!
			else //x+ optimization?
			{
				VGA_Renderer(Sequencer); //Tick the VGA once!
				VGA_Renderer(Sequencer); //Tick the VGA once!
				VGA_Renderer(Sequencer); //Tick the VGA once!
				VGA_Renderer(Sequencer); //Tick the VGA once!
				VGA_Renderer(Sequencer); //Tick the VGA once!
				renderings -= 4; //We've processed 4 more!
			}
		} while (--renderings); //Ticks left to tick?

		getActiveVGA()->registers->ExternalRegisters.INPUTSTATUS1REGISTER.DisplayDisabled = getActiveVGA()->CRTC.DisplayEnabled^1; //Only update the display disabled when required to: it's only needed by the CPU, not the renderer!

		#ifdef LIMITVGA
		if (passedcounter && currentVGASpeed) //Still counting?
		{
			limitcalc = getnspassed(&VGA_test); //How long have we taken?

			//timeprocessed=how much time to use, limitcalc=how much time we have taken, renderingsbackup=How many pixels have we processed.
			VGA_limit = floorf(((float)renderingsbackup/(float)limitcalc)*timeprocessed); //Don't process any more than we're allowed to (timepassed).
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
		case 6: //Tseng ET4000?
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
		byte CGAMode;
		CGAMode = BIOS_Settings.CGAModel; //What CGA is emulated?
		if ((CGAMode&3)!=CGAMode) CGAMode = 0; //Default to RGB, old-style CGA!
		setCGA_NTSC(CGAMode&1); //RGB with modes 0&2, NTSC with modes 1&3
		setCGA_NewCGA(CGAMode&2); //New-style with modes 2&3, Old-style with modes 0&1
	}
}