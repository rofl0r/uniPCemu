#include "headers/emu/gpu/gpu.h" //Basic GPU!
#include "headers/emu/gpu/gpu_text.h" //Our prototypes!
#include "headers/emu/gpu/gpu_sdl.h" //Our prototypes!
#include "headers/interrupts/textmodedata.h" //VGA for font!
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/support/log.h" //Logging support!
#include "headers/support/bmp.h" //Bitmap support!
#include "headers/support/highrestimer.h" //High resolution timer!

#define __HW_DISABLED 0

//\n is newline? Else \r\n is newline!
#define USESLASHN 1

//Clickable bit values (used internally)!
//Clickable character!
#define CLICKABLE_CLICKABLE 1
//Mouse has been pressed!
#define CLICKABLE_BUTTONDOWN 2
//Mouse has clicked!
#define CLICKABLE_CLICKED 4

extern GPU_SDL_Surface *rendersurface; //The PSP's surface to use when flipping!

word TEXT_xdelta = 0;
word TEXT_ydelta = 0; //Delta x,y!

BACKLISTITEM backlist[8] = { { 1,1 },{ 1,0 },{ 0,1 },{ 1,-1 },{ -1,1 },{ 0,-1 },{ -1,0 },{ -1,-1 } }; //Default back list!

OPTINLINE byte GPU_textcalcpixel(byte *x, byte *y, word *charx, word *chary, int rx, int ry)
{
	register word cx, cy;
	register byte ix, iy;
	//Check for overflowing character coordinates!
	if (rx<0) return 1; //Invalid x!
	if (rx>=GPU_TEXTPIXELSX) return 1; //Invalid x!
	//Check for overflowing character coordinates!
	if (ry<0) return 1; //Invalid y!
	if (ry>=GPU_TEXTPIXELSY) return 1; //Invalid y!

	cx = (word)rx;
	cx >>= 3; //Shift to the character we're from!
	*charx = cx; //Set!

	cy = (word)ry;
	cy >>= 3; //Shift to the character we're from!
	*chary = cy; //Set!

	//Now the pixel within!
	ix = (byte)rx;
	ix &= 7;
	*x = ix;

	iy = (byte)ry;
	iy &= 7;
	*y = iy;
	return 0; //Valid!
}

uint_32 lastcharinfo = 0; //attribute|character|0x80|row, bit8=Set?

OPTINLINE byte getcharxy_8(byte character, byte x, byte y) //Retrieve a characters x,y pixel on/off from the unmodified 8x8 table!
{
	register uint_32 location;

	x &= 7;
	y &= 7;
	location = 0x800 | (character << 3) | y; //The location to look up!

	if ((lastcharinfo & 0xFFF) != location) //Last row not yet loaded?
	{
		lastcharinfo = (int10_font_08[location&~0x800]<<12)|location; //Read the row from the character generator!
	}

	register byte bitpos=19; //Load initial value!
	bitpos -= x; //Substract to get the bit!
	return ((lastcharinfo>>bitpos)&1); //Give result!
}


OPTINLINE byte GPU_textget_pixel(GPU_TEXTSURFACE *surface, int x, int y) //Get direct pixel from handler (overflow handled)!
{
	byte x2, y2;
	word charx, chary;
	if (GPU_textcalcpixel(&x2, &y2, &charx, &chary,x,y)) return 0; //Calculate our info. Our of range = Background!
	return getcharxy_8(surface->text[chary][charx], x2, y2); //Give the pixel of the character!
}

OPTINLINE uint_32 GPU_textgetcolor(GPU_TEXTSURFACE *surface, int x, int y, int border) //border = either border(1) or font(0)
{
	if (((x<0) || (y<0) || ((y >> 3) >= GPU_TEXTSURFACE_HEIGHT) || ((x >> 3) >= GPU_TEXTSURFACE_WIDTH))) return TRANSPARENTPIXEL; //None when out of bounds!
	word charx=0, chary=0;
	byte x2, y2;
	GPU_textcalcpixel(&x2, &y2, &charx, &chary,x,y); //Calculate our info!
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
	register byte c = 0;
	for (;;)
	{
		if (GPU_textget_pixel(surface, fx + backlist[c].x, fy + backlist[c].y)) //Border?
		{
			surface->notdirty[fy][fx] = GPU_textgetcolor(surface,fx,fy,1); //Back of the current character!
			return; //Done: we've gotten a pixel!
		}
		if (++c==8) break; //Abort when finished!
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

GPU_TEXTSURFACE *alloc_GPUtext()
{
	GPU_TEXTSURFACE *surface = (GPU_TEXTSURFACE *)zalloc(sizeof(GPU_TEXTSURFACE),"GPU_TEXTSURFACE",NULL); //Create an empty initialised surface!
	if (!surface) //Failed to allocate?
	{
		return NULL; //Failed to allocate!
	}
	//We don't need a screen, because we plot straight to the destination surface (is way faster than blitting)!

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

byte GPU_startClickable(GPU_TEXTSURFACE *surface, word x, word y); //Internal: start clickable character prototype!
void GPU_stopClickableXY(GPU_TEXTSURFACE *surface, word x, word y); //Internal: stop clickable character prototype!

byte GPU_textsetxyclickable(GPU_TEXTSURFACE *surface, int x, int y, byte character, uint_32 font, uint_32 border) //Set x/y coordinates for clickable character! Result is bit value of SETXYCLICKED_*
{
	byte result;
	if (!memprotect(surface, sizeof(GPU_TEXTSURFACE), "GPU_TEXTSURFACE")) return 0; //Abort without surface!
	if (y >= GPU_TEXTSURFACE_HEIGHT) return 0; //Out of bounds?
	if (x >= GPU_TEXTSURFACE_WIDTH) return 0; //Out of bounds?
	byte oldtext = surface->text[y][x];
	uint_32 oldfont = surface->font[y][x];
	uint_32 oldborder = surface->font[y][x];
	surface->text[y][x] = character;
	surface->font[y][x] = font;
	surface->border[y][x] = border;
	result = GPU_startClickable(surface, x, y) ? (SETXYCLICKED_OK | SETXYCLICKED_CLICKED) : SETXYCLICKED_OK; //We're starting to be clickable if not yet clickable! Give 3 for clicked and 1 for normal success without click!
	uint_32 change;
	character ^= oldtext;
	font ^= oldfont;
	border ^= oldborder;
	change = character;
	change |= font;
	change |= border;
	if (change) surface->flags |= TEXTSURFACE_FLAG_DIRTY; //Mark us as dirty when needed!
	return result; //OK with error condition!
}

int GPU_textsetxy(GPU_TEXTSURFACE *surface,int x, int y, byte character, uint_32 font, uint_32 border) //Write a character+attribute!
{
	if (!memprotect(surface, sizeof(GPU_TEXTSURFACE), "GPU_TEXTSURFACE")) return 0; //Abort without surface!
	if (y>=GPU_TEXTSURFACE_HEIGHT) return 0; //Out of bounds?
	if (x>=GPU_TEXTSURFACE_WIDTH) return 0; //Out of bounds?
	byte oldtext = surface->text[y][x];
	uint_32 oldfont = surface->font[y][x];
	uint_32 oldborder = surface->border[y][x];
	surface->text[y][x] = character;
	surface->font[y][x] = font;
	surface->border[y][x] = border;
	GPU_stopClickableXY(surface,x,y); //We're stopping being clickable: we're a normal character from now on!
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

void GPU_textclearcurrentrownext(GPU_TEXTSURFACE *surface) //For clearing the rest of the current row!
{
	int x = surface->x; //Start at the current coordinates!
	for (;;)
	{
		GPU_textsetxy(surface, x, surface->y, 0, 0, 0); //Clear the row fully!
		if (++x >= GPU_TEXTSURFACE_WIDTH) return; //Done!
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

byte GPU_textprintfclickable(GPU_TEXTSURFACE *surface, uint_32 font, uint_32 border, char *text, ...)
{
	if (!memprotect(surface, sizeof(GPU_TEXTSURFACE), "GPU_TEXTSURFACE")) return 0; //Abort without surface!
	char msg[256];
	bzero(msg, sizeof(msg)); //Init!

	va_list args; //Going to contain the list!
	va_start(args, text); //Start list!
	vsprintf(msg, text, args); //Compile list!

	int curx = surface->x; //Init x!
	int cury = surface->y; //init y!
	int i;
	byte result = SETXYCLICKED_OK; //Default: we're OK!
	byte setstatus; //Status when setting!
	for (i = 0; i<(int)strlen(msg); i++) //Process text!
	{
		while (curx >= GPU_TEXTSURFACE_WIDTH) //Overflow?
		{
			++cury; //Next row!
			curx -= GPU_TEXTSURFACE_WIDTH; //Decrease columns for every row size!
		}
		if ((msg[i] == '\r' && !USESLASHN) || (msg[i] == '\n' && USESLASHN)) //LF? If use \n, \n uses linefeed too, else just newline.
		{
			curx = 0; //Move to the left!
		}
		if (msg[i] == '\n') //CR?
		{
			++cury; //Next Y!
		}
		else if (msg[i] != '\r') //Never display \r!
		{
			setstatus = GPU_textsetxyclickable(surface, curx, cury, (byte)msg[i], font, border); //Write the character to our screen!
			if (!(setstatus&SETXYCLICKED_OK)) //Invalid character location or unknown status value?
			{
				result &= ~SETXYCLICKED_OK; //Error out: we have one or more invalid writes!
			}

			if (setstatus&SETXYCLICKED_CLICKED) //Are we clicked?
			{
				result |= SETXYCLICKED_CLICKED; //We're clicked!
			}
			++curx; //Next character!
		}
	}
	surface->x = curx; //Update x!
	surface->y = cury; //Update y!
	return result; //Give the result!
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
	if (!memprotect(surface, sizeof(*surface), "GPU_TEXTSURFACE")) return; //Invalid surface!
	if (!surface->lock) return; //no lock?
	WaitSem(surface->lock) //Wait for us to be available and locked!
}

void GPU_text_releasesurface(GPU_TEXTSURFACE *surface) //Unlock a surface when done with it!
{
	if (!memprotect(surface, sizeof(*surface), "GPU_TEXTSURFACE")) return; //Invalid surface!
	if (!surface->lock) return; //no lock?
	PostSem(surface->lock) //Release our lock: we're done!
}

void GPU_textbuttondown(GPU_TEXTSURFACE *surface, word x, word y) //We've been clicked at these coordinates!
{
	if (!memprotect(surface, sizeof(*surface), "GPU_TEXTSURFACE")) return; //Invalid surface!
	word x1, y1;
	x1 = 0;
	y1 = 0;
	if (surface->xdelta) x1 += TEXT_xdelta; //Apply delta position to the output pixel!
	if (surface->ydelta) y1 += TEXT_ydelta; //Apply delta position to the output pixel!

	//Now x1,y1 is the start of the surface!
	if (x >= x1) //Within x range?
	{
		if (y >= y1) //Within y range?
		{
			x -= x1; //X coordinate within the surface!
			y -= y1;  //Y coordinate within the surface!
			if (x < GPU_TEXTPIXELSX) //Within horizontal range?
			{
				if (y < GPU_TEXTPIXELSY) //Within vertical range?
				{
					x >>= 3; //X character within the surface!
					y >>= 3; //Y character within the surface!
					if (surface->clickable[y][x] & CLICKABLE_CLICKABLE) //Is this a clickable character?
					{
						surface->clickable[y][x] |= CLICKABLE_BUTTONDOWN; //Set button down flag!
					}
				}
			}
		}
	}
}

void GPU_textbuttonup(GPU_TEXTSURFACE *surface, word x, word y) //We've been released at these coordinates!
{
	if (!memprotect(surface, sizeof(*surface), "GPU_TEXTSURFACE")) return; //Invalid surface!

	word sx, sy;
	for (sx = 0;sx < GPU_TEXTSURFACE_WIDTH;)
	{
		for (sy = 0;sy < GPU_TEXTSURFACE_HEIGHT;)
		{
			byte clickable;
			clickable = surface->clickable[sy][sx]; //Load clickable info on the current character!
			if (clickable & CLICKABLE_CLICKABLE) //Clickable character?
			{
				if (clickable&CLICKABLE_BUTTONDOWN) //We're pressed?
				{
					clickable &= ~CLICKABLE_BUTTONDOWN; //Release hold!
					clickable |= CLICKABLE_CLICKED; //We've been clicked!
					surface->clickable[sy][sx] = clickable; //Update clicked information!
				}
			}
			++sy;
		}
		++sx;
	}
}

byte GPU_startClickable(GPU_TEXTSURFACE *surface, word x, word y) //Internal: start clickable character!
{
	if (!memprotect(surface, sizeof(*surface), "GPU_TEXTSURFACE")) return 0; //Invalid surface!
	byte result = 0;
	if (!(surface->clickable[y][x] & CLICKABLE_CLICKABLE)) //We're not clickable yet?
	{
		surface->clickable[y][x] = CLICKABLE_CLICKABLE; //Enable clickable and start fresh!
	}
	else
	{
		if (surface->clickable[y][x] & CLICKABLE_CLICKED) //Are we clicked?
		{
			surface->clickable[y][x] &= ~CLICKABLE_CLICKED; //We're processing the click now!
			result = 1; //We're clicked!
		}
	}
	return result; //Give if we're clicked or not!
}

byte GPU_isclicked(GPU_TEXTSURFACE *surface, word x, word y) //Are we clicked?
{
	byte result;
	if (!memprotect(surface, sizeof(*surface), "GPU_TEXTSURFACE")) return 0; //Invalid surface!
	result = (surface->clickable[y][x]&(CLICKABLE_CLICKABLE|CLICKABLE_CLICKED))== (CLICKABLE_CLICKABLE | CLICKABLE_CLICKED); //Give if we're clickable and clicked!
	return result; //Give the result if we're clicked!
}

void GPU_stopClickableXY(GPU_TEXTSURFACE *surface, word x, word y)
{
	if (!memprotect(surface, sizeof(*surface), "GPU_TEXTSURFACE")) return; //Invalid surface!
	surface->clickable[y][x] = 0; //Destroy any click information! We're a normal character again!
}