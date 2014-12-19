#ifndef EMU_MAIN_H
#define EMU_MAIN_H


//For boot process! Always mode 7 for compatibility!
#define VIDEOMODE_BOOT 0x07

void mainthread(); //The main thread for the emulator!
void finishEMU(); //Called on emulator quit.
int EMU_BIOSPOST(); //The BIOS (INT19h) POST Loader!
void finishEMU(); //Called on emulator quit.

#endif