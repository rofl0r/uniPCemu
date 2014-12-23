#include "headers/types.h" //Global stuff!
#include "headers/emu/gpu/gpu.h" //Our stuff!
#include "headers/mmu/mmu.h" //TEXT mode data!
#include "headers/hardware/vga_rest/textmodedata.h" //TEXT mode data!
#include "headers/hardware/vga_rest/colorconversion.h" //Color conversion functions!
#include "headers/cpu/interrupts.h" //For int10 refresh function!
#include "headers/mmu/bda.h" //BDA support!
#include "headers/hardware/vga.h" //VGA support!
#include "headers/emu/threads.h" //Thread support!
#include "headers/support/highrestimer.h" //High resolution timer!
#include "headers/emu/input.h" //For the on-screen keyboard!
#include "headers/support/zalloc.h" //Memory allocation support!
#include "headers/support/log.h" //Log support!

#include "headers/emu/gpu/gpu_framerate.h" //GPU framerate support!"
#include "headers/emu/gpu/gpu_sdl.h" //SDL support!
#include "headers/emu/gpu/gpu_emu.h" //Emulator support (for resetting)!
#include "headers/emu/gpu/gpu_renderer.h" //Renderer support!

//Are we disabled?
#define __HW_DISABLED 0

GPU_type GPU; //The GPU itself!

GPU_SDL_Surface *rendersurface = NULL; //The PSP's surface to use when flipping! We can only be freed using SDL_Quit.
SDL_Surface *originalrenderer = NULL; //Original renderer from above! Above is just the wrapper!
/*

VIDEO BASICS!

*/

//On starting only.
void initVideoLayer() //We're for allocating the main video layer, only deallocated using SDL_Quit (when quitting the application)!
{
	if (SDL_WasInit(SDL_INIT_VIDEO)) //Initialised?
	{
		if (!originalrenderer) //Not allocated yet?
		{
			SDL_ShowCursor(SDL_DISABLE); //We don't want cursors on empty screens!
			originalrenderer = SDL_SetVideoMode(PSP_SCREEN_COLUMNS, PSP_SCREEN_ROWS, 32, SDL_SWSURFACE); //Start fullscreen, 32BPP pixel mode! Don't use double buffering: this changes our address (too slow to use without in hardware surface, so use sw surface)!
			if (!originalrenderer) //Failed to allocate?
			{
				raiseError("GPU","Error allocating PSP Main Rendering Surface!");
			}
		}
		if (originalrenderer)
		{
			rendersurface = getSurfaceWrapper(originalrenderer); //Allocate a surface wrapper!
			if (rendersurface) //Allocated?
			{
				registerSurface(rendersurface,"PSP SDL Main Rendering Surface",0); //Register, but don't allow release: this is done by SDL_Quit only!
				if (memprotect(rendersurface,sizeof(*rendersurface),NULL)) //Valid?
				{
					if (memprotect(rendersurface->sdllayer,sizeof(*rendersurface->sdllayer),NULL)) //Valid?
					{
						if (!memprotect(rendersurface->sdllayer->pixels,sizeof(*rendersurface->sdllayer->pixels)*get_pixelrow_pitch(rendersurface)*rendersurface->sdllayer->h,NULL)) //Valid?
						{
							raiseError("GPU","Rendering surface pixels not registered!");
						}
					}
					else
					{
						raiseError("GPU","Rendering SDL surface not registered!");
					}
				}
				else
				{
					raiseError("GPU","Rendering surface not registered!");
				}
			}
			else
			{
				raiseError("GPU","Error allocating PSP Main Rendering Surface Wrapper");
			}
		}
	}
}

//Wrapped arround the EMU.
void initVideoMain() //Everything SDL PRE-EMU!
{
	memset(&GPU,0,sizeof(GPU)); //Init all GPU data!
	if (SDL_WasInit(SDL_INIT_VIDEO)) //Initialised?
	{
		initFramerate(); //Start the framerate handler!
		initKeyboardOSK(); //Start the OSK handler!
		allocBIOSMenu(); //BIOS menu has the highest priority!
	}
}

void doneVideoMain() //Everything SDL POST-EMU!
{
	doneKeyboardOSK(); //Stop the OSK handler!
	doneFramerate(); //Finish framerate!
	freeBIOSMenu(); //We're done with the BIOS menu!
	done_GPURenderer(); //Finish up any rest rendering stuff!
}

//Below is called during emulation itself!

void initVideo(int show_framerate) //Initialises the video
{
	if (__HW_DISABLED) return; //Abort!
	//dolog("GPU","Initialising screen buffers...");
	
	//dolog("zalloc","Allocating GPU EMU_screenbuffer...");
	GPU.emu_screenbuffer = (uint_32 *)zalloc(EMU_SCREENBUFFERSIZE*4,"EMU_ScreenBuffer"); //Emulator screen buffer, 32-bits (x4)!
	if (!GPU.emu_screenbuffer) //Failed to allocate?
	{
		raiseError("GPU InitVideo","Failed to allocate the emulator screen buffer!");
	}
	//dolog("GPU","Setting up misc. settings...");

	GPU.show_framerate = show_framerate; //Show framerate?

	//dolog("GPU","Setting up frameskip...");
	setGPUFrameskip(0); //No frameskip, by default!
//VRAM access enable!
	//dolog("GPU","Setting up VRAM Access...");
	GPU.vram = (uint_32 *)VRAM_START; //VRAM access enabled!

	//dolog("GPU","Setting up pixel emulation...");
	GPU.showpixels = ALLOW_GPU_GRAPHICS; //Video is turned on!

	//dolog("GPU","Setting up video basic...");
	GPU.video_on = 0; //Start video?

	//dolog("GPU","Setting up debugger...");
	resetVideo(); //Initialise the video!

	GPU.use_Letterbox = 0; //Disable letterboxing by default!

//We're running with SDL?
	//dolog("GPU","Device ready.");
}

void doneVideo() //We're done with video operations?
{
	if (__HW_DISABLED) return; //Abort!
	stopVideo(); //Make sure we've stopped!
	//Nothing to do!
	if (GPU.emu_screenbuffer) //Allocated?
	{
		freez((void **)&GPU.emu_screenbuffer,EMU_SCREENBUFFERSIZE*4,"doneVideo_EMU_ScreenBuffer"); //Free!
	}
	done_GPURenderer(); //Clean up renderer stuff!
}

void startVideo()
{
	if (__HW_DISABLED) return; //Abort!
	GPU.video_on = ALLOW_VIDEO; //Turn video on when allowed!
}

void stopVideo()
{
	if (__HW_DISABLED) return; //Abort!
	GPU.video_on = 0; //Turn video off!
}

void GPU_keepAspectRatio(byte letterbox) //Keep aspect ratio with letterboxing?
{
	if (__HW_DISABLED) return; //Abort!
	GPU.use_Letterbox = (letterbox>0); //To use letterbox?
}

void resetVideo() //Resets the screen (clears)
{
	if (__HW_DISABLED) return; //Abort!
	//Debugger:
	//dolog("GPU","Setting basic white text color...");
	EMU_textcolor(0xF); //Default color: white on black!
}

void GPU_addTextSurface(PSP_TEXTSURFACE *surface, Handler handler) //Register a text surface for usage with the GPU!
{
	int i=0;
	for (;i<NUMITEMS(GPU.textsurfaces);i++)
	{
		if (GPU.textsurfaces[i]==surface) //Already registered?
		{
			return; //Abort!
		}
	}
	i = 0; //Reset!
	for (;i<NUMITEMS(GPU.textsurfaces);i++) //Process all entries!
	{
		if (!GPU.textsurfaces[i]) //Unused?
		{
			GPU.textrenderers[i] = handler; //Register the handler!
			GPU.textsurfaces[i] = surface; //Register the surface!
			return; //Done!
		}
	}
}

void GPU_removeTextSurface(PSP_TEXTSURFACE *surface)
{
	int i=0;
	for (;i<NUMITEMS(GPU.textsurfaces);i++)
	{
		if (GPU.textsurfaces[i]==surface) //Already registered?
		{
			GPU.textsurfaces[i] = NULL; //Unregister!
			GPU.textrenderers[i] = NULL; //Unregister!
			return; //Done!
		}
	}	
}