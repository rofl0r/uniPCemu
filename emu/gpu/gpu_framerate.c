#include "headers/emu/gpu/gpu.h" //GPU typedefs etc.
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/support/highrestimer.h" //High resolution timer!
#include "headers/emu/threads.h" //Thread support!
#include "headers/emu/gpu/gpu_text.h" //Emulator support text/debug output!
#include "headers/emu/gpu/gpu_sdl.h" //Emulator support text/debug output for displaying framerate only!
#include "headers/support/log.h" //Logging support!
#include "headers/emu/gpu/gpu_renderer.h" //For empty rows!
#include "headers/hardware/vga/vga.h" //VGA!
#include "headers/hardware/vga/vga_sequencer.h" //Sequencer support!
#include "headers/emu/timers.h" //Timer support!
#include "headers/support/zalloc.h" //Protection for pointers!
#include "headers/support/locks.h" //Locking support!
#include "headers/emu/emucore.h" //Core support for busy flags!

//Are we disabled?
#define __HW_DISABLED 0

//Show BIOS data area Equipment word?
//#define SHOW_EQUIPMENT_WORD

//Debug CPU speed?
//#define DEBUG_CPU_SPEED
//Define pixel(stage/(scan&)newline) speed?
//#define DEBUG_PIXEL_SPEED
//Framerate step in times per second!
#define FRAMERATE_SPEED 1.0f
//Log VGA speed?
//#define LOG_VGA_SPEED

extern GPU_type GPU; //GPU!

byte framerate_running = 0; //Not running by default!

uint_32 framerate_rendertime; //Time for framerate rendering!
extern uint_32 keyboard_rendertime; //See above, but for keyboard!

//Simples for per-second calculation!
float framerate = 0.0f; //Current framerate -1 stands for never updated!
uint_32 totalstepssec = 0; //Total steps (reset every second)

//Totals calculation!
float totalframerate = 0.0f;
uint_32 totalframes = 0;
float totalscanlinepercentage = 0.0f;
uint_32 totalsteps = 0;

byte framerateupdated = 0;

uint_32 SCREENS_RENDERED = 0; //Ammount of GPU screens rendered!

GPU_TEXTSURFACE *frameratesurface = NULL; //Framerate surface!

//Everything from the renderer:
uint_32 frames; //Frames processed!
float curscanlinepercentage = 0.0f; //Current scanline percentage (0.0-1.0)!

TicksHolder lastcheck; //Last check we did!

void GPU_FrameRendered() //A frame has been rendered?
{
	if (GPU.xres && GPU.yres) //Gotten anything rendered?
	{
		lock(LOCK_FRAMERATE);
		++frames; //A frame has been rendered!
		curscanlinepercentage = 0.0f; //Reset for future references!
		unlock(LOCK_FRAMERATE);
	}
}

//The main thread!
void GPU_Framerate_tick() //One second has passed thread (called every second!)?
{
	if (__HW_DISABLED) return; //Disabled?
	uint_64 timepassed;
	lock(LOCK_FRAMERATE); //Lock us!
	timepassed = getuspassed(&lastcheck); //Real time passed!
	if (timepassed) //Time passed?
	{
		//Update total framerate data!
		totalframes += frames; //Add to the total frames rendered!
		totalstepssec += (uint_32)timepassed; //Add to total steps!

		//Recalculate totals!
		framerate = (frames+curscanlinepercentage)/(timepassed/1000000.0f); //Calculate framerate!
		totalframerate = (totalframes+curscanlinepercentage)/(totalstepssec/1000000.0f); //Calculate total framerate!

		frames = 0; //Reset complete frames counted for future reference!
		framerateupdated = 1; //We're updated!
	}
	//Finally delay for next update!
	//delay(FRAMERATE_STEP); //Wait for the next update as good as we can!
	unlock(LOCK_FRAMERATE); //Unlock us!
	#ifdef LOG_VGA_SPEED
	logVGASpeed(); //Log the speed for our frames!
	#endif
}

void finish_screen() //Extra stuff after rendering!
{
	++SCREENS_RENDERED; //Count ammount of screens rendered!
}

extern double last_timing; //Last timing!
extern double last_timing_start; //Start of valid last timing to start counting from!
extern TicksHolder CPU_timing; //CPU timing counter!

OPTINLINE byte getCPUSpeedPercentage()
{
	double result, last_timing_current, last_timing_start_current, timenow;
	//First, get current state!
	//lock(LOCK_CPU); //Lock the CPU!
	timenow = (double)getnspassed_k(&CPU_timing); //Current time!
	last_timing_current = last_timing; //Last timing!
	last_timing_start_current = last_timing_start; //Last timing start!
	//unlock(LOCK_CPU); //Finished with the CPU!
	
	//Apply start time to get the relative time since last check!
	last_timing_current -= last_timing_start_current; //Decrease by the timing when we started!
	timenow -= last_timing_start_current; //Decrease by the timing when we started!

	//Give the current result!
	if (!timenow) return 0; //Nothing yet, since no time has passed yet!
	result = (last_timing_current/timenow)*100.0f; //Give the current speed percentage (still in ns percision)!
	if (result<1.0) //<1.0%?
	{
		result = (result>0.0)?1.0:0.0; //Patch 0%-1% to 1% and 0% to 0%!
	}
	return (byte)round(result); //Give the current speed percentage!
}

void renderFramerate()
{
	static byte CPUspeed; //Current CPU speed!
	if (frameratesurface) //Existing surface and showing?
	{
		GPU_text_locksurface(frameratesurface); //Lock!
		if (GPU.show_framerate)
		{
			GPU_textclearrow(frameratesurface, 0); //Clear the first row!
			GPU_textgotoxy(frameratesurface, 0, 0); //For output!
			lock(LOCK_FRAMERATE); //We're using framerate info!
			GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x22,0x22,0x22),"FPS: %02.5f, AVG: %02.5f",
				framerate, //Current framrate (FPS)
				totalframerate //AVG framerate (FPS)
				); //Show the framerate and average!
			GPU_textclearcurrentrownext(frameratesurface); //Clear the rest of the current row!
			unlock(LOCK_FRAMERATE);
			#ifdef DEBUG_PIXEL_SPEED
				SEQ_DATA *Sequencer;
				VGA_Type *VGA;
				if ((VGA = getActiveVGA())) //Gotten active VGA?
				{
					Sequencer = (SEQ_DATA *)VGA->Sequencer; //Sequencer!
					if (memprotect(Sequencer, sizeof(SEQ_DATA), NULL)) //Readable?
					{
						GPU_textprintf(frameratesurface, RGB(0xFF, 0xFF, 0xFF), RGB(0xBB, 0x00, 0x00), "\nVGA@Scanline: %i               ", Sequencer->Scanline); //Log the time taken per pixel AVG!
					}
				}
			#endif
			#ifdef SHOW_EQUIPMENT_WORD
				if (hasmemory())
				{
					GPU_textgotoxy(frameratesurface, 0, 2);
					GPU_textprintf(frameratesurface, RGB(0xFF, 0xFF, 0xFF), RGB(0x22, 0x22, 0x22), "%04X", MMU_directrw(0x410)); //Show the BIOS equipment word!
				}
			#endif
				if (BIOS_Settings.ShowCPUSpeed) //Are we to show the CPU speed?
				{
					if (framerateupdated) //We're updated!
					{
						framerateupdated = 0; //Not anymore!
						CPUspeed = getCPUSpeedPercentage(); //Current CPU speed percentage (how much the current time is compared to required time)!
					}
					GPU_textprintf(frameratesurface, RGB(0xFF, 0xFF, 0xFF), RGB(0xBB, 0x00, 0x00), "\nCPU speed: %i%%  ", CPUspeed); //Current CPU speed percentage!
				}
		}
		else //Don't debug framerate, but still render?
		{
			if (BIOS_Settings.ShowCPUSpeed) //Showing the CPU speed?
			{
				if (framerateupdated) //We're to be updated with the framerate rate!
				{
					framerateupdated = 0; //Not anymore!
					CPUspeed = getCPUSpeedPercentage(); //Current CPU speed percentage (how much the current time is compared to required time)!
				}
				GPU_textgotoxy(frameratesurface, 0, 0); //For output!
				GPU_textprintf(frameratesurface, RGB(0xFF, 0xFF, 0xFF), RGB(0xBB, 0x00, 0x00), "CPU speed: %i%%  ", CPUspeed); //Current CPU speed percentage!
				GPU_textclearcurrentrownext(frameratesurface); //Clear the rest of the current row!
			}
			else
			{
				GPU_textclearrow(frameratesurface, 0); //Clear the rows we use!
			}
			int i;
			for (i = 0;i < (GPU_TEXTSURFACE_WIDTH - 6);i++)
			{
				GPU_textsetxy(frameratesurface,i,1,0,0,0); //Clear a bit until the busy indicators!
			}
			GPU_textclearrow(frameratesurface, 2); //Clear the rows we don't use!
			EMU_drawBusy(0); //Draw busy flag disk A!
			EMU_drawBusy(1); //Draw busy flag disk B!
			EMU_drawBusy(2); //Draw busy flag disk C!
			EMU_drawBusy(3); //Draw busy flag disk D!
			EMU_drawBusy(4); //Draw busy flag disk E!
			EMU_drawBusy(5); //Draw busy flag disk F!
			GPU_textclearcurrentrownext(frameratesurface); //Clear the rest of the current row!
		}
		GPU_text_releasesurface(frameratesurface); //Unlock!
	}
}

void doneFramerate()
{
	free_GPUtext(&frameratesurface); //Release the framerate!
	removetimer("framerate");
}

void initFramerate()
{
	if (frameratesurface) //Already allocated?
	{
		doneFramerate(); //Finish first!
	}
	initTicksHolder(&lastcheck); //Init for counting!
	frameratesurface = alloc_GPUtext(); //Allocate GPU text surface for us to use!
	if (!frameratesurface) return; //Couldn't allocate the surface!
	GPU_addTextSurface(frameratesurface,&renderFramerate); //Register our renderer!
	if (!framerate_running) //Not running yet and enabled?
	{
		framerate_running = 1; //Already running!
	}
	else
	{
		frames = 0; //Reset frames!
	}
	addtimer(FRAMERATE_SPEED,&GPU_Framerate_tick,"Framerate",1,1,NULL);
}

extern GPU_SDL_Surface *rendersurface; //The PSP's surface!

void renderFramerateOnly()
{
	if (frameratesurface) //Existing surface?
	{
		lockGPU(); //Lock the GPU!
		uint_32 *emptyrow = get_rowempty(); //Get empty row!
		uint_32 y;
		for (y=0;y<(uint_32)rendersurface->sdllayer->h;y++)
		{
			put_pixel_row(rendersurface,y,PSP_SCREEN_COLUMNS,emptyrow,0,0); //Clear the screen!
		}

		framerate_rendertime = (uint_32)GPU_textrenderer(frameratesurface); //Render it!
		renderScreenFrame(); //Render our renderered framerate only!
		unlockGPU(); //We're finished with the GPU!
	}
}

void logVGASpeed()
{
	static uint_32 counter = 0;
	if (!(counter++%5)) //To log every 5 callcs?
	{
		dolog("Framerate","FPS: %02.5f, AVG: %02.5f",
			framerate, //Current framrate (FPS)
			totalframerate //AVG framerate (FPS)
		); //Log the current framerate speed!
	}
}

/*

Frameskip support!

*/

void setGPUFrameskip(byte Frameskip)
{
	lockGPU(); //Lock us!
	GPU.framenr = 0; //Reset frame nr to draw immediately!
	GPU.frameskip = Frameskip;
	unlockGPU(); //Lock us!
}

void setGPUFramerate(byte Show)
{
	lockGPU(); //Lock us!
	GPU.show_framerate = Show?1:0; //Show the framerate?
	unlockGPU(); //Unlock us!
}