#ifndef BIOSMENU_H
#define BIOSMENU_H

void allocBIOSMenu(); //Stuff that take extra video memory etc. for seperated BIOS allocation (so before MMU, because it may take it all)!
void freeBIOSMenu(); //Free up all BIOS related memory!
void initBIOSMenu(); //Stuff that take extra video memory etc. for seperated BIOS!

int CheckBIOSMenu(uint_32 timeout); //To run the BIOS Menus! Result: to reboot?
byte runBIOS(byte showloadingtext); //Run the BIOS!

#endif