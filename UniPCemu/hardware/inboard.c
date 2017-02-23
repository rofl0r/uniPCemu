#include "headers/hardware/inboard.h" //Inboard support!
#include "headers/cpu/cpu.h" //CPU support! 
#include "headers/hardware/ports.h" //I/O port support!
#include "headers/hardware/8042.h" //8042 support!
#include "headers/mmu/mmuhandler.h" //MMU support!

extern byte CPU386_WAITSTATE_DELAY; //386+ Waitstate, which is software-programmed?
extern byte is_XT; //XT?
extern Controller8042_t Controller8042; //The PS/2 Controller chip!
extern byte MoveLowMemoryHigh; //Move HMA physical memory high?

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
				case 0xDF: //Enable A20 line?
					Controller8042.outputport = SETBITS(Controller8042.outputport,1,1,GETBITS(value,0,1)); //Wrap arround: disable/enable A20 line!
					refresh_outputport(); //Handle the new output port!
					return 1;
					break;
				default: //Unsupported?
					dolog("inboard","Unknown Inboard port 60h command: %02X",value);
					break;
				}
			}
			break;
		case 0x670: //Special flags 2? Used for special functions on the Inboard 386!
			if (EMULATED_CPU==CPU_80386) //Inboard 386 emulation?
			{
				switch (value) //Special Inboard 386 commands?
				{
				case 0x1E: //Move memory high?
				case 0x1F: //Move memory normal(using memory hole)?
					MoveLowMemoryHigh = GETBITS(~value,0,1); //Disable/enable the HMA memory or BIOS ROM!
					return 1;
					break;
				default: //Unsupported?
					dolog("inboard","Unknown Inboard port 670h command: %02X",value);
					break;
				}
			}
			break;
	}
	return 0; //Unknown port!
}

extern MMU_type MMU;

void initInboard() //Initialize the Inboard chipset, if needed for the current CPU!
{
	uint_32 extendedmemory;
	MMU.maxsize = 0; //Default: no limit!
	MoveLowMemoryHigh = 1; //Default: enable the HMA memory and enable the memory hole and BIOS ROM!
	//Add any Inboard support!
	if ((EMULATED_CPU==CPU_80386) && is_XT) //XT 386? We're an Inboard 386!
	{
		MoveLowMemoryHigh = 0; //Default: disable the HMA memory and enable the memory hole and BIOS ROM!
		if (MMU.size>=0xA0000) //1MB+ detected?
		{
			/*
			extendedmemory = (MMU.size-0xA0000); //The amount of extended memory!
			MMU.maxsize = 0xA0000+(extendedmemory&0xFFF00000); //Only take extended memory in chunks of 1MB!
			*/
			MMU.maxsize = ((MMU.size+(0x100000-0xA0000))&0xFFF00000)-(0x100000-0xA0000); //Round memory down to 1MB chunks!
		}
		register_PORTOUT(&Inboard_writeIO);
		register_PORTIN(&Inboard_readIO);
	}
}