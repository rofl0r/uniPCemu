#include "headers/header_dosbox.h" //Our typedefs!

VideoModeBlock ModeList_VGA[0x15] = {  //VGA Modelist!
	/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
	{ 0x000, M_TEXT, 360, 400, 40, 25, 9, 16, 8, 0xB8000, 0x0800, 50, 449, 40, 400, _EGA_HALF_CLOCK },
	{ 0x001, M_TEXT, 360, 400, 40, 25, 9, 16, 8, 0xB8000, 0x0800, 50, 449, 40, 400, _EGA_HALF_CLOCK },
	{ 0x002, M_TEXT, 720, 400, 80, 25, 9, 16, 8, 0xB8000, 0x1000, 100, 449, 80, 400, 0 },
	{ 0x003, M_TEXT, 720, 400, 80, 25, 9, 16, 8, 0xB8000, 0x1000, 100, 449, 80, 400, 0 },
	{ 0x004, M_CGA4, 320, 200, 40, 25, 8, 8, 1, 0xB8000, 0x4000, 50, 449, 40, 400, _EGA_HALF_CLOCK | _DOUBLESCAN | _REPEAT1 },
	{ 0x005, M_CGA4, 320, 200, 40, 25, 8, 8, 1, 0xB8000, 0x4000, 50, 449, 40, 400, _EGA_HALF_CLOCK | _DOUBLESCAN | _REPEAT1 },
	{ 0x006, M_CGA2, 640, 200, 80, 25, 8, 8, 1, 0xB8000, 0x4000, 100, 449, 80, 400, _DOUBLESCAN | _REPEAT1 },
	{ 0x007, M_TEXT, 720, 400, 80, 25, 9, 16, 8, 0xB0000, 0x1000, 100, 449, 80, 400, 0 },

	{ 0x00D, M_EGA, 320, 200, 40, 25, 8, 8, 8, 0xA0000, 0x2000, 50, 449, 40, 400, _EGA_HALF_CLOCK | _DOUBLESCAN },
	{ 0x00E, M_EGA, 640, 200, 80, 25, 8, 8, 4, 0xA0000, 0x4000, 100, 449, 80, 400, _DOUBLESCAN },
	{ 0x00F, M_EGA, 640, 350, 80, 25, 8, 14, 2, 0xA0000, 0x8000, 100, 449, 80, 350, 0 },/*was EGA_2*/
	{ 0x010, M_EGA, 640, 350, 80, 25, 8, 14, 2, 0xA0000, 0x8000, 100, 449, 80, 350, 0 },
	{ 0x011, M_EGA, 640, 480, 80, 30, 8, 16, 1, 0xA0000, 0xA000, 100, 525, 80, 480, 0 },/*was EGA_2 */
	{ 0x012, M_EGA, 640, 480, 80, 30, 8, 16, 1, 0xA0000, 0xA000, 100, 525, 80, 480, 0 },
	{ 0x013, M_VGA, 320, 200, 40, 25, 8, 8, 1, 0xA0000, 0x2000, 100, 449, 80, 400, _REPEAT1 },
	{ 0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 }
}; //VGA Modelist!

VideoModeBlock ModeList_VGA_Tseng[35]={
/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
{ 0x000  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x001  ,M_TEXT   ,360 ,400 ,40 ,25 ,9 ,16 ,8 ,0xB8000 ,0x0800 ,50  ,449 ,40 ,400 ,_EGA_HALF_CLOCK	},
{ 0x002  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x003  ,M_TEXT   ,720 ,400 ,80 ,25 ,9 ,16 ,8 ,0xB8000 ,0x1000 ,100 ,449 ,80 ,400 ,0	},
{ 0x004, M_CGA4, 320, 200, 40, 25, 8, 8, 1, 0xB8000, 0x4000, 50, 449, 40, 400, _EGA_HALF_CLOCK | _DOUBLESCAN | _REPEAT1 },
{ 0x005, M_CGA4, 320, 200, 40, 25, 8, 8, 1, 0xB8000, 0x4000, 50, 449, 40, 400, _EGA_HALF_CLOCK | _DOUBLESCAN | _REPEAT1 },
{ 0x006, M_CGA2, 640, 200, 80, 25, 8, 8, 1, 0xB8000, 0x4000, 100, 449, 80, 400, _DOUBLESCAN | _REPEAT1 },
{ 0x007, M_TEXT, 720, 400, 80, 25, 9, 16, 8, 0xB0000, 0x1000, 100, 449, 80, 400, 0 },

{ 0x00D, M_EGA, 320, 200, 40, 25, 8, 8, 8, 0xA0000, 0x2000, 50, 449, 40, 400, _EGA_HALF_CLOCK | _DOUBLESCAN },
{ 0x00E, M_EGA, 640, 200, 80, 25, 8, 8, 4, 0xA0000, 0x4000, 100, 449, 80, 400, _DOUBLESCAN },
{ 0x00F, M_EGA, 640, 350, 80, 25, 8, 14, 2, 0xA0000, 0x8000, 100, 449, 80, 350, 0 },/*was EGA_2*/
{ 0x010, M_EGA, 640, 350, 80, 25, 8, 14, 2, 0xA0000, 0x8000, 100, 449, 80, 350, 0 },
{ 0x011, M_EGA, 640, 480, 80, 30, 8, 16, 1, 0xA0000, 0xA000, 100, 525, 80, 480, 0 },/*was EGA_2 */
{ 0x012, M_EGA, 640, 480, 80, 30, 8, 16, 1, 0xA0000, 0xA000, 100, 525, 80, 480, 0 },
{ 0x013, M_VGA, 320, 200, 40, 25, 8, 8, 1, 0xA0000, 0x2000, 100, 449, 80, 400, _REPEAT1 },

{ 0x018  ,M_TEXT   ,1056 ,688, 132,44, 8, 8, 1 ,0xB0000 ,0x4000, 192, 800, 132, 704, 0 },
{ 0x019  ,M_TEXT   ,1056 ,400, 132,25, 8, 16,1 ,0xB0000 ,0x2000, 192, 449, 132, 400, 0 },
{ 0x01A  ,M_TEXT   ,1056 ,400, 132,28, 8, 16,1 ,0xB0000 ,0x2000, 192, 449, 132, 448, 0 },
{ 0x022  ,M_TEXT   ,1056 ,688, 132,44, 8, 8, 1 ,0xB8000 ,0x4000, 192, 800, 132, 704, 0 },
{ 0x023  ,M_TEXT   ,1056 ,400, 132,25, 8, 16,1 ,0xB8000 ,0x2000, 192, 449, 132, 400, 0 },
{ 0x024  ,M_TEXT   ,1056 ,400, 132,28, 8, 16,1 ,0xB8000 ,0x2000, 192, 449, 132, 448, 0 },
{ 0x025  ,M_LIN4   ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0xA000 ,100 ,525 ,80 ,480 , 0 },
{ 0x029  ,M_LIN4   ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0xA000, 128 ,663 ,100,600 , 0 },
{ 0x02D  ,M_LIN8   ,640 ,350 ,80 ,21 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,350 , 0 },
{ 0x02E  ,M_LIN8   ,640 ,480 ,80 ,30 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,525 ,80 ,480 , 0 },
{ 0x02F  ,M_LIN8   ,640 ,400 ,80 ,25 ,8 ,16 ,1 ,0xA0000 ,0x10000,100 ,449 ,80 ,400 , 0 },/* ET4000 only */
{ 0x030  ,M_LIN8   ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0x10000,128 ,663 ,100,600 , 0 },
{ 0x036  ,M_LIN4   ,960 , 720,120,45 ,8 ,16 ,1 ,0xA0000 ,0xA000, 120 ,800 ,120,720 , 0 },/* STB only */
{ 0x037  ,M_LIN4   ,1024, 768,128,48 ,8 ,16 ,1 ,0xA0000 ,0xA000, 128 ,800 ,128,768 , 0 },
{ 0x038  ,M_LIN8   ,1024 ,768,128,48 ,8 ,16 ,1 ,0xA0000 ,0x10000,128 ,800 ,128,768 , 0 },/* ET4000 only */
{ 0x03D  ,M_LIN4   ,1280,1024,160,64 ,8 ,16 ,1 ,0xA0000 ,0xA000, 160 ,1152,160,1024, 0 },/* newer ET4000 */
{ 0x03E  ,M_LIN4   ,1280, 960,160,60 ,8 ,16 ,1 ,0xA0000 ,0xA000, 160 ,1024,160,960 , 0 },/* Definicon only */ 
{ 0x06A  ,M_LIN4   ,800 ,600 ,100,37 ,8 ,16 ,1 ,0xA0000 ,0xA000, 128 ,663 ,100,600 , 0 },/* newer ET4000 */

{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	}
};

byte text_palette[64][3]=
{
	{0x00,0x00,0x00},{0x00,0x00,0x2a},{0x00,0x2a,0x00},{0x00,0x2a,0x2a},{0x2a,0x00,0x00},{0x2a,0x00,0x2a},{0x2a,0x2a,0x00},{0x2a,0x2a,0x2a},
	{0x00,0x00,0x15},{0x00,0x00,0x3f},{0x00,0x2a,0x15},{0x00,0x2a,0x3f},{0x2a,0x00,0x15},{0x2a,0x00,0x3f},{0x2a,0x2a,0x15},{0x2a,0x2a,0x3f},
	{0x00,0x15,0x00},{0x00,0x15,0x2a},{0x00,0x3f,0x00},{0x00,0x3f,0x2a},{0x2a,0x15,0x00},{0x2a,0x15,0x2a},{0x2a,0x3f,0x00},{0x2a,0x3f,0x2a},
	{0x00,0x15,0x15},{0x00,0x15,0x3f},{0x00,0x3f,0x15},{0x00,0x3f,0x3f},{0x2a,0x15,0x15},{0x2a,0x15,0x3f},{0x2a,0x3f,0x15},{0x2a,0x3f,0x3f},
	{0x15,0x00,0x00},{0x15,0x00,0x2a},{0x15,0x2a,0x00},{0x15,0x2a,0x2a},{0x3f,0x00,0x00},{0x3f,0x00,0x2a},{0x3f,0x2a,0x00},{0x3f,0x2a,0x2a},
	{0x15,0x00,0x15},{0x15,0x00,0x3f},{0x15,0x2a,0x15},{0x15,0x2a,0x3f},{0x3f,0x00,0x15},{0x3f,0x00,0x3f},{0x3f,0x2a,0x15},{0x3f,0x2a,0x3f},
	{0x15,0x15,0x00},{0x15,0x15,0x2a},{0x15,0x3f,0x00},{0x15,0x3f,0x2a},{0x3f,0x15,0x00},{0x3f,0x15,0x2a},{0x3f,0x3f,0x00},{0x3f,0x3f,0x2a},
	{0x15,0x15,0x15},{0x15,0x15,0x3f},{0x15,0x3f,0x15},{0x15,0x3f,0x3f},{0x3f,0x15,0x15},{0x3f,0x15,0x3f},{0x3f,0x3f,0x15},{0x3f,0x3f,0x3f}
};

byte mtext_palette[64][3]=
{
	{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
	{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
	{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
	{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},
	{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
	{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
	{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
	{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f}
};

byte mtext_s3_palette[64][3]=
{
	{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
	{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
	{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
	{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},
	{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},{0x00,0x00,0x00},
	{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
	{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},{0x2a,0x2a,0x2a},
	{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f},{0x3f,0x3f,0x3f}
};

byte ega_palette[64][3]=
{
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f}
};

byte cga_palette[16][3]=
{
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
};

byte cga_palette_2[64][3]=
{
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x00,0x00,0x00}, {0x00,0x00,0x2a}, {0x00,0x2a,0x00}, {0x00,0x2a,0x2a}, {0x2a,0x00,0x00}, {0x2a,0x00,0x2a}, {0x2a,0x15,0x00}, {0x2a,0x2a,0x2a},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
	{0x15,0x15,0x15}, {0x15,0x15,0x3f}, {0x15,0x3f,0x15}, {0x15,0x3f,0x3f}, {0x3f,0x15,0x15}, {0x3f,0x15,0x3f}, {0x3f,0x3f,0x15}, {0x3f,0x3f,0x3f},
};

byte vga_palette[256][3]=
{
	{0x00,0x00,0x00},{0x00,0x00,0x2a},{0x00,0x2a,0x00},{0x00,0x2a,0x2a},{0x2a,0x00,0x00},{0x2a,0x00,0x2a},{0x2a,0x15,0x00},{0x2a,0x2a,0x2a},
	{0x15,0x15,0x15},{0x15,0x15,0x3f},{0x15,0x3f,0x15},{0x15,0x3f,0x3f},{0x3f,0x15,0x15},{0x3f,0x15,0x3f},{0x3f,0x3f,0x15},{0x3f,0x3f,0x3f},
	{0x00,0x00,0x00},{0x05,0x05,0x05},{0x08,0x08,0x08},{0x0b,0x0b,0x0b},{0x0e,0x0e,0x0e},{0x11,0x11,0x11},{0x14,0x14,0x14},{0x18,0x18,0x18},
	{0x1c,0x1c,0x1c},{0x20,0x20,0x20},{0x24,0x24,0x24},{0x28,0x28,0x28},{0x2d,0x2d,0x2d},{0x32,0x32,0x32},{0x38,0x38,0x38},{0x3f,0x3f,0x3f},
	{0x00,0x00,0x3f},{0x10,0x00,0x3f},{0x1f,0x00,0x3f},{0x2f,0x00,0x3f},{0x3f,0x00,0x3f},{0x3f,0x00,0x2f},{0x3f,0x00,0x1f},{0x3f,0x00,0x10},
	{0x3f,0x00,0x00},{0x3f,0x10,0x00},{0x3f,0x1f,0x00},{0x3f,0x2f,0x00},{0x3f,0x3f,0x00},{0x2f,0x3f,0x00},{0x1f,0x3f,0x00},{0x10,0x3f,0x00},
	{0x00,0x3f,0x00},{0x00,0x3f,0x10},{0x00,0x3f,0x1f},{0x00,0x3f,0x2f},{0x00,0x3f,0x3f},{0x00,0x2f,0x3f},{0x00,0x1f,0x3f},{0x00,0x10,0x3f},
	{0x1f,0x1f,0x3f},{0x27,0x1f,0x3f},{0x2f,0x1f,0x3f},{0x37,0x1f,0x3f},{0x3f,0x1f,0x3f},{0x3f,0x1f,0x37},{0x3f,0x1f,0x2f},{0x3f,0x1f,0x27},

	{0x3f,0x1f,0x1f},{0x3f,0x27,0x1f},{0x3f,0x2f,0x1f},{0x3f,0x37,0x1f},{0x3f,0x3f,0x1f},{0x37,0x3f,0x1f},{0x2f,0x3f,0x1f},{0x27,0x3f,0x1f},
	{0x1f,0x3f,0x1f},{0x1f,0x3f,0x27},{0x1f,0x3f,0x2f},{0x1f,0x3f,0x37},{0x1f,0x3f,0x3f},{0x1f,0x37,0x3f},{0x1f,0x2f,0x3f},{0x1f,0x27,0x3f},
	{0x2d,0x2d,0x3f},{0x31,0x2d,0x3f},{0x36,0x2d,0x3f},{0x3a,0x2d,0x3f},{0x3f,0x2d,0x3f},{0x3f,0x2d,0x3a},{0x3f,0x2d,0x36},{0x3f,0x2d,0x31},
	{0x3f,0x2d,0x2d},{0x3f,0x31,0x2d},{0x3f,0x36,0x2d},{0x3f,0x3a,0x2d},{0x3f,0x3f,0x2d},{0x3a,0x3f,0x2d},{0x36,0x3f,0x2d},{0x31,0x3f,0x2d},
	{0x2d,0x3f,0x2d},{0x2d,0x3f,0x31},{0x2d,0x3f,0x36},{0x2d,0x3f,0x3a},{0x2d,0x3f,0x3f},{0x2d,0x3a,0x3f},{0x2d,0x36,0x3f},{0x2d,0x31,0x3f},
	{0x00,0x00,0x1c},{0x07,0x00,0x1c},{0x0e,0x00,0x1c},{0x15,0x00,0x1c},{0x1c,0x00,0x1c},{0x1c,0x00,0x15},{0x1c,0x00,0x0e},{0x1c,0x00,0x07},
	{0x1c,0x00,0x00},{0x1c,0x07,0x00},{0x1c,0x0e,0x00},{0x1c,0x15,0x00},{0x1c,0x1c,0x00},{0x15,0x1c,0x00},{0x0e,0x1c,0x00},{0x07,0x1c,0x00},
	{0x00,0x1c,0x00},{0x00,0x1c,0x07},{0x00,0x1c,0x0e},{0x00,0x1c,0x15},{0x00,0x1c,0x1c},{0x00,0x15,0x1c},{0x00,0x0e,0x1c},{0x00,0x07,0x1c},

	{0x0e,0x0e,0x1c},{0x11,0x0e,0x1c},{0x15,0x0e,0x1c},{0x18,0x0e,0x1c},{0x1c,0x0e,0x1c},{0x1c,0x0e,0x18},{0x1c,0x0e,0x15},{0x1c,0x0e,0x11},
	{0x1c,0x0e,0x0e},{0x1c,0x11,0x0e},{0x1c,0x15,0x0e},{0x1c,0x18,0x0e},{0x1c,0x1c,0x0e},{0x18,0x1c,0x0e},{0x15,0x1c,0x0e},{0x11,0x1c,0x0e},
	{0x0e,0x1c,0x0e},{0x0e,0x1c,0x11},{0x0e,0x1c,0x15},{0x0e,0x1c,0x18},{0x0e,0x1c,0x1c},{0x0e,0x18,0x1c},{0x0e,0x15,0x1c},{0x0e,0x11,0x1c},
	{0x14,0x14,0x1c},{0x16,0x14,0x1c},{0x18,0x14,0x1c},{0x1a,0x14,0x1c},{0x1c,0x14,0x1c},{0x1c,0x14,0x1a},{0x1c,0x14,0x18},{0x1c,0x14,0x16},
	{0x1c,0x14,0x14},{0x1c,0x16,0x14},{0x1c,0x18,0x14},{0x1c,0x1a,0x14},{0x1c,0x1c,0x14},{0x1a,0x1c,0x14},{0x18,0x1c,0x14},{0x16,0x1c,0x14},
	{0x14,0x1c,0x14},{0x14,0x1c,0x16},{0x14,0x1c,0x18},{0x14,0x1c,0x1a},{0x14,0x1c,0x1c},{0x14,0x1a,0x1c},{0x14,0x18,0x1c},{0x14,0x16,0x1c},
	{0x00,0x00,0x10},{0x04,0x00,0x10},{0x08,0x00,0x10},{0x0c,0x00,0x10},{0x10,0x00,0x10},{0x10,0x00,0x0c},{0x10,0x00,0x08},{0x10,0x00,0x04},
	{0x10,0x00,0x00},{0x10,0x04,0x00},{0x10,0x08,0x00},{0x10,0x0c,0x00},{0x10,0x10,0x00},{0x0c,0x10,0x00},{0x08,0x10,0x00},{0x04,0x10,0x00},

	{0x00,0x10,0x00},{0x00,0x10,0x04},{0x00,0x10,0x08},{0x00,0x10,0x0c},{0x00,0x10,0x10},{0x00,0x0c,0x10},{0x00,0x08,0x10},{0x00,0x04,0x10},
	{0x08,0x08,0x10},{0x0a,0x08,0x10},{0x0c,0x08,0x10},{0x0e,0x08,0x10},{0x10,0x08,0x10},{0x10,0x08,0x0e},{0x10,0x08,0x0c},{0x10,0x08,0x0a},
	{0x10,0x08,0x08},{0x10,0x0a,0x08},{0x10,0x0c,0x08},{0x10,0x0e,0x08},{0x10,0x10,0x08},{0x0e,0x10,0x08},{0x0c,0x10,0x08},{0x0a,0x10,0x08},
	{0x08,0x10,0x08},{0x08,0x10,0x0a},{0x08,0x10,0x0c},{0x08,0x10,0x0e},{0x08,0x10,0x10},{0x08,0x0e,0x10},{0x08,0x0c,0x10},{0x08,0x0a,0x10},
	{0x0b,0x0b,0x10},{0x0c,0x0b,0x10},{0x0d,0x0b,0x10},{0x0f,0x0b,0x10},{0x10,0x0b,0x10},{0x10,0x0b,0x0f},{0x10,0x0b,0x0d},{0x10,0x0b,0x0c},
	{0x10,0x0b,0x0b},{0x10,0x0c,0x0b},{0x10,0x0d,0x0b},{0x10,0x0f,0x0b},{0x10,0x10,0x0b},{0x0f,0x10,0x0b},{0x0d,0x10,0x0b},{0x0c,0x10,0x0b},
	{0x0b,0x10,0x0b},{0x0b,0x10,0x0c},{0x0b,0x10,0x0d},{0x0b,0x10,0x0f},{0x0b,0x10,0x10},{0x0b,0x0f,0x10},{0x0b,0x0d,0x10},{0x0b,0x0c,0x10}
};


//Extra stuff:

VideoModeBlock ModeList_VGA_Text_200lines[5]=
{
	/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
	{ 0x000, M_TEXT, 320, 200, 40, 25, 8, 8, 8, 0xB8000, 0x0800, 50, 449, 40, 400, _EGA_HALF_CLOCK | _DOUBLESCAN },
	{ 0x001, M_TEXT, 320, 200, 40, 25, 8, 8, 8, 0xB8000, 0x0800, 50, 449, 40, 400, _EGA_HALF_CLOCK | _DOUBLESCAN },
	{ 0x002, M_TEXT, 640, 200, 80, 25, 8, 8, 8, 0xB8000, 0x1000, 100, 449, 80, 400, _DOUBLESCAN },
	{ 0x003, M_TEXT, 640, 200, 80, 25, 8, 8, 8, 0xB8000, 0x1000, 100, 449, 80, 400, _DOUBLESCAN },
	{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	}
};

VideoModeBlock ModeList_VGA_Text_350lines[5]=
{
	/* mode  ,type     ,sw  ,sh  ,tw ,th ,cw,ch ,pt,pstart  ,plength,htot,vtot,hde,vde special flags */
	{ 0x000, M_TEXT, 320, 350, 40, 25, 8, 14, 8, 0xB8000, 0x0800, 50, 449, 40, 350, _EGA_HALF_CLOCK },
	{ 0x001, M_TEXT, 320, 350, 40, 25, 8, 14, 8, 0xB8000, 0x0800, 50, 449, 40, 350, _EGA_HALF_CLOCK },
	{ 0x002, M_TEXT, 640, 350, 80, 25, 8, 14, 8, 0xB8000, 0x1000, 100, 449, 80, 350, 0 },
	{ 0x003, M_TEXT, 640, 350, 80, 25, 8, 14, 8, 0xB8000, 0x1000, 100, 449, 80, 350, 0 },
	{0xFFFF  ,M_ERROR  ,0   ,0   ,0  ,0  ,0 ,0  ,0 ,0x00000 ,0x0000 ,0   ,0   ,0  ,0   ,0 	}
};

Bit8u cga_masks[4]={0x3f,0xcf,0xf3,0xfc};
Bit8u cga_masks2[8]={0x7f,0xbf,0xdf,0xef,0xf7,0xfb,0xfd,0xfe};