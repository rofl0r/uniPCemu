#include "headers/hardware/ps2_keyboard.h" //Keyboard support!

//Basic keyboard support for emulator, mapping key presses and releases to the PS/2 keyboard!

extern byte SCREEN_CAPTURE; //Screen capture requested?

byte key_status[0x100]; //Status of all keys!
byte keys_ispressed[0x100]; //All possible keys to be pressed!

//We're running each ms, so 1000 steps/second!
#define KEYBOARD_MAXSTEP 1000
uint_64 pressedkeytimer = 0; //Pressed key timer!
uint_64 keyboard_step = 0; //How fast do we update the keys (repeated keys only)!

void onKeyPress(char *key) //On key press/hold!
{
	if (strcmp(key,"CAPTURE")==0) //Screen capture requested?
	{
		SCREEN_CAPTURE = 1; //Screen capture next frame!
	}
	int keyid;
	keyid = EMU_keyboard_handler_nametoid(key); //Try to find the name!
	if (keyid!=-1) //Key found?
	{
		key_status[keyid] = 1; //We're pressed!
	}
}

void onKeyRelease(char *key) //On key release!
{
	int keyid;
	keyid = EMU_keyboard_handler_nametoid(key); //Try to find the name!
	if (keyid!=-1) //Key found and pressed atm?
	{
		key_status[keyid] = 0; //We're released!
	}
}

extern char keys_names[104][11]; //Keys names!

void calculateKeyboardStep()
{
	if (keyboard_step) //Delay executed?
	{
		keyboard_step = KEYBOARD_MAXSTEP / HWkeyboard_getrepeatrate(); //Apply this count per keypress!
	}
	else
	{
		keyboard_step = HWkeyboard_getrepeatdelay(); //Delay before giving the repeat rate!
	}
}

void tickPendingKeys() //Handle all pending keys from our emulation! Every 1/1000th second!
{
	int i;
	byte keys_active = 0;
	if (++pressedkeytimer > keyboard_step) //Timer expired?
	{
		for (i = 0;i < NUMITEMS(key_status);i++) //Process all keys needed!
		{
			if (key_status[i]) //Pressed?
			{
				keys_active = 1; //We're active!
				if (EMU_keyboard_handler(i, 1)) //Fired the handler for pressing!
				{
					keys_ispressed[i] = 1; //We're pressed!
				}
			}
			else //Released?
			{
				if (keys_ispressed[i]) //Are we pressed?
				{
					if (EMU_keyboard_handler(i, 0)) //Fired the handler for releasing!
					{
						keys_ispressed[i] = 0; //We're not pressed anymore!
					}
				}
			}
		}
	}
}

void ReleaseKeys() //Force release all normal keys (excluding ctrl,alt&shift) currently pressed!
{
	int i;
	for (i=0;i<NUMITEMS(keys_names);i++) //Process all keys!
	{
		if (key_status[i]) //We're pressed?
		{
			onKeyRelease(keys_names[i]); //Release the key!
		}
	}
}

void onKeySetChange()
{
	ReleaseKeys(); //Release all keys when changing sets, except CTRL,ALT,SHIFT
	//Now, recheck input!
}