#ifndef INTERRUPT10_H
#define INTERRUPT10_H

void BIOS_int10(); //Interrupt #10h: (Video Services)! Overridable!

//Stuff for VGA screen/INT10!

void GPU_setresolution(word mode); //Apply current video mode and clear screen if needed!
void int10_refreshscreen(); //Interrupt 10 screen refresh!
void int10_dumpscreen(); //Dump screen to file!
void int10_vram_readcharacter(byte x, byte y, byte page, byte *character, byte *fontcolor); //Read character+attribute!

//Putpixel functions for interrupt 10h!
//Putpixel variants!
void MEMGRAPHICS_put2colors(uint_32 startaddr, int x, int y, byte color);
void MEMGRAPHICS_put4colors(uint_32 startaddr, int x, int y, byte color);
void MEMGRAPHICS_put4colorsSHADE(uint_32 startaddr, int x, int y, byte color);
void MEMGRAPHICS_put16colors(uint_32 startaddr, int x, int y, byte color);
void MEMGRAPHICS_put256colors(uint_32 startaddr, int x, int y, byte color);

//Getpixel variants!
byte MEMGRAPHICS_get2colors(uint_32 startaddr, int x, int y);
byte MEMGRAPHICS_get4colors(uint_32 startaddr, int x, int y);
byte MEMGRAPHICS_get4colorsSHADE(uint_32 startaddr, int x, int y);
byte MEMGRAPHICS_get16colors(uint_32 startaddr, int x, int y);
byte MEMGRAPHICS_get256colors(uint_32 startaddr, int x, int y);

void cursorXY(byte displaypage, byte x, byte y); //Set cursor x,y!
void emu_setactivedisplaypage(byte page); //Set active display page!
int INT10_Internal_SetVideoMode(VGA_Type *VGA, word mode); //For internal int10 video mode switching!
void int10_internal_outputchar(byte videopage, byte character, byte attribute); //Ourput character (see int10,function 0xE)
void EMU_CPU_setCursorScanlines(byte start, byte end);
void GPU_clearscreen(); //Clears the screen!

//Finally, for switching VGA sets...
void int10_useVGA(VGA_Type *VGA);
void initint10(); //Fully initialise interrupt 10h!

byte getscreenwidth(byte displaypage); //Get the screen width (in characters), based on the video mode!

//Character fonts!
void INT10_ReloadFont(); //Reload active font at address 0 in the VGA Plane 2!
void INT10_LoadFont(VGA_Type *VGA, byte height, word offset); //Load a font at an offset!
void INT10_ActivateFont(VGA_Type *VGA, byte height, word offset);

int GPU_putpixel(int x, int y, byte page, byte color); //Writes a video buffer pixel to the real emulated screen buffer

//ROM support!
void INT10_SetupRomMemory(void); //ROM memory installation!

#endif