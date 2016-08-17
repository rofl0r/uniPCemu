#ifndef BOOT_H
#define BOOT_H

//Boot successfull or error?
#define BOOT_OK TRUE
#define BOOT_ERROR FALSE

//CD-ROM boot image for when booting. (is written to from cd-rom, destroyed at closing the emulator)
#define BOOT_CD_IMG "tmpcdrom.img"

int CPU_boot(int device); //Boots from an i/o device (result TRUE: booted, FALSE: unable to boot/unbootable/read error etc.)!

#endif