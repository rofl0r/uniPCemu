#include "headers/types.h"
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/support/log.h" //Logging support!

//Are we disabled?
#define __HW_DISABLED 0

byte BMPType[2] = {0x42,0x4D}; //The type, identifying the BMP file 'BM'!

typedef struct
{
word OurSize; //Should be sizeof(this)
uint_32 Width; //Width of the image!
uint_32 Height; //Height of the image!
word Planes; //Ammount of planes = 1
word BitDepth; //Bit count (1,4,8,16,24)
uint_32 Compression; //Type of compression, 0 for none!
uint_32 CompressionSize; //(compressed) Size of Image. =0 When biCompression=0
uint_32 XPixelsPerMeter; //Horizontal resolution: pixels/meter
uint_32 YPixelsPerMeter; //Vertical resolution: pixels/meter
uint_32 ActuallyUsedColors; //Number of actually used colors
uint_32 Importantcolors; //Number of important colors (0=All)
} TBMPInfoHeader;

typedef struct
{
uint_32 Filesize; //File size in bytes!
word Reserved1_0; //0
word Reserved2_0; //0
uint_32 PixelOffset; //Start offset of pixels!
} TBMPHeader;

typedef struct
{
byte B; //Blue
byte G; //Green
byte R; //Red
} TRGB;

void swap(byte *a, byte *b)
{
byte buf;
buf = *a; //Store old a!
*a = *b; //Store b in a!
*b = buf; //Store old a in b!
}

uint_32 convertEndianness32(uint_32 val)
{
union
{
uint_32 result;
byte data[4];
} holder;
holder.data[0] = (byte)val; //Low16Low
holder.data[1] = (byte)(val>>8); //Low16High
holder.data[2] = (byte)(val>>16); //High16Low
holder.data[3] = (byte)(val>>24); //High16High
return holder.result; //Give the result!
}

word convertEndianness16(word val)
{
union
{
word result;
byte data[2];
} holder;
holder.data[0] = (byte)val; //Low!
holder.data[1] = (byte)(val>>8); //High!
return holder.result; //Give the result!
}

static OPTINLINE void getBMP(TRGB *pixel,int x, int y, uint_32 *image, int w, int h, int virtualwidth, byte doublexres, byte doubleyres, int originalw, int originalh)
{
	uint_32 index;
	float a,r,g,b;
	uint_32 thepixel;
	x >>= doublexres; //Apply double!
	y = (h-y-1); //The pixel Y is reversed in the BMP, so reverse it!
	y >>= doubleyres; //Apply double!
	w = originalw; //Apply double!
	h = originalh; //Apply double!
	index = (y*virtualwidth)+x; //Our index of our pixel!
	a = (float)(GETA(image[(y*virtualwidth)+x]))/255.0f; //A gradient!
	thepixel = image[(y*virtualwidth)+x];
	r = (float)GETR(thepixel); //Get R!
	g = (float)GETG(thepixel); //Get G!
	b = (float)GETB(thepixel); //Get B!
	r *= a; //Apply alpha!
	g *= a; //Apply alpha!
	b *= a; //Apply alpha!
	pixel->R = (byte)r;
	pixel->G = (byte)g;
	pixel->B = (byte)b;
}

byte writeBMP(char *thefilename, uint_32 *image, int w, int h, byte doublexres, byte doubleyres, int virtualwidth)
{
	int originalw, originalh;
	char filename[256];
	byte rowpadding;
	FILE *f;
	uint_32 imgsize = 0;
	TBMPHeader BMPHeader; //BMP itself!
	TBMPInfoHeader BMPInfo; //Info about the BMP!
	uint_32 dataStartOffset;
	static byte bmppad[3] = {0,0,0}; //For padding!
	TRGB pixel; //A pixel for writing to the file!
	int y = 0;
	int x = 0;

	if (__HW_DISABLED) return 0; //Abort!
	if (!w || !h)
	{
		//dolog("BMP","Error writing BMP: %s@%ix%i pixels",thefilename,w,h); //Log our error!
		return 0; //Can't write: empty height/width!
	}

	originalw = w; //Original width!
	originalh = h; //Original height!
	w <<= doublexres; //Apply double!
	h <<= doubleyres; //Apply double!	

	bzero(filename,sizeof(filename));
	strcpy(filename,thefilename); //Copy!
	strcat(filename,".bmp"); //Add extension!

	imgsize = sizeof(TRGB)*w*h; //The size we allocated, to be remembered!

	rowpadding = (4-(w*sizeof(TRGB))%4)%4; //Padding for 1 row!

	//Fill the header!
	memset(&BMPHeader,0,sizeof(BMPHeader)); //Clear the header!
	memset(&BMPInfo,0,sizeof(BMPInfo)); //Clear the info header!

	dataStartOffset = sizeof(BMPType)+sizeof(BMPHeader)+sizeof(BMPInfo); //Start offset of the data!

	//Header!
	BMPHeader.Filesize = convertEndianness32(dataStartOffset+imgsize); //Full filesize in bytes!
	BMPHeader.PixelOffset = convertEndianness32(sizeof(BMPType)+sizeof(BMPHeader)+sizeof(BMPInfo)); //File offset to raster data!
	//Info!
	BMPInfo.OurSize = convertEndianness32(sizeof(BMPInfo)); //Size of InfoHeader=40
	BMPInfo.Width = convertEndianness32(w); //Width!
	BMPInfo.Height = convertEndianness32(h); //Height!
	BMPInfo.Planes = convertEndianness16(1); //Number of Planes(=1)
	BMPInfo.BitDepth = convertEndianness16(24); //Bits per Pixel.
	//Rest is cleared&not used!

	//Now write the file!

	f = fopen(filename,"wb");
	fwrite(&BMPType,1,sizeof(BMPType),f); //Write the type, unpadded!
	fwrite(&BMPHeader,1,sizeof(BMPHeader),f); //Write the header!
	fwrite(&BMPInfo,1,sizeof(BMPInfo),f); //Write the header!

	//dolog("BMP","Header written, writing Bitmap data...");

	for (;y<h;y++) //Process all rows!
	{
		for (x=0;x<w;x++) //Process all columns!
		{
			getBMP(&pixel,x,y,image,w,h,virtualwidth,doublexres,doubleyres,originalw,originalh); //Get the pixel to be written!
			fwrite(&pixel,1,sizeof(pixel),f); //Write the pixel to the file!
		}
		fwrite(&bmppad,1,rowpadding,f); //Write the padding!
	}

	fclose(f);
	return 1; //Success!
}