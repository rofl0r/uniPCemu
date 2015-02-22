#ifndef EMU_VGA_BIOS_H
#define EMU_VGA_BIOS_H
void bios_gotoxy(int x, int y);
void BIOS_enableCursor(byte enabled); //Enable/disable the cursor!
void bios_displaypage(); //Select the display page!
void printCRLF();
#endif