#include "headers/hardware/8042.h" //Basic PS/2 Controller support!
#include "headers/hardware/ps2_keyboard.h" //Basic keyboard support!
#include "headers/support/log.h" //Logging support!
#include "headers/cpu/cpu.h" //CPU support for PS/2 vs XT detection!
#include "headers/hardware/ports.h" //Support for initializing the controller!

extern char keys_names[104][11]; //All names of the above keys (for textual representation/labeling)
extern KEYBOARDENTRY scancodesets[3][104]; //All scan codes for all sets!
extern word kbd_repeat_delay[0x4]; //Time before we start trashing!
extern float kbd_repeat_rate[0x20]; //Rate in keys per second when trashing!
byte scancodeset_typematic[104]; //Typematic keys!
byte scancodeset_break[104]; //Break enable keys!

extern Controller8042_t Controller8042; //The 8042 itself!

//Are we disabled?
#define __HW_DISABLED 0

//Timeout between commands or parameters and results being buffered!
#define KEYBOARD_DEFAULTTIMEOUT 100000000.0

PS2_KEYBOARD Keyboard; //Active keyboard settings!

void give_keyboard_input(byte data)
{
	if (__HW_DISABLED) return; //Abort!
	writefifobuffer(Keyboard.buffer,data); //Write to the buffer, ignore the result!
}

OPTINLINE void input_lastwrite_keyboard()
{
	if (__HW_DISABLED) return; //Abort!
	fifobuffer_gotolast(Keyboard.buffer); //Goto last!
}

OPTINLINE void loadKeyboardDefaults()
{
	//We set: rate/delay: 10.9cps/500ms; key types (all keys typematic/make/break) and scan code set (2)
	memset(scancodeset_typematic, 1, sizeof(scancodeset_typematic)); //Enable all typematic!
	memset(scancodeset_break, 1, sizeof(scancodeset_break)); //Enable all break!
	Keyboard.typematic_rate_delay = 0x1B; //rate/delay: 10.9cps/500ms!
	Keyboard.scancodeset = 2; //Scan code set 2!
}

OPTINLINE void resetKeyboard(byte flags, byte is_ATInit) //Reset the keyboard controller!
{
	if (__HW_DISABLED) return; //Abort!
	FIFOBUFFER *oldbuffer = Keyboard.buffer; //Old buffer!
	memset(&Keyboard,0,sizeof(Keyboard)); //Reset the controller!
	Keyboard.keyboard_enabled = 1; //Enable scanning by default!
	Keyboard.buffer = oldbuffer; //Restore the buffer!
	if (!is_ATInit)
	{
		give_keyboard_input(0xAA); //Give OK status code!
		IRQ8042(flags); //We've got data in our input buffer!
	}
	Keyboard.last_send_byte = 0xAA; //Set last send byte!
	loadKeyboardDefaults(); //Load our defaults!
}

void keyboardtranslation_8042(byte enabled)
{
	Keyboard.enable_translation = enabled?1:0; //Enable translation to scancode set 0?
}

void resetKeyboard_8042(byte flags)
{
	input_lastwrite_keyboard(); //Force to user!
	resetKeyboard(flags,0); //Reset us! Execute an interrupt as well!
	input_lastwrite_keyboard(); //Force to user!
}

float HWkeyboard_getrepeatrate() //Which repeat rate to use after the repeat delay! (chars/second)
{
	if (__HW_DISABLED) return 1.0f; //Abort!
	float result;
	result = kbd_repeat_rate[(Keyboard.typematic_rate_delay&0x1F)]; //Get result!
	if (!result) //No rate?
	{
		return 1.0f; //Give 1 key/second by default!
	}
	return result; //Give the repeat rate!
}

word HWkeyboard_getrepeatdelay() //Delay after which to start using the repeat rate till release! (in ms)
{
	if (__HW_DISABLED) return 1; //Abort!
	return kbd_repeat_delay[(Keyboard.typematic_rate_delay&0x60)>>5]; //Give the repeat delay!
}

int EMU_keyboard_handler_nametoid(char *name) //Same as above, but with unique names from the keys_names table!
{
	byte b=0;
	for (;b<NUMITEMS(keys_names);) //Find the key!
	{
		if (strcmp(keys_names[b],name)==0) //Found?
		{
			return b; //Give the ID!
		}
		++b; //Next key!
	}
	//Unknown name: don't do anything!
	return -1; //Unknown key!
}

int EMU_keyboard_handler_idtoname(int id, char *name) //Same as above, but with unique names from the keys_names table!
{
	if (id<(int)NUMITEMS(keys_names)) //Valid?
	{
		strcpy(name,keys_names[id]); //Set name!
		return 1; //Gotten!
	}
	return 0; //Unknown!
}

//key is an index into the scancode set!
byte EMU_keyboard_handler(byte key, byte pressed) //A key has been pressed (with interval) or released CALLED BY HARDWARE KEYBOARD (Virtual Keyboard?)? Bit1=Pressed(1) or released (0), Bit2=Repeating(1) or not repeating(0)
{
	if (__HW_DISABLED) return 1; //Abort!
	if (Keyboard.has_command) return 0; //Have a command: command mode inhabits keyboard input?
	if (Keyboard.keyboard_enabled) //Keyboard enabled?
	{
		if (!Controller8042.PS2ControllerConfigurationByte.FirstPortDisabled) //We're enabled?
		{
			int i; //Counter for key codes!
			byte scancodeset;
			scancodeset = Keyboard.scancodeset; //Get the current scancode set!
			if (Keyboard.enable_translation) scancodeset = 0; //Force scan code set 1 when translated!
			if (pressed&1) //Key pressed?
			{
				if ((scancodeset_typematic[key] && ((pressed>>1)&1)) || (!(pressed&2)) || (scancodeset!=3)) //Allowed typematic make codes? Also allow non-typematic always!
				{
					if (fifobuffer_freesize(Keyboard.buffer) < scancodesets[scancodeset][key].keypress_size) return 0; //Buffer full: we can't add it!
					for (i=0;i<scancodesets[scancodeset][key].keypress_size;i++) //Process keypress!
					{
						give_keyboard_input(scancodesets[scancodeset][key].keypress[i]); //Give control byte(s) of keypress!
					}
					IRQ8042(1); //We've got data in our input buffer!
				}
			}
			else //Released?
			{
				if (scancodeset_break[key] || (scancodeset!=3)) //Break codes allowed?
				{
					if (fifobuffer_freesize(Keyboard.buffer) < scancodesets[scancodeset][key].keyrelease_size) return 0; //Buffer full: we can't add it!
					for (i=0;i<scancodesets[scancodeset][key].keyrelease_size;i++) //Process keyrelease!
					{
						give_keyboard_input(scancodesets[scancodeset][key].keyrelease[i]); //Give control byte(s) of keyrelease!
					}
					IRQ8042(1); //We've got data in our input buffer!
				}
			}
		}
	}
	return 1; //OK: we're processed!
}

extern byte force8042; //Force 8042 style handling?

void updatePS2Keyboard(double timepassed)
{
	if (Keyboard.timeout) //Gotten a timeout?
	{
		Keyboard.timeout -= timepassed; //Pass some time!
		if (Keyboard.timeout <= 0.0) //Done?
		{
			switch (Keyboard.command) //What command?
			{
			case 0xFF: //Reset command?
				switch (Keyboard.command_step) //What step?
				{
				case 1: //First stage?
					input_lastwrite_keyboard(); //Force 0x00(dummy byte) to user!
					give_keyboard_input(0xFA); //Acnowledge!
					IRQ8042(1); //We've got data in our input buffer!
					resetKeyboard(1, 1); //Reset the Keyboard Controller! Don't give a result(this will be done in time)!
					Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear!
					Keyboard.command_step = 2; //Step 2!
					break;
				case 2: //Final stage?
					Keyboard.timeout = (double)0; //Finished!
					give_keyboard_input(0xAA); //Give the result code!
					IRQ8042(1); //We've got data in our input buffer!
					Keyboard.command_step = 0; //Finished!
					break;
				}
				break;
			case 0xFE: //Resend?
				give_keyboard_input(Keyboard.last_send_byte); //Resend last non-0xFE byte!
				input_lastwrite_keyboard(); //Force 0xFA to user!
				IRQ8042(1); //We've got data in our input buffer!
				Keyboard.has_command = 0; //No command anymore!
				break;
			case 0xFA: //Plain ACK and finish!
			case 0xF9: //Plain ACK and finish!
			case 0xF8: //Plain ACK and finish!
			case 0xF7: //Plain ACK and finish!
				give_keyboard_input(0xFA); //ACK!
				input_lastwrite_keyboard(); //Force 0xFA to user!
				IRQ8042(1); //We've got data in our input buffer!
				Keyboard.has_command = 0; //No command anymore!
				break;
			case 0xF2: //Read ID
				give_keyboard_input(0xFA); //ACK!
				input_lastwrite_keyboard(); //Force 0xFA to user!
				give_keyboard_input(0xAB); //First byte!
				give_keyboard_input(0x83); //Second byte given!
				IRQ8042(1); //We've got data in our input buffer!
				Keyboard.has_command = 0; //No command anymore!
				break;
			case 0xF0: //ACK and next phase!
			case 0xED: //ACK and next phase!
				if (Keyboard.cmdOK) //Second+ step?
				{
					if ((Keyboard.cmdOK&3) == 1) //OK?
					{
						give_keyboard_input(0xFA); //FA: Valid value!
						input_lastwrite_keyboard(); //Force 0xFA to user!
						IRQ8042(1); //We've got data in our input buffer!
						++Keyboard.command_step; //Next step!
					}
					else if ((Keyboard.cmdOK&3) == 2) //Error?
					{
						give_keyboard_input(0xFE); //FE: Invalid value!
						input_lastwrite_keyboard(); //Force 0xFA to user!
						IRQ8042(1); //We've got data in our input buffer!
					}
					else if ((Keyboard.command == 0xF0) && (Keyboard.command_step == 2)) //Second step gives input?
					{
						switch (Keyboard.scancodeset) //What set?
						{
						case 0:
							give_keyboard_input(0x43); //Get scan code set!
							break;
						case 1:
							give_keyboard_input(0x41); //Get scan code set!
							break;
						case 2:
							give_keyboard_input(0x3F); //Get scan code set!
							break;
						}
						IRQ8042(1); //We've got data in our input buffer!
						Keyboard.cmdOK |= 4; //We're finished!
					}
					if (Keyboard.cmdOK & 4) //Finish?
					{
						Keyboard.command_step = 0; //No command anymore!
						Keyboard.has_command = 0; //Finish!
					}
					else if (Keyboard.cmdOK & 8) //We're to add another timer for the next step?
					{
						Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //Delay until next response!
					}
				}
				break;
			case 0xEE: //Echo 0xEE!
				give_keyboard_input(0xEE); //Respond with "Echo"!
				input_lastwrite_keyboard(); //Force 0xFA to user!
				IRQ8042(1); //We've got data in our input buffer!
				Keyboard.has_command = 0; //No command anymore!
				break;
			default: //Unknown command?
			case 0xFD:
			case 0xFC:
			case 0xFB:
				give_keyboard_input(0xFE); //Unknown command!
				input_lastwrite_keyboard(); //Force 0xFA to user!
				IRQ8042(1); //We've got data in our input buffer!
				Keyboard.has_command = 0; //No command anymore!
				Keyboard.timeout = (double)0; //Finished!
				break;
			}
		}
	}
}

//Unknown: respond with 0xFE: Resend!
OPTINLINE void commandwritten_keyboard() //Command has been written?
{
	if (__HW_DISABLED) return; //Abort!
	Keyboard.has_command = 1; //We have a command!
	Keyboard.command_step = 0; //Reset command step!
	switch (Keyboard.command) //What command?
	{
	case 0xFF: //Reset?
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		break;
	case 0xFE: //Resend?
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		break;
	case 0xFD: //Mode 3 change: Set Key Type Make
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		break;
	case 0xFC: //Mode 3 change: 
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		break;
	case 0xFB: //Mode 3 change:
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		break;
	case 0xFA: //Mode 3 change:
		memset(scancodeset_typematic,1,sizeof(scancodeset_typematic)); //Enable all typematic!
		memset(scancodeset_break,1,sizeof(scancodeset_break)); //Enable all break!
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		break;
	case 0xF9: //Mode 3 change:
		memset(scancodeset_typematic,0,sizeof(scancodeset_typematic)); //Disable all typematic!
		memset(scancodeset_break,0,sizeof(scancodeset_break)); //Disable all break!
		IRQ8042(1); //We've got data in our input buffer!
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		break;
	case 0xF8: //Mode 3 change:
		IRQ8042(1); //We've got data in our input buffer!
		memset(scancodeset_typematic,0,sizeof(scancodeset_typematic)); //Disable all typematic!
		memset(scancodeset_break,1,sizeof(scancodeset_break)); //Enable all break!
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		break;
	case 0xF7: //Set All Keys Typematic: every type is one character send only!
		memset(scancodeset_typematic,1,sizeof(scancodeset_typematic)); //Enable all typematic!
		memset(scancodeset_break,0,sizeof(scancodeset_break)); //Disable all break!
		Keyboard.has_command = 0; //No command anymore!
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		break;
	//0xFD-0xFB not supported, because we won't support mode 3!
	case 0xF5: //Same as 0xF6, but with scanning stop!
	case 0xF6: //Load default!
		if (Keyboard.command==0xF5) //Stop scanning?
		{
			Keyboard.keyboard_enabled = 0; //Disable keyboard!
		}
		loadKeyboardDefaults(); //Load our defaults!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xF4: //Enable scanning?
		Keyboard.keyboard_enabled = 1; //Enable keyboard!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xF3: //Set typematic rate/delay?
		//We handle after the parameters have been set!
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		Keyboard.cmdOK = 1; //ACK and next step!
		break;
	case 0xF2: //Read ID: return 0xAB, 0x83!
		if ((EMULATED_CPU<=CPU_NECV30) && (force8042==0)) //Allowed to ignore?
		{
			Keyboard.has_command = 0; //No command anymore!
			return; //Ignored on XT controller: there's no keyboard ID!
		}
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		break;
	case 0xF0: //Set Scan Code Set!
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		Keyboard.cmdOK = 1; //ACK and no finish!
		break;
	//Still need 0xF7-0xFD!
	case 0xEE: //Echo 0xEE!
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		break;
	case 0xED: //Set/reset LEDs!
		//Next parameter is data!
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		Keyboard.cmdOK = 1; //ACK and no finish!
		break;
	default: //Unknown command?
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		return; //Abort!
		break;
	}
	if (Keyboard.has_command) //Still a command?
	{
		++Keyboard.command_step; //Next step (step starts at 1 always)!
	}
}

OPTINLINE void handle_keyboard_data(byte data)
{
	if (__HW_DISABLED) return; //Abort!
	switch (Keyboard.command)
	{
	case 0xF3: //We're the typematic rate/delay value?
		if (data<0x80) //Valid?
		{
			Keyboard.typematic_rate_delay = data; //Set typematic rate/delay!
			Keyboard.cmdOK = 1|4; //OK&Finish!
		}
		else //Invalid: bit 7 is never used?
		{
			Keyboard.cmdOK = 2|4; //Error&Finish!
		}
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		Keyboard.has_command = 0; //No command anymore!
		return; //Done!
		break;
	case 0xF0: //Scan code set: the parameter that contains the scan code set!
		if (data==0) //ACK and then active scan code set?
		{
			Keyboard.cmdOK = 1|4|8; //OK&Continue!
			Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		}
		else
		{
			if (data<4) //Valid mode
			{
				Keyboard.scancodeset =(data-1); //Set scan code set!
				Keyboard.cmdOK = 1|4; //OK&Finish!
				Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
			}
			else
			{
				Keyboard.cmdOK = 2 | 4; //Error&Finish!
				Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
			}
			return; //Done!
		}
		break;
	case 0xED: //Set/reset LEDs?
		Keyboard.LEDS = data; //Set/reset LEDs!
		Keyboard.cmdOK = 1|4; //OK&Finish!
		Keyboard.timeout = KEYBOARD_DEFAULTTIMEOUT; //A small delay for the result code to appear(needed by the AT BIOS)!
		return; //Done!
		break;
	}
}

void handle_keyboardwrite(byte data)
{
	if (__HW_DISABLED) return; //Abort!
	if (Keyboard.has_command) //Parameter of command?
	{
		handle_keyboard_data(data); //Handle parameters!
		if (!Keyboard.has_command) //No command anymore?
		{
			Keyboard.command_step = 0; //Reset command step!
		}
	}
	else //Command itself?
	{
		Keyboard.command = data; //Becomes a command!
		commandwritten_keyboard(); //Process keyboard command?
		if (!Keyboard.has_command) //No command anymore?
		{
			Keyboard.command_step = 0; //Reset command step!
		}
	}
}

byte handle_keyboardread() //Read from the keyboard!
{
	if (__HW_DISABLED) return 0; //Abort!
	byte result;
	if (readfifobuffer(Keyboard.buffer,&result)) //Try to read?
	{
		return result; //Read successful!
	}
	else //Nothing to read?
	{
		return 0x00; //NULL!
	}
}

int handle_keyboardpeek(byte *result) //Peek at the keyboard!
{
	if (__HW_DISABLED) return 0; //Abort!
	return peekfifobuffer(Keyboard.buffer,result); //Peek at the buffer!
}

//Initialisation stuff!

OPTINLINE void keyboardControllerInit() //Part before the BIOS at computer bootup (self test)!
{
	if (__HW_DISABLED) return; //Abort!
	force8042 = 1; //We're forcing 8042 style init!
	byte result; //For holding the result from the hardware!
	Controller8042.RAM[0] &= ~0x50; //Enable our input, disable translation!
	for (;!(PORT_IN_B(0x64)&0x1);) //Wait for input data?
	{
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}
	result = PORT_IN_B(0x60); //Must be 0xAA!
	if (result!=0xAA) //Error?
	{
		raiseError("Keyboard Hardware initialisation","Couldn't get Self Test passed! Result: %02X",result);
	}


	PORT_OUT_B(0x60,0xED); //Set/reset status indicators!
	for (;!(PORT_IN_B(0x64) & 0x1);) //Wait for input data?
	{
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}
	result = PORT_IN_B(0x60); //Must be 0xFA!
	if (result!=0xFA) //Error?
	{
		raiseError("Keyboard Hardware initialisation","Couldn't set/reset status indicators command! Result: %02X",result);
	}

	PORT_OUT_B(0x60,0x00); //Set/reset status indicators: all off!
	for (;!(PORT_IN_B(0x64) & 0x1);) //Wait for input data?
	{
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}
	result = PORT_IN_B(0x60); //Must be 0xFA!
	if (result!=0xFA) //Error?
	{
		raiseError("Keyboard Hardware initialisation","Couldn't set/reset status indicators! Result: %02X",result);
	}

	PORT_OUT_B(0x60,0xF2); //Read ID!
	for (;!(PORT_IN_B(0x64) & 0x1);) //Wait for input data?
	{
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}
	result = PORT_IN_B(0x60); //Must be 0xFA!
	if (result!=0xFA) //Error?
	{
		raiseError("Keyboard Hardware initialisation","Invalid function: 0xF2!",result);
	}

	for (;!(PORT_IN_B(0x64) & 0x1);) //Wait for input data?
	{
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}
	result = PORT_IN_B(0x60); //Must be 0xAB!
	if (result!=0xAB) //First byte invalid?
	{
		raiseError("Keyboard Hardware initialisation","Invalid ID#1! Result: %02X",result);
	}

	for (;!(PORT_IN_B(0x64) & 0x1);) //Wait for input data?
	{
		updatePS2Keyboard(KEYBOARD_DEFAULTTIMEOUT); //Update the keyboard when allowed!
	}
	result = PORT_IN_B(0x60); //Must be 0x83!
	if (result!=0x83) //Second byte invalid?
	{
		raiseError("Keyboard Hardware initialisation","Invalid ID#2! Result: %02X",result);
	}
	fifobuffer_clear(Keyboard.buffer); //Clear our output buffer for compatibility!
	resetKeyboard(0,1); //Reset us to a known state on AT PCs when needed!
	force8042 = 0; //Disable 8042 style init!
	Controller8042.RAM[0] |= 0x50; //Disable our input, enable translation!
	keyboardtranslation_8042(Controller8042.RAM[0]&0x40); //Enable translation?
}

void keyboardControllerInit_extern()
{
	keyboardControllerInit(); //Part before the BIOS at computer bootup (self test), external!
}

void BIOS_initKeyboard() //Initialise the keyboard, after the 8042!
{
	if (__HW_DISABLED) return; //Abort!
	//First, register ourselves!
	register_PS2PortWrite(0,&handle_keyboardwrite); //Write functionality!
	register_PS2PortRead(0,&handle_keyboardread,&handle_keyboardpeek); //Read functionality!		
	Keyboard.buffer = allocfifobuffer(32,1); //Allocate a small keyboard buffer (originally 16, dosbox uses double buffer (release size=2 by default)!
	memset(scancodeset_typematic,1,sizeof(scancodeset_typematic)); //Typematic?
	memset(scancodeset_break,1,sizeof(scancodeset_break)); //Allow break codes?
	resetKeyboard(1,0); //Reset the keyboard controller, XT style!
	input_lastwrite_keyboard(); //Force to user!
	if (EMULATED_CPU>CPU_NECV30) keyboardControllerInit(); //Initialise the basic keyboard controller when allowed!
	else //IBM XT initialization required?
	{
		keyboardtranslation_8042(0x40); //Enable the translation always with IBM XT!
	}
}

void BIOS_doneKeyboard()
{
	if (__HW_DISABLED) return; //Abort!
	free_fifobuffer(&Keyboard.buffer); //Free the keyboard buffer!
}
