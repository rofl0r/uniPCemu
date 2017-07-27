#ifndef BIOSROM_H
#define BIOSROM_H

void BIOS_registerROM();
void BIOS_freeOPTROMS();
byte BIOS_checkOPTROMS(); //Check and load Option ROMs!
int BIOS_load_ROM(byte nr);
int BIOS_load_custom(char *path, char *rom);
void BIOS_free_ROM(byte nr);
void BIOS_free_custom(char *rom);
int BIOS_load_systemROM(); //Load custom ROM from emulator itself!
void BIOS_free_systemROM(); //Release the system ROM from the emulator itself!

int BIOS_load_VGAROM(); //Load custom ROM from emulator itself!
void BIOS_free_VGAROM();

void BIOS_DUMPSYSTEMROM(); //Dump the ROM currently set (debugging purposes)!

void BIOSROM_dumpBIOS(); /* For dumping the ROMs */
void BIOSROM_updateTimers(double timepassed);

#endif
