#include "headers/types.h" //Basic types!
#include "headers/hardware/vga.h" //VGA support!

//Are we a blinking 16-bit color?
OPTINLINE byte col16_blink(byte color)
{
	return (color&0x80)>0; //Blink?
}

//16 colors taken from here: http://en.wikipedia.org/wiki/Enhanced_Graphics_Adapter

OPTINLINE uint_32 getcol16bw(byte color)
{
	byte c = color*0x11; //Make cch
	return RGB(c,c,c); //Gray scale!
}

OPTINLINE uint_32 getcol4(byte color)
{
	uint_32 c[4] = {RGB(0x00,0x00,0x00),RGB(0x55,0x55,0x55),RGB(0xAA,0xAA,0xAA),RGB(0xFF,0xFF,0xFF)};
	return c[color]; //Translate through easy table!
}

OPTINLINE uint_32 getcol2(byte color)
{
	return color?RGB(0xFF,0XFF,0XFF):RGB(0x00,0x00,0x00); //2-color: B/W!
}

OPTINLINE uint_32 getcolX(byte r, byte g, byte b, byte size) //Convert color with max value size to RGB!
{
	return RGB(convertrel(r,size,255),convertrel(g,size,255),convertrel(b,size,255)); //Use relative RGB convert!
}

OPTINLINE uint_32 getcol64k(byte r, byte g, byte b) //64k color convert!
{
	return RGB(convertrel(r,0x1F,255),convertrel(g,0x3F,255),convertrel(b,0x1F,255)); //Use relative RGB convert!
}