#ifndef JOYSTICK_H
#define JOYSTICK_H

//Some easy defines!
#define JOYSTICK_DISABLED 0
#define JOYSTICK_ENABLED 1
#define JOYSTICK_ENABLED_BUTTONSONLY 2
#define JOYSTICK_ENABLED_BUTTONS_AND_XONLY 3

//Functionality itself!
void joystickInit();
void enableJoystick(byte joystick, byte enabled); //Enable a joystick for usage!
void setJoystick(byte joystick, byte button1, byte button2, sword analog_x, sword analog_y);
void updateJoystick(double timepassed);

#endif