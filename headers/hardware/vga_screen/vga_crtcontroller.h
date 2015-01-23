#ifndef VGA_DISPLAYGENERATION_CRTCONTROLLER_H
#define VGA_DISPLAYGENERATION_CRTCONTROLLER_H

#include "headers/types.h"

//Different signals!

#define VGA_SIGNAL_VTOTAL 0x1
#define VGA_SIGNAL_HTOTAL 0x2
#define VGA_SIGNAL_VRETRACESTART 0x4
#define VGA_SIGNAL_HRETRACESTART 0x8
#define VGA_SIGNAL_VRETRACEEND 0x10
#define VGA_SIGNAL_HRETRACEEND 0x20
#define VGA_SIGNAL_VBLANKSTART 0x40
#define VGA_SIGNAL_HBLANKSTART 0x80
#define VGA_SIGNAL_VBLANKEND 0x100
#define VGA_SIGNAL_HBLANKEND 0x200
#define VGA_VACTIVEDISPLAY 0x400
#define VGA_HACTIVEDISPLAY 0x800
#define VGA_OVERSCAN 0x1000

//Display check
//Bits to check:
#define VGA_DISPLAYMASK (VGA_VACTIVEDISPLAY|VGA_HACTIVEDISPLAY)
//Bits set within above bits:
#define VGA_DISPLAYACTIVE (VGA_VACTIVEDISPLAY|VGA_HACTIVEDISPLAY)

//Do we have a signal with these bits on!
#define VGA_SIGNAL_HASSIGNAL 0x3FF

//Pixel manipulation!
//OPTINLINE byte getVRAMScanlineMultiplier(VGA_Type *VGA); //VRAM scanline multiplier!
OPTINLINE byte getVRAMMemAddrSize(VGA_Type *VGA); //Current memory address size?
//OPTINLINE word getHorizontalStart(VGA_Type *VGA); //How many pixels to take off the active display x to get the start x!
//OPTINLINE word getHorizontalEnd(VGA_Type *VGA); //How many pixels to take off the display x to get the start of the right border?
//OPTINLINE word getVerticalDisplayEnd(VGA_Type *VGA);
//OPTINLINE word getVerticalBlankingStart(VGA_Type *VGA);
//OPTINLINE word getHorizontalBlankingStart(VGA_Type *VGA);
//OPTINLINE byte is_activedisplay(VGA_Type *VGA,word ScanLine, word x);
//OPTINLINE byte is_overscan(VGA_Type *VGA,word ScanLine, word x);
//OPTINLINE word getxres(VGA_Type *VGA);
//OPTINLINE word getyres(VGA_Type *VGA);
//OPTINLINE word getxresfull(VGA_Type *VGA); //Full resolution (border+active display area) width
//OPTINLINE word getyresfull(VGA_Type *VGA); //Full resolution (border+active display area) height
//OPTINLINE word getrowsize(VGA_Type *VGA); //Give the size of a row in VRAM!
//OPTINLINE word getTopWindowStart(VGA_Type *VGA); //Get Top Window Start scanline!
OPTINLINE byte VGA_ScanDoubling(VGA_Type *VGA); //Scanline doubling?
OPTINLINE uint_32 getVRAMScanlineStart(VGA_Type *VGA,word Scanline); //Start of a scanline!
OPTINLINE word getHorizontalTotal(VGA_Type *VGA); //Get horizontal total (for calculating refresh speed timer)
OPTINLINE word get_display(VGA_Type *VGA, word Scanline, word x); //Get/adjust the current display part for the next pixel (going from 0-total on both x and y)!

//For precalcs only!
word get_display_y(VGA_Type *VGA, word scanline); //Vertical check!
word get_display_x(VGA_Type *VGA, word x); //Horizontal check!

//Character sizes in pixels!
OPTINLINE byte getcharacterwidth(VGA_Type *VGA);
OPTINLINE byte getcharacterheight(VGA_Type *VGA);

#endif