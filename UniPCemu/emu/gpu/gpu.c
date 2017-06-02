//We're the GPU!
#define IS_GPU

#include "headers/types.h" //Global stuff!
#include "headers/emu/gpu/gpu.h" //Our stuff!
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
#include "headers/support/locks.h" //Lock support!
#include "headers/bios/biosmenu.h" //For allocating BIOS menu surface!

//Are we disabled?
#define __HW_DISABLED 0

//How many frames to render max?
#define GPU_FRAMERATE 60.0f

extern BIOS_Settings_TYPE BIOS_Settings; //Current settings!

GPU_type GPU; //The GPU itself!

GPU_SDL_Surface *rendersurface = NULL; //The PSP's surface to use when flipping!
SDL_Surface *originalrenderer = NULL; //Original renderer from above! We can only be freed using SDL_Quit. Above is just the wrapper!
extern GPU_SDL_Surface *resized; //Standard resized data, keep between unchanged screens!

byte rshift=0, gshift=0, bshift=0, ashift=0; //All shift values!
uint_32 rmask=0, gmask=0, bmask=0, amask=0; //All mask values!

uint_32 transparentpixel = 0xFFFFFFFF; //Transparent pixel!

/*

VIDEO BASICS!

*/

byte firstwindow = 1;
word window_xres = 0;
word window_yres = 0;
uint_32 window_flags = 0; //Current flags for the window!
byte video_aspectratio = 0; //Current aspect ratio!

TicksHolder renderTiming;
double currentRenderTiming = 0.0;
double renderTimeout = 0.0; //60Hz refresh!

#ifdef SDL2
SDL_Window *sdlWindow = NULL;
SDL_Renderer *sdlRenderer = NULL;
SDL_Texture *sdlTexture = NULL;
#endif

void updateWindow(word xres, word yres, uint_32 flags)
{
	byte useFullscreen; //Are we to use fullscreen?
	if ((xres!=window_xres) || (yres!=window_yres) || (flags!=window_flags) || !originalrenderer) //Do we need to update the Window?
	{
#include "headers/emu/icon.h" //We need our icon!
		SDL_Surface *icon = NULL; //Our icon!
		icon = SDL_CreateRGBSurfaceFrom((void *)&icondata,ICON_BMPWIDTH,ICON_BMPHEIGHT,32,ICON_BMPWIDTH<<2, 0x000000FF, 0x0000FF00,0x00FF0000,0); //We have a RGB icon only!
		window_xres = xres;
		window_yres = yres;
		window_flags = flags;
		#ifndef SDL2
		//SDL1?
		if (icon) //Gotten an icon?
		{
			SDL_WM_SetIcon(icon,NULL); //Set the icon to use!
		}
		originalrenderer = SDL_SetVideoMode(xres, yres, 32, flags); //Start rendered display, 32BPP pixel mode! Don't use double buffering: this changes our address (too slow to use without in hardware surface, so use sw surface)!
		#else
		useFullscreen = 0; //Default: not fullscreen!
		if (flags&SDL_WINDOW_FULLSCREEN) //Fullscreen specified?
		{
			flags &= ~SDL_WINDOW_FULLSCREEN; //Don't apply fullscreen this way!
			useFullscreen = 1; //We're using fullscreen!
		}
		if (sdlTexture)
		{
			SDL_DestroyTexture(sdlTexture);
			sdlTexture = NULL; //Nothing!
		}
		if (sdlRenderer)
		{
			SDL_DestroyRenderer(sdlRenderer);
			sdlRenderer = NULL; //Nothing!
		}
		if (rendersurface) //Gotten a surface we're rendering?
		{
			rendersurface = freeSurface(rendersurface); //Release our rendering surface!
		}
		if (!sdlWindow) //We don't have a window&renderer yet?
		{
			sdlWindow = SDL_CreateWindow("UniPCemu", SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,xres,yres,SDL_WINDOW_SHOWN); //Create the window and renderer we use at our resolution!
		}
		else
		{
			SDL_SetWindowSize(sdlWindow,xres,yres); //Set the new window size!
		}
		SDL_SetWindowFullscreen(sdlWindow,useFullscreen?SDL_WINDOW_FULLSCREEN:0); //Are we to apply fullscreen?
		if (sdlWindow) //Gotten a window?
		{
			if (icon) //Gotten an icon?
			{
				SDL_SetWindowIcon(sdlWindow,icon); //Set the icon to use!
			}
			if (!sdlRenderer) //No renderer yet?
			{
				sdlRenderer = SDL_CreateRenderer(sdlWindow,-1,0);
			}
		}

		if (sdlRenderer) //Gotten a renderer?
		{
			SDL_RenderSetLogicalSize(sdlRenderer,window_xres,window_yres); //Set the new resolution!
			sdlTexture = SDL_CreateTexture(sdlRenderer,
				SDL_PIXELFORMAT_ARGB8888,
				SDL_TEXTUREACCESS_STREAMING,
				xres, yres); //The texture we use!
		}

		originalrenderer = SDL_CreateRGBSurface(0, window_xres, window_yres, 32,
			0x00FF0000,
			0x0000FF00,
			0x000000FF,
			0xFF000000); //The SDL Surface we render to!
		#endif
		if (icon)
		{
			SDL_FreeSurface(icon); //Free the icon!
			icon = NULL; //No icon anymore!
		}
	}
}

byte GPU_plotsetting = 0;

SDL_Surface *getGPUSurface()
{
	uint_32 xres, yres; //Our determinated resolution!
	#ifdef IS_PSP
	//PSP?
	xres = PSP_SCREEN_COLUMNS;
	yres = PSP_SCREEN_ROWS; //Start fullscreen, 32BPP pixel mode! Don't use double buffering: this changes our address (too slow to use without in hardware surface, so use sw surface)!
	GPU.fullscreen = 1; //Forced full screen!
	goto windowready; //Skip other calculations!
	#else
	#ifdef STATICSCREEN
	#ifndef SDL2
	//SDL Autodetection of fullscreen resolution!
	SDL_Rect **modes;
	if ((!window_xres) || (!window_yres)) //Not initialized yet?
	{
		/* Get available fullscreen/hardware modes */
		modes = SDL_ListModes(NULL, SDL_FULLSCREEN | SDL_HWSURFACE);

		/* Check is there are any modes available */
		if (modes != (SDL_Rect **)0)
		{
			/* Check if our resolution is restricted */
			if (modes != (SDL_Rect **)-1)
			{
				xres = modes[0]->w; //Use first hardware resolution!
				yres = modes[0]->h; //Use first hardware resolution!
				GPU.fullscreen = 1; //Forced full screen!	
				goto windowready; //Skip other calculations!
			}
		}
	}
	#else
	//SDL2 Autodetction of fullscreen resolution!
	//Get device display mode
	SDL_DisplayMode displayMode;
	if (SDL_GetCurrentDisplayMode(0,&displayMode)==0)
	{
		xres = displayMode.w;
		yres = displayMode.h;
		GPU.fullscreen = 1; //Forced full screen!	
		goto windowready; //Skip other calculations!
	}
	#endif
	#endif
	#endif

	//Windows etc?
	//Other architecture?
	word destxres, destyres;
	if (VIDEO_DFORCED) //Forced?
	{
		if (video_aspectratio) //Keep aspect ratio set and gotten something to take information from?
		{
			switch (video_aspectratio) //Forced resolution?
			{
			case 4: //4:3(VGA) medium-res
				destxres = 1024;
				destyres = 768;
				break;
			case 5: //4:3(VGA) high-res(fullHD)
				destxres = 1440; //We're resizing the destination ratio itself instead!
				destyres = 1080; //We're resizing the destination ratio itself instead!
				break;
			case 6: //4K
				destxres = 3840; //We're resizing the destination ratio itself instead!
				destyres = 2160; //We're resizing the destination ratio itself instead!			
				break;
			default: //Unhandled?
				destxres = 800;
				destyres = 600;
				break;
			}
			calcResize(video_aspectratio,GPU.xres,GPU.yres,destxres,destyres,&xres,&yres,1); //Calculate resize using aspect ratio set for our screen on maximum size(use the smalles window size)!
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

	uint_32 flags;
	#if defined(STATICSCREEN) || defined(IS_PSP)
	windowready:
	#endif
	flags = SDL_SWSURFACE; //Default flags!
	#ifndef SDL2
	if (GPU.fullscreen) flags |= SDL_FULLSCREEN; //Goto fullscreen mode!
	#else
	if (GPU.fullscreen) flags |= SDL_WINDOW_FULLSCREEN; //Goto fullscreen mode!
	#endif

	updateWindow(xres,yres,flags); //Update the window resolution if needed!

	if (firstwindow)
	{
		firstwindow = 0; //Not anymore!
		#ifndef SDL2
		SDL_WM_SetCaption( "UniPCemu", 0 ); //Initialise our window title!
		#else
		if (sdlWindow) //Gotten a window?
		{
			SDL_SetWindowTitle(sdlWindow,"UniPCemu"); //Initialise our window title!
		}
		#endif
	}
	GPU_text_updatedelta(originalrenderer); //Update delta if needed, so the text is at the correct position!

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
			amask = 0xFF;
			ashift = 0; //Default position!
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
			#if defined(STATICSCREEN)
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
				#ifndef SDL2
				registerSurface(rendersurface,"PSP SDL Main Rendering Surface",0); //Register, but don't allow release: this is done by SDL_Quit only!
				#else
				registerSurface(rendersurface, "PSP SDL Main Rendering Surface", 1); //Register, allow release: this is allowed in SDL2!
				#endif
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

	//Initialize our timing!
	renderTimeout = 1000000000.0 / GPU_FRAMERATE;
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

	transparentpixel = RGBA(SDL_ALPHA_TRANSPARENT, SDL_ALPHA_TRANSPARENT, SDL_ALPHA_TRANSPARENT, SDL_ALPHA_TRANSPARENT); //Set up the transparent pixel!

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
	if (needvideoupdate && (haswindowactive==3)) //We need to update the screen resolution and we're not hidden (We can't update the Window resolution correctly when we're hidden)?
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
			}
			if (!rendersurface) //We don't have a valid rendering surface?
			{
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
	//We're disabled with the PSP&Android: it doesn't update resolution!
	#if !defined(STATICSCREEN)
	byte reschange = 0, restype = 0; //Resolution change and type!
	static word xres=0;
	static word yres=0;
	static byte fullscreen = 0; //Are we fullscreen?
	static byte resolutiontype = 0; //Last resolution type!
	static byte plotsetting = 0; //Direct plot setting!
	static byte aspectratio = 0; //Last aspect ratio!
	if ((VIDEO_DIRECT) && (!video_aspectratio)) //Direct aspect ratio?
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
		plotsetting = GPU_plotsetting = BIOS_Settings.VGA_AllowDirectPlot; //Update the plot setting!
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
	GPU.aspectratio = video_aspectratio = (aspectratio<7)?aspectratio:0; //To use aspect ratio?
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

extern byte EMU_RUNNING; //Emulator running? 0=Not running, 1=Running, Active CPU, 2=Running, Inactive CPU (BIOS etc.)

int_32 lightpen_x=-1, lightpen_y=-1; //Current lightpen location, if any!
byte lightpen_pressed = 0; //Lightpen pressed?
byte lightpen_status = 0; //Are we capturing lightpen motion and presses?

extern word renderarea_x_start, renderarea_y_start; //X and Y start of the rendering area of the real rendered active display!

void updateLightPenLocation(word x, word y)
{
	lightpen_x = (int_32)(SAFEDIV((float)(x-renderarea_x_start),(float)window_xres)*(float)GPU.xres); //Convert the X location to the GPU renderer location!
	lightpen_y = (int_32)(SAFEDIV((float)(y-renderarea_y_start),(float)window_yres)*(float)GPU.yres); //Convert the X location to the GPU renderer location!
}

void GPU_mousebuttondown(word x, word y, byte finger)
{
	int i = (int)NUMITEMS(GPU.textsurfaces)-1; //Start with the last surface! The last registered surface has priority!
	for (;i>=0;--i) //Process all registered surfaces!
	{
		if (GPU.textsurfaces[i]) //Registered?
		{
			if (GPU_textbuttondown(GPU.textsurfaces[i], finger, x, y)) //We're pressed here!
			{
				return; //Abort: don't let lower priority surfaces override us!
			}
		}
	}
	if (EMU_RUNNING==1) //Handle light pen as well?
	{
		if ((finger==0xFF) && (EMU_RUNNING==1)) //Right mouse button? Handle as lightpen input activation!
		{
			lightpen_status = 1; //Capture as lightpen!
		}		
	}
	if (lightpen_status) //Lightpen active?
	{
		if (finger==0xFE) //Left mouse button? Handle as lightpen pressing!
		{
			lightpen_pressed = 1; //We're pressed!
		}
		updateLightPenLocation(x,y); //Update the light pen location!
	}
	else
	{
		lightpen_x = lightpen_y = -1; //No lightpen used!
	}
}

byte GPU_surfaceclicked = 0; //Surface clicked to handle?

void GPU_mousebuttonup(word x, word y, byte finger)
{
	int i = 0;
	for (;i<(int)NUMITEMS(GPU.textsurfaces);i++) //Process all registered surfaces!
	{
		if (GPU.textsurfaces[i]) //Registered?
		{
			GPU_textbuttonup(GPU.textsurfaces[i],finger,x,y); //We're released here!
		}
	}
	GPU_surfaceclicked = 1; //Signal a click of a GPU surface!
	if (finger==0xFF) //Right mouse button? Handle as lightpen input deactivation!
	{
		lightpen_status = 0; //Don't capture as lightpen anymore!
		lightpen_pressed = 0; //Not pressed anymore!
		lightpen_x = lightpen_y = -1; //Nothing pressed!
	}
	else if (finger==0xFE) //Left mouse button released? Handle as lightpen button release always!
	{
		lightpen_pressed = 0; //Not pressed anymore!
	}
}

void GPU_mousemove(word x, word y, byte finger)
{
	if (lightpen_status) //Lightpen is active?
	{
		updateLightPenLocation(x,y); //Update the light pen location!
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