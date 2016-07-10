#ifndef JOYSTICK_H
#define JOYSTICK_H

//Some easy defines!
#define JOYSTICK_DISABLED 0
#define JOYSTICK_ENABLED 1
#define JOYSTICK_ENABLED_BUTTONSONLY 2
#define JOYSTICK_ENABLED_BUTTONS_AND_XONLY 3

#define MODEL_NONE 0
#define MODEL_LOGITECH_WINGMAN_EXTREME_DIGITAL 1

//Functionality itself!
void joystickInit();
void joystickDone();

void enableJoystick(byte joystick, byte enabled); //Enable a joystick for usage!
void setJoystickModel(byte model); //Set the model to use!
void setJoystick(byte joystick, byte button1, byte button2, sword analog_x, sword analog_y);
void setJoystick_other(byte button1, byte button2, byte button3, byte button4, byte button5, byte button6, byte hatleft, byte hatright, byte hatup, byte hatdown, sword analog_x, sword analog_y, sword analog2_x, sword analog2_y); //Input from theuser!
void updateJoystick(double timepassed);

#endif