#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //Port I/O support!

byte joystick_readIO(word port, byte *result)
{
	switch (port)
	{
		case 0x201: //Read joystick position and status?
			*result = 0; //Nothing emulated yet!
			return 1; //OK!
		default:
			break;
	}
	return 0; //Not an used port!
}

byte joystick_writeIO(word port, byte value)
{
	switch (port)
	{
		case 0x201: //Fire joystick four one-shots?
			return 1; //OK!
		default:
			break;
	}
	return 0; //Not an used port!
}

void joystickInit()
{
	register_PORTIN(&joystick_readIO); //Register our handler!
	register_PORTOUT(&joystick_writeIO); //Register our handler!
}
