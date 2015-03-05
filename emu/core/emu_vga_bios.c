#include "headers/types.h" //Basic types!
#include "headers/cpu/cpu.h" //CPU support!
#include "headers/interrupts/interrupt10.h" //Interrupt 10h support!

/*

Basic Screen I/O!

*/

void bios_gotoxy(int x, int y)
{
CPU.registers->AH = 2; //Set cursor position!
CPU.registers->BH = 0; //Our page!
CPU.registers->DH = y; //Our row!
CPU.registers->DL = x; //Our column!
BIOS_int10(); //Goto row!
}

void bios_displaypage() //Select the display page!
{
CPU.registers->AH = 5; //Set display page!
CPU.registers->AL = 0; //Our page!
BIOS_int10(); //Switch page!
}

void updatechar(byte attribute, byte character)
{
	CPU.registers->AH = 0x9; //Update cursor location character/attribute pair!
	CPU.registers->AL = character; //Character!
	CPU.registers->BH = 0; //Page: always 0!
	CPU.registers->BL = attribute; //The attribute to use!
	CPU.registers->CX = 1; //One time!
	BIOS_int10(); //Output the character at the current location!
}

void printmsg(byte attribute, char *text, ...) //Print a message at page #0!
{
	char msg[256];
	bzero(msg,sizeof(msg)); //Init!

	va_list args; //Going to contain the list!
	va_start (args, text); //Start list!
	vsprintf (msg, text, args); //Compile list!
	va_end (args); //Destroy list, we're done with it!

	byte length = safe_strlen(msg,sizeof(msg)); //Check the length!
	int i;
	if (CPU.registers) //Gotten a CPU to work with?
	{
		for (i=0; i<length; i++) //Process text!
		{
			switch (msg[i])
			{
			case 0x7:
			case 0x8:
			case 0x9:
			case 0xA:
			case 0xB:
			case 0xD:
				//We're a control character: process as a control character (don't update video output)!
				CPU.registers->AH = 0xE; //Teletype ouput!
				CPU.registers->AL = msg[i]; //Character, we don't want to change this!
				CPU.registers->BH = 0; //Page: always 0!
				BIOS_int10(); //Output!
				break;
			default: //Default character to output?
				updatechar(attribute, msg[i]); //Update the current character attribute: we're output!
				CPU.registers->AH = 0xE; //Teletype ouput!
				CPU.registers->AL = msg[i]; //Character, we don't want to change this!
				CPU.registers->BH = 0;//Page: always 0!
				BIOS_int10(); //Output!
				break;
			}
		}
	}
}

void printCRLF()
{
	CPU.registers->AH = 0xE; //Teletype ouput!
	CPU.registers->AL = 0xD;//Character!
	CPU.registers->BH = 0;//Page
	BIOS_int10(); //Output!

	CPU.registers->AH = 0xE; //Teletype ouput!
	CPU.registers->AL = 0xA;//Character!
	CPU.registers->BH = 0;//Page
	BIOS_int10(); //Output!
}

void BIOS_enableCursor(byte enabled)
{
	return; //Ignore!
	CPU.registers->AH = 0x03; //Get old cursor position and size!
	BIOS_int10(); //Get data!
	CPU.registers->AH = 0x01; //Set cursor shape!
	if (enabled) //Enabled?
	{
		CPU.registers->CH &= ~0x20; //Enable cursor!
	}
	else //Disabled?
	{
		CPU.registers->CH |= 0x20; //Disable cursor!
	}
	BIOS_int10(); //Set data!
}