#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //Port I/O support!

//A bit of length for the timeout to decrease 1 step out of 65536 steps, in nanoseconds! Use 0.5ms for full decay(1.5ms for full fetch in 3 steps)?
//according to http://www.epanorama.net/documents/joystick/pc_joystick.html
//Rewritten to a different interval based on Duke Nukem(about 0.28 seconds per input).
#define JOYSTICK_TICKLENGTH (5000000.0/USHRT_MAX)

struct
{
	byte buttons[2][2]; //Two button status for two joysticks!
	sword Joystick_X[2]; //X location for two joysticks!
	sword Joystick_Y[2]; //Y location for two joysticks!
	uint_32 timeoutx[2]; //Keep line high while set(based on Joystick_X when triggered)
	uint_32 timeouty[2]; //Keep line high while set(based on Joystick_Y when triggered)
} JOYSTICK;

void setJoystick(byte joystick, byte button1, byte button2, sword analog_x, sword analog_y) //Input from theuser!
{
	if (joystick&0xFE) return; //Only two joysticks are supported!
	//Set the buttons of the joystick!
	JOYSTICK.buttons[joystick][0] = button1?1:0; //Button 1
	JOYSTICK.buttons[joystick][1] = button2?1:0; //Button 2
	JOYSTICK.Joystick_X[joystick] = analog_x; //Joystick x axis!
	JOYSTICK.Joystick_Y[joystick] = analog_y; //Joystick y axis!
}

double joystick_ticktiming = 0.0;
void updateJoystick(double timepassed)
{
	//Adlib timer!
	joystick_ticktiming += timepassed; //Get the amount of time passed!
	if (joystick_ticktiming >= JOYSTICK_TICKLENGTH) //Enough time passed?
	{
		for (;joystick_ticktiming >= JOYSTICK_TICKLENGTH;) //All that's left!
		{
			if (JOYSTICK.timeoutx[0]) --JOYSTICK.timeoutx[0];
			if (JOYSTICK.timeouty[0]) --JOYSTICK.timeouty[0];
			if (JOYSTICK.timeoutx[1]) --JOYSTICK.timeoutx[1];
			if (JOYSTICK.timeouty[1]) --JOYSTICK.timeouty[1];
			joystick_ticktiming -= JOYSTICK_TICKLENGTH; //Decrease timer to get time left!
		}
	}
}

byte joystick_readIO(word port, byte *result)
{
	INLINEREGISTER byte temp;
	switch (port)
	{
		case 0x201: //Read joystick position and status?
			temp = 0; //No input yet!
			//bits 8-7 are joystick B buttons 2/1, Bits 6-5 are joystick A buttons 2/1, bit 4-3 are joystick B Y-X timeout timing, bits 1-0 are joystick A Y-X timeout timing.
			temp |= JOYSTICK.buttons[1][1]?0:0x80; //Not pressed?
			temp |= JOYSTICK.buttons[1][0]?0:0x40; //Not pressed?
			temp |= JOYSTICK.buttons[0][1]?0:0x20; //Not pressed?
			temp |= JOYSTICK.buttons[0][0]?0:0x10; //Not pressed?
			temp |= JOYSTICK.timeouty[1]?0x08:0; //Timing?
			temp |= JOYSTICK.timeoutx[1]?0x04:0; //Timing?
			temp |= JOYSTICK.timeouty[0]?0x02:0; //Timing?
			temp |= JOYSTICK.timeoutx[0]?0x02:0; //Timing?
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
			JOYSTICK.timeoutx[1] = (uint_32)(SHRT_MIN+JOYSTICK.Joystick_X[1]);
			JOYSTICK.timeoutx[0] = (uint_32)(SHRT_MIN+JOYSTICK.Joystick_X[0]);
			JOYSTICK.timeouty[1] = (uint_32)(SHRT_MIN+JOYSTICK.Joystick_Y[1]);
			JOYSTICK.timeouty[0] = (uint_32)(SHRT_MIN+JOYSTICK.Joystick_Y[0]);
			if (JOYSTICK.Joystick_X[0] != 0)
			{
				dolog("Joystick","timeoutx:%i->%u",JOYSTICK.Joystick_X[0],JOYSTICK.timeoutx[0]);
			}
			if (JOYSTICK.Joystick_Y[0] != 0)
			{
				dolog("Joystick", "timeouty:%i->%u",JOYSTICK.Joystick_Y[0],JOYSTICK.timeouty[0]);
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
}
