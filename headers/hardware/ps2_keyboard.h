#ifndef PS2_KEYBOARD_H
#define PS2_KEYBOARD_H

#include "headers/types.h" //Basic types!
#include "headers/support/fifobuffer.h" //FIFOBUFF#R support!

typedef struct
{
byte keypress[8];
byte keypress_size; //1-8
byte keyrelease[8];
byte keyrelease_size; //0-8: 0 for none!
} KEYBOARDENTRY; //Entry containing keyboard character data (press/hold and release data)!

typedef struct
{
	byte keyboard_enabled; //Keyboard enabled?
	byte typematic_rate_delay; //Typematic rate/delay setting (see lookup table) bits 0-4=rate(chars/second);5-6=delay(ms)!
	byte scancodeset; //Scan code set!

	byte LEDS; //The LEDS active!

	byte has_command; //Has command?
	byte command; //Current command given!
	byte command_step; //Step within command (if any)?

	byte last_send_byte; //Last send byte from keyboard controller!

	FIFOBUFFER *buffer; //Buffer for output!
} PS2_KEYBOARD; //Active keyboard settings!

void give_keyboard_input(byte data); //For the 8042!

byte EMU_keyboard_handler(byte key, byte pressed); //A key has been pressed (with interval) or released CALLED BY HARDWARE KEYBOARD (Virtual Keyboard?)? 0 indicates failure sending it!
void EMU_keyboard_handler_name(char *name, byte pressed); //Handle key press/hold(pressed=1) or release(pressed=0) by name for hardware!
void EMU_mouse_handler(byte *data, byte datasize); //Mouse results (packets) handler!
//Name/ID conversion functionality!
int EMU_keyboard_handler_nametoid(char *name); //Same as above, but with unique names from the keys_names table!
int EMU_keyboard_handler_idtoname(int id, char *name); //Same as above, but with unique names from the keys_names table!

float HWkeyboard_getrepeatrate(); //Which repeat rate to use after the repeat delay! (chars/second)
word HWkeyboard_getrepeatdelay(); //Delay after which to start using the repeat rate till release! (in ms)

void BIOS_initKeyboard(); //Initialise the PS/2 keyboard (AFTER the 8042!)
void BIOS_doneKeyboard(); //Finish the PS/2 keyboard.

void resetKeyboard_8042(); //8042 reset for XT compatibility!

#endif