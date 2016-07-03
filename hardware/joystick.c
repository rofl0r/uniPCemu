#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //Port I/O support!

//Time until we time out!
#define CALCTIMEOUT(position) ((24.2+(((double)position/65535.0)*1100.0))*1000.0)

struct
{
	byte buttons[2]; //Two button status for two joysticks!
	sword Joystick_X[2]; //X location for two joysticks!
	sword Joystick_Y[2]; //Y location for two joysticks!
	double timeoutx[2]; //Keep line high while set(based on Joystick_X when triggered)
	double timeouty[2]; //Keep line high while set(based on Joystick_Y when triggered)
} JOYSTICK;

void setJoystick(byte joystick, byte button1, byte button2, sword analog_x, sword analog_y) //Input from theuser!
{
	if (joystick&0xFE) return; //Only two joysticks are supported!
	//Set the buttons of the joystick!
	JOYSTICK.buttons[joystick] = (button2?0x0:0x2)|(button1?0x0:0x1); //Button 2, not pressed!
	JOYSTICK.Joystick_X[joystick] = analog_x; //Joystick x axis!
	JOYSTICK.Joystick_Y[joystick] = analog_y; //Joystick y axis!
}

void updateJoystick(double timepassed)
{
	//Joystick timer!
	if (JOYSTICK.timeoutx[0]>0.0) JOYSTICK.timeoutx[0] -= timepassed; //Add the time to what's left!
	if (JOYSTICK.timeouty[0]>0.0) JOYSTICK.timeouty[0] -= timepassed; //Add the time to what's left!
	if (JOYSTICK.timeoutx[1]>0.0) JOYSTICK.timeoutx[1] -= timepassed; //Add the time to what's left!
	if (JOYSTICK.timeouty[1]>0.0) JOYSTICK.timeouty[1] -= timepassed; //Add the time to what's left!
}

byte joystick_readIO(word port, byte *result)
{
	INLINEREGISTER byte temp;
	switch (port)
	{
		case 0x201: //Read joystick position and status?
			temp = 0; //No input yet!
			//bits 8-7 are joystick B buttons 2/1, Bits 6-5 are joystick A buttons 2/1, bit 4-3 are joystick B Y-X timeout timing, bits 1-0 are joystick A Y-X timeout timing.
			temp |= (JOYSTICK.buttons[1]<<6); //Not pressed joystick B?
			temp |= (JOYSTICK.buttons[0]<<4); //Not pressed joystick A?
			temp |= (JOYSTICK.timeouty[1]>0.0)?0x08:0; //Timing?
			temp |= (JOYSTICK.timeoutx[1]>0.0)?0x04:0; //Timing?
			temp |= (JOYSTICK.timeouty[0]>0.0)?0x02:0; //Timing?
			temp |= (JOYSTICK.timeoutx[0]>0.0)?0x01:0; //Timing?
			*result = temp; //Give the result!
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
			//Set timeoutx and timeouty based on the relative status of Joystick_X and Joystick_Y to fully left/top!
			//First joystick timeout!
			if (JOYSTICK.timeoutx[1]<=0.0)
			{
				JOYSTICK.timeoutx[1] = CALCTIMEOUT(JOYSTICK.Joystick_X[1]+(-SHRT_MIN));
			}
			if (JOYSTICK.timeouty[1]<=0.0)
			{
				JOYSTICK.timeouty[1] = CALCTIMEOUT(JOYSTICK.Joystick_Y[1]+(-SHRT_MIN));
			}
			//Second joystick timeout!
			if (JOYSTICK.timeoutx[0] <= 0.0)
			{
				JOYSTICK.timeoutx[0] = CALCTIMEOUT(JOYSTICK.Joystick_X[0]+(-SHRT_MIN));
			}
			if (JOYSTICK.timeouty[0] <= 0.0)
			{
				JOYSTICK.timeouty[0] = CALCTIMEOUT(JOYSTICK.Joystick_Y[0]+(-SHRT_MIN));
			}
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
	JOYSTICK.buttons[0] = JOYSTICK.buttons[1] = 3; //Not pressed!
	JOYSTICK.timeoutx[0] = JOYSTICK.timeouty[0] = JOYSTICK.timeoutx[1] = JOYSTICK.timeouty[1] = 0.0; //Init timeout!
}
