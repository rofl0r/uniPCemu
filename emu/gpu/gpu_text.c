#include "headers/emu/gpu/gpu.h" //Basic GPU!
#include "headers/emu/gpu/gpu_text.h" //Our prototypes!
#include "headers/emu/gpu/gpu_sdl.h" //Our prototypes!
#include "headers/hardware/vga_screen/vga_vramtext.h" //VGA for font!
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
	cx /= 10; //Shift to the character we're from!
	cy /= 10; //Shift to the character we're from!
	*charx = cx;
	*chary = cy;
	*x %= 10; //Within the character!
	*y %= 10; //Within the character!
	/*
	--*x; //Adjust for the border!
	--*y; //Adjust for the border!
	*/
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
	surface->dirty[fy][fx] = 0; //We're going to un-dirty this!
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

OPTINLINE void GPU_textput_pixel(GPU_SDL_Surface *dest, GPU_TEXTSURFACE *surface,int fx, int fy) //Get the pixel font, back or show through. Automatically plotted if set.
{
	if (surface->dirty[fy][fx]) updateDirty(surface,fx,fy); //Update dirty if needed!
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
	GPU_TEXTSURFACE *surface = (GPU_TEXTSURFACE *)zalloc(sizeof(GPU_TEXTSURFACE),"GPU_TEXTSURFACE"); //Create an empty initialised surface!
	if (!surface) //Failed to allocate?
	{
		return NULL; //Failed to allocate!
	}
	memset(surface->dirty,1,sizeof(surface->dirty)); //All dirty, to be rendered!
	//We don't need a screen, because we plot straight to the destination surface (is way faster than blitting)!
	byte c;
	for (c=0;c<8;c++)
	{
		surface->backlist[c].x = defaultbacklist[c].x; //Copy x!
		surface->backlist[c].y = defaultbacklist[c].y; //Copy y!
	}
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
	if (!surface) return 0; //Abort without surface!
	TicksHolder ms_render_lastcheck; //For counting ms to render (GPU_framerate)!
	initTicksHolder(&ms_render_lastcheck); //Init for counting time of rendering on the device directly from VGA data!
	getuspassed(&ms_render_lastcheck); //Get first value!
	register int y=0;
	GPU_TEXTSURFACE *tsurface = (GPU_TEXTSURFACE *)surface; //Convert!
	for (;;) //Process all rows!
	{
		register int x=0; //Reset x!
		for (;;) //Process all columns!
		{
			GPU_textput_pixel(rendersurface,tsurface,x,y); //Plot a pixel?
			if (++x==GPU_TEXTPIXELSX) break; //Stop searching now!
		}
		if (++y==GPU_TEXTPIXELSY) break; //Stop searching now!
	}
	tsurface->flags &= ~TEXTSURFACE_FLAG_DIRTY; //Clear dirty flag!
	return getuspassed(&ms_render_lastcheck); //Give processing time!
}

int GPU_textgetxy(GPU_TEXTSURFACE *surface,int x, int y, byte *character, uint_32 *font, uint_32 *border) //Read a character+attribute!
{
	if (!surface) return 0; //Not allocated!
	if (y>=GPU_TEXTSURFACE_HEIGHT) return 0; //Out of bounds?
	if (x>=GPU_TEXTSURFACE_WIDTH) return 0; //Out of bounds?
	*character = surface->text[y][x];
	*font = surface->font[y][x];
	*border = surface->border[y][x];
	return 1; //OK!
}

void GPU_markdirty(GPU_TEXTSURFACE *surface, int x, int y) //Mark a character as dirty (GPU_text only, to prevent multirendering!)
{
	int rx;
	int ry;

	int tx = x * 10;
	int ty = y * 10;
	for (rx=-1;rx<11;) //Take one pixel extra for neighbouring pixels.
	{
		int cx = tx + rx;
		for (ry = -1; ry<11;)
		{
			int cy = ty+ry;
			if (cx>=0 && cy>=0) //Valid positions?
			{
				surface->dirty[cy][cx] = 1; //Set dirty!
			}
			++ry;
		}
		++rx;
	}
	surface->flags |= TEXTSURFACE_FLAG_DIRTY; //Set dirty!
}

int GPU_textsetxy(GPU_TEXTSURFACE *surface,int x, int y, byte character, uint_32 font, uint_32 border) //Write a character+attribute!
{
	if (!surface) return 0; //Not allocated!
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
	if (change) GPU_markdirty(surface,x,y); //Mark us as dirty when needed!
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
	if (!surface) return; //Not allocated!
	char msg[256];
	bzero(msg,sizeof(msg)); //Init!

	va_list args; //Going to contain the list!
	va_start (args, text); //Start list!
	vsprintf (msg, text, args); //Compile list!

	int curx=surface->x; //Init x!
	int cury=surface->y; //init y!
	int i;
	for (i=0; i<strlen(msg); i++) //Process text!
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
	if (!surface) return; //Not allocated!
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

OPTINLINE byte GPU_textdirty(void *surface)
{
	GPU_TEXTSURFACE *tsurface = (GPU_TEXTSURFACE *)surface;
	if (tsurface)
	{
		return (tsurface->flags&TEXTSURFACE_FLAG_DIRTY) > 0; //Are we dirty?
	}
	return 0; //No surface = not dirty!
}

void GPU_enableDelta(GPU_TEXTSURFACE *surface, byte xdelta, byte ydelta) //Enable delta coordinates on the x/y axis!
{
	surface->xdelta = xdelta; //Enable x delta?
	surface->ydelta = ydelta; //Enable y delta?
}

void GPU_text_updatedelta(SDL_Surface *surface)
{
	if (!surface) return; //Invalid surface!
	sword xdelta, ydelta;
	xdelta = surface->w; //Current resolution!
	ydelta = surface->h; //Current resolution!
	xdelta -= GPU_TEXTPIXELSX;
	ydelta -= GPU_TEXTPIXELSY; //Calculate delta!
	TEXT_xdelta = xdelta; //Horizontal delta!
	TEXT_ydelta = ydelta; //Vertical delta!
}