#ifndef GPU_EMU_H
#define GPU_EMU_H

void EMU_textcolor(byte color);
void EMU_gotoxy(word x, word y);
void EMU_getxy(word *x, word *y);
uint_32 getemucol16(byte color); //Special for the emulator, like the keyboard presets etc.!
void GPU_EMU_printscreen(sword x, sword y, char *text, ...); //Direct text output (from emu)!
void EMU_clearscreen(); //Clear the screen!

//Locking support for block actions!
void EMU_locktext();
void EMU_unlocktext();

#endif