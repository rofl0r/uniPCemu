#ifndef GPU_EMU_H
#define GPU_EMU_H

void EMU_textcolor(byte color);
void EMU_gotoxy(byte x, byte y);
void EMU_getxy(byte *x, byte *y);
uint_32 getemucol16(byte color); //Special for the emulator, like the keyboard presets etc.!
void GPU_EMU_printscreen(int x, int y, char *text, ...); //Direct text output (from emu)!
void EMU_clearscreen(); //Clear the screen!

#endif