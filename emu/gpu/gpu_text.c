#include "headers/emu/gpu/gpu.h" //Basic GPU!
#include "headers/emu/gpu/gpu_text.h" //Our prototypes!
#include "headers/emu/gpu/gpu_sdl.h" //Our prototypes!
#include "headers/hardware/vga.h" //VGA for font!
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/support/log.h" //Logging support!
#include "headers/support/bmp.h" //Bitmap support!
#include "headers/support/highrestimer.h" //High resolution timer!

//Special transparent pixel!
#define TRANSPARENTPIXEL 0

//\n is newline? Else \r\n is newline!
#define USESLASHN 1

extern GPU_SDL_Surface *rendersurface; //The PSP's surface to use when flipping!

static byte GPU_textget_pixel(PSP_TEXTSURFACE *surface, int x, int y) //Get direct pixel from handler (overflow handled)!
{
	if (((x<0) || (y<0) || ((y>>3)>GPU_ROWS) || ((x>>3)>GPU_COLUMNS))) return 0; //None when out of bounds!
	return getcharxy_8(surface->text[y>>3][x>>3], x&7, y&7); //Give the pixel of the character!
}

static uint_32 GPU_textgetcolor(PSP_TEXTSURFACE *surface, int x, int y, int border) //border = either border(1) or font(0)
{
	if (((x<0) || (y<0) || ((y>>3)>GPU_ROWS) || ((x>>3)>GPU_COLUMNS))) return TRANSPARENTPIXEL; //None when out of bounds!
	return border?surface->border[y>>3][x>>3]:surface->font[y>>3][x>>3]; //Give the border or font of the character!
}

static void updateDirty(PSP_TEXTSURFACE *surface, int fx, int fy)
{
	surface->dirty[fy][fx] = 0; //We're going to un-dirty this!
	//Undirty!
	if (GPU_textget_pixel(surface,fx,fy)) //Font?
	{
		surface->notdirty[fy][fx] = GPU_textgetcolor(surface,fx,fy,0); //Font!
		return;
	}

	BACKLISTITEM *curbacklist = &surface->backlist[0]; //The current backlist!
	byte c=0;
	for (;;)
	{
		if (GPU_textget_pixel(surface,fx+curbacklist->x,fy+curbacklist->y)) //Background?
		{
			surface->notdirty[fy][fx] = GPU_textgetcolor(surface,fx,fy,1); //Back!
			return; //Done!
		}
		++c;++curbacklist; //Next backlist item!
		if (c==8) break; //Stop searching!
	}

	surface->notdirty[fy][fx] = TRANSPARENTPIXEL;
}

static void GPU_textput_pixel(GPU_SDL_Surface *dest, PSP_TEXTSURFACE *surface,int fx, int fy) //Get the pixel font, back or show through. Automatically plotted if set.
{
	if (surface->dirty[fy][fx]) updateDirty(surface,fx,fy); //Update dirty if needed!
	uint_32 color = surface->notdirty[fy][fx];
	if (color)
	{
		put_pixel(dest,fx,fy,color); //Plot the pixel!
	}
	//We're transparent, do don't plot!
}

BACKLISTITEM defaultbacklist[8] = {{1,1},{1,0},{0,1},{1,-1},{-1,1},{0,-1},{-1,0},{-1,-1}}; //Default back list!

PSP_TEXTSURFACE *alloc_GPUtext()
{
	PSP_TEXTSURFACE *surface = (PSP_TEXTSURFACE *)zalloc(sizeof(PSP_TEXTSURFACE),"PSP_TextSurface"); //Create an empty initialised surface!
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

void free_GPUtext(PSP_TEXTSURFACE **surface)
{
	if (!surface) return; //Still allocated or not!
	if (!*surface) return; //Still allocated or not!
	freez((void **)surface,sizeof(PSP_TEXTSURFACE),"PSP_TextSurface"); //Release the memory, if possible!
	if (*surface) //Still allocated?
	{
		dolog("zalloc","GPU_TextSurface still allocated?");
	}
}

uint_64 GPU_textrenderer(PSP_TEXTSURFACE *surface) //Run the text rendering on rendersurface!
{
	if (!surface) return 0; //Abort without surface!
	TicksHolder ms_render_lastcheck; //For counting ms to render (GPU_framerate)!
	initTicksHolder(&ms_render_lastcheck); //Init for counting time of rendering on the device directly from VGA data!
	getmspassed(&ms_render_lastcheck); //Get first value!
	int y=0;
	for (;;) //Process all rows!
	{
		int x=0; //Reset x!
		for (;;) //Process all columns!
		{
			GPU_textput_pixel(rendersurface,surface,x,y); //Plot a pixel?
			if (++x==GPU_TEXTPIXELSX) break; //Stop searching now!
		}
		if (++y==GPU_TEXTPIXELSY) break; //Stop searching now!
	}
	surface->flags &= ~TEXTSURFACE_FLAG_DIRTY; //Clear dirty flag!
	return getmspassed(&ms_render_lastcheck); //Give processing time!
}

int GPU_textgetxy(PSP_TEXTSURFACE *surface,int x, int y, byte *character, uint_32 *font, uint_32 *border) //Read a character+attribute!
{
	if (!surface) return 0; //Not allocated!
	if (y>=GPU_ROWS) return 0; //Out of bounds?
	if (x>=GPU_COLUMNS) return 0; //Out of bounds?
	*character = surface->text[y][x];
	*font = surface->font[y][x];
	*border = surface->border[y][x];
	return 1; //OK!
}

static void GPU_markdirty(PSP_TEXTSURFACE *surface, int x, int y) //Mark a character as dirty (GPU_text only, to prevent multirendering!)
{
	int rx;
	int ry;

	for (rx=0;rx<10;rx++)
	{
		for (ry=0;ry<10;ry++)
		{
			surface->dirty[y*8+ry][x*8+rx] = 1; //Set dirty!
		}
	}
	surface->flags |= TEXTSURFACE_FLAG_DIRTY; //Set dirty!
}

int GPU_textsetxy(PSP_TEXTSURFACE *surface,int x, int y, byte character, uint_32 font, uint_32 border) //Write a character+attribute!
{
	if (!surface) return 0; //Not allocated!
	if (y>=GPU_ROWS) return 0; //Out of bounds?
	if (x>=GPU_COLUMNS) return 0; //Out of bounds?
	//dolog("GPU","GPU_textsetxy(x,y)",x,y);
	byte oldtext = surface->text[y][x];
	uint_32 oldfont = surface->font[y][x];
	uint_32 oldborder = surface->font[y][x];
	surface->text[y][x] = character;
	surface->font[y][x] = font;
	surface->border[y][x] = border;
	if ((character!=oldtext) || (font!=oldfont) || (border!=oldborder)) GPU_markdirty(surface,x,y); //Mark us as dirty when needed!
	return 1; //OK!
}

void GPU_textclearrow(PSP_TEXTSURFACE *surface, int y)
{
	int x=0;
	for (;;)
	{
		GPU_textsetxy(surface,x,y,0,0,0); //Clear the row fully!
		if (++x>=GPU_COLUMNS) return; //Done!
	}
}

void GPU_textclearscreen(PSP_TEXTSURFACE *surface)
{
	int y=0;
	for (;;)
	{
		GPU_textclearrow(surface,y); //Clear all rows!
		if (++y>=GPU_ROWS) return; //Done!
	}
}

void GPU_textprintf(PSP_TEXTSURFACE *surface, uint_32 font, uint_32 border, char *text, ...)
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
		while (curx>=GPU_COLUMNS) //Overflow?
		{
			++cury; //Next row!
			curx -= GPU_COLUMNS; //Decrease columns for every row size!
		}
		if ((msg[i]=='\r' && !USESLASHN) || (msg[i]=='\n' && USESLASHN)) //LF? If use \n, \n uses linefeed too, else just newline.
		{
			curx = 0; //Move to the left!
		}
		else if (msg[i]=='\n') //CR?
		{
			++cury; //Next Y!
		}
		else
		{
			GPU_textsetxy(surface,curx,cury,(byte)msg[i],font,border); //Write the character to our screen!
			++curx; //Next character!
		}
	}
	surface->x = curx; //Update x!
	surface->y = cury; //Update y!
}

void GPU_textgotoxy(PSP_TEXTSURFACE *surface,int x, int y) //Goto coordinates!
{
	if (!surface) return; //Not allocated!
	int curx = x;
	int cury = y;
	while (curx>=GPU_COLUMNS) //Overflow?
	{
		++cury; //Next row!
		curx -= GPU_COLUMNS; //Decrease columns for every row size!
	}
	surface->x = curx; //Real x!
	surface->y = cury; //Real y!
}
