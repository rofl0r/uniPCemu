#include "headers/emu/gpu/gpu.h" //GPU typedefs etc.
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/support/highrestimer.h" //High resolution timer!
#include "headers/emu/threads.h" //Thread support!
#include "headers/emu/gpu/gpu_text.h" //Emulator support text/debug output!
#include "headers/emu/gpu/gpu_sdl.h" //Emulator support text/debug output for displaying framerate only!
#include "headers/support/log.h" //Logging support!
#include "headers/emu/gpu/gpu_renderer.h" //For empty rows!
#include "headers/hardware/vga.h" //VGA!
#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer support!

//Are we disabled?
#define __HW_DISABLED 0

//Define pixel(stage/(scan&)newline) speed?
#define DEBUG_PIXEL_SPEED
//Framerate step in us!
#define FRAMERATE_STEP 1000000
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

extern uint_32 ms_render; //MS it took to render (125000 for 8fps, which is plenty!)

extern VGA_Type *ActiveVGA; //Active VGA!
uint_32 SCREENS_RENDERED = 0; //Ammount of GPU screens rendered!

PSP_TEXTSURFACE *frameratesurface = NULL; //Framerate surface!

//Everything from the renderer:
uint_32 frames; //Frames processed!
float curscanlinepercentage = 0.0f; //Current scanline percentage (0.0-1.0)!

void GPU_FrameRendered() //A frame has been rendered?
{
	++frames; //A frame has been rendered!
	curscanlinepercentage = 0.0f; //Reset for future references!
}

//The main thread!
void GPU_Framerate_Thread() //One second has passed thread (called every second!)?
{
	if (__HW_DISABLED) return; //Disabled?
	TicksHolder lastcheck; //Last check we did!
	initTicksHolder(&lastcheck); //Init for counting!
	uint_64 timepassed;

	while (1) //Not done yet?
	{
		timepassed = getuspassed(&lastcheck); //Real time passed!
		if (timepassed) //Time passed?
		{
			//Update total framerate data!
			totalframes += frames; //Add to the total frames rendered!
			totalstepssec += timepassed; //Add to total steps!

			//Recalculate totals!
			framerate = (frames+curscanlinepercentage)/(timepassed/1000000.0f); //Calculate framerate!
			totalframerate = (totalframes+curscanlinepercentage)/(totalstepssec/1000000.0f); //Calculate total framerate!

			frames = 0; //Reset complete frames counted for future reference!
		}
		#ifdef LOG_VGA_SPEED
		logVGASpeed(); //Log the speed for our frames!
		#endif
		//Finally delay for next update!
		delay(FRAMERATE_STEP); //Wait for the next update as good as we can!
	}
}

void finish_screen() //Extra stuff after rendering!
{
	++SCREENS_RENDERED; //Count ammount of screens rendered!
}

void renderFramerate()
{
	if (frameratesurface) //Existing surface and showing?
	{
		GPU_textgotoxy(frameratesurface,0,0); //For output!
		if (GPU.show_framerate)
		{
			SEQ_DATA *Sequencer;
			VGA_Type *VGA;
			if ((VGA = getActiveVGA())) //Gotten active VGA?
			{
				Sequencer = (SEQ_DATA *)VGA->Sequencer; //Sequencer!
				GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x22,0x22,0x22),"FPS: %02.5f, AVG: %02.5f, Render time: %09ius",
					framerate, //Current framrate (FPS)
					totalframerate, //AVG framerate (FPS)
					ms_render //Time it took to render (MS)
					); //Show the framerate and average!
				GPU_textgotoxy(frameratesurface,0,1); //Goto row 1!
				GPU_textprintf(frameratesurface,RGB(0xFF,0x00,0x00),RGB(0x22,0x22,0x22),"Frames rendered: %i",totalframes); //Total # of frames rendered!

				#ifdef DEBUG_PIXEL_SPEED
				if (Sequencer->totalpixels) //Valid pixels to check speed?
				{
					//GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0xBB,0x00,0x00),"\nPixels rendered: %i               ",Sequencer->totalpixels); //Log the time taken per pixel AVG!
					//GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0xBB,0x00,0x00),"\nPixels time rendered: %i               ",Sequencer->totalpixeltime); //Log the time taken per pixel AVG!

					//GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0xBB,0x00,0x00),"\nRenderer rendered: %i               ",Sequencer->totalrenders); //Log the time taken per pixel AVG!
					//GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0xBB,0x00,0x00),"\nRenderer time rendered: %i               ",Sequencer->totalrendertime); //Log the time taken per pixel AVG!

					GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0xBB,0x00,0x00),"\nVGA@Scanline: %i               ",Sequencer->Scanline); //Log the time taken per pixel AVG!

					float avgpixeltime;
					avgpixeltime = (float)((float)Sequencer->totalpixeltime/(float)Sequencer->totalpixels); //Calculate!
					GPU_textgotoxy(frameratesurface,0,20);
					GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0xBB,0x00,0x00),"\nAVG pixel time: %02.5f us   ",avgpixeltime); //Log the time taken per pixel AVG!
					uint_32 avgpixels;
					if (avgpixeltime) //Gotten time at all?
					{
						avgpixels = (uint_32)((float)PS_SECOND/(float)avgpixeltime);
					}
					else
					{
						avgpixels = 0; //None!
					}
					GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0xBB,0x00,0x00),"\nAVG pixels/second: %i               ",avgpixels); //Log the time taken per pixel AVG!
					if (avgpixels) //Any processed a second?
					{
						GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0xBB,0x00,0x00),"\nAVG FPS@640x480: %f               ",(float)((float)avgpixels/(640.0f*480.0f))); //Log the time taken per pixel AVG!
					}
				}
				#endif
			}
		}
		else //Don't debug framerate, but still render?
		{
			GPU_textclearscreen(frameratesurface); //Clear the rows we use!
		}
	}
}

void initFramerate()
{
	frameratesurface = alloc_GPUtext(); //Allocate GPU text surface for us to use!
	if (!frameratesurface) return; //Couldn't allocate the surface!
	GPU_addTextSurface(frameratesurface,&renderFramerate); //Register our renderer!
	if (!framerate_running) //Not running yet and enabled?
	{
		startThread(&GPU_Framerate_Thread,"Framerate",DEFAULT_PRIORITY); //Framerate thread!
		framerate_running = 1; //Already running!
	}
	else
	{
		frames = 0; //Reset frames!
	}
}

void doneFramerate()
{
	free_GPUtext(&frameratesurface); //Release the framerate!
}

extern GPU_type GPU; //The GPU itself!

extern GPU_SDL_Surface *rendersurface; //The PSP's surface!

void renderFramerateOnly()
{
	if (frameratesurface) //Existing surface?
	{
		uint_32 *emptyrow = get_rowempty(); //Get empty row!
		uint_32 y;
		for (y=0;y<rendersurface->sdllayer->h;y++)
		{
			put_pixel_row(rendersurface,y,PSP_SCREEN_COLUMNS,emptyrow,0,0); //Clear the screen!
		}

		framerate_rendertime = GPU_textrenderer(frameratesurface); //Render it!
		renderScreenFrame(); //Render our renderered framerate only!
	}
}

void logVGASpeed()
{
	static uint_32 counter = 0;
	char rendertime[20];
	convertTime(ms_render,&rendertime[0]); //Convert to viewable time!
	if (!(counter++%5)) //To log every 5 callcs?
	{
		dolog("Framerate","FPS: %02.5f, AVG: %02.5f, Render time: %s",
			framerate, //Current framrate (FPS)
			totalframerate, //AVG framerate (FPS)
			rendertime //Time it took to render (MS))
		); //Log the current framerate speed!
	}
}

/*

Frameskip support!

*/

void setGPUFrameskip(byte Frameskip)
{
	GPU.framenr = 0; //Reset frame nr to draw immediately!
	GPU.frameskip = Frameskip;
}