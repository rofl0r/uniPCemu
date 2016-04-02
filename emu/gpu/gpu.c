//We're the GPU!
#define IS_GPU

#include "headers/types.h" //Global stuff!
#include "headers/emu/gpu/gpu.h" //Our stuff!
#include "headers/mmu/mmu.h" //TEXT mode data!
#include "headers/interrupts/textmodedata.h" //TEXT mode data!
#include "headers/cpu/interrupts.h" //For int10 refresh function!
#include "headers/mmu/bda.h" //BDA support!
#include "headers/hardware/vga/vga.h" //VGA support!
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

//How many frames to render max?
#define GPU_FRAMERATE 30.0f

extern BIOS_Settings_TYPE BIOS_Settings; //Current settings!

GPU_type GPU; //The GPU itself!

GPU_SDL_Surface *rendersurface = NULL; //The PSP's surface to use when flipping!
SDL_Surface *originalrenderer = NULL; //Original renderer from above! We can only be freed using SDL_Quit. Above is just the wrapper!
extern GPU_SDL_Surface *resized; //Standard resized data, keep between unchanged screens!

byte rshift=0, gshift=0, bshift=0, ashift=0; //All shift values!
uint_32 rmask=0, gmask=0, bmask=0, amask=0; //All mask values!

/*

VIDEO BASICS!

*/

byte firstwindow = 1;
word window_xres = 0;
word window_yres = 0;
byte video_aspectratio = 0; //Current aspect ratio!

TicksHolder renderTiming;
double currentRenderTiming = 0.0;
double renderTimeout = 1000000000.0f/GPU_FRAMERATE; //60Hz refresh!

void updateWindow(word xres, word yres, uint_32 flags)
{
	if ((xres!=window_xres) || (yres!=window_yres) || !originalrenderer) //Do we need to update the Window?
	{
		window_xres = xres;
		window_yres = yres;
		originalrenderer = SDL_SetVideoMode(xres, yres, 32, flags); //Start rendered display, 32BPP pixel mode! Don't use double buffering: this changes our address (too slow to use without in hardware surface, so use sw surface)!
	}
}

SDL_Surface *getGPUSurface()
{
	#ifdef __psp__
	//PSP?
	updateWindow(PSP_SCREEN_COLUMNS,PSP_SCREEN_ROWS,SDL_SWSURFACE); //Start fullscreen, 32BPP pixel mode! Don't use double buffering: this changes our address (too slow to use without in hardware surface, so use sw surface)!
	#else
	//Windows etc?
	//Other architecture?
	uint_32 xres, yres; //Our determinated resolution!
	if (VIDEO_DFORCED) //Forced?
	{
		if (video_aspectratio) //Keep aspect ratio set and gotten something to take information from?
		{
			calcResize(video_aspectratio,GPU.xres,GPU.yres,EMU_MAX_X,EMU_MAX_Y,&xres,&yres); //Calculate resize using aspect ratio set for our screen on maximum size!
		}
		else //Default: Take the information from the monitor input resolution!
		{
			xres = GPU.xres; //Literal x resolution!
			yres = GPU.yres; //Literal y resolution!
		}
	}
	else //Normal operations? Use PSP resolution!
	{
		xres = PSP_SCREEN_COLUMNS; //PSP resolution x!
		yres = PSP_SCREEN_ROWS; //PSP resolution y!
	}

	//Apply limits!
	if (xres > EMU_MAX_X) xres = EMU_MAX_X;
	if (yres > EMU_MAX_Y) yres = EMU_MAX_Y;
	
	//Determine minimum by text/screen resolution!
	word minx, miny;
	minx = (GPU_TEXTPIXELSX > PSP_SCREEN_COLUMNS) ? GPU_TEXTPIXELSX : PSP_SCREEN_COLUMNS;
	miny = (GPU_TEXTPIXELSY > PSP_SCREEN_ROWS) ? GPU_TEXTPIXELSY : PSP_SCREEN_ROWS;

	if (xres < minx) xres = minx; //Minimum width!
	if (yres < miny) yres = miny; //Minimum height!

	uint_32 flags = SDL_SWSURFACE; //Default flags!
	if (GPU.fullscreen) flags |= SDL_FULLSCREEN; //Goto fullscreen mode!

	updateWindow(xres,yres,flags); //Update the window resolution if needed!

	if (firstwindow)
	{
		firstwindow = 0; //Not anymore!
		SDL_WM_SetCaption( "x86EMU", 0 ); //Initialise our window title!
	}
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
		//addtimer(60.0, &refreshscreen, "RefreshScreen", 1, 1,NULL); //Refresh the screen at this frequency MAX!
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

	debugrow("Video: Initialising screen buffers...");
	
	debugrow("Video: Waiting for access to GPU...");
	lockGPU(); //Wait for access!
	debugrow("Video: Allocating screen buffer...");
	GPU.emu_screenbuffer = (uint_32 *)zalloc(EMU_SCREENBUFFERSIZE * 4, "EMU_ScreenBuffer", NULL); //Emulator screen buffer, 32-bits (x4)!
	if (!GPU.emu_screenbuffer) //Failed to allocate?
	{
		unlockGPU(); //Unlock the GPU for Software access!
		raiseError("GPU InitVideo", "Failed to allocate the emulator screen buffer!");
		return; //Just here to shut Visual C++ code checks up. We cannot be here because the application should have already terminated because of the raiseError call.
	}

	GPU.emu_screenbufferend = &GPU.emu_screenbuffer[EMU_SCREENBUFFERSIZE]; //A quick reference to end of the display buffer!

	debugrow("Video: Setting up misc. settings...");
	GPU.show_framerate = show_framerate; //Show framerate?

//VRAM access enable!
	debugrow("Video: Setting up VRAM Access...");
	GPU.vram = (uint_32 *)VRAM_START; //VRAM access enabled!

	debugrow("Video: Setting up pixel emulation...");
	GPU.showpixels = ALLOW_GPU_GRAPHICS; //Video is turned on!

	debugrow("Video: Setting up video basic...");
	GPU.video_on = 0; //Start video?

	debugrow("Video: Setting up debugger...");
	resetVideo(); //Initialise the video!

	GPU.aspectratio = video_aspectratio = 0; //Default aspect ratio by default!

//We're running with SDL?
	unlockGPU(); //Unlock the GPU for Software access!

	debugrow("Video: Setting up frameskip...");
	setGPUFrameskip(0); //No frameskip, by default!

	debugrow("Video: Device ready.");

	initTicksHolder(&renderTiming);
	currentRenderTiming = 0.0; //Init!
}

byte needvideoupdate = 0; //Default: no update needed!

extern byte haswindowactive; //Are we displayed on-screen?

void CPU_updateVideo()
{
	lock(LOCK_VIDEO);
	if (needvideoupdate && haswindowactive) //We need to update the screen resolution and we're not hidden (We can't update the Window resolution correctly when we're hidden)?
	{
		unlock(LOCK_VIDEO);
		lockGPU(); //Lock the GPU: we're working on it!
		SDL_Surface *oldwindow; //Old window!
		oldwindow = originalrenderer; //Old rendering surface!
		if (getGPUSurface()) //Update the current surface if needed!
		{
			if (oldwindow!=originalrenderer) //We're changed?
			{
				freez((void **)&rendersurface, sizeof(*rendersurface), "SDL Main Rendering Surface"); //Release the rendering surface!
				rendersurface = getSurfaceWrapper(originalrenderer); //New wrapper!
			}
			registerSurface(rendersurface, "PSP SDL Main Rendering Surface", 0); //Register, but don't allow release: this is done by SDL_Quit only!
		}
		needvideoupdate = 0; //Not needed anymore!
		unlockGPU(); //We're finished with the GPU!
	}
	else unlock(LOCK_VIDEO); //We're done with the video!
}

void updateVideo() //Update the screen resolution on change!
{
	//We're disabled with the PSP: it doesn't update resolution!
	#ifndef __psp__
	byte reschange = 0, restype = 0; //Resolution change and type!
	static word xres=0;
	static word yres=0;
	static byte fullscreen = 0; //Are we fullscreen?
	static byte resolutiontype = 0; //Last resolution type!
	static byte plotsetting = 0; //Direct plot setting!
	static byte aspectratio = 0; //Last aspect ratio!
	if ((VIDEO_DIRECT || VIDEO_DFORCED) && (!video_aspectratio)) //Direct aspect ratio?
	{
		lock(LOCK_VIDEO);
		reschange = ((window_xres!=GPU.xres) || (window_yres!=GPU.yres)); //Resolution update based on Window Resolution?
		restype = 0; //Default resolution type!
		unlock(LOCK_VIDEO);
	}
	else if (resized) //Resized available?
	{
		reschange = ((xres!=resized->sdllayer->w) || (yres!=resized->sdllayer->h)); //This is the effective resolution!
		restype = 1; //Resized resolution type!
	}
	else
	{
		reschange = 0; //No resolution change when unknown!
	}
	if (reschange || (fullscreen!=GPU.fullscreen) || (aspectratio!=video_aspectratio) || (resolutiontype!=restype) || (BIOS_Settings.VGA_AllowDirectPlot!=plotsetting)) //Resolution (type) changed or fullscreen changed or plot setting changed?
	{
		lock(LOCK_VIDEO);
		GPU.forceRedraw = 1; //We're forcing a full redraw next frame to make sure the screen is always updated nicely!
		xres = restype?resized->sdllayer->w:GPU.xres;
		yres = restype?resized->sdllayer->h:GPU.yres;
		plotsetting = BIOS_Settings.VGA_AllowDirectPlot; //Update the plot setting!
		resolutiontype = restype; //Last resolution type!
		fullscreen = GPU.fullscreen;
		aspectratio = video_aspectratio; //Save the new values for comparing the next time we're changed!
		needvideoupdate = 1; //We need a video update!
		unlock(LOCK_VIDEO); //Finished with the GPU!
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
	lockGPU(); //Lock us!
	GPU.video_on = ALLOW_VIDEO; //Turn video on when allowed!
	unlockGPU(); //Unlock us!
}

void stopVideo()
{
	if (__HW_DISABLED) return; //Abort!
	lockGPU(); //Lock us!
	GPU.video_on = 0; //Turn video off!
	unlockGPU(); //Unlock us!
}

void GPU_AspectRatio(byte aspectratio) //Keep aspect ratio with letterboxing?
{
	if (__HW_DISABLED) return; //Abort!
	lockGPU(); //Lock us!
	GPU.aspectratio = video_aspectratio = (aspectratio<3)?aspectratio:0; //To use aspect ratio?
	GPU.forceRedraw = 1; //We're forcing a redraw of the screen using the new aspect ratio!
	unlockGPU(); //Unlock us!
}

void resetVideo() //Resets the screen (clears)
{
	if (__HW_DISABLED) return; //Abort!
	EMU_textcolor(0xF); //Default color: white on black!
}

void GPU_addTextSurface(void *surface, Handler handler) //Register a text surface for usage with the GPU!
{
	int i=0;
	for (;i<(int)NUMITEMS(GPU.textsurfaces);i++)
	{
		if (GPU.textsurfaces[i]==surface) //Already registered?
		{
			return; //Abort!
		}
	}
	i = 0; //Reset!
	for (;i<(int)NUMITEMS(GPU.textsurfaces);i++) //Process all entries!
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
	for (;i<(int)NUMITEMS(GPU.textsurfaces);i++)
	{
		if (GPU.textsurfaces[i]==surface) //Already registered?
		{
			GPU.textsurfaces[i] = NULL; //Unregister!
			GPU.textrenderers[i] = NULL; //Unregister!
			return; //Done!
		}
	}	
}

void GPU_mousebuttondown(word x, word y)
{
	int i = 0;
	for (;i<(int)NUMITEMS(GPU.textsurfaces);i++) //Process all registered surfaces!
	{
		if (GPU.textsurfaces[i]) //Registered?
		{
			GPU_textbuttondown(GPU.textsurfaces[i],x,y); //We're pressed here!
		}
	}
}

void GPU_mousebuttonup(word x, word y)
{
	int i = 0;
	for (;i<(int)NUMITEMS(GPU.textsurfaces);i++) //Process all registered surfaces!
	{
		if (GPU.textsurfaces[i]) //Registered?
		{
			GPU_textbuttonup(GPU.textsurfaces[i], x, y); //We're released here!
		}
	}
}

void GPU_tickVideo()
{
	currentRenderTiming += (double)getnspassed(&renderTiming); //Add the time passed to calculate!
	if (currentRenderTiming >= renderTimeout) //Timeout?
	{
		currentRenderTiming = fmod(currentRenderTiming,renderTimeout); //Rest time to count!
		refreshscreen(); //Refresh the screen!
	}
}