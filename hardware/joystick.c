#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //Port I/O support!

//Time until we time out!
//R=(t-24.2)/0.011, where t is in microseconds(us). Our timing is in nanoseconds(1000us). r=0-100. R(max)=2200 ohm.
//Thus, t(microseconds)=(R(ohms)*0.011)+24.2 microseconds.
#define OHMS (120000.0/2.0)
#define POS2OHM(position) ((((double)(position+1))/65535.0)*OHMS)
#define CALCTIMEOUT(position) (((24.2+(POS2OHM(position)*0.011)))*1000.0)

struct
{
	byte enabled[2]; //Is this joystick enabled for emulation?
	byte buttons[2]; //Two button status for two joysticks!
	sword Joystick_X[2]; //X location for two joysticks!
	sword Joystick_Y[2]; //Y location for two joysticks!
	double timeoutx[2]; //Keep line high while set(based on Joystick_X when triggered)
	double timeouty[2]; //Keep line high while set(based on Joystick_Y when triggered)
	byte timeout; //Timeout status of the four data lines(logic 0/1)! 1 for timed out(default state), 0 when timing!
} JOYSTICK;

void enableJoystick(byte joystick, byte enabled)
{
	if (joystick&0xFE) return; //Only two joysticks are supported!
	JOYSTICK.enabled[joystick] = enabled?1:0; //Enable this joystick?
}

void setJoystick(byte joystick, byte button1, byte button2, sword analog_x, sword analog_y) //Input from theuser!
{
	if (joystick&0xFE) return; //Only two joysticks are supported!
	//Set the buttons of the joystick!
	JOYSTICK.buttons[joystick] = (button2?0x0:0x2)|(button1?0x0:0x1); //Button 1&2, not pressed!
	JOYSTICK.Joystick_X[joystick] = analog_x; //Joystick x axis!
	JOYSTICK.Joystick_Y[joystick] = analog_y; //Joystick y axis!
}

void updateJoystick(double timepassed)
{
	//Joystick timer!
	if (JOYSTICK.timeout) //Timing?
	{
		if (JOYSTICK.enabled[0]) //Joystick A enabled?
		{
			if (JOYSTICK.timeout&1) //AX timing?
			{
				JOYSTICK.timeoutx[0] -= timepassed; //Add the time to what's left!
				if (JOYSTICK.timeoutx[0]<=0.0) //Finished timing?
				{
					JOYSTICK.timeout &= ~1; //Finished timing, go logic 1(digital 0)!
				}
			}
			if (JOYSTICK.timeout&2) //AY timing?
			{
				JOYSTICK.timeouty[0] -= timepassed; //Add the time to what's left!
				if (JOYSTICK.timeouty[0]<=0.0) //Finished timing?
				{
					JOYSTICK.timeout &= ~2; //Finished timing, go logic 1(digital 0)!
				}
			}
		}
		if (JOYSTICK.enabled[1]) //Joystick B enabled?
		{
			if (JOYSTICK.timeout&4) //BX timing?
			{
				JOYSTICK.timeoutx[1] -= timepassed; //Add the time to what's left!
				if (JOYSTICK.timeoutx[1]<=0.0) //Finished timing?
				{
					JOYSTICK.timeout &= ~4; //Finished timing, go logic 1(digital 0)!
				}
			}
			if (JOYSTICK.timeout&8) //BY timing?
			{
				JOYSTICK.timeouty[1] -= timepassed; //Add the time to what's left!
				if (JOYSTICK.timeouty[1]<=0.0) //Finished timing?
				{
					JOYSTICK.timeout &= ~8; //Finished timing, go logic 1(digital 0)!
				}
			}
		}
	}
}

byte joystick_readIO(word port, byte *result)
{
	INLINEREGISTER byte temp,temp2;
	switch (port)
	{
		case 0x201: //Read joystick position and status?
			//bits 8-7 are joystick B buttons 2/1, Bits 6-5 are joystick A buttons 2/1, bit 4-3 are joystick B Y-X timeout timing, bits 1-0 are joystick A Y-X timeout timing.
			temp = 0xFF; //Init the result!
			if (JOYSTICK.enabled[1]) //Joystick B enabled?
			{
				temp2 = (((JOYSTICK.buttons[1]<<6)|JOYSTICK.timeout)&0xCC); //Our bits to set!
				temp &= 0x33; //Not pressed joystick B or timing status?
				temp |= temp2; //Apply the joystick!
			}
			if (JOYSTICK.enabled[0]) //Joystick A enabled?
			{
				temp2 = (((JOYSTICK.buttons[0]<<4)|JOYSTICK.timeout)&0x33); //Our bits to set!
				temp &= 0xCC; //Not pressed joystick A or timing status?
				temp |= temp2; //Apply the joystick!
			}
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
			if (JOYSTICK.enabled[1]) //Joystick B enabled?
			{
				JOYSTICK.timeoutx[1] = CALCTIMEOUT((int_32)JOYSTICK.Joystick_X[1]-SHRT_MIN);
				JOYSTICK.timeouty[1] = CALCTIMEOUT((int_32)JOYSTICK.Joystick_Y[1]-SHRT_MIN);
			}
			//Second joystick timeout!
			if (JOYSTICK.enabled[0]) //Joystick A enabled?
			{
				JOYSTICK.timeoutx[0] = CALCTIMEOUT((int_32)JOYSTICK.Joystick_X[0]-SHRT_MIN);
				JOYSTICK.timeouty[0] = CALCTIMEOUT((int_32)JOYSTICK.Joystick_Y[0]-SHRT_MIN);
			}
			JOYSTICK.timeout = 0xF; //Start the timeout on all channels, regardless if they're enabled. Multivibrator output goes to logic 0.
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
	JOYSTICK.timeoutx[0] = JOYSTICK.timeouty[0] = JOYSTICK.timeoutx[1] = JOYSTICK.timeouty[1] = 0.0; //Init timeout to nothing!
	JOYSTICK.timeout = 0x0; //Default: all lines timed out!
}
