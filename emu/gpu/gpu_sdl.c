#include "headers/emu/gpu/gpu.h" //GPU typedefs etc.
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/support/zalloc.h" //For registering our data we've allocated!
#include "headers/emu/gpu/gpu_sdl.h" //SDL support!
#include "headers/support/log.h" //Logging support!

//Log put_pixel_row errors?
//#define PPRLOG

//Container/wrapper support
void freeSurfacePtr(void **ptr, uint_32 size, SDL_sem *lock) //Free a pointer (used internally only) allocated with nzalloc/zalloc and our internal functions!
{
	GPU_SDL_Surface *surface = (GPU_SDL_Surface *)*ptr; //Take the surface out of the pointer!
	if (surface->lock) WaitSem(surface->lock)
	if (!(surface->flags&SDL_FLAG_NODELETE)) //The surface is allowed to be deleted?
	{
		//Start by freeing the surfaces in the handlers!
		uint_32 pixels_size = surface->sdllayer->h*get_pixelrow_pitch(surface)*sizeof(uint_32); //Calculate surface pixels size!
		if (!(surface->flags&SDL_FLAG_NODELETE_PIXELS)) //Valid to delete?
		{
			unregisterptr(surface->sdllayer->pixels,pixels_size); //Release the pixels within the surface!
		}
		if (unregisterptr(surface->sdllayer,sizeof(*surface->sdllayer))) //The surface itself!
		{
			//Next release the data associated with it using the official functionality!
			SDL_FreeSurface(surface->sdllayer); //Release the surface fully using native support!
		}
	}
	if (surface->lock) PostSem(surface->lock) //We're done with the contents!
	changedealloc(surface, sizeof(*surface), getdefaultdealloc()); //Change the deallocation function back to it's default!
	//We're always allowed to release the container.
	if (surface->lock)
	{
		SDL_DestroySemaphore(surface->lock); //Destory the semaphore!
		surface->lock = NULL; //No lock anymore!
	}
	freez((void **)ptr, sizeof(GPU_SDL_Surface), "freeSurfacePtr GPU_SDL_Surface");
}

GPU_SDL_Surface *getSurfaceWrapper(SDL_Surface *surface) //Retrieves a surface wrapper to use with our functions!
{
	GPU_SDL_Surface *wrapper = NULL;
	wrapper = (GPU_SDL_Surface *)zalloc(sizeof(GPU_SDL_Surface),"GPU_SDL_Surface",NULL); //Allocate the wrapper!
	if (!wrapper) //Failed to allocate the wrapper?
	{
		return NULL; //Error!
	}
	wrapper->sdllayer = surface; //The surface to use within the wrapper!
	wrapper->lock = SDL_CreateSemaphore(1); //The lock!
	return wrapper; //Give the allocated wrapper!
}

//registration of a wrapped surface.
void registerSurface(GPU_SDL_Surface *surface, char *name, byte allowsurfacerelease) //Register a surface!
{
	if (!surface) return; //Invalid surface!
	if (!changedealloc(surface, sizeof(*surface), &freeSurfacePtr)) //We're changing the default dealloc function for our override!
	{
		return; //Can't change registry for 'releasing the surface container' handler!
	}
	if (!registerptr(surface->sdllayer, sizeof(*surface->sdllayer), name, NULL,NULL)) //The surface itself!
	{
		if (!memprotect(surface->sdllayer, sizeof(*surface->sdllayer), name)) //Failed to register?
		{
			dolog("registerSurface", "Registering the surface failed.");
			return;
		}
	}

	uint_32 pixels_size;
	pixels_size = surface->sdllayer->h*get_pixelrow_pitch(surface)*sizeof(uint_32); //The size of the pixels structure!
	if (!memprotect(surface->sdllayer->pixels, pixels_size, NULL)) //Not already registered (fix for call from createSurfaceFromPixels)?
	{
		if (!registerptr(surface->sdllayer->pixels, pixels_size, "Surface_Pixels", NULL,NULL)) //The pixels within the surface! We can't be released natively!
		{
			if (!memprotect(surface->sdllayer->pixels, pixels_size, "Surface_Pixels")) //Not registered?
			{
				dolog("registerSurface", "Registering the surface pixels failed.");
				logpointers("registerSurface");
				unregisterptr(surface->sdllayer, sizeof(*surface->sdllayer)); //Undo!
				return;
			}
		}
	}

	//Next our userdata!
	surface->flags |= SDL_FLAG_DIRTY; //Initialise to a dirty surface (first rendering!)
	if (!allowsurfacerelease) //Don't allow surface release?
	{
		surface->flags |= SDL_FLAG_NODELETE; //Don't delete the surface!
	}
}

//Memory value comparision.

//Returns 1 on not equal, 0 on equal!
OPTINLINE byte diffmem(void *start, byte value, uint_32 size)
{
	byte *current = (byte *)start; //Convert to byte list!
	byte result = 0; //Default: equal!
	if (size) //Gotten size?
	{
		byte restsize = 0;
		byte valid = 0;
		for (;;) //Check the data!
		{
			//Recheck valid!
			if (!restsize) //Not set or not checked yet?
			{
				restsize = (size>50)?50:size; //Limit to 50 bytes checked at once!
				valid = (memprotect(current,sizeof(*current)*restsize,NULL)==current); //Update valid for following 50 bytes?
				if (!valid)
				{
					restsize = 1; //Recheck after next!
				}
			}

			if (restsize-- && valid) //Valid?
			{
				if (*current!=value) //Gotten a different value?
				{
					result = 1; //Set changed!
				}
			}
			if (!--size) //Done?
			{
				break;
			}
			++current; //Next item!
		}
	}
	return result; //Give the result!
}

//Returns 1 on not equal, 0 on equal!
OPTINLINE byte memdiff(void *start, void *value, uint_32 size)
{
	byte *current = (byte *)start; //Convert to byte list!
	byte *ref = (byte *)value; //To compare to!
	byte result = 0; //Default: equal!
	if (size) //Gotten size?
	{
		byte restsize = 0;
		byte valid = 0;
		for (;;) //Check the data!
		{
			//Recheck valid!
			if (!restsize) //Not set or not checked yet?
			{
				restsize = (size>50)?50:size; //Limit to 50 bytes checked at once!
				valid = ((memprotect(current,sizeof(*current)*restsize,NULL)==current) && (memprotect(ref,sizeof(*ref)*restsize,NULL)==ref)); //Update valid for following 50 bytes?
				if (!valid)
				{
					restsize = 1; //Recheck after next!
				}
			}

			if (restsize-- && valid) //Still valid?
			{
				if (*current!=*ref) //Gotten a different value?
				{
					result = 1; //Set changed!
				}
			}
			if (!--size) //Done?
			{
				break;
			}
			++current; //Next item!
			++ref; //Next item!
		}
	}
	return result; //Give the result!
}

//Color key matching.
OPTINLINE void matchColorKeys(const GPU_SDL_Surface* src, GPU_SDL_Surface* dest ){
	if (!(src && dest)) return; //Abort: invalid src/dest!
	if (!memprotect((void *)src,sizeof(*src),NULL) || !memprotect((void *)dest,sizeof(dest),NULL)) return; //Invalid?
	if( src->sdllayer->flags & SDL_SRCCOLORKEY )
	{
		Uint32 colorkey = src->sdllayer->format->colorkey;
		SDL_SetColorKey( dest->sdllayer, SDL_SRCCOLORKEY, colorkey );
	}
}

//Resizing.
GPU_SDL_Surface *resizeImage( GPU_SDL_Surface *img, const uint_32 newwidth, const uint_32 newheight, byte doublexres, byte doubleyres, int keepaspectratio)
{
	//dolog("SDL","ResizeImage called!");
	if (!img) //No image to resize?
	{
		return NULL; //Nothin to resize is nothing back!
	}
	if ((!img->sdllayer->w) || (!img->sdllayer->h) || (!newwidth) || (!newheight)) //No size to resize?
	{
		return NULL; //Nothing to resize!
	}

	//dolog("SDL","ResizeImage: valid surface to resize. Calculating new size...");
	//Calculate destination resolution!
	uint_32 n_width = newwidth;
	uint_32 n_height = newheight; //New width/height!
	if (keepaspectratio) //Keeping the aspect ratio?
	{
		double ar = (double)img->sdllayer->w / (double)img->sdllayer->h;
		double newAr = (double)n_width / (double)n_height;
		double f = MAX(ar,newAr);
		if (f==ar) //Fit to width?
		{
			n_height = (uint_32)(((double)n_width)/ar);
		}
		else //Fit to height?
		{
			n_width = (uint_32)(n_height*ar);
		}
	}

	//dolog("SDL","ResizeImage: Verifying new height/width...");
	if (!n_width || !n_height) //No size in src or dest?
	{
		return NULL; //Nothing to render, so give nothing!
	}

	byte doubleres;
	doubleres = 0; //Default: normal resolution!
	if (doublexres ^ doubleyres) //Either double x or y resolution, but not both?
	{
		n_width <<= doublexres; //Apply double width!
		n_height <<= doubleyres; //Apply double height!
		doubleres = 1; //Apply after double resolution!
	}


	//dolog("SDL","ResizeImage: calculating zoomx&y factor...");
	//Calculate factor to destination resolution!
	double zoomx = SAFEDIV(n_width,(double)img->sdllayer->w); //Resize to new width!
	double zoomy = SAFEDIV(n_height,(double)img->sdllayer->h); //Resize to new height!

	//dolog("SDL","ResizeImage: Checking memory requirements...");
	//Calcualted required memory!
	uint_32 memreq = (((n_width*n_height)*sizeof(uint_32))+sizeof(*img)); //Memory required for the new surface!
	if (freemem()<memreq) //Not enough memory left to render?
	{
		raiseError("SDL","Not enough memory left to resize: free:%i bytes, required: %i bytes; Shortage: %i bytes",freemem(),memreq,freemem()-memreq); //Log our shortage!
		return NULL; //Disabled: not enough memory left!
	}

	SDL_Surface* sized = NULL; //Sized?
	if (zoomx && zoomy) //Valid?
	{
		//dolog("SDL","Resizing screen...");
		sized = zoomSurface( img->sdllayer, zoomx, zoomy, SMOOTHING_ON );
		//dolog("SDL","Resizing done.");
		if (sized) //Memory left?
		{
			//dolog("SDL","Generating surface wrapper...");
			GPU_SDL_Surface *wrapper = getSurfaceWrapper(sized); //Get our wrapper!
			//dolog("SDL","Wrapper createn?");
			if (!wrapper) //Failed to generate a wrapper?
			{
				//dolog("SDL","Wrapper failed. Cleaning up surface...");
				SDL_FreeSurface(sized); //Release the generated surface!
				//dolog("SDL","Surface cleaned up. Returning non-used surface...");
				return NULL; //Error!
			}
			registerSurface(wrapper,"SDL_Surface",1); //Register the surface itself!
			//dolog("SDL","Filling wrapper...");
			wrapper->flags |= SDL_FLAG_DIRTY; //Mark as dirty by default!
			//dolog("SDL","Matching color keys...");
			//Valid wrapper?
			matchColorKeys( img, wrapper ); //Match the color keys!
			//dolog("SDL","Done!");

			if (doubleres) //Apply double resolution?
			{
				GPU_SDL_Surface *aftereffect;
				aftereffect = resizeImage(wrapper,newwidth,newheight,0,0,keepaspectratio); //Try and resize to destination resolution normally!
				freeSurface(wrapper); //Free our generated surface of our double sized step!
				if (!aftereffect) //Failed to generate?
				{
					dolog("GPU","Error applying aftereffect after scaling the double resolution image!");
					return NULL; //Error!
				}
				return aftereffect; //Give the double resolution resized version!
			}

			return wrapper; //Give the wrapper!
		}
	}
	return NULL; //Error!
}

//Pixels between rows.
uint_32 get_pixelrow_pitch(GPU_SDL_Surface *surface) //Get the difference between two rows!
{
	if (!surface)
	{
		dolog("GPP","Pitch: invalid NULL-surface!");
		return 0; //No surface = no pitch!
	}
	if (memprotect(surface,sizeof(*surface),NULL)!=surface)
	{
		dolog("GPP","Pitch: invalid protected surface container!");
		return 0; //Invalid surface container!
	}
	if (memprotect(surface->sdllayer,sizeof(surface->sdllayer),NULL)!=surface->sdllayer)
	{
		dolog("GPP","Pitch: invalid SDL_Surface!");
		return 0; //Invalid surface!
	}
	if (surface->sdllayer->pitch && surface->sdllayer->pitch>=sizeof(uint_32)) //Got pitch?
	{
		return (surface->sdllayer->pitch/sizeof(uint_32)); //Pitch in pixels!
	}
	return surface->sdllayer->w; //Just use the width as a pitch to fall back to!
}

//Retrieve a pixel
uint_32 get_pixel(GPU_SDL_Surface* surface, const int x, const int y ){
	if (!surface) return 0; //Disable if no surface!
	Uint32 *pixels = (Uint32*)surface->sdllayer->pixels;
	if (memprotect(&pixels[ ( y * get_pixelrow_pitch(surface) ) + x ],sizeof(uint_32),NULL)) //Valid?
	{
		return pixels[ ( y * get_pixelrow_pitch(surface) ) + x ];
	}
	return 0; //Invalid pixel!
}

//Draw a pixel
void put_pixel(GPU_SDL_Surface *surface, const int x, const int y, const Uint32 pixel ){
	if (!surface) return; //Disable if no surface!
	if (!memprotect(surface,sizeof(*surface),NULL)) return; //Invalid surface!
	if (!memprotect(surface->sdllayer,sizeof(*surface->sdllayer),NULL)) return; //Invalid layer!
	if (y >= surface->sdllayer->h) return; //Invalid row!
	if (x >= surface->sdllayer->w) return; //Invalid column!
	Uint32 *pixels = (Uint32 *)surface->sdllayer->pixels;
	Uint32 *pixelpos = &pixels[ ( y * get_pixelrow_pitch(surface) ) + x ]; //The pixel!
	if (*pixelpos!=pixel) //Different?
	{
		surface->flags |= SDL_FLAG_DIRTY; //Mark as dirty!
	}
	*pixelpos = pixel;
}

//Retrieve a pixel/row(pixel=0).
OPTINLINE void *get_pixel_ptr(GPU_SDL_Surface *surface, const int y, const int x)
{
	if (!surface) return NULL; //Invalid surface altogether!
	if (!memprotect(surface,sizeof(GPU_SDL_Surface),NULL))
	{
		#ifdef PPRLOG
			dolog("PPR","Get_pixel_ptr: Invalid surface container!");
		#endif
		return NULL; //Unknown!
	}
	if (!memprotect(surface->sdllayer,sizeof(*surface->sdllayer),NULL))
	{
		#ifdef PPRLOG
			dolog("PPR","Get_pixel_ptr: Invalid surface!");
		#endif
		return NULL; //Unknown!
	}
	if ((y<surface->sdllayer->h) && (x<surface->sdllayer->w)) //Within range?
	{
		uint_32 *pixels = (uint_32 *)surface->sdllayer->pixels;
		if (memprotect(pixels,get_pixelrow_pitch(surface)*surface->sdllayer->h*sizeof(uint_32),NULL)) //Valid pixels?
		{
			uint_32 *result = &pixels[ ( y * get_pixelrow_pitch(surface) ) + x ]; //Our result!
			//No pitch? Use width to fall back!
			if (memprotect(result,sizeof(uint_32),NULL)) //Valid?
			{
				return result; //The pixel ptr!
			}
			else
			{
#ifdef PPRLOG
				dolog("PPR","Get_pixel_ptr: Invalid SDL_Surface pixels@%i,%i!",x,y);
#endif
			}
		}
		#ifdef PPRLOG
			else
			{
				dolog("PPR", "Get_pixel_ptr: Invalid SDL_Surface pixels:entry:%p!", pixels);
				logpointers("PPR: Invalid SDL_Surface pixels!"); //Log all pointers!
			}
		#endif
	}
	#ifdef PPRLOG
		else
		{
			dolog("PPR", "Get_pixel_ptr: Invalid row!");
		}
	#endif
	return NULL; //Out of range!
}

//Row functions, by me!
uint_32 *get_pixel_row(GPU_SDL_Surface *surface, const int y, const int x)
{
	return (uint_32 *)get_pixel_ptr(surface,y,x); //Give the pointer!
}

/*

put_pixel_row: Puts an entire row in the buffer!
parameters:
	surface: The surface to write to.
	y: The line to write to.
	rowsize: The ammount of pixels to consider to copy.
	pixels: The pixels to copy itself (uint_32 array).
	center: Centering flags:
		Bits 0-1: What centering action to use:
			=0: Left centering (Default)
			=1: Horizontal centering
			=2: Right centering
		Bit 2: To disable clearing on the line (for multiple data copies per row).
			=0: Clear both sides if available.
			=1: Disable clearing
	row_start: Where to start copying the pixels on the surface line. Only used when aligning left. Also affect left align clearing (As the screen is shifted to the right).


*/

void put_pixel_row(GPU_SDL_Surface *surface, const int y, uint_32 rowsize, uint_32 *pixels, int center, uint_32 row_start) //Based upon above, but for whole rows at once!
{
	if (surface && pixels) //Got surface and pixels!
	{
		if (y >= surface->sdllayer->h) return; //Invalid row detection!
		uint_32 use_rowsize = MIN(get_pixelrow_pitch(surface),rowsize); //Minimum is decisive!
		if (use_rowsize) //Got something to copy and valid row?
		{
			if ((row_start+use_rowsize)>get_pixelrow_pitch(surface) && ((!(center&3)) && (!(center&4)))) //More than we can handle?
			{
				use_rowsize -= ((row_start+use_rowsize)-get_pixelrow_pitch(surface)); //Make it no larger than the surface can handle (no overflow protection)!
				//dolog("SDL_putpixelrow","Use rowsize: %i; Max pitch: %i, Requested size: %i",use_rowsize,get_pixelrow_pitch(surface),rowsize);
			}
			if (use_rowsize>0) //Gotten row size to copy?
			{
				uint_32 *row = get_pixel_row(surface,y,0); //Row at the left!
				if (row) //Gotten the row (valid row?)
				{
					uint_32 restpixels = (surface->sdllayer->w)-use_rowsize; //Rest ammount of pixels!
					uint_32 start = (surface->sdllayer->w/2) - (use_rowsize/2); //Start of the drawn part!
					switch (center&3) //What centering method?
					{
					case 2: //Right side plot?
						//Just plain plot at the right, filling with black on the left when not centering!
						if ((restpixels>0) && (!(center&4))) //Still a part of the row not rendered and valid rest location?
						{
							if (diffmem(row,0,restpixels*4)) //Different?
							{
								surface->flags |= SDL_FLAG_DIRTY; //Mark as dirty!
							}
							if (memprotect(&row[0],restpixels*4,NULL)) //Valid?
							{
								memset(row,0,restpixels*4); //Clear to the start of the row, so that only the part we specified gets something!
							}
						}
						if (memprotect(&row[restpixels],use_rowsize*4,NULL) && memprotect(pixels,use_rowsize*4,NULL)) //Both valid?
						{
							if (memdiff(&row[restpixels],pixels,use_rowsize*4)) //Different?
							{
								surface->flags |= SDL_FLAG_DIRTY; //Mark as dirty!
							}
							memcpy(&row[restpixels],pixels,use_rowsize*4); //Copy the row to the buffer as far as we can go!
						}
						#ifdef PPRLOG
							else
							{
								dolog("GPU_Renderer", "Invalid src/dest specified!");
							}
						#endif
						break;
					case 1: //Use horizontal centering?
						if ((sword)surface->sdllayer->w>(sword)(use_rowsize+2)) //We have space left&right to plot? Also must have at least 2 pixels left&right to center!
						{
							if (!(center&4)) //Clear enabled?
							{
								if (diffmem(row,0,start*4) || diffmem(&row[start+use_rowsize],0,(surface->sdllayer->w-(start+use_rowsize))*4)) //Different left or right?
								{
									surface->flags |= SDL_FLAG_DIRTY; //Mark as dirty!
								}
								if (memprotect(row,start*4,NULL)) //Valid?
								{
									memset(row,0,start*4); //Clear the left!
								}
								if (memprotect(&row[start+use_rowsize],(surface->sdllayer->w-(start+use_rowsize))*4,NULL)) //Valid?
								{
									memset(&row[start+use_rowsize],0,(surface->sdllayer->w-(start+use_rowsize))*4); //Clear the right!
								}
							}
							if (memdiff(&row[start],pixels,use_rowsize*4)) //Different?
							{
								surface->flags |= SDL_FLAG_DIRTY; //Mark as dirty!
							}
							if (memprotect(&row[start],use_rowsize*4,NULL) && memprotect(pixels,use_rowsize*4,NULL)) //Valid?
							{
								memcpy(&row[start],pixels,use_rowsize*4); //Copy the pixels to the center!
							}
							#ifdef PPRLOG
								else
								{
									dolog("PPR","Invalid src/dest specified!");
								}
							#endif
							return; //Done: we've written the pixels at the center!
						}
						//We don't need centering: just do left side plot!
					default: //We default to left side plot!
					case 0: //Left side plot?
						restpixels -= row_start; //The pixels that are left are lessened by row_start in this mode too!
						if (memprotect(&row[row_start],use_rowsize*4,NULL) && memprotect(pixels,use_rowsize*4,NULL)) //Valid?
						{
							if (memcmp(&row[row_start],pixels,use_rowsize*4)) //Different?
							{
								surface->flags |= SDL_FLAG_DIRTY; //Mark as dirty!
							}
							memcpy(&row[row_start],pixels,use_rowsize*4); //Copy the row to the buffer as far as we can go!
						}
#ifdef PPRLOG
						else
						{
							dolog("PPR", "Invalid src/dest specified!");
						}
#endif
						//Now just render the rest part of the line to black!
						if ((restpixels>0) && (!(center&4))) //Still a part of the row not rendered and valid rest location and not disable clearing?
						{
							if (diffmem(&row[row_start+use_rowsize],0,restpixels*4)) //Different?
							{
								surface->flags |= SDL_FLAG_DIRTY; //Mark as dirty!
							}
							if (memprotect(&row[row_start+use_rowsize],restpixels*4,NULL)) //Valid?
							{
								memset(&row[row_start+use_rowsize],0,restpixels*4); //Clear to the end of the row, so that only the part we specified gets something!
							}
						}
						break;
					}
				}
				else
				{
#ifdef PPRLOG
					dolog("PPR", "Invalid surface row:%i!", y);
#endif
				}
			}
		}
#ifdef PPRLOG
		else
		{
			dolog("PPR","Invalid row size: Surface: %i, Specified: %i",get_pixelrow_pitch(surface),rowsize); //Log it!
		}
#endif
	}
	else if (surface && (!(center&4))) //Surface, but no pixels: clear the row? Also clearing must be enabled to do so.
	{
#ifdef PPRLOG
		dolog("PPR", "Rendering empty pixels because of invalid data to copy.");
#endif
		uint_32 *row = get_pixel_row(surface,y,0); //Row at the left!
		if (row && surface->sdllayer->w) //Got row?
		{
			if (diffmem(row,0,surface->sdllayer->w*4)) //Different?
			{
				surface->flags |= SDL_FLAG_DIRTY; //Mark as dirty!
			}
			if (memprotect(row,surface->sdllayer->w*4,NULL)) //Valid?
			{
				memset(row,0,surface->sdllayer->w*4); //Clear the row, because we have no pixels!
			}
		}
	}
	else
	{
#ifdef PPRLOG
		dolog("PPR", "Invalid surface specified!");
#endif
	}
}

//Generate a byte order for SDL.
OPTINLINE void loadByteOrder(uint_32 *rmask, uint_32 *gmask, uint_32 *bmask, uint_32 *amask)
{
	//Entirely dependant upon the system itself!
	*rmask = RGBA(0xFF,0x00,0x00,0x00);
	*gmask = RGBA(0x00,0xFF,0x00,0x00);
	*bmask = RGBA(0x00,0x00,0xFF,0x00);
	*amask = RGBA(0x00,0x00,0x00,0xFF);
}

//Create a new surface.
GPU_SDL_Surface *createSurface(int columns, int rows) //Create a new 32BPP surface!
{
	uint_32 rmask,gmask,bmask,amask; //Masks!
	loadByteOrder(&rmask,&gmask,&bmask,&amask); //Load our masks!
	SDL_Surface *surface = SDL_CreateRGBSurface(SDL_SWSURFACE,columns,rows, 32, rmask,gmask,bmask,amask); //Try to create it!
	if (!surface) //Failed to allocate?
	{
		return NULL; //Not allocated: we've failed to allocate the pointer!
	}
	GPU_SDL_Surface *wrapper;
	wrapper = getSurfaceWrapper(surface); //Give the surface we've allocated in the standard wrapper!
	registerSurface(wrapper,"SDL_Surface",1); //Register the surface we've wrapped!
	return wrapper;
}

//Create a new surface from an existing buffer.
GPU_SDL_Surface *createSurfaceFromPixels(int columns, int rows, void *pixels, uint_32 pixelpitch) //Create a 32BPP surface, but from an allocated/solid buffer (not deallocated when freed)! Can be used for persistent buffers (always there, like the GPU screen buffer itself)
{
	uint_32 rmask,gmask,bmask,amask; //Masks!
	loadByteOrder(&rmask,&gmask,&bmask,&amask); //Load our masks!

	pixelpitch <<= 2; //4 bytes a pixel!
	SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels,columns,rows, 32, pixelpitch, rmask,gmask,bmask,amask); //Try to create it!
	if (!surface) //Failed to allocate?
	{
		return NULL; //Not allocated: we've failed to allocate the pointer!
	}
	GPU_SDL_Surface *wrapper;
	wrapper = getSurfaceWrapper(surface); //Give the surface we've allocated in the standard wrapper!
	registerSurface(wrapper,"SDL_Surface",1); //Register the surface we've wrapped!
	surface->flags |= SDL_FLAG_NODELETE_PIXELS; //Don't delete the pixels: we're protected from being deleted together with the surface!
	return wrapper;
}

//Release a surface.
GPU_SDL_Surface *freeSurface(GPU_SDL_Surface *surface)
{
	if (!surface) return NULL; //Invalid surface?
	if (memprotect(surface,sizeof(GPU_SDL_Surface),NULL)) //Allocated?
	{
		if (memprotect(surface->sdllayer->pixels,surface->sdllayer->h*get_pixelrow_pitch(surface)*sizeof(uint_32),NULL)) //Pixels also allocated?
		{
			GPU_SDL_Surface *newsurface = surface; //Take the surface to use!
			freeSurfacePtr((void **)&newsurface,sizeof(*newsurface),NULL); //Release the surface via our kernel function!
			return newsurface; //We're released (or not)!
		}
	}
	return surface; //Still allocated!
}

//Draw the screen with a surface.
void safeFlip(GPU_SDL_Surface *surface) //Safe flipping (non-null)
{
	if (memprotect(surface,sizeof(GPU_SDL_Surface),NULL)) //Surface valid and allowed to show pixels?
	{
		if (surface->flags&SDL_FLAG_DIRTY) //Dirty surface needs rendering only?
		{
			if (memprotect(surface->sdllayer,sizeof(surface->sdllayer),NULL)) //Valid?
			{
				//If the surface must be locked
				/*
				if( SDL_MUSTLOCK( surface->sdllayer ) )
				{
					//Lock the surface
					SDL_LockSurface( surface->sdllayer );
				}

				SDL_Flip(surface->sdllayer); //Flip!

				//Unlock surface
				if( SDL_MUSTLOCK( surface->sdllayer ) )
				{
					SDL_UnlockSurface( surface->sdllayer );
				}
				*/ //Just call updaterect!
				SDL_UpdateRect(surface->sdllayer, 0, 0, 0, 0); //Make sure we update!
			}
			surface->flags &= ~SDL_FLAG_DIRTY; //Not dirty anymore!
		}
	}
}
