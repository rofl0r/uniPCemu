//We're the GPU!
#define IS_GPU

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
#include "headers/emu/gpu/gpu_text.h" //Text delta position support!

#include "headers/emu/timers.h" //Timer support!

#include "headers/support/locks.h" //Lock support!

//Are we disabled?
#define __HW_DISABLED 0

extern BIOS_Settings_TYPE BIOS_Settings; //Current settings!

GPU_type GPU; //The GPU itself!

GPU_SDL_Surface *rendersurface = NULL; //The PSP's surface to use when flipping! We can only be freed using SDL_Quit.
SDL_Surface *originalrenderer = NULL; //Original renderer from above! Above is just the wrapper!

byte rshift=0, gshift=0, bshift=0, ashift=0; //All shift values!
uint_32 rmask=0, gmask=0, bmask=0, amask=0; //All mask values!

/*

VIDEO BASICS!

*/

SDL_Surface *getGPUSurface()
{
	#ifdef __psp__
	//PSP?
	originalrenderer = SDL_SetVideoMode(PSP_SCREEN_COLUMNS, PSP_SCREEN_ROWS, 32, SDL_SWSURFACE); //Start fullscreen, 32BPP pixel mode! Don't use double buffering: this changes our address (too slow to use without in hardware surface, so use sw surface)!
	#else
	//Windows etc?
	//If the limit is broken, don't change resolution! Keep old resolution!
	if (GPU.xres > EMU_MAX_X)
	{
		if (originalrenderer) return originalrenderer; //Unchanged!
		GPU.xres = 0; //Discard: overflow!
	}
	if (GPU.yres > EMU_MAX_Y)
	{
		if (originalrenderer) return originalrenderer; //Unchanged!
		GPU.yres = 0; //Discard: overflow!
	}

	//Other architecture?
	word xres, yres; //Our determinated resolution!
	if (VIDEO_DFORCED) //Forced?
	{
		xres = GPU.xres; //Literal x resolution!
		yres = GPU.yres; //Literal y resolution!
	}
	else //Normal operations? Use PSP resolution!
	{
		xres = PSP_SCREEN_COLUMNS; //PSP resolution x!
		yres = PSP_SCREEN_ROWS; //PSP resolution y!
	}

	if (xres > EMU_MAX_X) xres = EMU_MAX_X;
	if (yres > EMU_MAX_Y) yres = EMU_MAX_Y; //Apply limits!
	
	//Determine minimum by text/screen resolution!
	word minx, miny;
	minx = (GPU_TEXTPIXELSX > PSP_SCREEN_COLUMNS) ? GPU_TEXTPIXELSX : PSP_SCREEN_COLUMNS;
	miny = (GPU_TEXTPIXELSY > PSP_SCREEN_ROWS) ? GPU_TEXTPIXELSY : PSP_SCREEN_ROWS;

	if (xres < minx) xres = minx; //Minimum width!
	if (yres < miny) yres = miny; //Minimum height!

	uint_32 flags = SDL_SWSURFACE; //Default flags!
	if (GPU.fullscreen) flags |= SDL_FULLSCREEN; //Goto fullscreen mode!

	originalrenderer = SDL_SetVideoMode(xres, yres, 32, flags); //Start rendered display, 32BPP pixel mode! Don't use double buffering: this changes our address (too slow to use without in hardware surface, so use sw surface)!

	SDL_WM_SetCaption( "x86EMU", 0 );
	GPU_text_updatedelta(originalrenderer); //Update delta if needed, so the text is at the correct position!
	#endif

	//Determine the display masks!
	if (originalrenderer) //Valid renderer?
	{
		//Load our detected settings!
		rmask = originalrenderer->format->Rmask;
		rshift = originalrenderer->format->Rshift;
		gmask = originalrenderer->format->Gmask;
		gshift = originalrenderer->format->Gshift;
		bmask = originalrenderer->format->Bmask;
		bshift = originalrenderer->format->Bshift;
		amask = originalrenderer->format->Amask;
		ashift = originalrenderer->format->Ashift;
		if (!amask) //No alpha supported?
		{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			amask = ashift = 0; //Default position!
#else
			amask = 0xFF000000; //High part!
			ashift = 24; //Shift by 24 bits to get alpha!
#endif
		}
	}

	return originalrenderer;
}

//On starting only.
void initVideoLayer() //We're for allocating the main video layer, only deallocated using SDL_Quit (when quitting the application)!
{
	if (SDL_WasInit(SDL_INIT_VIDEO)) //Initialised?
	{
		if (!originalrenderer) //Not allocated yet?
		{
			//PSP has solid resolution!
			getGPUSurface(); //Allocate our display!
			#ifdef __psp__
			//We don't want the cursor to show on the PSP!
			SDL_ShowCursor(SDL_DISABLE); //We don't want cursors on empty screens!
			#endif
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
						if (!memprotect(rendersurface->sdllayer->pixels,sizeof(uint_32)*get_pixelrow_pitch(rendersurface)*rendersurface->sdllayer->h,NULL)) //Valid?
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
		addtimer(60.0, &refreshscreen, "RefreshScreen", 1, 1,NULL); //Refresh the screen at this frequency MAX!
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

SDL_sem *GPU_Lock; //Our lock!

void freeGPULock(void)
{
	SDL_DestroySemaphore(GPU_Lock); //Add the lock for hardware/software conflicts!
}

byte lockGPU()
{
	return lock("GPU");
}

void unlockGPU()
{
	unlock("GPU");
}

void initVideo(int show_framerate) //Initialises the video
{
	if (__HW_DISABLED) return; //Abort!

	if (!GPU_Lock) //No lock yet?
	{
		GPU_Lock = SDL_CreateSemaphore(1); //Add the lock for hardware/software conflicts!
		atexit(&freeGPULock);
	}

	//dolog("GPU","Initialising screen buffers...");
	
	//dolog("zalloc","Allocating GPU EMU_screenbuffer...");
	lockGPU();
	GPU.emu_screenbuffer = (uint_32 *)zalloc(EMU_SCREENBUFFERSIZE * 4, "EMU_ScreenBuffer",GPU_Lock); //Emulator screen buffer, 32-bits (x4)!
	if (!GPU.emu_screenbuffer) //Failed to allocate?
	{
		unlockGPU(); //Unlock the GPU for Software access!
		raiseError("GPU InitVideo", "Failed to allocate the emulator screen buffer!");
	}
	unlockGPU(); //Unlock the GPU for Software access!
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

void updateVideo() //Update the screen resolution on change!
{
	//We're disabled with the PSP: it doesn't update resolution!
	#ifndef __psp__
	static word xres=0;
	static word yres=0;
	static byte fullscreen = 0; //Are we fullscreen?
	if (rendersurface) //Already started?
	{
		if ((xres^GPU.xres) || (yres^GPU.yres) || (fullscreen^GPU.fullscreen)) //Resolution changed?
		{
			xres = GPU.xres;
			yres = GPU.yres;
			fullscreen = GPU.fullscreen; //Save the new values for comparing the next time we're changed!
			freez((void **)&rendersurface,sizeof(*rendersurface),"SDL Main Rendering Surface"); //Release the rendering surface!
			if (getGPUSurface()) //Update the current surface if needed!
			{
				rendersurface = getSurfaceWrapper(originalrenderer); //New wrapper!
				registerSurface(rendersurface, "PSP SDL Main Rendering Surface", 0); //Register, but don't allow release: this is done by SDL_Quit only!
			}
		}
	}
	#endif
}

void doneVideo() //We're done with video operations?
{
	if (__HW_DISABLED) return; //Abort!
	stopVideo(); //Make sure we've stopped!
	//Nothing to do!
	if (GPU.emu_screenbuffer) //Allocated?
	{
		if (!lockGPU()) return; //Lock ourselves!
		freez((void **)&GPU.emu_screenbuffer, EMU_SCREENBUFFERSIZE * 4, "doneVideo_EMU_ScreenBuffer"); //Free!
		unlockGPU(); //Unlock the GPU for Software access!
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

void GPU_addTextSurface(void *surface, Handler handler) //Register a text surface for usage with the GPU!
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

void GPU_removeTextSurface(void *surface)
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