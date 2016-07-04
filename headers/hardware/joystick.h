#ifndef JOYSTICK_H
#define JOYSTICK_H

void joystickInit();
void enableJoystick(byte joystick, byte enabled); //Enable a joystick for usage!
void setJoystick(byte joystick, byte button1, byte button2, sword analog_x, sword analog_y);
void updateJoystick(double timepassed);

#endif