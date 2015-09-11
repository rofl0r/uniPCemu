#include "headers/types.h" //Basic stuff!
#include "headers/emu/gpu/gpu.h" //GPU typedefs etc.
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/support/highrestimer.h" //High resolution timer!
#include "headers/emu/gpu/gpu_sdl.h" //SDL support!
#include "headers/support/log.h" //Logging support!
#include "headers/bios/bios.h" //BIOS support!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate support!
#include "headers/support/bmp.h" //Bitmap support!
#include "headers/support/zalloc.h" //Zalloc support!
#include "headers/emu/gpu/gpu_text.h" //Text rendering support!

//Are we disabled?
#define __HW_DISABLED 0

//Allow HW rendering? (VGA or other hardware)
#define ALLOW_HWRENDERING 1

extern BIOS_Settings_TYPE BIOS_Settings; //The BIOS Settings!

byte SCREEN_CAPTURE = 0; //To capture a screen? Set to 1 to make a capture next frame!

extern GPU_type GPU; //GPU!

extern GPU_SDL_Surface *rendersurface; //The PSP's surface to use when flipping!
extern uint_32 frames; //Frames processed!

uint_32 frames_rendered = 0;

void renderScreenFrame() //Render the screen frame!
{
	if (__HW_DISABLED) return; //Abort?
	if (SDL_WasInit(SDL_INIT_VIDEO) && rendersurface) //Rendering using SDL?
	{
		++frames_rendered; //Increase ammount of frames rendered!
		safeFlip(rendersurface); //Set the new resized screen to use, if possible!
		return; //Done!
	}
	//Already on-screen rendered: We're using direct mode!
}

char filename[256];
OPTINLINE char *get_screencapture_filename() //Filename for a screen capture!
{
	domkdir("captures"); //Captures directory!
	uint_32 i=0; //For the number!
	char filename2[256];
	memset(&filename2,0,sizeof(filename2)); //Init filename!
	do
	{
		sprintf(filename2,"captures/%i.bmp",++i); //Next bitmap file!
	} while (file_exists(filename2)); //Still exists?
	sprintf(filename,"captures/%i",i); //The capture filename!
	return &filename[0]; //Give the filename for quick reference!
}

uint_32 *row_empty = NULL; //A full row, non-initialised!
uint_32 row_empty_size = 0; //No size!
GPU_SDL_Surface *resized = NULL; //Standard resized data, keep between unchanged screens!

OPTINLINE void init_rowempty()
{
	if (__HW_DISABLED) return; //Abort?
	if (!row_empty) //Not allocated yet?
	{
		row_empty_size = EMU_MAX_X*sizeof(uint_32); //Load the size of an empty row for deallocation purposes!
		row_empty = (uint_32 *)zalloc(row_empty_size,"Empty row",NULL); //Initialise empty row!
	}
}

OPTINLINE void GPU_finishRenderer() //Finish the rendered surface!
{
	if (__HW_DISABLED) return; //Abort?
	if (resized) //Resized still buffered?
	{
		resized = freeSurface(resized); //Try and free the surface!
	}
}

void done_GPURenderer() //Cleanup only!
{
	if (__HW_DISABLED) return; //Abort?
	if (row_empty) //Allocated?
	{
		freez((void **)&row_empty,row_empty_size,"GPURenderer_EmptyRow"); //Clean up!
	}
	GPU_finishRenderer(); //Finish the renderer!
}

OPTINLINE uint_32 *get_rowempty()
{
	if (__HW_DISABLED) return NULL; //Abort?
	init_rowempty(); //Init empty row!
	return row_empty; //Give the empty row!
}

OPTINLINE void GPU_directRenderer() //Plot directly 1:1 on-screen!
{
	if (__HW_DISABLED) return; //Abort?
	init_rowempty(); //Init empty row!
#ifdef __psp__
	int pspy = 0;
#endif
	if (SDL_WasInit(SDL_INIT_VIDEO) && rendersurface) //Rendering using SDL?
	{
		uint_32 virtualrow = 0; //Virtual row to use! (From the source)
		uint_32 start = 0; //Start row of the drawn part!
		word y = 0; //Init Y to the beginning!
		if (GPU.use_Letterbox) //Using letterbox for aspect ratio?
		{
			#ifndef __psp__
				if (VIDEO_DFORCED) //Forced video?
				{
					goto drawpixels; //No letterbox top!
				}
			#endif
			start = (rendersurface->sdllayer->h / 2) - (GPU.yres / 2); //Calculate start row of contents!
			for (y = 0; y<start;) //Process top!
			{
				put_pixel_row(rendersurface, y, PSP_SCREEN_COLUMNS, &row_empty[0], 0, 0); //Plot empty row, don't care about more black!
				++y; //Next row!
			}
		}

#ifndef __psp__
		drawpixels:
#endif
		for (; virtualrow<GPU.yres;) //Process row-by-row!
		{
			put_pixel_row(rendersurface, y++, GPU.xres, &EMU_BUFFER(0, virtualrow++), 0, 0); //Copy the row to the screen buffer, centered horizontally if needed, from virtual if needed!
		}

		//Always clear the bottom: letterbox and direct plot both have to clear the bottom!
		for (; y<rendersurface->sdllayer->h;) //Process bottom!
		{
			put_pixel_row(rendersurface, y, PSP_SCREEN_COLUMNS, &row_empty[0], 0, 0); //Plot empty row for the bottom, don't care about more black!
			++y; //Next row!
		}

		GPU.emu_buffer_dirty = 0; //Not dirty anymore!
		return; //Don't render anymore!
	}

#ifdef __psp__
	//PSP only?
	if (GPU.emu_buffer_dirty) //Dirty?
	{
		//Old method, also fine&reasonably fast!
		for (; pspy<PSP_SCREEN_ROWS;) //Process row!
		{
			int pspx = 0;
			for (; pspx<PSP_SCREEN_COLUMNS;) //Process column!
			{
				if ((pspx>=GPU.xres) || (pspy>=GPU.yres)) //Out of range?
				{
					PSP_BUFFER(pspx, pspy) = 0; //Clear color for out of range!
				}
				else //Exists in buffer?
				{
					PSP_BUFFER(pspx, pspy) = GPU_GETPIXEL(pspx, pspy); //Get pixel from buffer!
				}
				++pspx; //Next X!
			}
			++pspy; //Next Y!
		}
		GPU.emu_buffer_dirty = 0; //Not dirty anymore!
	}
#endif

	//We can't use the keyboard with the old renderer, so you just have to do it from the top of your head!
	//OK: rendered to PSP buffer!
}

OPTINLINE void render_EMU_screen() //Render the EMU buffer to the screen!
{
	uint_32 *srcrow; //Current pixel row rendering!
	if (VIDEO_DIRECT) //Direct mode?
	{
		GPU_directRenderer(); //Render directly!
		return;
	}
	if (!memprotect(rendersurface,sizeof(*rendersurface),NULL)) return; //Nothing to render to!
	if (!memprotect(rendersurface->sdllayer,sizeof(*rendersurface->sdllayer),NULL)) return; //Nothing to render to!
	//Now, render our screen, or clear it!
	byte rendered = 0;
	if (memprotect(resized,sizeof(*resized),NULL)) //Resized available (anti-NULL protection)?
	{
		if (memprotect(resized->sdllayer,sizeof(*resized->sdllayer),NULL))
		{
			rendered = 1; //We're rendered from here on!
			word y = 0; //Current row counter!
			word count;
			uint_32 virtualrow = 0; //Virtual row to use! (From the source)
			
			byte letterbox = GPU.use_Letterbox; //Use letterbox?
			if (letterbox) //Using letterbox for aspect ratio?
			{
				count = ((rendersurface->sdllayer->h/2) - (resized->sdllayer->h/2))-1; //The total ammount to process: up to end+1!
				nextrowtop: //Process top!
				{
					if (!count--) goto startemurendering; //Done?
					put_pixel_row(rendersurface,y++,PSP_SCREEN_COLUMNS,get_rowempty(),0,0); //Plot empty row, don't care about more black!
					goto nextrowtop; //Next row!
				}
			}
			
		startemurendering:
			if (resized) //Valid layer?
			{
				if (resized->sdllayer->h) //Gotten height?
				{
					count = resized->sdllayer->h; //How many!
				nextrowemu: //Process row-by-row!
					{
						if (!count--) goto startbottomrendering; //Stop when done!
						if (!resized) goto startbottomrendering; //Skip when no resized anymore!
						srcrow = get_pixel_row(resized, virtualrow++, 0); //Get the current pixel row!
						if (!srcrow) goto startbottomrendering; //Skip unknown rows!
						put_pixel_row(rendersurface, y++, resized->sdllayer->w, srcrow, (letterbox & 1), 0); //Copy the row to the screen buffer, centered horizontally if needed, from virtual if needed!
						goto nextrowemu;
					}
				}
			}
			
			startbottomrendering:
			if (letterbox) //Using letterbox for aspect ratio?
			{
				count = PSP_SCREEN_ROWS-y; //How many left to process!
				nextrowbottom: //Process bottom!
				{
					if (!count--) goto finishbottomrendering; //Stop when done!
					put_pixel_row(rendersurface,y++,PSP_SCREEN_COLUMNS,get_rowempty(),0,0); //Plot empty row for the bottom, don't care about more black!
					goto nextrowbottom;
				}
			}
		}
		
		finishbottomrendering:
		if (memprotect(resized, sizeof(*resized), NULL) && resized)
		{
			resized->flags &= ~SDL_FLAG_DIRTY; //Not dirty anymore!
		}
	}
	if (!rendered) //Nothing to render = clear screen!
	{
		word count2 = rendersurface->sdllayer->h; //How many to process!
		if (count2) //Got something to render?
		{
			for (;;)
			{
				put_pixel_row(rendersurface,--count2,PSP_SCREEN_COLUMNS,get_rowempty(),0,0); //Clear the screen row!
				if (!count2) break; //Done?
			}
		}
	}
}

OPTINLINE byte getresizeddirty() //Is the emulated screen dirty?
{
	if (VIDEO_DIRECT)
	{
		return GPU.emu_buffer_dirty; //Force dirty from EMU Buffer when in direct mode!
	}
	return resized ? ((resized->flags&SDL_FLAG_DIRTY)>0) : 0; //Are we dirty?
}

OPTINLINE void renderFrames() //Render all frames to the screen!
{
	if (SDL_WasInit(SDL_INIT_VIDEO) && rendersurface) //Rendering using SDL?
	{
		byte dirty;
		dirty = getresizeddirty(); //Check if resized is dirty!

		int i; //For processing surfaces!
		//Check for dirty text surfaces!
		for (i=0;i<NUMITEMS(GPU.textsurfaces);i++) //Process all text surfaces!
		{
			if (GPU.textsurfaces[i]) //Surface specified?
			{
				if (GPU.textrenderers[i]) //Gotten a handler?
				{
					GPU.textrenderers[i](); //Execute the handler for filling the screen!
				}
				GPU_text_locksurface(GPU.textsurfaces[i]); //Lock the surface!
				if (GPU_textdirty(GPU.textsurfaces[i])) //Marked dirty?
				{
					dirty = 1; //We're dirty!
				}
				GPU_text_releasesurface(GPU.textsurfaces[i]); //Release the surface lock!
			}
		}

		if (dirty) //Any surfaces dirty?
		{
			render_EMU_screen(); //Render the emulator surface to the screen!
			for (i=0;i<NUMITEMS(GPU.textsurfaces);i++) //Render the text surfaces to the screen!
			{
				if (GPU.textsurfaces[i]) //Specified?
				{
					GPU_textrenderer(GPU.textsurfaces[i]); //Render the text layer!
				}
			} //Leave these for now!
		}
		
		if (getresizeddirty()) //Still dirty?
		{
			dolog("GPU","Warning: resized is still dirty after rendering?");
		}
		
		//Render the frame!
		renderScreenFrame(); //Render the current frame!
		return; //Normal!
	}

	//Fallback to direct plot!
	GPU_directRenderer(); //Fallback!
}

//Rendering functionality!
OPTINLINE void render_EMU_buffer() //Render the EMU to the buffer!
{
	lockGPU(); //Lock the GPU!
	//Next, allocate all buffers!
	//First, check the emulated screen for updates and update it if needed!
	if (rendersurface && ((GPU.xres*GPU.yres)>0)) //Got emu screen to render to the PSP and not testing and dirty?
	{
		//Move entire emulator buffer to the rendering buffer when needed (updated)!
		
		if (GPU.emu_buffer_dirty) //Dirty = to render again, if allowed!
		{
			GPU_finishRenderer(); //Done with the resizing!
			//First, init&fill emu_screen data!
			word xres, yres;
			xres = GPU.xres; //Load x resolution!
			yres = GPU.yres; //Load y resolution!
			//Limit broken = no display!
			if (xres > EMU_MAX_X)
			{
				unlockGPU(); //Unlock the GPU!
				return; //Limit to buffer!
			}
			if (yres > EMU_MAX_Y)
			{
				unlockGPU(); //Unlock the GPU!
				return; //Limit to buffer!
			}
			GPU_SDL_Surface *emu_screen = createSurfaceFromPixels(GPU.xres, GPU.yres, GPU.emu_screenbuffer, EMU_MAX_X); //Create container 32BPP pixel mode!
			if (emu_screen) //Createn to render?
			{
				//Resize to resized!
				resized = resizeImage(emu_screen,rendersurface->sdllayer->w,rendersurface->sdllayer->h,GPU.doublewidth,GPU.doubleheight,GPU.use_Letterbox); //Render it to the PSP screen, keeping aspect ratio with letterboxing!
				if (!resized) //Error?
				{
					dolog("GPU","Error resizing the EMU screenbuffer to the PSP screen!");
				}
				//Clean up and reset flags!
				emu_screen = freeSurface(emu_screen); //Done with the emulator screen!
			}
			GPU.emu_buffer_dirty = 0; //Not dirty anymore: we've been updated!
			unlockGPU(); //Free the GPU!
		}
	}
	unlockGPU(); //Unlock the GPU!
}

byte SplitScreen = 0; //Default: no split-screen!
uint_32 SplitScreen_Start; //Start of split-screen operations!

OPTINLINE void GPU_fullRenderer()
{
	if (__HW_DISABLED) return; //Abort?
	if (SDL_WasInit(SDL_INIT_VIDEO) && rendersurface) //Rendering using SDL?
	{
		renderFrames(); //Render all frames to the screen!
		return; //OK: rendered, so don't render anymore!
	}
	
	GPU_directRenderer(); //Render direct instead, since we don't support this!
}

/*

THE RENDERER!

*/



byte candraw = 0; //Can we draw (determined by max framerate)?
byte GPU_is_rendering = 0; //We're rendering currently: for preventing multirendering?
extern float curscanlinepercentage; //Current scanline percentage (0.0-1.0)!

void renderHWFrame() //Render a frame from hardware!
{
	if (__HW_DISABLED) return; //Abort?
	if (!ALLOW_RENDERING) return; //Disable when not allowed to render!

	if (GPU_is_rendering) return; //Don't render multiple frames at the same time!
	GPU_is_rendering = 1; //We're rendering, so block other renderers!

	if (ALLOW_HWRENDERING)
	{
		updateVideo(); //Change display resolution of output when needed!
		init_rowempty(); //Init empty row!
		//Start the rendering!
		if (!VIDEO_DIRECT) //To do scaled mapping to the screen?
		{
			if (SDL_WasInit(SDL_INIT_VIDEO) && rendersurface) //Allowed rendering?
			{
				render_EMU_buffer(); //Render the EMU to the buffer, if updated! This is our main layer!
			}
		}
		lockGPU();
		if (SCREEN_CAPTURE) //Screen capture?
		{
			if (!--SCREEN_CAPTURE) //Capture this frame?
			{
				writeBMP(get_screencapture_filename(),&EMU_BUFFER(0,0),GPU.xres,GPU.yres,GPU.doublewidth,GPU.doubleheight,EMU_MAX_X); //Dump our raw screen!
			}
		}
		unlockGPU();
	}
	
	GPU_FrameRendered(); //A frame has been rendered, so update our stats!
	GPU_is_rendering = 0; //We're not rendering anymore!
}

/*

FPS LIMITER!

*/

void refreshscreen() //Handler for a screen frame (60 fps) MAXIMUM.
{
	lockGPU();
	if (__HW_DISABLED) return; //Abort?
	int do_render = 1; //Do render?

	if (GPU.frameskip) //Got frameskip?
	{
		do_render = !GPU.framenr; //To render the current frame each <frameskip> frames!
		GPU.framenr = (GPU.framenr+1)%(GPU.frameskip+1); //Next frame!
	}
	
	if (do_render && !GPU.video_on) //Disable when Video is turned off or skipped!
	{
		if (!VIDEO_DIRECT) //To do scaled mapping to the screen?
		{
			GPU_fullRenderer(); //Render a full frame, or direct when needed!
		}
	}

	renderFrames(); //Render all frames needed!
	
	GPU_is_rendering = 0; //We're done rendering!
	finish_screen(); //Finish stuff on-screen!	
	unlockGPU(); //Finished with the GPU!
}
