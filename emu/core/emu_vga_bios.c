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
			CPU.registers->AH = 0xE; //Teletype ouput!
			CPU.registers->AL = msg[i];//Character!
			CPU.registers->BH = 0;//Page: always 0!
			CPU.registers->BL = attribute;//Color (depends on character, not black)
			BIOS_int10(); //Output!
		}
	}
}

void printCRLF()
{
	CPU.registers->AH = 0xE; //Teletype ouput!
	CPU.registers->AL = 0xD;//Character!
	CPU.registers->BH = 0;//Page
	CPU.registers->BL = 0;//Color (depends on character, not black)
	BIOS_int10(); //Output!

	CPU.registers->AH = 0xE; //Teletype ouput!
	CPU.registers->AL = 0xA;//Character!
	CPU.registers->BH = 0;//Page
	CPU.registers->BL = 0;//Color (depends on character, not black)
	BIOS_int10(); //Output!
}