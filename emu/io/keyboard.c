#include "headers/hardware/ps2_keyboard.h" //Keyboard support!

//Basic keyboard support for emulator, mapping key presses and releases to the PS/2 keyboard!

byte keys_pressed[0x100]; //All possible keys to be pressed!

void onKeyPress(char *key) //On key press/hold!
{
	int keyid;
	keyid = EMU_keyboard_handler_nametoid(key); //Try to find the name!
	if (keyid!=-1) //Key found?
	{
		EMU_keyboard_handler(keyid,1); //Fire the handler for pressing!
		keys_pressed[keyid] = 1; //We're pressed!
	}
}

void onKeyRelease(char *key) //On key release!
{
	int keyid;
	keyid = EMU_keyboard_handler_nametoid(key); //Try to find the name!
	if (keyid!=-1) //Key found and pressed atm?
	{
		if (keys_pressed[keyid])
		{
			EMU_keyboard_handler(keyid,0); //Fire the handler for releasing!
			keys_pressed[keyid] = 0; //We're not pressed atm!
		}
	}
}

extern char keys_names[104][11]; //Keys names!

void ReleaseKeys() //Force release all normal keys (excluding ctrl,alt&shift) currently pressed!
{
	int i;
	for (i=0;i<256;i++) //Process all keys!
	{
		onKeyRelease(keys_names[i]); //Release the key!
	}
}

void onKeySetChange()
{
	ReleaseKeys(); //Release all keys when changing sets, except CTRL,ALT,SHIFT
	//Now, recheck input!
}