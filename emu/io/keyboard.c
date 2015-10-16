#include "headers/hardware/ps2_keyboard.h" //Keyboard support!
#include "headers/support/highrestimer.h" //High resolution timer support!

//Basic keyboard support for emulator, mapping key presses and releases to the PS/2 keyboard!

//How many ns to wait to check for input (1000000=1ms)!
#define KEYBOARD_CHECKTIME 1000000
//How many us to take for each rapid fire divider (up to x/30 us)!
#define KEYBOARD_MAXSTEP 1000
//Multiplier on keyboard delay (in ms) to get equal to check time (KEYBOARD_CHECKTIME/10 => KEYBOARD_DELAYSTEP*10).
#define KEYBOARD_DELAYSTEP 1


extern byte SCREEN_CAPTURE; //Screen capture requested?

uint_64 key_pressed_counter = 0; //Counter for pressed keys!
uint_32 keys_pressed = 0; //Currently ammount of keys pressed!

byte key_status[0x100]; //Status of all keys!
byte keys_ispressed[0x100]; //All possible keys to be pressed!
uint_64 key_pressed_time[0x100]; //What time was the key pressed?
byte capture_status; //Capture key status?

//We're running each ms, so 1000 steps/second!
uint_64 pressedkeytimer = 0; //Pressed key timer!
uint_64 keyboard_step = 0; //How fast do we update the keys (repeated keys only)!
uint_64 keyboard_time = 0; //Total time currently counted!

TicksHolder keyboard_ticks;

void onKeyPress(char *key) //On key press/hold!
{
	if (strcmp(key,"CAPTURE")==0) //Screen capture requested?
	{
		if (!capture_status) //New key pressed?
		{
			++keys_pressed; //Increase the ammount of keys pressed!
		}
		capture_status = 1; //We're pressed!
		return; //Finished!
	}
	int keyid;
	keyid = EMU_keyboard_handler_nametoid(key); //Try to find the name!
	if (keyid!=-1) //Key found?
	{
		if (!key_status[keyid]) //New key pressed?
		{
			key_pressed_time[keyid] = key_pressed_counter++; //Increasing time of the key being pressed!
			++keys_pressed; //Increase the ammount of keys pressed!
		}
		key_status[keyid] = 1; //We've pressed a new key!
	}
}

void onKeyRelease(char *key) //On key release!
{
	if (strcmp(key, "CAPTURE") == 0) //Screen capture requested?
	{
		if (capture_status) //Pressed?
		{
			--keys_pressed; //One key has been released!
		}
		capture_status = 0; //We're released!
		return; //Finished!
	}
	int keyid;
	keyid = EMU_keyboard_handler_nametoid(key); //Try to find the name!
	if (keyid!=-1) //Key found and pressed atm?
	{
		if (key_status[keyid]) //Pressed?
		{
			--keys_pressed; //One key has been released!
		}
		key_status[keyid] = 0; //We're released!
	}
}

void calculateKeyboardStep()
{
	if (keyboard_step) //Delay executed?
	{
		keyboard_step = (uint_64)(KEYBOARD_MAXSTEP / HWkeyboard_getrepeatrate()); //Apply this count per keypress!
	}
	else
	{
		keyboard_step = HWkeyboard_getrepeatdelay()*KEYBOARD_DELAYSTEP; //Delay before giving the repeat rate!
	}
}

void releaseKeysReleased()
{
	int i;
	for (i = 0;i < NUMITEMS(key_status);i++) //Process all keys needed!
	{
		if (keys_ispressed[i] && !key_status[i]) //Are we released?
		{
			if (EMU_keyboard_handler(i, 0)) //Fired the handler for releasing!
			{
				keys_ispressed[i] = 0; //We're not pressed anymore!
			}
		}
	}
}

byte keys_active = 0;

void tickPressedKeys() //Tick any keys needed to be pressed!
{
	int i;
	keys_active = 0; //Initialise keys active!
	if (capture_status) //Pressed?
	{
		keys_active = 1; //We're active!
		SCREEN_CAPTURE = 1; //Do a screen capture next frame!
	}
	if (keyboard_step) //Typematic key?
	{
		int last_key_pressed = -1; //Last key pressed!
		uint_64 last_key_pressed_time = 0; //Last key pressed time!

		//Now, take the last pressed key, any press it!
		for (i = 0;i < NUMITEMS(key_status);i++) //Process all keys pressed!
		{
			if (key_status[i]) //Pressed?
			{
				if ((key_pressed_time[i] > last_key_pressed_time) || (last_key_pressed == -1)) //Pressed later or first one to check?
				{
					last_key_pressed = i; //This is the last key pressed!
					last_key_pressed_time = key_pressed_time[i]; //The last key pressed time!
				}
			}
		}

		if (last_key_pressed != -1) //Still gotten a last key pressed?
		{
			keys_active = 1; //We're active!
			if (EMU_keyboard_handler(last_key_pressed, 1)) //Fired the handler for pressing!
			{
				keys_ispressed[last_key_pressed] = 1; //We're pressed!
			}
		}
	}
	else //Not repeating the last key (non-typematic)?
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
		}
	}
}

void tickPendingKeys() //Handle all pending keys from our emulation! Updating every 1/1000th second!
{
	keyboard_time += getnspassed(&keyboard_ticks); //Add the ammount of nanoseconds passed!

	//Release keys as fast as possible!
	releaseKeysReleased(); //Release any unpressed keys!

	if (keyboard_step) //Waiting for a key?
	{
		if (!keys_pressed) //No keys pressed anymore?
		{
			keyboard_step = 0; //Release the timer: we're restarting the count!
		}
	}

	if (keyboard_time >= KEYBOARD_CHECKTIME) //1us passed or more?
	{
		keyboard_time -= KEYBOARD_CHECKTIME; //Rest the time passed, allow overflow!
		
		if (++pressedkeytimer > keyboard_step) //Timer expired? Tick pressed keys!
		{
			pressedkeytimer = 0; //Reset the timer!
			if (keys_pressed) //Gotten any keys pressed?
			{
				tickPressedKeys(); //Tick any pressed keys!
				calculateKeyboardStep(keys_active); //Calculate the step for pressed keys!
			}
		}
	}
}

extern char keys_names[104][11]; //Keys names!
void ReleaseKeys() //Force release all normal keys currently pressed!
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

void onKeySetChange() //PSP input: keyset change!
{
	ReleaseKeys(); //Release all keys when changing sets, except CTRL,ALT,SHIFT
	//Now, recheck input!
}

void initEMUKeyboard() //Initialise the keyboard support for emulating!
{
	initTicksHolder(&keyboard_ticks); //Initialise the ticks holder for our timing!
	getuspassed(&keyboard_ticks); //Initialise to current time!
}

void cleanEMUKeyboard()
{
	getuspassed(&keyboard_ticks); //Initialise to current time!
}