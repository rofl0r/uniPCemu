#include "headers/emu/gpu/gpu.h" //Basic GPU!
#include "headers/emu/gpu/gpu_text.h" //Our prototypes!
#include "headers/emu/gpu/gpu_sdl.h" //Our prototypes!
#include "headers/hardware/vga_rest/textmodedata.h" //VGA for font!
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/support/log.h" //Logging support!
#include "headers/support/bmp.h" //Bitmap support!
#include "headers/support/highrestimer.h" //High resolution timer!

#define __HW_DISABLED 0

//\n is newline? Else \r\n is newline!
#define USESLASHN 1

extern GPU_SDL_Surface *rendersurface; //The PSP's surface to use when flipping!

word TEXT_xdelta = 0;
word TEXT_ydelta = 0; //Delta x,y!

OPTINLINE void GPU_textcalcpixel(int *x, int *y, int *charx, int *chary)
{
	int cx=*x;
	int cy=*y;
	int cx2, cy2;

	cx /= 10; //Shift to the character we're from!
	cy /= 10; //Shift to the character we're from!
	*charx = cx; //Set!
	*chary = cy; //Set!

	//Now the pixel within!
	cx2 = *x; //Read original x!
	cy2 = *y; //Read original y!
	cx *= 10; //Get original pixel!
	cy *= 10; //Get original pixel!
	cx2 -= cx; //Substract from the pixel!
	cy2 -= cy; //Substract from the pixel!
	*x = cx2; //Our inner x!
	*y = cy2; //Our inner y!
}

OPTINLINE byte getcharxy_8(byte character, int x, int y) //Retrieve a characters x,y pixel on/off from the unmodified 8x8 table!
{
	static uint_32 lastcharinfo; //attribute|character|0x80|row, bit8=Set?

	if ((lastcharinfo & 0xFFFF) != ((character << 8) | 0x80 | y)) //Last row not yet loaded?
	{
		uint_32 addr = 0; //Address for old method!
		addr += character << 3; //Start adress of character!
		addr += (y & 7); //1 byte per row!

		byte lastrow = int10_font_08[addr]; //Read the row from the character generator!
		lastcharinfo = ((lastrow << 16) | (character << 8) | 0x80 | y); //Last character info loaded!
	}

	byte bitpos = 23 - (x % 8); //x or 7-x for reverse?
	return ((lastcharinfo&(1 << bitpos)) >> bitpos); //Give result!
}


OPTINLINE byte GPU_textget_pixel(GPU_TEXTSURFACE *surface, int x, int y) //Get direct pixel from handler (overflow handled)!
{
	if (((x<0) || (y<0) || ((y/10)>=GPU_TEXTSURFACE_HEIGHT) || ((x/10)>=GPU_TEXTSURFACE_WIDTH))) return 0; //None when out of bounds!
	int charx, chary, x2=x, y2=y;
	GPU_textcalcpixel(&x2, &y2, &charx, &chary); //Calculate our info!
	if ((x2 < 0) || (x2 >= 8) || (y2 < 0) || (y2 >= 8)) return 0; //Out of range = background!
	return getcharxy_8(surface->text[chary][charx], x2, y2); //Give the pixel of the character!
}

OPTINLINE uint_32 GPU_textgetcolor(GPU_TEXTSURFACE *surface, int x, int y, int border) //border = either border(1) or font(0)
{
	if (((x<0) || (y<0) || ((y / 10) >= GPU_TEXTSURFACE_HEIGHT) || ((x / 10) >= GPU_TEXTSURFACE_WIDTH))) return TRANSPARENTPIXEL; //None when out of bounds!
	int charx, chary, x2=x, y2=y;
	GPU_textcalcpixel(&x2, &y2, &charx, &chary); //Calculate our info!
	return border ? surface->border[chary][charx] : surface->font[chary][charx]; //Give the border or font of the character!
}

OPTINLINE void updateDirty(GPU_TEXTSURFACE *surface, int fx, int fy)
{
	//Undirty!
	if (GPU_textget_pixel(surface,fx,fy)) //Font?
	{
		surface->notdirty[fy][fx] = GPU_textgetcolor(surface,fx,fy,0); //Font!
		return;
	}

	//We're background/transparent!
	BACKLISTITEM *curbacklist = &surface->backlist[0]; //The current backlist!
	register int x, y;
	register byte c = 0;
	for (;;)
	{
		x = fx;
		y = fy;
		x += curbacklist->x;
		y += curbacklist->y;
		if (GPU_textget_pixel(surface,x,y)) //Border?
		{
			surface->notdirty[fy][fx] = GPU_textgetcolor(surface,x,y,1); //Back of the related pixel!
			return; //Done!
		}
		if (++c==8) break; //Stop searching!
		++curbacklist; //Next backlist item!
	}

	//We're transparent!
	surface->notdirty[fy][fx] = TRANSPARENTPIXEL;
}

OPTINLINE void GPU_textput_pixel(GPU_SDL_Surface *dest, GPU_TEXTSURFACE *surface,int fx, int fy, byte redraw) //Get the pixel font, back or show through. Automatically plotted if set.
{
	if (!surface) return; //Invalid surface?
	if (redraw) updateDirty(surface,fx,fy); //Update dirty if needed!
	register uint_32 color = surface->notdirty[fy][fx];
	if (color!=TRANSPARENTPIXEL)
	{
		if (surface->xdelta) fx += TEXT_xdelta; //Apply delta position to the output pixel!
		if (surface->ydelta) fy += TEXT_ydelta; //Apply delta position to the output pixel!
		put_pixel(dest,fx,fy,color); //Plot the pixel!
	}
	//We're transparent, do don't plot!
}

BACKLISTITEM defaultbacklist[8] = {{1,1},{1,0},{0,1},{1,-1},{-1,1},{0,-1},{-1,0},{-1,-1}}; //Default back list!

GPU_TEXTSURFACE *alloc_GPUtext()
{
	GPU_TEXTSURFACE *surface = (GPU_TEXTSURFACE *)zalloc(sizeof(GPU_TEXTSURFACE),"GPU_TEXTSURFACE",NULL); //Create an empty initialised surface!
	if (!surface) //Failed to allocate?
	{
		return NULL; //Failed to allocate!
	}
	//We don't need a screen, because we plot straight to the destination surface (is way faster than blitting)!
	byte c;
	for (c=0;c<8;c++)
	{
		surface->backlist[c].x = defaultbacklist[c].x; //Copy x!
		surface->backlist[c].y = defaultbacklist[c].y; //Copy y!
	}

	surface->lock = SDL_CreateSemaphore(1); //Create our lock for when we are used!
	return surface; //Give the allocated surface!
}

void free_GPUtext(GPU_TEXTSURFACE **surface)
{
	if (!surface) return; //Still allocated or not!
	if (!*surface) return; //Still allocated or not!
	freez((void **)surface,sizeof(GPU_TEXTSURFACE),"GPU_TEXTSURFACE"); //Release the memory, if possible!
	if (*surface) //Still allocated?
	{
		dolog("zalloc","GPU_TextSurface still allocated?");
	}
}

uint_64 GPU_textrenderer(void *surface) //Run the text rendering on rendersurface!
{
	if (__HW_DISABLED) return 0; //Disabled!
	if (!memprotect(surface,sizeof(GPU_TEXTSURFACE),"GPU_TEXTSURFACE")) return 0; //Abort without surface!
	register int y=0;
	GPU_TEXTSURFACE *tsurface = (GPU_TEXTSURFACE *)surface; //Convert!
	WaitSem(tsurface->lock);
	byte redraw;
	redraw = tsurface->flags&TEXTSURFACE_FLAG_DIRTY; //Redraw when dirty only?
	for (;;) //Process all rows!
	{
		register int x=0; //Reset x!
		for (;;) //Process all columns!
		{
			GPU_textput_pixel(rendersurface,tsurface,x,y,redraw); //Plot a pixel?
			if (++x==GPU_TEXTPIXELSX) break; //Stop searching now!
		}
		if (++y==GPU_TEXTPIXELSY) break; //Stop searching now!
	}
	tsurface->flags &= ~TEXTSURFACE_FLAG_DIRTY; //Clear dirty flag!
	PostSem(tsurface->lock); //We're finished with the surface!
	return 0; //Ignore processing time!
}

int GPU_textgetxy(GPU_TEXTSURFACE *surface,int x, int y, byte *character, uint_32 *font, uint_32 *border) //Read a character+attribute!
{
	if (!memprotect(surface, sizeof(GPU_TEXTSURFACE), "GPU_TEXTSURFACE")) return 0; //Abort without surface!
	if (y >= GPU_TEXTSURFACE_HEIGHT) return 0; //Out of bounds?
	if (x>=GPU_TEXTSURFACE_WIDTH) return 0; //Out of bounds?
	*character = surface->text[y][x];
	*font = surface->font[y][x];
	*border = surface->border[y][x];
	return 1; //OK!
}

int GPU_textsetxy(GPU_TEXTSURFACE *surface,int x, int y, byte character, uint_32 font, uint_32 border) //Write a character+attribute!
{
	if (!memprotect(surface, sizeof(GPU_TEXTSURFACE), "GPU_TEXTSURFACE")) return 0; //Abort without surface!
	if (y>=GPU_TEXTSURFACE_HEIGHT) return 0; //Out of bounds?
	if (x>=GPU_TEXTSURFACE_WIDTH) return 0; //Out of bounds?
	byte oldtext = surface->text[y][x];
	uint_32 oldfont = surface->font[y][x];
	uint_32 oldborder = surface->font[y][x];
	surface->text[y][x] = character;
	surface->font[y][x] = font;
	surface->border[y][x] = border;
	uint_32 change;
	character ^= oldtext;
	font ^= oldfont;
	border ^= oldborder;
	change = character;
	change |= font;
	change |= border;
	if (change) surface->flags |= TEXTSURFACE_FLAG_DIRTY; //Mark us as dirty when needed!
	return 1; //OK!
}

void GPU_textclearrow(GPU_TEXTSURFACE *surface, int y)
{
	int x=0;
	for (;;)
	{
		GPU_textsetxy(surface,x,y,0,0,0); //Clear the row fully!
		if (++x>=GPU_TEXTSURFACE_WIDTH) return; //Done!
	}
}

void GPU_textclearscreen(GPU_TEXTSURFACE *surface)
{
	int y=0;
	for (;;)
	{
		GPU_textclearrow(surface,y); //Clear all rows!
		if (++y>=GPU_TEXTSURFACE_HEIGHT) return; //Done!
	}
}

void GPU_textprintf(GPU_TEXTSURFACE *surface, uint_32 font, uint_32 border, char *text, ...)
{
	if (!memprotect(surface, sizeof(GPU_TEXTSURFACE), "GPU_TEXTSURFACE")) return; //Abort without surface!
	char msg[256];
	bzero(msg,sizeof(msg)); //Init!

	va_list args; //Going to contain the list!
	va_start (args, text); //Start list!
	vsprintf (msg, text, args); //Compile list!

	int curx=surface->x; //Init x!
	int cury=surface->y; //init y!
	int i;
	for (i=0; i<(int)strlen(msg); i++) //Process text!
	{
		while (curx>=GPU_TEXTSURFACE_WIDTH) //Overflow?
		{
			++cury; //Next row!
			curx -= GPU_TEXTSURFACE_WIDTH; //Decrease columns for every row size!
		}
		if ((msg[i]=='\r' && !USESLASHN) || (msg[i]=='\n' && USESLASHN)) //LF? If use \n, \n uses linefeed too, else just newline.
		{
			curx = 0; //Move to the left!
		}
		if (msg[i]=='\n') //CR?
		{
			++cury; //Next Y!
		}
		else if (msg[i]!='\r') //Never display \r!
		{
			GPU_textsetxy(surface,curx,cury,(byte)msg[i],font,border); //Write the character to our screen!
			++curx; //Next character!
		}
	}
	surface->x = curx; //Update x!
	surface->y = cury; //Update y!
}

void GPU_textgotoxy(GPU_TEXTSURFACE *surface,int x, int y) //Goto coordinates!
{
	if (!memprotect(surface, sizeof(GPU_TEXTSURFACE), "GPU_TEXTSURFACE")) return; //Abort without surface!
	int curx = x;
	int cury = y;
	while (curx>=GPU_TEXTSURFACE_WIDTH) //Overflow?
	{
		++cury; //Next row!
		curx -= GPU_TEXTSURFACE_WIDTH; //Decrease columns for every row size!
	}
	surface->x = curx; //Real x!
	surface->y = cury; //Real y!
}

void GPU_enableDelta(GPU_TEXTSURFACE *surface, byte xdelta, byte ydelta) //Enable delta coordinates on the x/y axis!
{
	if (!memprotect(surface, sizeof(GPU_TEXTSURFACE), "GPU_TEXTSURFACE")) return; //Abort without surface!
	surface->xdelta = xdelta; //Enable x delta?
	surface->ydelta = ydelta; //Enable y delta?
}

void GPU_text_updatedelta(SDL_Surface *surface)
{
	if (!surface) //Invalid surface!
	{
		TEXT_xdelta = TEXT_ydelta = 0; //No delta!
		return; //Invalid surface: no delta used!
	}
	sword xdelta, ydelta;
	xdelta = surface->w; //Current resolution!
	ydelta = surface->h; //Current resolution!
	xdelta -= GPU_TEXTPIXELSX;
	ydelta -= GPU_TEXTPIXELSY; //Calculate delta!
	TEXT_xdelta = xdelta; //Horizontal delta!
	TEXT_ydelta = ydelta; //Vertical delta!
}

void GPU_text_locksurface(GPU_TEXTSURFACE *surface) //Lock a surface for usage!
{
	if (!memprotect(surface,sizeof(*surface),"GPU_TEXTSURFACE")) return; //Invalid surface!
	if (!surface->lock) return; //no lock?
	WaitSem(surface->lock) //Wait for us to be available and locked!
}

void GPU_text_releasesurface(GPU_TEXTSURFACE *surface) //Unlock a surface when done with it!
{
	if (!memprotect(surface, sizeof(*surface), "GPU_TEXTSURFACE")) return; //Invalid surface!
	if (!surface->lock) return; //no lock?
	PostSem(surface->lock) //Release our lock: we're done!
}