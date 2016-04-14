#include "headers/hardware/8042.h" //Basic PS/2 Controller support!
#include "headers/hardware/ps2_keyboard.h" //Basic keyboard support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/ports.h" //Port support!
#include "headers/cpu/cpu.h" //CPU support for PS/2 vs XT detection!

extern char keys_names[104][11]; //All names of the above keys (for textual representation/labeling)
extern KEYBOARDENTRY scancodesets[3][104]; //All scan codes for all sets!
extern word kbd_repeat_delay[0x4]; //Time before we start trashing!
extern float kbd_repeat_rate[0x20]; //Rate in keys per second when trashing!
byte scancodeset_typematic[104]; //Typematic keys!
byte scancodeset_break[104]; //Break enable keys!

extern Controller8042_t Controller8042; //The 8042 itself!

//Are we disabled?
#define __HW_DISABLED 0

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

OPTINLINE void resetKeyboard(byte initAA) //Reset the keyboard controller!
{
	if (__HW_DISABLED) return; //Abort!
	FIFOBUFFER *oldbuffer = Keyboard.buffer; //Old buffer!
	memset(&Keyboard,0,sizeof(Keyboard)); //Reset the controller!
	Keyboard.keyboard_enabled = 1; //Enable scanning by default!
	Keyboard.buffer = oldbuffer; //Restore the buffer!
	give_keyboard_input(0xAA); //Give OK status code!
	if (initAA) input_lastwrite_keyboard(); //Force to user!
	IRQ8042(); //We've got data in our input buffer!
	Keyboard.last_send_byte = 0xAA; //Set last send byte!
}

void resetKeyboard_8042()
{
	resetKeyboard(1); //Reset us!
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
			if (pressed&1) //Key pressed?
			{
				if ((scancodeset_typematic[key] && ((pressed>>1)&1)) || (!(pressed&2)) || (Keyboard.scancodeset!=3)) //Allowed typematic make codes? Also allow non-typematic always!
				{
					if (fifobuffer_freesize(Keyboard.buffer) < scancodesets[Keyboard.scancodeset][key].keypress_size) return 0; //Buffer full: we can't add it!
					for (i=0;i<scancodesets[Keyboard.scancodeset][key].keypress_size;i++) //Process keypress!
					{
						give_keyboard_input(scancodesets[Keyboard.scancodeset][key].keypress[i]); //Give control byte(s) of keypress!
					}
					IRQ8042(); //We've got data in our input buffer!
				}
			}
			else //Released?
			{
				if (scancodeset_break[key] || (Keyboard.scancodeset!=3)) //Break codes allowed?
				{
					if (fifobuffer_freesize(Keyboard.buffer) < scancodesets[Keyboard.scancodeset][key].keyrelease_size) return 0; //Buffer full: we can't add it!
					for (i=0;i<scancodesets[Keyboard.scancodeset][key].keyrelease_size;i++) //Process keyrelease!
					{
						give_keyboard_input(scancodesets[Keyboard.scancodeset][key].keyrelease[i]); //Give control byte(s) of keyrelease!
					}
					IRQ8042(); //We've got data in our input buffer!
				}
			}
		}
	}
	return 1; //OK: we're processed!
}

extern byte force8042; //Force 8042 style handling?

//Unknown: respond with 0xFE: Resend!
OPTINLINE void commandwritten_keyboard() //Command has been written?
{
	if (__HW_DISABLED) return; //Abort!
	Keyboard.has_command = 1; //We have a command!
	Keyboard.command_step = 0; //Reset command step!
	switch (Keyboard.command) //What command?
	{
	case 0xFF: //Reset?
		resetKeyboard(1); //Reset the Keyboard Controller!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xFE: //Resend?
		give_keyboard_input(Keyboard.last_send_byte); //Resend last non-0xFE byte!
		input_lastwrite_keyboard(); //Force 0xFA to user!
		IRQ8042(); //We've got data in our input buffer!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xFD: //Mode 3 change: Set Key Type Make
		give_keyboard_input(0xFE);
		input_lastwrite_keyboard(); //Force 0xFA to user!
		IRQ8042(); //We've got data in our input buffer!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xFC: //Mode 3 change: 
		give_keyboard_input(0xFE);
		input_lastwrite_keyboard(); //Force 0xFA to user!
		IRQ8042(); //We've got data in our input buffer!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xFB: //Mode 3 change:
		give_keyboard_input(0xFE);
		input_lastwrite_keyboard(); //Force 0xFA to user!
		IRQ8042(); //We've got data in our input buffer!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xFA: //Mode 3 change:
		give_keyboard_input(0xFA); //ACK!
		input_lastwrite_keyboard(); //Force 0xFA to user!
		IRQ8042(); //We've got data in our input buffer!
		memset(scancodeset_typematic,1,sizeof(scancodeset_typematic)); //Enable all typematic!
		memset(scancodeset_break,1,sizeof(scancodeset_break)); //Enable all break!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xF9: //Mode 3 change:
		give_keyboard_input(0xFA); //ACK!
		input_lastwrite_keyboard(); //Force 0xFA to user!
		memset(scancodeset_typematic,0,sizeof(scancodeset_typematic)); //Disable all typematic!
		memset(scancodeset_break,0,sizeof(scancodeset_break)); //Disable all break!
		IRQ8042(); //We've got data in our input buffer!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xF8: //Mode 3 change:
		give_keyboard_input(0xFA); //ACK!
		input_lastwrite_keyboard(); //Force 0xFA to user!
		IRQ8042(); //We've got data in our input buffer!
		memset(scancodeset_typematic,0,sizeof(scancodeset_typematic)); //Disable all typematic!
		memset(scancodeset_break,1,sizeof(scancodeset_break)); //Enable all break!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xF7: //Set All Keys Typematic: every type is one character send only!
		give_keyboard_input(0xFA); //ACK!
		input_lastwrite_keyboard(); //Force 0xFA to user!
		IRQ8042(); //We've got data in our input buffer!
		memset(scancodeset_typematic,1,sizeof(scancodeset_typematic)); //Enable all typematic!
		memset(scancodeset_break,0,sizeof(scancodeset_break)); //Disable all break!
		Keyboard.has_command = 0; //No command anymore!
		break;
	//0xFD-0xFB not supported, because we won't support mode 3!
	case 0xF5: //Same as 0xF6, but with scanning stop!
	case 0xF6: //Load default!
		if (Keyboard.command==0xF5) //Stop scanning?
		{
			Keyboard.keyboard_enabled = 0; //Disable keyboard!
		}
		//We set: rate/delay: 10.9cps/500ms; key types (all keys typematic/make/break) and scan code set (2)
		memset(scancodeset_typematic,1,sizeof(scancodeset_typematic)); //Enable all typematic!
		memset(scancodeset_break,1,sizeof(scancodeset_break)); //Enable all break!
		Keyboard.typematic_rate_delay = 0x1B; //rate/delay: 10.9cps/500ms!
		Keyboard.scancodeset = 2; //Scan code set 2!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xF4: //Enable scanning?
		Keyboard.keyboard_enabled = 1; //Enable keyboard!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xF3: //Set typematic rate/delay?
		//We handle after the parameters have been set!
		break;
	case 0xF2: //Read ID: return 0xAB, 0x83!
		if (EMULATED_CPU<=CPU_80186 && (!force8042)) //Allowed to ignore?
		{
			Keyboard.has_command = 0; //No command anymore!
			return; //Ignored on XT controller: there's no keyboard ID!
		}
		give_keyboard_input(0xFA); //ACK!
		input_lastwrite_keyboard(); //Force 0xFA to user!
		give_keyboard_input(0xAB); //First byte!
		give_keyboard_input(0x83); //Second byte given!
		IRQ8042(); //We've got data in our input buffer!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xF0: //Set Scan Code Set!
		give_keyboard_input(0xFA); //Give ACK first!
		input_lastwrite_keyboard(); //Force 0xFA to user!
		IRQ8042(); //We've got data in our input buffer!
		break;
	//Still need 0xF7-0xFD!
	case 0xEE: //Echo 0xEE!
		give_keyboard_input(0xEE); //Respond with "Echo"!
		input_lastwrite_keyboard(); //Force 0xFA to user!
		IRQ8042(); //We've got data in our input buffer!
		Keyboard.has_command = 0; //No command anymore!
		break;
	case 0xED: //Set/reset LEDs!
		//Next parameter is data!
		give_keyboard_input(0xFA); //Give ACK first!
		input_lastwrite_keyboard(); //Force 0xFA to user!
		IRQ8042(); //We've got data in our input buffer!
		break;
	default: //Unknown command?
		give_keyboard_input(0xFE); //Error!
		input_lastwrite_keyboard(); //Force 0xFE to user!
		IRQ8042(); //We've got data in our input buffer!
		Keyboard.has_command = 0; //No command anymore!
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
			give_keyboard_input(0xFA); //FA: Valid value!
			input_lastwrite_keyboard(); //Force 0xFA to user!
			IRQ8042(); //We've got data in our input buffer!
		}
		else //Invalid: bit 7 is never used?
		{
			give_keyboard_input(0xFE); //Error!
			input_lastwrite_keyboard(); //Force 0xFE to user!
			IRQ8042(); //We've got data in our input buffer!
		}
		Keyboard.has_command = 0; //No command anymore!
		return; //Done!
		break;
	case 0xF0: //Scan code set: the parameter that contains the scan code set!
		if (data==0) //ACK and then active scan code set?
		{
			give_keyboard_input(0xFA); //ACK!
			input_lastwrite_keyboard(); //Force 0xFA to user!
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
			IRQ8042(); //We've got data in our input buffer!
		}
		else
		{
			if (data<4) //Valid mode
			{
				Keyboard.scancodeset =(data-1); //Set scan code set!
				give_keyboard_input(0xFA); //Give ACK first!
				input_lastwrite_keyboard(); //Force 0xFA to user!
				IRQ8042(); //We've got data in our input buffer!
			}
			else
			{
				give_keyboard_input(0xFE); //Give NAK first!
				input_lastwrite_keyboard(); //Force 0xFA to user!
				IRQ8042(); //We've got data in our input buffer!
			}
			Keyboard.has_command = 0; //No command anymore!
			return; //Done!
		}
		break;
	case 0xED: //Set/reset LEDs?
		Keyboard.LEDS = data; //Set/reset LEDs!
		Keyboard.has_command = 0; //No command anymore!
		give_keyboard_input(0xFA); //Give ACK: we're OK!
		input_lastwrite_keyboard(); //Force to user!
		IRQ8042(); //We've got data in our input buffer!
		return; //Done!
		break;
	}
	++Keyboard.command_step; //Next step!
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
		Keyboard.command = Controller8042.output_buffer; //Becomes a command!
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

	if (!(PORT_IN_B(0x64)&0x1)) //No input data?
	{
		raiseError("Keyboard Hardware initialisation","No self test passed result!");
	}
	result = PORT_IN_B(0x60); //Must be 0xAA!
	if (result!=0xAA) //Error?
	{
		raiseError("Keyboard Hardware initialisation","Couldn't get Self Test passed! Result: %02X",result);
	}


	PORT_OUT_B(0x60,0xED); //Set/reset status indicators!
	if (!(PORT_IN_B(0x64)&0x1)) //No input data?
	{
		raiseError("Keyboard Hardware initialisation","No set/reset status indicator command result:1!");
	}
	result = PORT_IN_B(0x60); //Must be 0xFA!
	if (result!=0xFA) //Error?
	{
		raiseError("Keyboard Hardware initialisation","Couldn't set/reset status indicators command! Result: %02X",result);
	}

	PORT_OUT_B(0x60,0x00); //Set/reset status indicators: all off!
	if (!(PORT_IN_B(0x64)&0x1)) //No input data?
	{
		raiseError("Keyboard Hardware initialisation","No set/reset status indicator parameter result!");
	}
	result = PORT_IN_B(0x60); //Must be 0xFA!
	if (result!=0xFA) //Error?
	{
		raiseError("Keyboard Hardware initialisation","Couldn't set/reset status indicators! Result: %02X",result);
	}

	PORT_OUT_B(0x60,0xF2); //Read ID!
	if (!(PORT_IN_B(0x64)&0x1)) //No input data?
	{
		raiseError("Keyboard Hardware initialisation","No read ID ACK result!");
	}
	result = PORT_IN_B(0x60); //Must be 0xFA!
	if (result!=0xFA) //Error?
	{
		raiseError("Keyboard Hardware initialisation","Invalid function: 0xF2!",result);
	}

	if (!(PORT_IN_B(0x64)&0x1)) //No input data?
	{
		raiseError("Keyboard Hardware initialisation","No read ID result byte 1!");
	}
	result = PORT_IN_B(0x60); //Must be 0xAB!
	if (result!=0xAB) //First byte invalid?
	{
		raiseError("Keyboard Hardware initialisation","Invalid ID#1! Result: %02X",result);
	}

	if (!(PORT_IN_B(0x64)&0x1)) //No input data?
	{
		raiseError("Keyboard Hardware initialisation","No read ID result byte 2!");
	}
	result = PORT_IN_B(0x60); //Must be 0x83!
	if (result!=0x83) //Second byte invalid?
	{
		raiseError("Keyboard Hardware initialisation","Invalid ID#2! Result: %02X",result);
	}
	force8042 = 0; //Disable 8042 style init!
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
	resetKeyboard(1); //Reset the keyboard controller!
	keyboardControllerInit(); //Initialise the basic keyboard controller!
}

void BIOS_doneKeyboard()
{
	if (__HW_DISABLED) return; //Abort!
	free_fifobuffer(&Keyboard.buffer); //Free the keyboard buffer!
}
