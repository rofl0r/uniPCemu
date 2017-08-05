#include "headers/hardware/ps2_keyboard.h" //Basic types!

//Keyboard repeat rate table from http://www.computer-engineering.org/ps2keyboard/
float kbd_repeat_rate[0x20] = { //Rate in keys per second when trashing!
	30.0f, 26.7f, 24.0f, 21.8f, 20.7f, 18.5f, 17.1f, 16.0f,
	15.0f, 13.3f, 12.0f, 10.9f, 10.0f, 9.2f ,  8.6f,  8.0f,
	7.5f , 6.7f , 6.0f , 5.5f , 5.0f , 4.6f , 4.3f , 4.0f ,
	3.7f , 3.3f , 3.0f , 2.7f , 2.5f , 2.3f , 2.1f , 2.0f
    };

word kbd_repeat_delay[0x4] = { //Time before we start trashing!
	250, 500, 750, 1000
	};

KEYBOARDENTRY scancodesets[3][104] = { //All scan codes for all sets!
	{ //Set 1!
	{{0x1E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x9E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //A
	{{0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xB0,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //B
	{{0x2E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xAE,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //C
	{{0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xA0,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1},//D
	{{0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x92,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //E
	{{0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xA1,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F
	{{0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xA2,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //G
	{{0x23,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xA3,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //H
	{{0x17,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x97,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //I
	{{0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xA4,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //J
	{{0x25,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xA5,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //K
	{{0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xA6,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //L
	{{0x32,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xB2,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //M
	{{0x31,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xB1,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //N
	{{0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x98,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //O
	{{0x19,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x99,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //P
	{{0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x90,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //Q
	{{0x13,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x93,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //R
	{{0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x9F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //S
	{{0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x94,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //T
	{{0x16,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x96,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //U
	{{0x2F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xAF,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //V
	{{0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x91,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //W
	{{0x2D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xAD,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //X
	{{0x15,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x95,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //Y
	{{0x2C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xAC,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //Z

	{{0x0B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x8B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //0
	{{0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x82,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //1
	{{0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x83,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //2
	{{0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x84,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //3
	{{0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x85,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //4
	{{0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x86,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //5
	{{0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x87,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //6
	{{0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //7
	{{0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x89,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //8
	{{0x0A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x8A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //9
	
	//Column 2! (above '9' included)
	{{0x29,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x89,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //`
	{{0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x8C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //-
	{{0x0D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x8D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //=
	{{0x2B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xAB,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //'\'

	{{0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x8E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //BKSP
	{{0x39,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xB9,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //SPACE
	{{0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x8F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //TAB
	{{0x3A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xBA,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //CAPS

	{{0x2A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xAA,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //L SHFT
	{{0x1D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x9D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //L CTRL
	{{0xE0,0x5B,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xDB,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L GUI
	{{0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xB8,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //L ALT
	{{0x36,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xB6,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //R SHFT
	{{0xE0,0x1D,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0x9D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R CTRL
	{{0xE0,0x5C,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xDC,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R GUI
	{{0xE0,0x38,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xB8,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R ALT

	{{0xE0,0x5D,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xDD,0x00,0x00,0x00,0x00,0x00,0x00},2}, //APPS
	{{0x1C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x9C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //ENTER
	{{0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x81,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //ESC

	{{0x3B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xBB,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F1
	{{0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xBC,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F2
	{{0x3D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xBD,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F3
	{{0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xBE,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F4
	{{0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xBF,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F5
	{{0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F6
	{{0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xC1,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F7
	{{0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xC2,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F8
	{{0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xC3,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F9
	{{0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xC4,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F10
	{{0x57,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xD7,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F11
	{{0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xD8,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //F12

	{{0xE0,0x2A,0xE0,0x37,0x00,0x00,0x00,0x00},4,{0xE0,0xB7,0xE0,0xAA,0x00,0x00,0x00,0x00},4}, //PRNT SCRN

	{{0x46,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //SCROLL
	{{0xE1,0x1D,0x45,0xE1,0x9D,0xC5,0x00,0x00},1,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},0}, //PAUSE

	//Column 3!
	{{0x1A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x9A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //[

	{{0xE0,0x52,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xD2,0x00,0x00,0x00,0x00,0x00,0x00},2}, //INSERT
	{{0xE0,0x47,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0x97,0x00,0x00,0x00,0x00,0x00,0x00},2}, //HOME
	{{0xE0,0x49,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xC9,0x00,0x00,0x00,0x00,0x00,0x00},2}, //PG UP
	{{0xE0,0x53,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xD3,0x00,0x00,0x00,0x00,0x00,0x00},2}, //DELETE
	{{0xE0,0x4F,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xCF,0x00,0x00,0x00,0x00,0x00,0x00},2}, //END
	{{0xE0,0x51,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xD1,0x00,0x00,0x00,0x00,0x00,0x00},2}, //PG DN
	{{0xE0,0x48,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xC8,0x00,0x00,0x00,0x00,0x00,0x00},2}, //U ARROW
	{{0xE0,0x4B,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xCB,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L ARROW
	{{0xE0,0x50,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xD0,0x00,0x00,0x00,0x00,0x00,0x00},2}, //D ARROW
	{{0xE0,0x4D,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xCD,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R ARROW

	{{0x45,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xC5,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //NUM
	{{0xE0,0x35,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xB5,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP /
	{{0x37,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xB7,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP *
	{{0x4A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xCA,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP -
	{{0x4E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xCE,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP +
	{{0xE0,0x1C,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0x9C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP EN
	{{0x53,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xD3,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP .

	{{0x52,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xD2,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP 0
	{{0x4F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xCF,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP 1
	{{0x50,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xD0,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP 2
	{{0x51,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xD1,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP 3
	{{0x4B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP 4
	{{0x4C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP 5
	{{0x4D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xCD,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP 6
	{{0x47,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xC7,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP 7
	{{0x48,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xC8,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP 8
	{{0x49,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xC9,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //KP 9

	{{0x1B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0x9B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //]
	{{0x27,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xA7,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //;
	{{0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xA8,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //'
	{{0x33,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xB3,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //,
	{{0x34,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xB4,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}, //.
	{{0x35,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xB5,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1}  ///
	}, //End of set 1!

	{ //Set 2!
	{{0x1C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x1C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //A
	{{0x32,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x32,0x00,0x00,0x00,0x00,0x00,0x00},2}, //B
	{{0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x21,0x00,0x00,0x00,0x00,0x00,0x00},2}, //C
	{{0x23,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x23,0x00,0x00,0x00,0x00,0x00,0x00},2},//D
	{{0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x24,0x00,0x00,0x00,0x00,0x00,0x00},2}, //E
	{{0x2B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x2B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F
	{{0x34,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x34,0x00,0x00,0x00,0x00,0x00,0x00},2}, //G
	{{0x33,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x33,0x00,0x00,0x00,0x00,0x00,0x00},2}, //H
	{{0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x43,0x00,0x00,0x00,0x00,0x00,0x00},2}, //I
	{{0x3B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //J
	{{0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x42,0x00,0x00,0x00,0x00,0x00,0x00},2}, //K
	{{0x4B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L
	{{0x3A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x3A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //M
	{{0x31,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x31,0x00,0x00,0x00,0x00,0x00,0x00},2}, //N
	{{0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x44,0x00,0x00,0x00,0x00,0x00,0x00},2}, //O
	{{0x4D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //P
	{{0x15,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x15,0x00,0x00,0x00,0x00,0x00,0x00},2}, //Q
	{{0x2D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x2D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R
	{{0x1B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x1B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //S
	{{0x2C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x2C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //T
	{{0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x3C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //U
	{{0x2A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x2A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //V
	{{0x1D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x1D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //W
	{{0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x22,0x00,0x00,0x00,0x00,0x00,0x00},2}, //X
	{{0x35,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x35,0x00,0x00,0x00,0x00,0x00,0x00},2}, //Y
	{{0x1A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x1A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //Z

	{{0x45,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x45,0x00,0x00,0x00,0x00,0x00,0x00},2}, //0
	{{0x16,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x16,0x00,0x00,0x00,0x00,0x00,0x00},2}, //1
	{{0x1E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x1E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //2
	{{0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x26,0x00,0x00,0x00,0x00,0x00,0x00},2}, //3
	{{0x25,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x25,0x00,0x00,0x00,0x00,0x00,0x00},2}, //4
	{{0x2E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x2E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //5
	{{0x36,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x36,0x00,0x00,0x00,0x00,0x00,0x00},2}, //6
	{{0x3D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x3D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //7
	{{0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x3E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //8
	{{0x46,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x46,0x00,0x00,0x00,0x00,0x00,0x00},2}, //9
	
	//Column 2! (above '9' included)
	{{0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x0E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //`
	{{0x4E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //-
	{{0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x55,0x00,0x00,0x00,0x00,0x00,0x00},2}, //=
	{{0x5D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x5D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //'\'

	{{0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x66,0x00,0x00,0x00,0x00,0x00,0x00},2}, //BKSP
	{{0x29,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x29,0x00,0x00,0x00,0x00,0x00,0x00},2}, //SPACE
	{{0x0D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x0D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //TAB
	{{0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x58,0x00,0x00,0x00,0x00,0x00,0x00},2}, //CAPS

	{{0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x12,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L SHFT
	{{0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x14,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L CTRL
	{{0xE0,0x1F,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x1F,0x00,0x00,0x00,0x00,0x00},3}, //L GUI
	{{0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x11,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L ALT
	{{0x59,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x59,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R SHFT
	{{0xE0,0x14,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x14,0x00,0x00,0x00,0x00,0x00},3}, //R CTRL
	{{0xE0,0x27,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x27,0x00,0x00,0x00,0x00,0x00},3}, //R GUI
	{{0xE0,0x11,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x11,0x00,0x00,0x00,0x00,0x00},3}, //R ALT

	{{0xE0,0x2F,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x2F,0x00,0x00,0x00,0x00,0x00},3}, //APPS
	{{0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x5A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //ENTER
	{{0x76,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x76,0x00,0x00,0x00,0x00,0x00,0x00},2}, //ESC

	{{0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x05,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F1
	{{0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x06,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F2
	{{0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x04,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F3
	{{0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x0C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F4
	{{0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x03,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F5
	{{0x0B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x0B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F6
	{{0x83,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x83,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F7
	{{0x0A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x0A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F8
	{{0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x01,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F9
	{{0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x09,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F10
	{{0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x78,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F11
	{{0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x07,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F12

	{{0xE0,0x12,0xE0,0x7C,0x00,0x00,0x00,0x00},4,{0xE0,0xF0,0x7C,0xE0,0xF0,0x12,0x00,0x00},6}, //PRNT SCRN

	{{0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x7E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //SCROLL
	{{0xE1,0x14,0x77,0xE1,0xF0,0x14,0xF0,0x77},8,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},0}, //PAUSE

	//Column 3!
	{{0x54,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x54,0x00,0x00,0x00,0x00,0x00,0x00},2}, //[

	{{0xE0,0x70,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x70,0x00,0x00,0x00,0x00,0x00},3}, //INSERT
	{{0xE0,0x6C,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x6C,0x00,0x00,0x00,0x00,0x00},3}, //HOME
	{{0xE0,0x7D,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x7D,0x00,0x00,0x00,0x00,0x00},3}, //PG UP
	{{0xE0,0x71,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x71,0x00,0x00,0x00,0x00,0x00},3}, //DELETE
	{{0xE0,0x69,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x69,0x00,0x00,0x00,0x00,0x00},3}, //END
	{{0xE0,0x7A,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x7A,0x00,0x00,0x00,0x00,0x00},3}, //PG DN
	{{0xE0,0x75,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x75,0x00,0x00,0x00,0x00,0x00},3}, //U ARROW
	{{0xE0,0x6B,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x6B,0x00,0x00,0x00,0x00,0x00},3}, //L ARROW
	{{0xE0,0x72,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x72,0x00,0x00,0x00,0x00,0x00},3}, //D ARROW
	{{0xE0,0x74,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x74,0x00,0x00,0x00,0x00,0x00},3}, //R ARROW

	{{0x77,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x77,0x00,0x00,0x00,0x00,0x00,0x00},2}, //NUM
	{{0xE0,0x4A,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x4A,0x00,0x00,0x00,0x00,0x00},3}, //KP /
	{{0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x7C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP *
	{{0x7B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x7B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP -
	{{0x79,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x79,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP +
	{{0xE0,0x5A,0x00,0x00,0x00,0x00,0x00,0x00},2,{0xE0,0xF0,0x5A,0x00,0x00,0x00,0x00,0x00},3}, //KP EN
	{{0x71,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x71,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP .

	{{0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x70,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 0
	{{0x69,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x69,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 1
	{{0x72,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x72,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 2
	{{0x7A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x7A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 3
	{{0x6B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x6B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 4
	{{0x73,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x73,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 5
	{{0x74,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x74,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 6
	{{0x6C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x6C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 7
	{{0x75,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x75,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 8
	{{0x7D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x7D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 9

	{{0x5B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x5B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //]
	{{0x4C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //;
	{{0x52,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x52,0x00,0x00,0x00,0x00,0x00,0x00},2}, //'
	{{0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x41,0x00,0x00,0x00,0x00,0x00,0x00},2}, //,
	{{0x49,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x49,0x00,0x00,0x00,0x00,0x00,0x00},2}, //.
	{{0x4A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4A,0x00,0x00,0x00,0x00,0x00,0x00},2}  ///
	}, //End of set 2!

	{ //Set 3!
	{{0x1C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x1C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //A
	{{0x32,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x32,0x00,0x00,0x00,0x00,0x00,0x00},2}, //B
	{{0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x21,0x00,0x00,0x00,0x00,0x00,0x00},2}, //C
	{{0x23,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x23,0x00,0x00,0x00,0x00,0x00,0x00},2},//D
	{{0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x24,0x00,0x00,0x00,0x00,0x00,0x00},2}, //E
	{{0x2B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x2B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F
	{{0x34,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x34,0x00,0x00,0x00,0x00,0x00,0x00},2}, //G
	{{0x33,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x33,0x00,0x00,0x00,0x00,0x00,0x00},2}, //H
	{{0x43,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x43,0x00,0x00,0x00,0x00,0x00,0x00},2}, //I
	{{0x3B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x3B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //J
	{{0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x42,0x00,0x00,0x00,0x00,0x00,0x00},2}, //K
	{{0x4B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L
	{{0x3A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x3A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //M
	{{0x31,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x31,0x00,0x00,0x00,0x00,0x00,0x00},2}, //N
	{{0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x44,0x00,0x00,0x00,0x00,0x00,0x00},2}, //O
	{{0x4D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //P
	{{0x15,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x15,0x00,0x00,0x00,0x00,0x00,0x00},2}, //Q
	{{0x2D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x2D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R
	{{0x1B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x1B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //S
	{{0x2C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x2C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //T
	{{0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x3C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //U
	{{0x2A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x2A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //V
	{{0x1D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x1D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //W
	{{0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x22,0x00,0x00,0x00,0x00,0x00,0x00},2}, //X
	{{0x35,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x35,0x00,0x00,0x00,0x00,0x00,0x00},2}, //Y
	{{0x1A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x1A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //Z

	{{0x45,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x45,0x00,0x00,0x00,0x00,0x00,0x00},2}, //0
	{{0x16,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x16,0x00,0x00,0x00,0x00,0x00,0x00},2}, //1
	{{0x1E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x1E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //2
	{{0x26,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x26,0x00,0x00,0x00,0x00,0x00,0x00},2}, //3
	{{0x25,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x25,0x00,0x00,0x00,0x00,0x00,0x00},2}, //4
	{{0x2E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x2E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //5
	{{0x36,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x36,0x00,0x00,0x00,0x00,0x00,0x00},2}, //6
	{{0x3D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x3D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //7
	{{0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x3E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //8
	{{0x46,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x46,0x00,0x00,0x00,0x00,0x00,0x00},2}, //9
	
	//Column 2! (above '9' included)
	{{0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x0E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //`
	{{0x4E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //-
	{{0x55,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x55,0x00,0x00,0x00,0x00,0x00,0x00},2}, //=
	{{0x5C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x5C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //'\'

	{{0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x66,0x00,0x00,0x00,0x00,0x00,0x00},2}, //BKSP
	{{0x29,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x29,0x00,0x00,0x00,0x00,0x00,0x00},2}, //SPACE
	{{0x0D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x0D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //TAB
	{{0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x14,0x00,0x00,0x00,0x00,0x00,0x00},2}, //CAPS

	{{0x12,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x12,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L SHFT
	{{0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x11,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L CTRL
	{{0x8B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x8B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L WIN
	{{0x19,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x19,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L ALT
	{{0x59,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x59,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R SHFT
	{{0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x58,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R CTRL
	{{0x8C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x8C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R WIN
	{{0x39,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x39,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R ALT

	{{0x8D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x8D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //APPS
	{{0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x5A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //ENTER
	{{0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x08,0x00,0x00,0x00,0x00,0x00,0x00},2}, //ESC

	{{0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x07,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F1
	{{0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x0F,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F2
	{{0x17,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x17,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F3
	{{0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x1F,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F4
	{{0x27,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x27,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F5
	{{0x2F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x2F,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F6
	{{0x37,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x37,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F7
	{{0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x3F,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F8
	{{0x47,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x47,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F9
	{{0x4F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4F,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F10
	{{0x56,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x56,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F11
	{{0x5E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x5E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //F12

	{{0x57,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x57,0x00,0x00,0x00,0x00,0x00,0x00},6}, //PRNT SCRN

	{{0x5F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x5F,0x00,0x00,0x00,0x00,0x00,0x00},2}, //SCROLL
	{{0x62,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x62,0x00,0x00,0x00,0x00,0x00,0x00},0}, //PAUSE

	//Column 3!
	{{0x54,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x54,0x00,0x00,0x00,0x00,0x00,0x00},2}, //[

	{{0x67,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x67,0x00,0x00,0x00,0x00,0x00,0x00},2}, //INSERT
	{{0x6E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x6E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //HOME
	{{0x6F,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x6F,0x00,0x00,0x00,0x00,0x00,0x00},2}, //PG UP
	{{0x64,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x64,0x00,0x00,0x00,0x00,0x00,0x00},2}, //DELETE
	{{0x65,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x65,0x00,0x00,0x00,0x00,0x00,0x00},2}, //END
	{{0x6D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x6D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //PG DN
	{{0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x63,0x00,0x00,0x00,0x00,0x00,0x00},2}, //U ARROW
	{{0x61,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x61,0x00,0x00,0x00,0x00,0x00,0x00},2}, //L ARROW
	{{0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x60,0x00,0x00,0x00,0x00,0x00,0x00},2}, //D ARROW
	{{0x6A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x6A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //R ARROW

	{{0x76,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x76,0x00,0x00,0x00,0x00,0x00,0x00},2}, //NUM
	{{0x4A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP /
	{{0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x7E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP *
	{{0x4E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4E,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP -
	{{0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x7C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP +
	{{0x79,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x79,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP EN
	{{0x71,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x71,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP .

	{{0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x70,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 0
	{{0x69,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x69,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 1
	{{0x72,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x72,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 2
	{{0x7A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x7A,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 3
	{{0x6B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x6B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 4
	{{0x73,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x73,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 5
	{{0x74,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x74,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 6
	{{0x6C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x6C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 7
	{{0x75,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x75,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 8
	{{0x7D,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x7D,0x00,0x00,0x00,0x00,0x00,0x00},2}, //KP 9

	{{0x5B,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x5B,0x00,0x00,0x00,0x00,0x00,0x00},2}, //]
	{{0x4C,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4C,0x00,0x00,0x00,0x00,0x00,0x00},2}, //;
	{{0x52,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x52,0x00,0x00,0x00,0x00,0x00,0x00},2}, //'
	{{0x41,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x41,0x00,0x00,0x00,0x00,0x00,0x00},2}, //,
	{{0x49,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x49,0x00,0x00,0x00,0x00,0x00,0x00},2}, //.
	{{0x4A,0x00,0x00,0x00,0x00,0x00,0x00,0x00},1,{0xF0,0x4A,0x00,0x00,0x00,0x00,0x00,0x00},2}  ///
	} //End of set 3!
	};

char keys_names[104][11] = { //All names of the above keys (for textual representation/labeling)
	//column 1
	"a", //A
	"b", //B
	"c", //C
	"d",//D
	"e", //E
	"f", //F
	"g", //G
	"h", //H
	"i", //I
	"j", //J
	"k", //K
	"l", //L
	"m", //M
	"n", //N
	"o", //O
	"p", //P
	"q", //Q
	"r", //R
	"s", //S
	"t", //T
	"u", //U
	"v", //V
	"w", //W
	"x", //X
	"y", //Y
	"z", //Z

	"0", //0
	"1", //1
	"2", //2
	"3", //3
	"4", //4
	"5", //5
	"6", //6
	"7", //7
	"8", //8
	"9", //9
	
	//Column 2! (above '9' included)
	"`", //`
	"-", //-
	"=", //=
	"\\", //'\'

	"bksp", //BKSP
	"space", //SPACE
	"tab", //TAB
	"capslock", //CAPS

	"lshift", //L SHFT
	"lctrl", //L CTRL
	"lwin", //L WIN
	"lalt", //L ALT
	"rshift", //R SHFT
	"rctrl", //R CTRL
	"rwin", //R WIN
	"ralt", //R ALT

	"apps", //APPS
	"enter", //ENTER
	"esc", //ESC

	"f1", //F1
	"f2", //F2
	"f3", //F3
	"f4", //F4
	"f5", //F5
	"f6", //F6
	"f7", //F7
	"f8", //F8
	"f9", //F9
	"f10", //F10
	"f11", //F11
	"f12", //F12

	"prtsc", //PRNT SCRN

	"scroll", //SCROLL
	"pause", //PAUSE

	//Column 3!
	"[", //[

	"insert", //INSERT
	"home", //HOME
	"pgup", //PG UP
	"del", //DELETE
	"end", //END
	"pgdn", //PG DN
	"up", //U ARROW
	"left", //L ARROW
	"down", //D ARROW
	"right", //R ARROW

	"num", //NUM
	"kp/", //KP /
	"kp*", //KP *
	"kp-", //KP -
	"kp+", //KP +
	"kpen", //KP EN
	"kp.", //KP .

	"kp0", //KP 0
	"kp1", //KP 1
	"kp2", //KP 2
	"kp3", //KP 3
	"kp4", //KP 4
	"kp5", //KP 5
	"kp6", //KP 6
	"kp7", //KP 7
	"kp8", //KP 8
	"kp9", //KP 9

	"]", //]
	";", //;
	"'", //'
	",", //,
	".", //.
	"/"  ///
};