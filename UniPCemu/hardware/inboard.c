#include "headers/hardware/inboard.h" //Inboard support!
#include "headers/cpu/cpu.h" //CPU support! 
#include "headers/hardware/ports.h" //I/O port support!
#include "headers/hardware/8042.h" //8042 support!

extern byte CPU386_WAITSTATE_DELAY; //386+ Waitstate, which is software-programmed?
extern byte is_XT; //XT?
extern Controller8042_t Controller8042; //The PS/2 Controller chip!

byte Inboard_readIO(word port, byte *result)
{
	return 0; //Unknown port!
}

void refresh_outputport(); //For letting the 8042 refresh the output port!

byte Inboard_writeIO(word port, byte value)
{
	switch (port)
	{
		case 0x60: //Special flags? Used for special functions on the Inboard 386!
			if (EMULATED_CPU==CPU_80386) //Inboard 386 emulation?
			{
				switch (value) //Special Inboard 386 commands?
				{
				case 0xDD: //Disable A20 line?
					Controller8042.outputport = Controller8042.outputport&(~0x2); //Wrap arround: disable A20 line!
					refresh_outputport(); //Handle the new output port!
					return 1;
					break;
				case 0xDF: //Enable A20 line?
					Controller8042.outputport = Controller8042.outputport|(0x2); //Wrap arround: enable A20 line!
					refresh_outputport(); //Handle the new output port!
					return 1;
					break;
				default: //Unsupported?
					break;
				}
			}
			break;
	}
	return 0; //Unknown port!
}

void initInboard() //Initialize the Inboard chipset, if needed for the current CPU!
{
	//Add any Inboard support!
	if ((EMULATED_CPU==CPU_80386) && is_XT) //XT 386? We're an Inboard 386!
	{
		register_PORTOUT(&Inboard_writeIO);
		register_PORTIN(&Inboard_readIO);
	}
}