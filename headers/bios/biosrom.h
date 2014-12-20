#ifndef BIOSROM_H
#define BIOSROM_H

void BIOS_registerROM();
void BIOS_freeOPTROMS();
byte BIOS_checkOPTROMS(); //Check and load Option ROMs!
int BIOS_load_ROM(byte nr);
int BIOS_load_custom(char *rom);
void BIOS_free_ROM(byte nr);
void BIOS_free_custom(char *rom);
int BIOS_load_systemROM(); //Load custom ROM from emulator itself!

#endif