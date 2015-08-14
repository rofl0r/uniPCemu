#include "headers/hardware/ps2_keyboard.h" //Keyboard support!

#include "headers/support/highrestimer.h" //High resolution timer support!

//Basic keyboard support for emulator, mapping key presses and releases to the PS/2 keyboard!

extern byte SCREEN_CAPTURE; //Screen capture requested?

byte key_status[0x100]; //Status of all keys!
byte keys_ispressed[0x100]; //All possible keys to be pressed!
byte capture_status; //Capture key status?

//We're running each ms, so 1000 steps/second!
#define KEYBOARD_MAXSTEP 1000
uint_64 pressedkeytimer = 0; //Pressed key timer!
uint_64 keyboard_step = 0; //How fast do we update the keys (repeated keys only)!

TicksHolder keyboard_ticks;

void onKeyPress(char *key) //On key press/hold!
{
	if (strcmp(key,"CAPTURE")==0) //Screen capture requested?
	{
		capture_status = 1; //We're pressed!
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
	if (strcmp(key, "CAPTURE") == 0) //Screen capture requested?
	{
		capture_status = 0; //We're released!
	}
	int keyid;
	keyid = EMU_keyboard_handler_nametoid(key); //Try to find the name!
	if (keyid!=-1) //Key found and pressed atm?
	{
		key_status[keyid] = 0; //We're released!
	}
}

extern char keys_names[104][11]; //Keys names!

void calculateKeyboardStep(byte activekeys)
{
	if (activekeys) //Keys pressed?
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
	else //No keys pressed?
	{
		keyboard_step = 0; //Immediately react!
	}
}

void tickPendingKeys() //Handle all pending keys from our emulation! Every 1/1000th second!
{
	int i;
	byte keys_active = 0;

	if (getuspassed_k(&keyboard_ticks) >= 1000) //1us passed or more?
	{
		uint_64 ticks;
		ticks = getuspassed(&keyboard_ticks); //Get the ammount of ticks passed!
		ticks /= 1000; //Every 1000 ticks we update!
		for (;ticks;) //Process all ticks!
		{
			if (++pressedkeytimer > keyboard_step) //Timer expired?
			{
				pressedkeytimer = 0; //Reset the timer!
				if (capture_status) //Pressed?
				{
					keys_active = 1; //We're active!
					SCREEN_CAPTURE = 1; //Screen capture next frame!
				}
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
				calculateKeyboardStep(keys_active); //Calculate the step for pressed keys!
			}
			--ticks; //We've processed 1 tick!
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

void initEMUKeyboard() //Initialise the keyboard support for emulating!
{
	initTicksHolder(&keyboard_ticks); //Initialise the ticks holder for our timing!
	getuspassed(&keyboard_ticks); //Initialise to current time!
}