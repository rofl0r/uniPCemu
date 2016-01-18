#include "headers/types.h"
#include "headers/emu/input.h" //Own typedefs.
#include "headers/bios/bios.h" //BIOS Settings for keyboard input!
#include "headers/hardware/vga/vga.h" //VGA for simple 8x8 font!
#include "headers/emu/gpu/gpu.h" //GPU support!
#include "headers/hardware/ps2_keyboard.h" //PS/2 keyboard support!
#include "headers/hardware/ps2_mouse.h" //PS/2 mouse controller support!
#include "headers/emu/threads.h" //Thread management!
#include "headers/support/keyboard.h" //Keyboard support!
#include "headers/support/zalloc.h" //Zero allocation support!

#include "headers/emu/gpu/gpu_emu.h" //GPU emulator color support!
#include "headers/emu/gpu/gpu_sdl.h" //GPU SDL support!
#include "headers/emu/gpu/gpu_text.h"

#include "headers/support/log.h" //Logging support!

#include "headers/emu/timers.h" //Timer support!

#include "headers/support/highrestimer.h" //High resolution timer support!

#include "headers/hardware/sermouse.h" //Serial mouse support!

#include "headers/support/signedness.h" //Signedness support!

#ifdef _WIN32
#include "sdl_joystick.h" //Joystick support!
#include "sdl_events.h" //Event support!
#else
#include <SDL/SDL_joystick.h> //Joystick support!
#include <SDL/SDL_events.h> //Event support!
#endif

int_64 mouse_xmove = 0, mouse_ymove = 0; //Movement of the mouse not processed yet (in mm)!
byte Mouse_buttons = 0; //Currently pressed mouse buttons. 1=Left, 2=Right, 4=Middle.

byte mousebuttons = 0; //Active mouse buttons!

//Use direct input in windows!
byte Direct_Input = 0; //Direct input enabled?

extern byte EMU_RUNNING; //Are we running?

byte keysactive; //Ammount of keys active!

float mouse_interval = 0.0f; //Check never by default (unused timer)!

enum input_button_map { //All buttons we support!
INPUT_BUTTON_TRIANGLE, INPUT_BUTTON_CIRCLE, INPUT_BUTTON_CROSS, INPUT_BUTTON_SQUARE,
INPUT_BUTTON_LTRIGGER, INPUT_BUTTON_RTRIGGER,
INPUT_BUTTON_DOWN, INPUT_BUTTON_LEFT, INPUT_BUTTON_UP, INPUT_BUTTON_RIGHT,
INPUT_BUTTON_SELECT, INPUT_BUTTON_START, INPUT_BUTTON_HOME, INPUT_BUTTON_HOLD };

byte emu_keys_state[104]; //All states for all emulated keys!
int emu_keys_SDL[104] = {
	//column 1
	SDLK_a, //A
	SDLK_b, //B
	SDLK_c, //C
	SDLK_d,//D
	SDLK_e, //E
	SDLK_f, //F
	SDLK_g, //G
	SDLK_h, //H
	SDLK_i, //I
	SDLK_j, //J
	SDLK_k, //K
	SDLK_l, //L
	SDLK_m, //M
	SDLK_n, //N
	SDLK_o, //O
	SDLK_p, //P
	SDLK_q, //Q
	SDLK_r, //R
	SDLK_s, //S
	SDLK_t, //T
	SDLK_u, //U
	SDLK_v, //V
	SDLK_w, //W
	SDLK_x, //X
	SDLK_y, //Y
	SDLK_z, //Z

	SDLK_0, //0
	SDLK_1, //1
	SDLK_2, //2
	SDLK_3, //3
	SDLK_4, //4
	SDLK_5, //5
	SDLK_6, //6
	SDLK_7, //7
	SDLK_8, //8
	SDLK_9, //9

		 //Column 2! (above '9' included)
	SDLK_BACKQUOTE, //`
	SDLK_MINUS, //-
	SDLK_EQUALS, //=
	SDLK_BACKSLASH, //'\'

	SDLK_BACKSPACE, //BKSP
	SDLK_SPACE, //SPACE
	SDLK_TAB, //TAB
	SDLK_CAPSLOCK, //CAPS

	SDLK_LSHIFT, //L SHFT
	SDLK_LCTRL, //L CTRL
	SDLK_LSUPER, //L WIN
	SDLK_LALT, //L ALT
	SDLK_LSHIFT, //R SHFT
	SDLK_RCTRL, //R CTRL
	SDLK_RSUPER, //R WIN
	SDLK_RALT, //R ALT

	SDLK_MENU, //APPS
	SDLK_RETURN, //ENTER
	SDLK_ESCAPE, //ESC

	SDLK_F1, //F1
	SDLK_F2, //F2
	SDLK_F3, //F3
	SDLK_F4, //F4
	SDLK_F5, //F5
	SDLK_F6, //F6
	SDLK_F7, //F7
	SDLK_F8, //F8
	SDLK_F9, //F9
	SDLK_F10, //F10
	SDLK_F11, //F11
	SDLK_F12, //F12

	SDLK_SYSREQ, //PRNT SCRN

	SDLK_SCROLLOCK, //SCROLL
	SDLK_PAUSE, //PAUSE

			 //Column 3!
	SDLK_LEFTBRACKET, //[

	SDLK_INSERT, //INSERT
	SDLK_HOME, //HOME
	SDLK_PAGEUP, //PG UP
	SDLK_DELETE, //DELETE
	SDLK_END, //END
	SDLK_PAGEDOWN, //PG DN
	SDLK_UP, //U ARROW
	SDLK_LEFT, //L ARROW
	SDLK_DOWN, //D ARROW
	SDLK_RIGHT, //R ARROW

	SDLK_NUMLOCK, //NUM
	SDLK_KP_DIVIDE, //KP /
	SDLK_KP_MULTIPLY, //KP *
	SDLK_KP_MINUS, //KP -
	SDLK_KP_PLUS, //KP +
	SDLK_KP_ENTER, //KP EN
	SDLK_KP_PERIOD, //KP .

	SDLK_KP0, //KP 0
	SDLK_KP1, //KP 1
	SDLK_KP2, //KP 2
	SDLK_KP3, //KP 3
	SDLK_KP4, //KP 4
	SDLK_KP5, //KP 5
	SDLK_KP6, //KP 6
	SDLK_KP7, //KP 7
	SDLK_KP8, //KP 8
	SDLK_KP9, //KP 9

	SDLK_RIGHTBRACKET, //]
	SDLK_SEMICOLON, //;
	SDLK_QUOTE, //'
	SDLK_COMMA, //,
	SDLK_PERIOD, //.
	SDLK_SLASH  ///
};

uint_32 emu_keys_sdl_rev_size = 0; //EMU_KEYS_SDL_REV size!
int emu_keys_sdl_rev[UINT16_MAX+1]; //Reverse of emu_keys_sdl!

//Are we disabled?
#define __HW_DISABLED 0

//Time between keyboard set swapping.
#define KEYSWAP_DELAY 100000
//Keyboard enabled?
#define KEYBOARD_ENABLED 1
//Allow input?
#define ALLOW_INPUT 1

//Default mode: 0=Mouse, 1=Keyboard
#define DEFAULT_KEYBOARD 0

extern GPU_type GPU; //The real GPU, for rendering the keyboard!

byte input_enabled = 0; //To show the input dialog?
byte input_buffer_input = 0; //To buffer, instead of straight into emulation (giving the below value the key index)?
int input_buffer_shift = -1; //Ctrl-Shift-Alt Status for the pressed key!
int input_buffer = -1; //To contain the pressed key!

PSP_INPUTSTATE curstat; //Current status!

#define CAS_LCTRL 0x01
#define CAS_RCTRL 0x02
#define CAS_CTRL 0x03
#define CAS_LALT 0x04
#define CAS_RALT 0x08
#define CAS_ALT 0x0C
#define CAS_LSHIFT 0x10
#define CAS_RSHIFT 0x20
#define CAS_SHIFT 0x30

struct
{
	uint_32 Buttons; //Currently pressed buttons!
	sword Lx; //X axis!
	sword Ly; //Y axis!
	byte keyboardjoy_direction; //Keyboard joystick direction (internal use only)
	byte cas; //L&R Ctrl/alt/shift status!
} input;

int psp_inputkey() //Simple key sampling!
{
	return input.Buttons; //Give buttons pressed!
}

int psp_inputkeydelay(uint_32 waittime) //Don't use within any timers! This will hang up itself!
{
	uint_32 counter; //Counter for inputkeydelay!
	int key = psp_inputkey(); //Check for keys!
	if (key) //Key pressed?
	{
		counter = waittime; //Init counter!
		while ((int_64)counter>0) //Still waiting?
		{
			if (counter>(uint_32)INPUTKEYDELAYSTEP) //Big block?
			{
				delay(INPUTKEYDELAYSTEP); //Wait a delay step!
				counter -= INPUTKEYDELAYSTEP; //Substract!
			}
			else
			{
				delay(counter); //Wait rest!
				counter = 0; //Done!
			}
		}
	}
	return key; //Give the key!
}

int psp_readkey() //Wait for keypress and release!
{
	int result;
	result = psp_inputkeydelay(0); //Check for input!
	while (result==0) //No input?
	{
		result = psp_inputkeydelay(0); //Check for input!
	}
	return result; //Give the pressed key!
}

int psp_keypressed(int key) //Key pressed?
{
	return ((psp_inputkey()&key)==key); //Key(s) pressed?
}

extern BIOS_Settings_TYPE BIOS_Settings; //Our BIOS Settings!

void get_analog_state(PSP_INPUTSTATE *state) //Get the current state for mouse/analog driver!
{
	//Clear all we set!
	state->analogdirection_mouse_x = 0; //No mouse X movement!
	state->analogdirection_mouse_y = 0; //No mouse Y movement!
	state->buttonpress = 0; //No buttons pressed!
	state->analogdirection_keyboard_x = 0; //No keyboard X movement!
	state->analogdirection_keyboard_y = 0; //No keyboard Y movement!
	//We preserve the input mode and gaming mode flags, to be able to go back when using gaming mode!

	
	int x; //Analog x!
	int y; //Analog y!
	x = input.Lx; //Convert to signed!
	y = input.Ly; //Convert to signed!
	
	//Now, apply analog_minrange!
	
	if (x>0) //High?
	{
		if (x<BIOS_Settings.input_settings.analog_minrange) //Not enough?
		{
			x = 0; //Nothing!
		}
		else //Patch?
		{
			x -= BIOS_Settings.input_settings.analog_minrange; //Patch!
		}
	}
	else if (x<0) //Low?
	{
		if (x>(0-BIOS_Settings.input_settings.analog_minrange)) //Not enough?
		{
			x = 0; //Nothing!
		}
		else //Patch?
		{
			x += BIOS_Settings.input_settings.analog_minrange; //Patch!
		}
	}

	if (y>0) //High?
	{
		if (y<BIOS_Settings.input_settings.analog_minrange) //Not enough?
		{
			y = 0; //Nothing!
		}
		else //Patch?
		{
			y -= BIOS_Settings.input_settings.analog_minrange; //Patch!
		}
	}
	else if (y<0) //Low?
	{
		if (y>(0-BIOS_Settings.input_settings.analog_minrange)) //Not enough?
		{
			y = 0; //Nothing!
		}
		else //Patch?
		{
			y += BIOS_Settings.input_settings.analog_minrange; //Patch!
		}
	}
	if (state->gamingmode) //Gaming mode?
	{
		state->analogdirection_mouse_x = x; //Mouse X movement!
		state->analogdirection_mouse_y = y; //Mouse Y movement! Only to use mouse movement without analog settings!

		state->analogdirection_keyboard_x = x; //Keyboard X movement!
		state->analogdirection_keyboard_y = y; //Keyboard Y movement! Use this with ANALOG buttons!
		//Patch keyboard to -1&+1
		if (state->analogdirection_keyboard_x>0) //Positive?
		{
			state->analogdirection_keyboard_x = 1; //Positive simple!
		}
		else if (state->analogdirection_keyboard_x<0) //Negative?
		{
			state->analogdirection_keyboard_x = -1; //Negative simple!
		}

		if (state->analogdirection_keyboard_y>0) //Positive?
		{
			state->analogdirection_keyboard_y = 1; //Positive simple!
		}
		else if (state->analogdirection_keyboard_y<0) //Negative?
		{
			state->analogdirection_keyboard_y = -1; //Negative simple!
		}

		if ((input.Buttons&BUTTON_TRIANGLE)>0) //Triangle?
		{
			state->buttonpress |= 2; //Triangle!
		}
		if ((input.Buttons&BUTTON_SQUARE)>0) //Square?
		{
			state->buttonpress |= 1; //Square!
		}
		if ((input.Buttons&BUTTON_CROSS)>0) //Cross?
		{
			state->buttonpress |= 8; //Cross!
		}
		if ((input.Buttons&BUTTON_CIRCLE)>0) //Circle?
		{
			state->buttonpress |= 4; //Circle!
		}

		if ((input.Buttons&BUTTON_LEFT)>0) //Left?
		{
			state->buttonpress |= 16; //Left!
		}

		if ((input.Buttons&BUTTON_UP)>0) //Up?
		{
			state->buttonpress |= 32; //Up!
		}

		if ((input.Buttons&BUTTON_RIGHT)>0) //Right?
		{
			state->buttonpress |= 64; //Right!
		}

		if ((input.Buttons&BUTTON_DOWN)>0) //Down?
		{
			state->buttonpress |= 128; //Down!
		}

		if ((input.Buttons&BUTTON_LTRIGGER)>0) //L?
		{
			state->buttonpress |= 256; //L!
		}

		if ((input.Buttons&BUTTON_RTRIGGER)>0) //R?
		{
			state->buttonpress |= 512; //R!
		}

		if ((input.Buttons&BUTTON_START)>0) //START?
		{
			state->buttonpress |= 1024; //START!
		}		
	}
	else //Normal mode?
	{
		switch (state->mode) //Which input mode?
		{
		case 0: //Mouse?
			state->analogdirection_mouse_x = x; //Mouse X movement!
			state->analogdirection_mouse_y = y; //Mouse Y movement!
			//The face buttons are OR-ed!
			if ((input.Buttons&BUTTON_TRIANGLE)>0) //Triangle?
			{
				state->buttonpress |= 2; //Triangle!
			}
			else if ((input.Buttons&BUTTON_SQUARE)>0) //Square?
			{
				state->buttonpress |= 1; //Square!
			}
			else if ((input.Buttons&BUTTON_CROSS)>0) //Cross?
			{
				state->buttonpress |= 8; //Cross!
			}
			else if ((input.Buttons&BUTTON_CIRCLE)>0) //Circle?
			{
				state->buttonpress |= 4; //Circle!
			}
	
			if ((input.Buttons&BUTTON_LEFT)>0) //Left?
			{
				state->buttonpress |= 16; //Left!
			}
	
			if ((input.Buttons&BUTTON_UP)>0) //Up?
			{
				state->buttonpress |= 32; //Up!
			}
	
			if ((input.Buttons&BUTTON_RIGHT)>0) //Right?
			{
				state->buttonpress |= 64; //Right!
			}
	
			if ((input.Buttons&BUTTON_DOWN)>0) //Down?
			{
				state->buttonpress |= 128; //Down!
			}
			
			if ((input.Buttons&BUTTON_LTRIGGER)>0) //L?
			{
				state->buttonpress |= 256; //L!
			}
	
			if ((input.Buttons&BUTTON_RTRIGGER)>0) //R?
			{
				state->buttonpress |= 512; //R!
			}		
			
			if ((input.Buttons&BUTTON_START)) //START?
			{
				state->buttonpress |= 1024; //START!
			}
			break;
		case 1: //Keyboard; Same as mouse, but keyboard -1..+1!
			state->analogdirection_keyboard_x = x; //Keyboard X movement!
			state->analogdirection_keyboard_y = y; //Keyboard Y movement!
	
			//Patch keyboard to -1&+1
			if (state->analogdirection_keyboard_x>0) //Positive?
			{
				state->analogdirection_keyboard_x = 1; //Positive simple!
			}
			else if (state->analogdirection_keyboard_x<0) //Negative?
			{
				state->analogdirection_keyboard_x = -1; //Negative simple!
			}
	
			if (state->analogdirection_keyboard_y>0) //Positive?
			{
				state->analogdirection_keyboard_y = 1; //Positive simple!
			}
			else if (state->analogdirection_keyboard_y<0) //Negative?
			{
				state->analogdirection_keyboard_y = -1; //Negative simple!
			}
	
			//The face buttons are OR-ed!
			if ((input.Buttons&BUTTON_TRIANGLE)>0) //Triangle?
			{
				state->buttonpress |= 2; //Triangle!
			}
			else if ((input.Buttons&BUTTON_SQUARE)>0) //Square?
			{
				state->buttonpress |= 1; //Square!
			}
			else if ((input.Buttons&BUTTON_CROSS)>0) //Cross?
			{
				state->buttonpress |= 8; //Cross!
			}
			else if ((input.Buttons&BUTTON_CIRCLE)>0) //Circle?
			{
				state->buttonpress |= 4; //Circle!
			}
	
			if ((input.Buttons&BUTTON_LEFT)>0) //Left?
			{
				state->buttonpress |= 16; //Left!
			}
	
			if ((input.Buttons&BUTTON_UP)>0) //Up?
			{
				state->buttonpress |= 32; //Up!
			}
	
			if ((input.Buttons&BUTTON_RIGHT)>0) //Right?
			{
				state->buttonpress |= 64; //Right!
			}
	
			if ((input.Buttons&BUTTON_DOWN)>0) //Down?
			{
				state->buttonpress |= 128; //Down!
			}
	
			if ((input.Buttons&BUTTON_LTRIGGER)>0) //L?
			{
				state->buttonpress |= 256; //L!
			}
	
			if ((input.Buttons&BUTTON_RTRIGGER)>0) //R?
			{
				state->buttonpress |= 512; //R!
			}
			
			if ((input.Buttons&BUTTON_START)) //START?
			{
				state->buttonpress |= 1024; //START!
			}		
			break;
		default: //Unknown?
			//Can't handle unknown input devices!
			break;
		}
	}
}

int is_gamingmode()
{
	return curstat.gamingmode; //Are in gaming mode?
}

void mouse_handler() //Mouse handler at current packet speed (MAX 255 packets/second!)
{
	if (!curstat.mode || curstat.gamingmode) //Mouse mode or gaming mode?
	{
		if (useMouseTimer() || useSERMouse()) //We're using the mouse?
		{
			MOUSE_PACKET *mousepacket = (MOUSE_PACKET *)zalloc(sizeof(MOUSE_PACKET),"Mouse_Packet",NULL); //Allocate a mouse packet!
		
			if (mousepacket) //Not running out of memory?
			{
				mousepacket->xmove = 0; //No movement by default!
				mousepacket->ymove = 0; //No movement by default!
				
				//Fill the mouse data!
				//Mouse movement!
				lock(LOCK_INPUT);
				if (mouse_xmove || mouse_ymove) //Any movement at all?
				{
					sbyte xmove, ymove; //For calculating limits!
					xmove = (mouse_xmove < -128) ? -128 : ((mouse_xmove>127) ? 127 : (sbyte)mouse_xmove); //Clip to max value!
					ymove = (mouse_ymove < -128) ? -128 : ((mouse_ymove>127) ? 127 : (sbyte)mouse_ymove); //Clip to max value!
					mouse_xmove -= xmove; //Rest value!
					mouse_ymove -= ymove; //Rest value!
					mousepacket->xmove = xmove; //X movement in mm!
					mousepacket->ymove = ymove; //Y movement, scaled!
				}
				unlock(LOCK_INPUT);

				mousepacket->buttons = Mouse_buttons; //Take the mouse buttons pressed directly!

				if (!PS2mouse_packet_handler(mousepacket)) //Add the mouse packet! Not supported PS/2 mouse?
				{
					SERmouse_packet_handler(mousepacket); //Send the mouse packet to the serial mouse!
				}
			}
		}
	}
}

//Rows: 3, one for top, middle, bottom.
#define KEYBOARD_NUMY 3
//Columns: 21: 10 for left, 1 space, 10 for right
#define KEYBOARD_NUMX 21

//Calculate the middle!
#define CALCMIDDLE(size,length) ((size/2)-(length/2))

//Right

//Down

GPU_TEXTSURFACE *keyboardsurface = NULL; //Framerate surface!

void initKeyboardOSK()
{
	keyboardsurface = alloc_GPUtext(); //Allocate GPU text surface for us to use!
	if (!keyboardsurface) //Couldn't allocate?
	{
		raiseError("GPU","Error allocating OSK layer!");
	}
	GPU_enableDelta(keyboardsurface, 1, 1); //Enable both x and y delta coordinates: we want to be at the bottom-right of the screen always!
	GPU_addTextSurface(keyboardsurface,&keyboard_renderer); //Register our renderer!

	//dolog("GPU","Keyboard OSK allocated.");
}

void doneKeyboardOSK()
{
	free_GPUtext(&keyboardsurface); //Release the framerate!
}

byte displaytokeyboard[5] = {0,3,1,4,2}; //1,2,3,4 (keyboard display)=>3,1,4,2 (getkeyboard)

//Keyboard layout: shift,set,sety,setx,itemnr,values
char active_keyboard[3][3][3][2][5][10]; //Active keyboard!

//US keyboard!
byte keyboard_active = 1; //What's the active keyboard (from below list)?
char keyboard_names[2][20] = {"US(default)","US(linear)"}; //Names of the different keyboards!

char keyboards[2][3][3][3][2][5][10] = { //X times 3 sets of 3x3 key choices, every choise has 4 choises (plus 1 at the start to determine enabled, "enable" for enabled, else disabled. Also, empty is no key assigned.). Order: left,right,up,down.
										{ //US(default)
											{ //Set 1!
												//enable, first, second, third, last
												{ //Row 1
													{{"enable","q","w","e","r"},{"enable","Q","W","E","R"}},
													{{"enable","t","y","u","i"},{"enable","T","Y","U","I"}},
													{{"enable","o","p","[","]"},{"enable","O","P","{","}"}}
												},
												{ //Row 2
													{{"enable","a","s","d","f"},{"enable","A","S","D","F"}},
													{{"enable","g","h","j","k"},{"enable","G","H","J","K"}},
													{{"enable","l",";","'","enter"},{"enable","L",":","\"","enter"}}
												},
												{ //Row 3
													{{"enable","z","x","c","v"},{"enable","Z","X","C","V"}},
													{{"enable","b","n","m",","},{"enable","B","N","M","<"}},
													{{"enable",".","/","\\","space"},{"enable",">","?","|","space"}}
												}
											},

											{ //Set 2!
												{ //Row 1
													{{"enable","1","2","3","4"},{"enable","!","@","#","$"}},
													{{"enable","5","6","7","8"},{"enable","%","^","&","*"}},
													{{"enable","9","0","-","="},{"enable","(",")","_","+"}}
												},
												{ //Row 2
													{{"enable","f1","f2","f3","f4"},{"enable","f1","f2","f3","f4"}},
													{{"enable","f5","f6","f7","f8"},{"enable","f5","f6","f7","f8"}},
													{{"enable","f9","f10","f11","f12"},{"enable","f9","f10","f11","f12"}}
												},
												{ //Row 3
													{{"enable","home","end","pgup","pgdn"},{"enable","home","end","pgup","pgdn"}},
													{{"enable","left","right","up","down"},{"enable","left","right","up","down"}},
													{{"enable","esc","del","enter","bksp"},{"enable","esc","del","enter","bksp"}}
												}
											},

											{ //Set 3!
												{ //Row 1
													{{"enable","kp0","kp1","kp2","kp3"},{"enable","kpins","kpend","kpdown","kppgdn"}},
													{{"enable","kp4","kp5","kp6","kp7"},{"enable","kpleft","kp5","kpright","kphome"}},
													{{"enable","kp8","kp9","kp.","kpen"},{"enable","kpup","kppgup","kpdel","kpen"}}
												},
												{ //Row 2
													{{"enable","num","capslock","scroll","tab"},{"enable","num","capslock","scroll","tab"}},
													{{"enable","prtsc","pause","insert","win"},{"enable","prtsc","pause","insert","win"}},
													{{"enable","num/","num*","num-","num+"},{"enable","num/","num*","num-","num+"}}
												},
												{ //Row 3
													{{"enable","","","CAPTURE","`"},{"enable","","","CAPTURE","~"}},
													{{"","","","",""},{"","","","",""}},
													{{"","","","",""},{"","","","",""}}
												} //Not used.
											}
										},
										{ //US(linear)
											{ //Set 1!
												//enable, first, last, second, third
												{ //Row 1
													{{"enable","q","r","w","e"},{"enable","Q","R","W","E"}},
													{{"enable","t","i","y","u"},{"enable","T","I","Y","U"}},
													{{"enable","o","]","p","["},{"enable","O","}","P","{"}}
												},
												{ //Row 2
													{{"enable","a","f","s","d"},{"enable","A","F","S","D"}},
													{{"enable","g","k","h","j"},{"enable","G","K","H","J"}},
													{{"enable","l","enter",";","'"},{"enable","L","enter",":","\""}}
												},
												{ //Row 3
													{{"enable","z","v","x","c"},{"enable","Z","V","X","C"}},
													{{"enable","b",",","n","m"},{"enable","B",",","N","M"}},
													{{"enable",".","space","/","\\"},{"enable",">","space","?","|"}}
												}
											},

											{ //Set 2!
												{ //Row 1
													{{"enable","1","4","2","3"},{"enable","!","$","@","#"}},
													{{"enable","5","8","6","7"},{"enable","%","*","^","&"}},
													{{"enable","9","=","0","-"},{"enable","(","+",")","_"}}
												},
												{ //Row 2
													{{"enable","f1","f4","f2","f3"},{"enable","f1","f4","f2","f3"}},
													{{"enable","f5","f8","f6","f7"},{"enable","f5","f8","f6","f7"}},
													{{"enable","f9","f12","f10","f11"},{"enable","f9","f12","f10","f11"}}
												},
												{ //Row 3
													{{"enable","home","end","pgup","pgdn"},{"enable","home","end","pgup","pgdn"}},
													{{"enable","left","right","up","down"},{"enable","left","right","up","down"}},
													{{"enable","esc","del","enter","bksp"},{"enable","esc","del","enter","bksp"}}
												}
											},

											{ //Set 3!
												{ //Row 1
													{{"enable","kp0","kp3","kp1","kp2"},{"enable","kpins","kppgdn","kpend","kpdown"}},
													{{"enable","kp4","kp7","kp5","kp6"},{"enable","kpleft","kphome","kp5","kpright"}},
													{{"enable","kp8","kpen","kp9","kp."},{"enable","kpup","kppgup","kpdel","kpen"}}
												},
												{ //Row 2
													{{"enable","num","capslock","scroll","tab"},{"enable","num","capslock","scroll","tab"}},
													{{"enable","prtsc","pause","insert","win"},{"enable","prtsc","pause","insert","win"}},
													{{"enable","num/","num+","num*","num-"},{"enable","num/","num+","num*","num-"}}
												},
												{ //Row 3
													{{"enable","","","CAPTURE","`"},{"enable","","","CAPTURE","~"}},
													{{"","","","",""},{"","","","",""}},
													{{"","","","",""},{"","","","",""}}
												} //Barely used input.
											}
										}										
									};
									
/*

Calling the keyboard_us structure:
enabled = keyboard_us[0][set][row][column][0]=="enable"
onscreentext = keyboard_us[have_shift][set][row][column]
key = keyboard_us[0][set][row][column][pspkey]
pspkey = 1=square,2=circle,3=triangle,4=cross
*/

int currentset = 0; //Active set?
int currentkey = 0; //Current key. 0=none, next 1=triangle, square, cross, circle?
int currentshift = 0; //No shift (1=shift)
int currentctrl = 0; //No ctrl (1=Ctrl)
int currentalt = 0; //No alt (1=Alt)
int setx = 0; //X within set -1 to 1!
int sety = 0; //Y within set -1 to 1!

byte keyboard_attribute[KEYBOARD_NUMY][KEYBOARD_NUMX];
byte keyboard_display[KEYBOARD_NUMY][KEYBOARD_NUMX];

//Keyboard layout: shift,set,sety,setx,itemnr,values
//Shift,set,row,item=0=enable?;1+=left,right,up,down
#define getkeyboard(shift,set,sety,setx,itemnr) &active_keyboard[set][1+sety][1+setx][shift][itemnr][0]

extern PS2_KEYBOARD Keyboard; //Active keyboard!
extern byte SCREEN_CAPTURE; //Screen capture requested?

void fill_keyboarddisplay() //Fills the display for displaying on-screen!
{
	memset(keyboard_display,0,sizeof(keyboard_display)); //Init keyboard display!
	memcpy(&active_keyboard,&keyboards[keyboard_active],sizeof(active_keyboard)); //Set the active keyboard to the defined keyboard!
	memset(keyboard_attribute,0,sizeof(keyboard_attribute)); //Default attributes to font color!

	if (!input_enabled) //Input disabled atm?
	{
		return; //Keyboard disabled: don't show!
	}

	//LEDs on keyboard are always visible!
	
	if (!curstat.gamingmode) //Not gaming mode (mouse, keyboard and direct mode)?
	{
		keyboard_display[KEYBOARD_NUMY-2][KEYBOARD_NUMX-3] = 'N'; //NumberLock!
		if (Keyboard.LEDS&2) //NUMLOCK?
		{
			keyboard_attribute[KEYBOARD_NUMY-2][KEYBOARD_NUMX-3] = 3; //Special shift color active!
		}
		else
		{
			keyboard_attribute[KEYBOARD_NUMY-2][KEYBOARD_NUMX-3] = 2; //Special shift color inactive!
		}

		keyboard_display[KEYBOARD_NUMY-2][KEYBOARD_NUMX-2] = 'C'; //CAPS LOCK!
		if (Keyboard.LEDS&2) //CAPSLOCK?
		{
			keyboard_attribute[KEYBOARD_NUMY-2][KEYBOARD_NUMX-2] = 3; //Special shift color active!
		}
		else
		{
			keyboard_attribute[KEYBOARD_NUMY-2][KEYBOARD_NUMX-2] = 2; //Special shift color inactive!
		}

		keyboard_display[KEYBOARD_NUMY-2][KEYBOARD_NUMX-1] = 'S'; //Scroll lock!
		if (Keyboard.LEDS&1) //SCROLLLOCK?
		{
			keyboard_attribute[KEYBOARD_NUMY-2][KEYBOARD_NUMX-1] = 3; //Special shift color active!
		}
		else
		{
			keyboard_attribute[KEYBOARD_NUMY-2][KEYBOARD_NUMX-1] = 2; //Special shift color inactive!
		}
	}
	if (!Direct_Input) //Not direct input?
	{
		if (curstat.mode == 1 && !curstat.gamingmode) //Keyboard mode?
		{
			if (strcmp((char *)getkeyboard(0, currentset, sety, setx, 0), "enable") == 0) //Set enabled?
			{
				char *left = getkeyboard(currentshift, currentset, sety, setx, 1);
				char *right = getkeyboard(currentshift, currentset, sety, setx, 2);
				char *up = getkeyboard(currentshift, currentset, sety, setx, 3);
				char *down = getkeyboard(currentshift, currentset, sety, setx, 4);

				byte leftloc = CALCMIDDLE(CALCMIDDLE(KEYBOARD_NUMX, 0), safe_strlen(left, 0)); //Left location!
				byte rightloc = CALCMIDDLE(KEYBOARD_NUMX, 0); //Start at the right half!
				rightloc += CALCMIDDLE(CALCMIDDLE(KEYBOARD_NUMX, 0), safe_strlen(right, 0)); //Right location!
				byte uploc = CALCMIDDLE(KEYBOARD_NUMX, safe_strlen(up, 0)); //Up location!
				byte downloc = CALCMIDDLE(KEYBOARD_NUMX, safe_strlen(down, 0)); //Down location!

				memcpy(&keyboard_display[0][uploc], up, safe_strlen(up, 0)); //Set up!
				if (currentkey == 1) //Got a key pressed?
				{
					memset(&keyboard_attribute[0][uploc], 1, safe_strlen(up, 0)); //Active key!
				}

				memcpy(&keyboard_display[1][leftloc], left, safe_strlen(left, 0)); //Set left!
				if (currentkey == 2) //Got a key pressed?
				{
					memset(&keyboard_attribute[1][leftloc], 1, safe_strlen(left, 0)); //Active key!
				}

				memcpy(&keyboard_display[1][rightloc], right, safe_strlen(right, 0)); //Set up!
				if (currentkey == 4) //Got a key pressed?
				{
					memset(&keyboard_attribute[1][rightloc], 1, safe_strlen(right, 0)); //Active key!
				}

				memcpy(&keyboard_display[2][downloc], down, safe_strlen(down, 0)); //Set up!
				if (currentkey == 3) //Got a key pressed?
				{
					memset(&keyboard_attribute[2][downloc], 1, safe_strlen(down, 0)); //Active key!
				}
			}
		} //Keyboard mode?

		keyboard_display[KEYBOARD_NUMY - 3][KEYBOARD_NUMX - 3] = 'C'; //Screen capture!
		keyboard_display[KEYBOARD_NUMY - 3][KEYBOARD_NUMX - 2] = 'a'; //Screen capture!
		keyboard_display[KEYBOARD_NUMY - 3][KEYBOARD_NUMX - 1] = 'p'; //Screen capture!

		if (SCREEN_CAPTURE) //Screen capture status?
		{
			keyboard_attribute[KEYBOARD_NUMY - 3][KEYBOARD_NUMX - 3] = 3; //Special shift color active!
			keyboard_attribute[KEYBOARD_NUMY - 3][KEYBOARD_NUMX - 2] = 3; //Special shift color active!
			keyboard_attribute[KEYBOARD_NUMY - 3][KEYBOARD_NUMX - 1] = 3; //Special shift color active!
		}
		else
		{
			keyboard_attribute[KEYBOARD_NUMY - 3][KEYBOARD_NUMX - 3] = 2; //Special shift color inactive!
			keyboard_attribute[KEYBOARD_NUMY - 3][KEYBOARD_NUMX - 2] = 2; //Special shift color inactive!
			keyboard_attribute[KEYBOARD_NUMY - 3][KEYBOARD_NUMX - 1] = 2; //Special shift color inactive!
		}

		if (!curstat.mode && !curstat.gamingmode) //Mouse mode?
		{
			keyboard_display[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 4] = 'M'; //Mouse mode!
			keyboard_attribute[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 4] = 2; //Special shift color inactive!
		}

		if (!curstat.gamingmode) //Not gaming mode?
		{
			keyboard_display[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 3] = 'C'; //Ctrl!
			if (currentctrl)
			{
				keyboard_attribute[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 3] = 3; //Special shift color active!
			}
			else
			{
				keyboard_attribute[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 3] = 2; //Special shift color inactive!
			}

			keyboard_display[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 2] = 'A'; //Alt!
			if (currentalt)
			{
				keyboard_attribute[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 2] = 3; //Special shift color active!
			}
			else
			{
				keyboard_attribute[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 2] = 2; //Special shift color inactive!
			}

			keyboard_display[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 1] = 'S'; //Shift!
			if (currentshift)
			{
				keyboard_attribute[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 1] = 3; //Special shift color active!
			}
			else
			{
				keyboard_attribute[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 1] = 2; //Special shift color inactive!
			}
		}
		else //Gaming mode?
		{
			keyboard_display[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 1] = 'G'; //Gaming mode!
			keyboard_attribute[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 1] = 2; //Special shift color inactive!
		}
	}
	else
	{
		keyboard_display[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 4] = 'D'; //Direct Input mode!
		keyboard_attribute[KEYBOARD_NUMY - 1][KEYBOARD_NUMX - 4] = 2; //Special shift color inactive!
	}
}

uint_32 keyboard_rendertime; //Time for framerate rendering!

void keyboard_renderer() //Render the keyboard on-screen!
{
	static byte last_rendered = 0; //Last rendered keyboard status: 1=ON, 0=OFF!
	lock(LOCK_INPUT);
	if (!KEYBOARD_ENABLED) return; //Disabled?
	if (!input_enabled) //Keyboard disabled atm OR Gaming mode?
	{
		if (last_rendered) //We're rendered?
		{
			last_rendered = 0; //We're not rendered now!
			GPU_text_locksurface(keyboardsurface);
			GPU_textclearscreen(keyboardsurface); //Clear the rendered surface: there's nothing to show!
			GPU_text_releasesurface(keyboardsurface);
		}
		unlock(LOCK_INPUT);
		return; //Keyboard disabled: don't show!
	}

	last_rendered = 1; //We're rendered!
	fill_keyboarddisplay(); //Fill the keyboard display!

	int ybase,xbase;
	ybase = GPU_TEXTSURFACE_HEIGHT-KEYBOARD_NUMY; //Base Y on GPU's text screen!
	xbase = GPU_TEXTSURFACE_WIDTH-KEYBOARD_NUMX; //Base X on GPU's text screen!

	int x;
	int y; //The coordinates in the buffer!

	for (y=ybase;y<GPU_TEXTSURFACE_HEIGHT;y++)
	{
		for (x = xbase;x < GPU_TEXTSURFACE_WIDTH;x++)
		{
			uint_32 fontcolor = getemucol16(BIOS_Settings.input_settings.fontcolor); //Use font color by default!
			uint_32 bordercolor = getemucol16(BIOS_Settings.input_settings.bordercolor); //Use border color by default!
			switch (keyboard_attribute[y - ybase][x - xbase]) //What attribute?
			{
			case 1: //Active color?
				bordercolor = getemucol16(BIOS_Settings.input_settings.activecolor); //Use active color!
				break;
			case 2: //Special (Ctrl/Shift/Alt keys) inactive?
				fontcolor = getemucol16(BIOS_Settings.input_settings.specialcolor); //Use active color for special keys!
				bordercolor = getemucol16(BIOS_Settings.input_settings.specialbordercolor); //Use inactive border for special keys!
				break;
			case 3: //Special (Ctrl/Shift/Alt keys) active?
				fontcolor = getemucol16(BIOS_Settings.input_settings.specialcolor); //Use active color for special keys!
				bordercolor = getemucol16(BIOS_Settings.input_settings.specialactivecolor); //Use active color!
				break;
			default: //Default/standard border!
				break;
			}
			GPU_text_locksurface(keyboardsurface); //Lock us!
			GPU_textsetxy(keyboardsurface, x, y, keyboard_display[y - ybase][x - xbase], fontcolor, bordercolor);
			GPU_text_releasesurface(keyboardsurface); //Unlock us!
		}
	}
	unlock(LOCK_INPUT);
}

int ticking = 0; //Already ticking?
/*void keyboard_tick()
{
	if (ticking) return; //Disable when already ticking!
	ticking = 1; //We're ticking!
	delay((uint_32)(1000000/HWkeyboard_getrepeatrate())/2); //A bit of time before turning it off, for the user to see it, take half the time of the original function!
	int currentkeybackup;
	currentkeybackup = currentkey; //Backup!
	currentkey = 0; //Disable!
	keyboard_renderer(); //Render the off!
	currentkey = currentkeybackup; //Restore the current key!
	ticking = 0; //Not ticking anymore!
}*/

byte req_quit_gamingmode = 0; //Requesting to quit gaming mode?

void keyboard_swap_handler() //Swap handler for keyboard!
{
	lock(LOCK_INPUT);
	//while (1)
	{
		if (input_enabled && !Direct_Input) //Input enabled?
		{
			if (curstat.gamingmode) //Gaming mode?
			{
				if (psp_inputkey()&BUTTON_SELECT) //Quit gaming mode?
				{
					req_quit_gamingmode = 1; //We're requesting to quit gaming mode!
				}
				else if (req_quit_gamingmode) //Select released and requesting to quit gaming mode?
				{
					curstat.gamingmode = 0; //Disable gaming mode!
				}
			}
			else if (curstat.mode==1 && input_enabled) //Keyboard active and on-screen?
			{
				int curkey;
				curkey = psp_inputkey(); //Read current keys with delay!
				if ((curkey&BUTTON_LTRIGGER) && (!(curkey&BUTTON_RTRIGGER))) //Not L&R (which is CAPS LOCK) special?
				{
					currentset = (currentset+1)%3; //Next set!
					currentkey = 0; //No keys pressed!
					//Disable all output still standing!
					ReleaseKeys(); //Release all keys!
				}
				else if (curkey&BUTTON_DOWN) //Down pressed: swap to gaming mode!
				{
					currentkey = 0; //No keys pressed!
					ReleaseKeys(); //Release all keys!
					curstat.gamingmode = 1; //Enable gaming mode!
				}
				else if (curkey&BUTTON_START) //Swap to mouse mode!
				{
					currentkey = 0; //No keys pressed!
					ReleaseKeys(); //Release all keys!
					curstat.mode = 0; //Swap to mouse mode!
				}
			}
			else if (curstat.mode==0 && input_enabled) //Mouse active?
			{
				if (psp_inputkey()&BUTTON_DOWN) //Down pressed: swap to gaming mode!
				{
					curstat.gamingmode = 1; //Enable gaming mode!
				}
				else if (psp_inputkey()&BUTTON_START) //Swap to keyboard mode!
				{
					curstat.mode = 1; //Swap to keyboard mode!
				}
			}
		}
		//delay(KEYSWAP_DELAY); //Wait a bit, for as precise as we can simply (1ms), slower is never worse!
	}
	unlock(LOCK_INPUT);
}

byte oldshiftstatus = 0; //Old shift status, used for keyboard/gaming mode!
byte shiftstatus = 0; //New shift status!


extern char keys_names[104][11]; //All names of the used keys (for textual representation/labeling)

void handleMouseMovement() //Handles mouse movement using the analog direction of the mouse!
{
	if (curstat.analogdirection_mouse_x || curstat.analogdirection_mouse_y) //Mouse analog direction trigger?
	{
		mouse_xmove += (int_64)((float)(curstat.analogdirection_mouse_x / 32767.0f)*10.0f); //Apply x movement in mm (reversed)!
		mouse_ymove += (int_64)((float)(curstat.analogdirection_mouse_y / 32767.0f)*10.0f); //Apply y movement in mm (reversed)!
	}
}

void handleKeyboardMouse() //Handles keyboard input during mouse operations!
{
	//Also handle mouse movement here (constant factor)!
	handleMouseMovement(); //Handle mouse movement!

	Mouse_buttons = (curstat.buttonpress & 1) ? 1 : 0; //Left mouse button pressed?
	Mouse_buttons |= (curstat.buttonpress & 4) ? 2 : 0; //Right mouse button pressed?
	Mouse_buttons |= (curstat.buttonpress & 2) ? 4 : 0; //Middle mouse button pressed?

	shiftstatus = 0; //Init shift status!
	shiftstatus |= ((curstat.buttonpress&512)>0)*SHIFTSTATUS_SHIFT; //Apply shift status!
	shiftstatus |= ((curstat.buttonpress&(16|32))>0)*SHIFTSTATUS_CTRL; //Apply ctrl status!
	shiftstatus |= ((curstat.buttonpress&(64|32))>0)*SHIFTSTATUS_ALT; //Apply alt status!
	currentshift = (shiftstatus&SHIFTSTATUS_SHIFT)>0; //Shift pressed?
	currentctrl = (shiftstatus&SHIFTSTATUS_CTRL)>0; //Ctrl pressed?
	currentalt = (shiftstatus&SHIFTSTATUS_ALT)>0; //Alt pressed?

	if (!input_buffer_input) //Not buffering?
	{
		//First, process Ctrl,Alt,Shift Releases!
		if (((oldshiftstatus&SHIFTSTATUS_CTRL)>0) && (!(shiftstatus&SHIFTSTATUS_CTRL))) //Released CTRL?
		{
			if (input_buffer_input)
			{
				input_buffer_shift = oldshiftstatus; //Set shift status!
			}
			else
			{
				onKeyRelease("lctrl");
			}
		}
		if (((oldshiftstatus&SHIFTSTATUS_ALT)>0) && (!(shiftstatus&SHIFTSTATUS_ALT))) //Released ALT?
		{
			if (input_buffer_input)
			{
				input_buffer_shift = oldshiftstatus; //Set shift status!
			}
			else
			{
				onKeyRelease("lalt");
			}
		}
		if (((oldshiftstatus&SHIFTSTATUS_SHIFT)>0) && (!(shiftstatus&SHIFTSTATUS_SHIFT))) //Released SHIFT?
		{
			if (input_buffer_input)
			{
				input_buffer_shift = oldshiftstatus; //Set shift status!
			}
			else
			{
				onKeyRelease("lshift");
			}
		}
		//Next, process Ctrl,Alt,Shift presses!
		if ((shiftstatus&SHIFTSTATUS_CTRL)>0) //Pressed CTRL?
		{
			onKeyPress("lctrl");
		}
		if ((shiftstatus&SHIFTSTATUS_ALT)>0) //Pressed ALT?
		{
			onKeyPress("lalt");
		}
		if ((shiftstatus&SHIFTSTATUS_SHIFT)>0) //Pressed SHIFT?
		{
			onKeyPress("lshift");
		}
		oldshiftstatus = shiftstatus; //Save shift status to old shift status!
	} //Not buffering?
}

void handleKeyboard() //Handles keyboard input!
{
	keysactive = 0; //Reset keys active!
	static int lastkey = 0, lastx = 0, lasty = 0, lastset = 0, lastshift = 0; //Previous key that was pressed!
	setx = curstat.analogdirection_keyboard_x; //X in keyboard set!
	sety = curstat.analogdirection_keyboard_y; //Y in keyboard set!

	//Now handle current keys!
	currentkey = 0; //Default: no key pressed!
	//Order of currentkey: Up left down right.
	if (curstat.buttonpress & 1) //Left?
	{
		currentkey = 2; //Pressed square!
	}
	else if (curstat.buttonpress & 2) //Up?
	{
		currentkey = 1; //Pressed triangle!
	}
	else if (curstat.buttonpress & 4) //Right?
	{
		currentkey = 4; //Circle pressed!
	}
	else if (curstat.buttonpress & 8) //Down?
	{
		currentkey = 3; //Cross pressed!
	}

	//Now, process the keys!

	shiftstatus = 0; //Init shift status!
	shiftstatus |= ((curstat.buttonpress & 512) > 0)*SHIFTSTATUS_SHIFT; //Apply shift status!
	if ((curstat.buttonpress & 0x300) == 0x300) //L&R hold?
	{
		shiftstatus &= ~SHIFTSTATUS_SHIFT; //Shift isn't pressed: it's CAPS LOCK special case!
	}
	shiftstatus |= ((curstat.buttonpress&(16 | 32)) > 0)*SHIFTSTATUS_CTRL; //Apply ctrl status!
	shiftstatus |= ((curstat.buttonpress&(64 | 32)) > 0)*SHIFTSTATUS_ALT; //Apply alt status!
	currentshift = (shiftstatus&SHIFTSTATUS_SHIFT) > 0; //Shift pressed?
	currentctrl = (shiftstatus&SHIFTSTATUS_CTRL) > 0; //Ctrl pressed?
	currentalt = (shiftstatus&SHIFTSTATUS_ALT) > 0; //Alt pressed?

	if (!input_buffer_input) //Not buffering?
	{
		//First, process Ctrl,Alt,Shift Releases!
		if (((oldshiftstatus&SHIFTSTATUS_CTRL) > 0) && (!currentctrl)) //Released CTRL?
		{
			onKeyPress("lctrl");
		}
		if (((oldshiftstatus&SHIFTSTATUS_ALT) > 0) && (!currentalt)) //Released ALT?
		{
			onKeyPress("lalt");
		}
		if (((oldshiftstatus&SHIFTSTATUS_SHIFT) > 0) && (!currentshift)) //Released SHIFT?
		{
			onKeyPress("lshift");
		}
		//Next, process Ctrl,Alt,Shift presses!
		if (currentctrl) //Pressed CTRL?
		{
			onKeyRelease("lctrl");
		}
		if (currentalt) //Pressed ALT?
		{
			onKeyRelease("lalt");
		}
		if (currentshift) //Pressed SHIFT?
		{
			onKeyRelease("lshift");
		}

		if ((curstat.buttonpress & 0x300) == 0x300) //L&R hold? CAPS LOCK PRESSED! (Special case)
		{
			onKeyPress("capslock"); //Shift isn't pressed: it's CAPS LOCK special case!
		}
		else //No CAPS LOCK?
		{
			onKeyRelease("capslock"); //Release if needed, forming a button click!
		}

		oldshiftstatus = shiftstatus; //Save shift status to old shift status!

		if (currentkey) //Key pressed?
		{
			if (lastkey && ((lastkey != currentkey) || (lastx != setx) || (lasty != sety) || (lastset != currentset))) //We had a last key that's different?
			{
				onKeyRelease(getkeyboard(shiftstatus, lastset, lasty, lastx, displaytokeyboard[lastkey])); //Release the last key!
			}
			onKeyPress(getkeyboard(0, currentset, sety, setx, displaytokeyboard[currentkey]));
			//Save the active key information!
			lastset = currentset;
			lastx = setx;
			lasty = sety;
			lastkey = currentkey;
		}
		else if (lastkey) //We have a last key with nothing pressed?
		{
			onKeyRelease(getkeyboard(0, lastset, lasty, lastx, displaytokeyboard[lastkey])); //Release the last key!
			lastkey = 0; //We didn't have a last key!			
		}
	} //Not buffering?
	else //Buffering?
	{
		if (!(shiftstatus&SHIFTSTATUS_CTRL) && ((lastshift&SHIFTSTATUS_CTRL) > 0)) //Released CTRL?
		{
			goto keyreleased; //Released!
		}
		if (!(shiftstatus&SHIFTSTATUS_ALT) && ((lastshift&SHIFTSTATUS_ALT) > 0)) //Released ALT?
		{
			goto keyreleased; //Released!
		}
		if (!(shiftstatus&SHIFTSTATUS_SHIFT) && ((lastshift&SHIFTSTATUS_SHIFT) > 0)) //Released SHIFT?
		{
			goto keyreleased; //Released!
		}

		if (currentkey || shiftstatus) //More keys pressed?
		{
			if (lastkey && ((lastkey != currentkey) || (lastx != setx) || (lasty != sety) || (lastset != currentset))) //We had a last key that's different?
			{
				goto keyreleased; //Released after all!
			}
			//Save the active key information!

			lastset = currentset;
			lastx = setx;
			lasty = sety;
			lastkey = currentkey;
			lastshift = shiftstatus; //Shift status!
		}
		else //Key/shift released?
		{
			int key;
		keyreleased:
			if (!lastkey && !lastshift) //Nothing yet?
			{
				return; //Abort: we're nothing pressed!
			}
			key = EMU_keyboard_handler_nametoid(getkeyboard(0, lastset, lasty, lastx, displaytokeyboard[lastkey])); //Our key?
			input_buffer_shift = lastshift; //Set shift status!
			input_buffer = key; //Last key!
			//Update current information!
			lastkey = 0; //Update current information!
			lastx = setx;
			lasty = sety;
			lastset = currentset;
			lastshift = shiftstatus;
		}
		//Key presses aren't buffered: we only want to know the key and shift state when fully pressed, nothing more!
	}
}

void handleGaming() //Handles gaming mode input!
{
	//Test for all keys and process!
	if (input_buffer_input) return; //Don't handle buffered input, we don't allow mapping gaming mode to gaming mode!

	if ((BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGLEFT] == -1) &&
		(BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGUP] == -1) &&
		(BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGRIGHT] == -1) &&
		(BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGDOWN] == -1)) //No analog assigned? Process analog mouse movement!
	{
		//Also handle mouse movement here (constant factor)!
		handleMouseMovement(); //Handle mouse movement!
	}

	int keys[15]; //Key index for this key, -1 for none/nothing happened!
	byte keys_pressed[15] = {0,0,0,0,0,0,0,0,0,0,0}; //We have pressed the key?
	//Order: START, LEFT, UP, RIGHT, DOWN, L, R, TRIANGLE, CIRCLE, CROSS, SQUARE, ANALOGLEFT, ANALOGUP, ANALOGRIGHT, ANALOGDOWN

	int i;
	for (i=0;i<15;i++)
	{
		keys[i] = -1; //We have no index and nothing happened!
	}
	shiftstatus = 0; //Default new shift status: none!

	if (curstat.buttonpress&1) //Square?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_SQUARE]!=-1) //Mapped on?
		{
			keys[GAMEMODE_SQUARE] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_SQUARE]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_SQUARE] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_SQUARE]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_SQUARE]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_SQUARE]!=-1) //Mapped on?
		{
			keys[GAMEMODE_SQUARE] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_SQUARE]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_SQUARE] = 0; //We're released!
		}
	}

	if (curstat.buttonpress&2) //Triangle?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_TRIANGLE]!=-1) //Mapped on?
		{
			keys[GAMEMODE_TRIANGLE] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_TRIANGLE]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_TRIANGLE] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_TRIANGLE]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_TRIANGLE]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_TRIANGLE]!=-1) //Mapped on?
		{
			keys[GAMEMODE_TRIANGLE] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_TRIANGLE]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_TRIANGLE] = 0; //We're released!
		}
	}

	if (curstat.buttonpress&4) //Circle?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_CIRCLE]!=-1) //Mapped on?
		{
			keys[GAMEMODE_CIRCLE] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_CIRCLE]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_CIRCLE] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_CIRCLE]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_CIRCLE]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_CIRCLE]!=-1) //Mapped on?
		{
			keys[GAMEMODE_CIRCLE] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_CIRCLE]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_CIRCLE] = 0; //We're released!
		}
	}

	if (curstat.buttonpress&8) //Cross?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_CROSS]!=-1) //Mapped on?
		{
			keys[GAMEMODE_CROSS] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_CROSS]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_CROSS] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_CROSS]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_CROSS]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_CROSS]!=-1) //Mapped on?
		{
			keys[GAMEMODE_CROSS] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_CROSS]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_CROSS] = 0; //We're released!
		}
	}


	if (curstat.buttonpress&16) //Left?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_LEFT]!=-1) //Mapped on?
		{
			keys[GAMEMODE_LEFT] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_LEFT]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_LEFT] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_LEFT]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_LEFT]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_LEFT]!=-1) //Mapped on?
		{
			keys[GAMEMODE_LEFT] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_LEFT]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_LEFT] = 0; //We're released!
		}
	}


	if (curstat.buttonpress&32) //Up?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_UP]!=-1) //Mapped on?
		{
			keys[GAMEMODE_UP] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_UP]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_UP] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_UP]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_UP]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_UP]!=-1) //Mapped on?
		{
			keys[GAMEMODE_UP] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_UP]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_UP] = 0; //We're released!
		}
	}


	if (curstat.buttonpress&64) //Right?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_RIGHT]!=-1) //Mapped on?
		{
			keys[GAMEMODE_RIGHT] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_RIGHT]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_RIGHT] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_RIGHT]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_RIGHT]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_RIGHT]!=-1) //Mapped on?
		{
			keys[GAMEMODE_RIGHT] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_RIGHT]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_RIGHT] = 0; //We're released!
		}
	}


	if (curstat.buttonpress&128) //Down?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_DOWN]!=-1) //Mapped on?
		{
			keys[GAMEMODE_DOWN] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_DOWN]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_DOWN] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_DOWN]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_DOWN]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_DOWN]!=-1) //Mapped on?
		{
			keys[GAMEMODE_DOWN] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_DOWN]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_DOWN] = 0; //We're released!
		}
	}


	if (curstat.buttonpress&256) //L?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_LTRIGGER]!=-1) //Mapped on?
		{
			keys[GAMEMODE_LTRIGGER] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_LTRIGGER]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_LTRIGGER] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_LTRIGGER]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_LTRIGGER]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_LTRIGGER]!=-1) //Mapped on?
		{
			keys[GAMEMODE_LTRIGGER] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_LTRIGGER]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_LTRIGGER] = 0; //We're released!
		}
	}


	if (curstat.buttonpress&512) //R?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_RTRIGGER]!=-1) //Mapped on?
		{
			keys[GAMEMODE_RTRIGGER] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_RTRIGGER]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_RTRIGGER] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_RTRIGGER]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_RTRIGGER]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_RTRIGGER]!=-1) //Mapped on?
		{
			keys[GAMEMODE_RTRIGGER] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_RTRIGGER]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_RTRIGGER] = 0; //We're released!
		}
	}

	if (curstat.buttonpress&1024) //START?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_START]!=-1) //Mapped on?
		{
			keys[GAMEMODE_START] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_START]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_START] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_START]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_START]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_START]!=-1) //Mapped on?
		{
			keys[GAMEMODE_START] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_START]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_START] = 0; //We're released!
		}
	}

	if (curstat.analogdirection_keyboard_x<0 && !curstat.analogdirection_keyboard_y) //ANALOG LEFT?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGLEFT]!=-1) //Mapped on?
		{
			keys[GAMEMODE_ANALOGLEFT] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGLEFT]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_ANALOGLEFT] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_ANALOGLEFT]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_ANALOGLEFT]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGLEFT]!=-1) //Mapped on?
		{
			keys[GAMEMODE_ANALOGLEFT] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGLEFT]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_ANALOGLEFT] = 0; //We're released!
		}
	}

	if (!curstat.analogdirection_keyboard_x && curstat.analogdirection_keyboard_y<0) //ANALOG UP?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGUP]!=-1) //Mapped on?
		{
			keys[GAMEMODE_ANALOGUP] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGUP]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_ANALOGUP] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_ANALOGUP]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_ANALOGUP]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGUP]!=-1) //Mapped on?
		{
			keys[GAMEMODE_ANALOGUP] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGUP]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_ANALOGUP] = 0; //We're released!
		}
	}

	if (curstat.analogdirection_keyboard_x>0 && !curstat.analogdirection_keyboard_y) //ANALOG RIGHT?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGRIGHT]!=-1) //Mapped on?
		{
			keys[GAMEMODE_ANALOGRIGHT] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGRIGHT]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_ANALOGRIGHT] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_ANALOGRIGHT]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_ANALOGRIGHT]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGRIGHT]!=-1) //Mapped on?
		{
			keys[GAMEMODE_ANALOGRIGHT] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGRIGHT]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_ANALOGRIGHT] = 0; //We're released!
		}
	}

	if (!curstat.analogdirection_keyboard_x && curstat.analogdirection_keyboard_y>0) //ANALOG DOWN?
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGDOWN]!=-1) //Mapped on?
		{
			keys[GAMEMODE_ANALOGDOWN] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGDOWN]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_ANALOGDOWN] = 1; //We're pressed!
			shiftstatus |= BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[GAMEMODE_ANALOGDOWN]; //Ctrl-alt-shift status!
		}
	}
	else if (keys_pressed[GAMEMODE_ANALOGDOWN]) //Try release!
	{
		if (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGDOWN]!=-1) //Mapped on?
		{
			keys[GAMEMODE_ANALOGDOWN] = BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGDOWN]; //Set the key: we've changed!
			keys_pressed[GAMEMODE_ANALOGDOWN] = 0; //We're released!
		}
	}

	//First, process Ctrl,Alt,Shift Releases!
	if (((oldshiftstatus&SHIFTSTATUS_CTRL)>0) && (!(shiftstatus&SHIFTSTATUS_CTRL))) //Released CTRL?
	{
		onKeyPress("lctrl");
	}
	if (((oldshiftstatus&SHIFTSTATUS_ALT)>0) && (!(shiftstatus&SHIFTSTATUS_ALT))) //Released ALT?
	{
		onKeyPress("lalt");
	}
	if (((oldshiftstatus&SHIFTSTATUS_SHIFT)>0) && (!(shiftstatus&SHIFTSTATUS_SHIFT))) //Released SHIFT?
	{
		onKeyPress("lshift");
	}
	//Next, process Ctrl,Alt,Shift presses!
	if ((shiftstatus&SHIFTSTATUS_CTRL)>0) //Pressed CTRL?
	{
		onKeyRelease("lctrl");
	}
	if ((shiftstatus&SHIFTSTATUS_ALT)>0) //Pressed ALT?
	{
		onKeyRelease("lalt");
	}
	if ((shiftstatus&SHIFTSTATUS_SHIFT)>0) //Pressed SHIFT?
	{
		onKeyRelease("lshift");
	}
	oldshiftstatus = shiftstatus; //Save shift status to old shift status!
	//Next, process the keys!
	for (i=0;i<10;i++) //Process all keys!
	{
		if (keys[i]!=-1) //Action?
		{
			char keyname[256]; //For storing the name of the key!
			if (EMU_keyboard_handler_idtoname(keys[i],&keyname[0])) //Gotten ID (valid key)?
			{
				if (keys_pressed[i]) //Pressed?
				{
					onKeyPress(keyname); //Press the key!
				}
				else //Released?
				{
					onKeyRelease(keyname); //Release the key!
				}
			}
		}
	}
}

OPTINLINE void handleKeyPressRelease(int key)
{
	switch (emu_keys_state[key]) //What state are we in?
	{
	case 0: //Released?
		break;
	case 1: //Pressed?
		onKeyPress(&keys_names[key][0]); //Tick the keypress!
		break;
	case 2: //Releasing?
		onKeyRelease(&keys_names[key][0]); //Handle key release!
		emu_keys_state[key] = 0; //We're released!
		break;
	default: //Unknown?
		break;
	}
}

void keyboard_type_handler() //Handles keyboard typing: we're an interrupt!
{
	lock(LOCK_INPUT);
	if (!Direct_Input) //Not executing direct input?
	{
		if (input_enabled && ALLOW_INPUT) //Input enabled?
		{
			get_analog_state(&curstat); //Get the analog&buttons status for the keyboard!
			//Determine stuff for output!
			//Don't process shift atm!

			if (curstat.gamingmode) //Gaming mode?
			{
				handleGaming(); //Handle gaming input?
			}
			else //Normal input mode?
			{
				switch (curstat.mode) //What input mode?
				{
				case 0: //Mouse mode?
					handleKeyboardMouse(); //Handle keyboard input during mouse operations?
					break;
				case 1: //Keyboard mode?
					handleKeyboard(); //Handle keyboard input?
					break;
				default: //Unknown state?
					curstat.mode = 0; //Reset mode!
					break;
				}
			}
		}
	} //Input enabled?
	else //Direct input?
	{
		int key;
		for (key = 0;key < (int)NUMITEMS(emu_keys_state);)
		{
			handleKeyPressRelease(key++); //Handle key press or release!
		}
	}
	tickPendingKeys(); //Handle any pending keys if possible!
	unlock(LOCK_INPUT);
}

void setMouseRate(float packetspersecond)
{
	mouse_interval = (1000000.0f/packetspersecond); //Handles mouse input: we're a normal timer!
}

int KEYBOARD_STARTED = 0; //Default not started yet!
void psp_keyboard_init()
{
	if (__HW_DISABLED) return; //Abort!
	//dolog("osk","Keyboard init...");
	lock(LOCK_INPUT);
	input_enabled = 0; //Default: input disabled!
	oldshiftstatus = 0; //Init!
	curstat.mode = DEFAULT_KEYBOARD; //Keyboard mode enforced by default for now!

	if (!KEYBOARD_ENABLED) //Keyboard disabled?
	{
		unlock(LOCK_INPUT);
		return; //Keyboard disabled?
	}

	initEMUKeyboard(); //Initialise the keyboard support!
	unlock(LOCK_INPUT);

	
	//dolog("osk","Starting type handler");
	//dolog("osk","Starting swap handler");
	addtimer(3.0f,&keyboard_swap_handler,"Keyboard PSP Swap",1,1,NULL); //Handles keyboard set swapping: we're an interrupt!
	
	lock(LOCK_INPUT);
	setMouseRate(1.0f); //No mouse atm, so default to 1 packet/second!
	//dolog("osk","Starting mouse handler");
	KEYBOARD_STARTED = 1; //Started!
	//dolog("osk","keyboard&mouse ready.");
	unlock(LOCK_INPUT);
}

void psp_keyboard_done()
{
	if (__HW_DISABLED) return; //Abort!
	removetimer("Keyboard PSP Type"); //No typing!
	removetimer("Keyboard PSP Swap"); //No swapping!
	removetimer("PSP Mouse"); //No mouse!
}

void keyboard_loadDefaultColor(byte color)
{
	switch (color)
	{
	case 0:
		BIOS_Settings.input_settings.colors[0] = 0x1; //Blue font!
		break;
	case 1:
		BIOS_Settings.input_settings.colors[1] = 0x8; //Dark gray border inactive!
		break;
	case 2:
		BIOS_Settings.input_settings.colors[2] = 0xE; //Yellow border active!
		break;
	case 3:
		BIOS_Settings.input_settings.colors[3] = 0x7; //Special: Brown font!
		break;
	case 4:
		BIOS_Settings.input_settings.colors[4] = 0x6; //Special: Dark gray border inactive!
		break;
	case 5:
		BIOS_Settings.input_settings.colors[5] = 0xE; //Special: Yellow border active!
		break;
	default: //Unknown color?
		break;
	}
}

void keyboard_loadDefaults() //Load the defaults for the keyboard font etc.!
{
	BIOS_Settings.input_settings.analog_minrange = (int)(127/2); //Default to half to use!
	//Default: no game mode mappings!
	//Standard keys:
	int i;
	for (i = 0; i < 6; i++) keyboard_loadDefaultColor(i); //Load all default colors!
	for (i = 0; i<(int)NUMITEMS(BIOS_Settings.input_settings.keyboard_gamemodemappings); i++) //Process all keymappings!
	{
		BIOS_Settings.input_settings.keyboard_gamemodemappings[i] = -1; //Disable by default!
		BIOS_Settings.input_settings.keyboard_gamemodemappings_alt[i] = -1; //Disable by default!
	}
}

struct
{
byte input_buffer_shift;
int input_buffer;
int input_buffer_input;
int input_enabled;
} SAVED_KEYBOARD_STATUS;

void save_keyboard_status() //Save keyboard status to memory!
{
	lock(LOCK_INPUT);
	SAVED_KEYBOARD_STATUS.input_buffer_shift = input_buffer_shift;
	SAVED_KEYBOARD_STATUS.input_buffer = input_buffer;
	SAVED_KEYBOARD_STATUS.input_buffer_input = input_buffer_input;
	SAVED_KEYBOARD_STATUS.input_enabled = input_enabled; //Save all!
	unlock(LOCK_INPUT);
}

void load_keyboard_status() //Load keyboard status from memory!
{
	lock(LOCK_INPUT);
	input_buffer_shift = SAVED_KEYBOARD_STATUS.input_buffer_shift;
	input_buffer = SAVED_KEYBOARD_STATUS.input_buffer;
	input_buffer_input = SAVED_KEYBOARD_STATUS.input_buffer_input;
	input_enabled = SAVED_KEYBOARD_STATUS.input_enabled; //Load all that was saved!
	unlock(LOCK_INPUT);
}

void disableKeyboard() //Disables the keyboard/mouse functionality!
{
	lock(LOCK_INPUT);
	input_enabled = 0; //Disable input!
	unlock(LOCK_INPUT);
}

void enableKeyboard(int bufferinput) //Enables the keyboard/mouse functionnality param: to buffer into input_buffer?!
{
	disableKeyboard(); //Make sure the keyboard if off to start with!
	lock(LOCK_INPUT);
	input_buffer = -1; //Nothing pressed yet!
	input_buffer_shift = -1; //Shift status: nothing pressed yet!
	input_buffer_input = bufferinput; //To buffer?
	input_enabled = ALLOW_INPUT; //Enable input!
	unlock(LOCK_INPUT);
}

/* All update functionality for input */

SDL_Joystick *joystick; //Our joystick!

byte precisemousemovement = 0; //Precise mouse movement enabled?

void updateMOD()
{
	const float precisemovement = 0.2f; //Precise mouse movement constant!
	if (input.cas&CAS_CTRL) //Ctrl pressed?
	{
		input.Buttons |= BUTTON_HOME; //Pressed!
	}
	else
	{
		input.Buttons &= ~BUTTON_HOME; //Released!
	}

	sword axis;
	axis = 0; //Init!
	if (input.keyboardjoy_direction&1) //Up?
	{
		axis -= 32768; //Decrease
	}
	if (input.keyboardjoy_direction&2) //Down?
	{
		axis += 32767; //Increase!
	}
	input.Ly = axis; //Vertical axis!

	axis = 0; //Init!
	if (input.keyboardjoy_direction&4) //Left?
	{
		axis -= 32768; //Decrease
	}
	if (input.keyboardjoy_direction&8) //Right?
	{
		axis += 32767; //Increase!
	}
	input.Lx = axis; //Horizontal axis!
	if (precisemousemovement) //Enable precise movement?
	{
		input.Lx = (sword)(input.Lx*precisemovement); //Enable precise movement!
		input.Ly *= (sword)(input.Ly*precisemovement); //Enable precise movement!
	}
}

byte DirectInput_Middle = 0; //Is direct input toggled by middle mouse button?

//Toggle direct input on/off!
void toggleDirectInput(byte middlebutton)
{
	Direct_Input = !Direct_Input; //Toggle direct input!
	if (Direct_Input) //Enabled?
	{
		DirectInput_Middle = middlebutton; //Are we toggled on by the middle mouse button?
		SDL_WM_GrabInput(SDL_GRAB_ON); //Grab the mouse!
		SDL_ShowCursor(SDL_DISABLE); //Don't show the cursor!
	}
	else //Disabled?
	{
		DirectInput_Middle = 0; //Reset middle mouse button flag!
		SDL_WM_GrabInput(SDL_GRAB_OFF); //Don't grab the mouse!
		SDL_ShowCursor(SDL_ENABLE); //Show the cursor!
	}

	//Also disable mouse buttons pressed!
	Mouse_buttons &= ~3; //Disable left/right mouse buttons!
}

byte haswindowactive = 1; //Are we displayed on-screen?
byte hasmousefocus = 1; //Do we have mouse focus?
byte hasinputfocus = 1; //Do we have input focus?

void updateInput(SDL_Event *event) //Update all input!
{
	static byte RALT = 0;
	switch (event->type)
	{
	case SDL_KEYUP: //Keyboard up?
		lock(LOCK_INPUT); //Wait!
			if (!(SDL_NumJoysticks() && (SDL_JoystickNumButtons(joystick) >= 14)) && hasinputfocus) //Gotten no joystick?
			{
				switch (event->key.keysym.sym) //What key?
				{
					//Special first
				case SDLK_LCTRL: //LCTRL!
					input.cas &= ~CAS_LCTRL; // Released!
					break;
				case SDLK_RCTRL: //RCTRL!
					input.cas &= ~CAS_RCTRL; //Released!
					break;
				case SDLK_LALT: //LALT!
					input.cas &= ~CAS_LALT; //Released!
					break;
				case SDLK_RALT: //RALT!
					input.cas &= ~CAS_RALT; //Pressed!
					RALT = 0; //RALT is released!
					break;
				case SDLK_LSHIFT: //LSHIFT!
					input.cas &= ~CAS_LSHIFT; //Pressed!
					break;
				case SDLK_RSHIFT: //RSHIFT!
					input.cas &= ~CAS_RCTRL; //Pressed!
					break;

					//Normal keys
				case SDLK_BACKSLASH: //HOLD?
					input.Buttons &= ~BUTTON_HOLD; //Pressed!
					break;
				case SDLK_BACKSPACE: //SELECT?
					input.Buttons &= ~BUTTON_SELECT; //Pressed!
					break;
				case SDLK_RETURN: //START?
					input.Buttons &= ~BUTTON_START; //Pressed!
					if (RALT) //RALT pressed too?
					{
						GPU.fullscreen = !GPU.fullscreen; //Toggle fullscreen!
						updateVideo(); //Force an update of video!
					}
					break;
				case SDLK_UP: //UP?
					input.Buttons &= ~BUTTON_UP; //Pressed!
					break;
				case SDLK_DOWN: //DOWN?
					input.Buttons &= ~BUTTON_DOWN; //Pressed!
					break;
				case SDLK_LEFT: //LEFT?
					input.Buttons &= ~BUTTON_LEFT; //Pressed!
					break;
				case SDLK_RIGHT: //RIGHT?
					input.Buttons &= ~BUTTON_RIGHT; //Pressed!
					break;
				case SDLK_q: //LTRIGGER?
					input.Buttons &= ~BUTTON_LTRIGGER; //Pressed!
					break;
				case SDLK_w: //RTRIGGER?
					input.Buttons &= ~BUTTON_RTRIGGER; //Pressed!
					break;
				case SDLK_i: //Joy up?
					input.keyboardjoy_direction &= ~1; //Up!
					break;
				case SDLK_j: //Joy left?
					input.keyboardjoy_direction &= ~4; //Left!
					break;
				case SDLK_k: //Joy down?
					input.keyboardjoy_direction &= ~2; //Down!
					break;
				case SDLK_l: //Joy right?
					input.keyboardjoy_direction &= ~8; //Down!
					break;
				case SDLK_KP8: //TRIANGLE?
					input.Buttons &= ~BUTTON_TRIANGLE; //Pressed!
					break;
				case SDLK_KP4: //SQUARE?
					input.Buttons &= ~BUTTON_SQUARE; //Pressed!
					break;
				case SDLK_KP6: //CIRCLE?
					input.Buttons &= ~BUTTON_CIRCLE; //Pressed!
					break;
				case SDLK_KP2: //CROSS?
					input.Buttons &= ~BUTTON_CROSS; //Pressed!
					break;
				case SDLK_t: //Precise mouse movement?
					precisemousemovement = 0; //Disabled!
					break;
				case SDLK_F4: //F4?
					if (RALT) //ALT-F4?
					{
						SDL_Event quitevent;
						quitevent.quit.type = SDL_QUIT; //Add a quit to the queue!
						SDL_PushEvent(&quitevent); //Add an quit event!
					}
					break;
				default: //Unknown?
					break;
				}
				if (event->key.keysym.scancode == 34) //Play/pause?
				{
					input.Buttons &= ~BUTTON_PLAY; //Play button!
				}
				if (event->key.keysym.scancode == 36) //Stop?
				{
					input.Buttons &= ~BUTTON_STOP; //Stop button!
				}
				if (Direct_Input)
				{
					if (EMU_RUNNING) //Are we running?
					{
						input.Buttons = 0; //Ingore pressed buttons!
						input.cas = 0; //Ignore pressed buttons!
									   //Handle button press/releases!
						register int index;
						register int key;
						index = signed2unsigned16(event->key.keysym.sym); //Load the index to use!
						if (index<(int)NUMITEMS(emu_keys_sdl_rev)) //Valid key to lookup?
						{
							if ((key = emu_keys_sdl_rev[index]) != -1) //Valid key?
							{
								emu_keys_state[key] = 2; //We're released!
							}
						}
					}
				}
				updateMOD(); //Update rest keys!
				unlock(LOCK_INPUT);
			}
		break;
	case SDL_KEYDOWN: //Keyboard down?
		if (!(SDL_NumJoysticks() && (SDL_JoystickNumButtons(joystick) >= 14)) && hasinputfocus) //Gotten no joystick?
		{
			lock(LOCK_INPUT);
				switch (event->key.keysym.sym) //What key?
				{
					//Special first
				case SDLK_LCTRL: //LCTRL!
					input.cas |= CAS_LCTRL; //Pressed!
					break;
				case SDLK_RCTRL: //RCTRL!
					input.cas |= CAS_RCTRL; //Pressed!
					break;
				case SDLK_LALT: //LALT!
					input.cas |= CAS_LALT; //Pressed!
					break;
				case SDLK_RALT: //RALT!
					RALT = 1; //RALT is pressed!
					input.cas |= CAS_RALT; //Pressed!
					break;
				case SDLK_LSHIFT: //LSHIFT!
					input.cas |= CAS_LSHIFT; //Pressed!
					break;
				case SDLK_RSHIFT: //RSHIFT!
					input.cas |= CAS_RSHIFT; //Pressed!
					break;

				case SDLK_BACKSLASH: //HOLD?
					input.Buttons |= BUTTON_HOLD; //Pressed!
					break;
				case SDLK_BACKSPACE: //SELECT?
					input.Buttons |= BUTTON_SELECT; //Pressed!
					break;
				case SDLK_RETURN: //START?
					input.Buttons |= BUTTON_START; //Pressed!
					break;
				case SDLK_UP: //UP?
					input.Buttons |= BUTTON_UP; //Pressed!
					break;
				case SDLK_DOWN: //DOWN?
					input.Buttons |= BUTTON_DOWN; //Pressed!
					break;
				case SDLK_LEFT: //LEFT?
					input.Buttons |= BUTTON_LEFT; //Pressed!
					break;
				case SDLK_RIGHT: //RIGHT?
					input.Buttons |= BUTTON_RIGHT; //Pressed!
					break;
				case SDLK_q: //LTRIGGER?
					input.Buttons |= BUTTON_LTRIGGER; //Pressed!
					break;
				case SDLK_w: //RTRIGGER?
					input.Buttons |= BUTTON_RTRIGGER; //Pressed!
					break;
				case SDLK_i: //Joy up?
					input.keyboardjoy_direction |= 1; //Up!
					break;
				case SDLK_j: //Joy left?
					input.keyboardjoy_direction |= 4; //Left!
					input.Buttons |= BUTTON_STOP; //Stop!
					break;
				case SDLK_k: //Joy down?
					input.keyboardjoy_direction |= 2; //Down!
					break;
				case SDLK_l: //Joy right?
					input.keyboardjoy_direction |= 8; //Down!
					break;
				case SDLK_KP8: //TRIANGLE?
					input.Buttons |= BUTTON_TRIANGLE; //Pressed!
					break;
				case SDLK_KP4: //SQUARE?
					input.Buttons |= BUTTON_SQUARE; //Pressed!
					break;
				case SDLK_KP6: //CIRCLE?
					input.Buttons |= BUTTON_CIRCLE; //Pressed!
					break;
				case SDLK_KP2: //CROSS?
					input.Buttons |= BUTTON_CROSS; //Pressed!
					break;
				case SDLK_t: //Precise mouse movement?
					precisemousemovement = 1; //Enabled!
					break;
				default: //Unknown key?
					break;
				}
				if (event->key.keysym.scancode == 34) //Play/pause?
				{
					input.Buttons |= BUTTON_PLAY; //Play button!
				}
				if (event->key.keysym.scancode == 36) //Stop?
				{
					input.Buttons |= BUTTON_STOP; //Stop button!
				}
				if (Direct_Input)
				{
					if (EMU_RUNNING) //Are we running?
					{
						input.Buttons = 0; //Ingore pressed buttons!
						input.cas = 0; //Ignore pressed buttons!

						//Handle button press/releases!
						register int index;
						register int key;
						index = signed2unsigned16(event->key.keysym.sym); //Load the index to use!
						if (index<(int)NUMITEMS(emu_keys_sdl_rev)) //Valid key to lookup?
						{
							if ((key = emu_keys_sdl_rev[index]) != -1) //Valid key?
							{
								emu_keys_state[key] = 1; //We're pressed!
							}
						}
					}
				}
				updateMOD(); //Update rest keys!
				unlock(LOCK_INPUT);
			}
			break;
		case SDL_JOYAXISMOTION:  /* Handle Joystick Motion */
			if (SDL_NumJoysticks() && (SDL_JoystickNumButtons(joystick)>=14) && hasinputfocus) //Gotten a joystick?
			{
				lock(LOCK_INPUT);
				switch ( event->jaxis.axis)
				{
					case 0: /* Left-right movement code goes here */
						input.Lx = event->jaxis.value; //New value!
						break;
					case 1: /* Up-Down movement code goes here */
						input.Ly = event->jaxis.value; //New value!
						break;
				}
				if (Direct_Input)
				{
					if (EMU_RUNNING) //Are we running?
					{
						input.Lx = input.Ly = 0; //Ignore pressed buttons!
					}
				}
				unlock(LOCK_INPUT);
			}
			break;
		case SDL_JOYBUTTONDOWN:  /* Handle Joystick Button Presses */
			if (SDL_NumJoysticks() && (SDL_JoystickNumButtons(joystick)>=14) && hasinputfocus) //Gotten a joystick?
			{
				lock(LOCK_INPUT);
				switch (event->jbutton.button) //What button?
				{
					case INPUT_BUTTON_TRIANGLE:
						input.Buttons |= BUTTON_TRIANGLE; //Press!
						break;
					case INPUT_BUTTON_SQUARE:
						input.Buttons |= BUTTON_SQUARE; //Press!
						break;
					case INPUT_BUTTON_CROSS:
						input.Buttons |= BUTTON_CROSS; //Press!
						break;
					case INPUT_BUTTON_CIRCLE:
						input.Buttons |= BUTTON_CIRCLE; //Press!
						break;
					case INPUT_BUTTON_LTRIGGER:
						input.Buttons |= BUTTON_LTRIGGER; //Press!
						break;
					case INPUT_BUTTON_RTRIGGER:
						input.Buttons |= BUTTON_RTRIGGER; //Press!
						break;
					case INPUT_BUTTON_SELECT:
						input.Buttons |= BUTTON_SELECT; //Press!
						break;
					case INPUT_BUTTON_START:
						input.Buttons |= BUTTON_START; //Press!
						break;
					case INPUT_BUTTON_HOME:
						input.Buttons |= BUTTON_HOME; //Press!
						break;
					case INPUT_BUTTON_HOLD:
						input.Buttons |= BUTTON_HOLD; //Press!
						break;
					case INPUT_BUTTON_UP:
						input.Buttons |= BUTTON_UP; //Press!
						break;
					case INPUT_BUTTON_DOWN:
						input.Buttons |= BUTTON_DOWN; //Press!
						break;
					case INPUT_BUTTON_LEFT:
						input.Buttons |= BUTTON_LEFT; //Press!
						break;
					case INPUT_BUTTON_RIGHT:
						input.Buttons |= BUTTON_RIGHT; //Press!
						break;
					default: //Unknown button?
						break;
				}
				unlock(LOCK_INPUT);
			}
			break;
		case SDL_JOYBUTTONUP:  /* Handle Joystick Button Releases */
			if (SDL_NumJoysticks() && (SDL_JoystickNumButtons(joystick)>=14) && hasinputfocus) //Gotten a joystick?
			{
				lock(LOCK_INPUT);
				switch (event->jbutton.button) //What button?
				{
					case INPUT_BUTTON_TRIANGLE:
						input.Buttons &= ~BUTTON_TRIANGLE; //Release!
						break;
					case INPUT_BUTTON_SQUARE:
						input.Buttons &= ~BUTTON_SQUARE; //Release!
						break;
					case INPUT_BUTTON_CROSS:
						input.Buttons &= ~BUTTON_CROSS; //Release!
						break;
					case INPUT_BUTTON_CIRCLE:
						input.Buttons &= ~BUTTON_CIRCLE; //Release!
						break;
					case INPUT_BUTTON_LTRIGGER:
						input.Buttons &= ~BUTTON_LTRIGGER; //Release!
						break;
					case INPUT_BUTTON_RTRIGGER:
						input.Buttons &= ~BUTTON_RTRIGGER; //Release!
						break;
					case INPUT_BUTTON_SELECT:
						input.Buttons &= ~BUTTON_SELECT; //Release!
						break;
					case INPUT_BUTTON_START:
						input.Buttons &= ~BUTTON_START; //Release!
						break;
					case INPUT_BUTTON_HOME:
						input.Buttons &= ~BUTTON_HOME; //Release!
						break;
					case INPUT_BUTTON_HOLD:
						input.Buttons &= ~BUTTON_HOLD; //Release!
						break;
					case INPUT_BUTTON_UP:
						input.Buttons &= ~BUTTON_UP; //Release!
						break;
					case INPUT_BUTTON_DOWN:
						input.Buttons &= ~BUTTON_DOWN; //Release!
						break;
					case INPUT_BUTTON_LEFT:
						input.Buttons &= ~BUTTON_LEFT; //Release!
						break;
					case INPUT_BUTTON_RIGHT:
						input.Buttons &= ~BUTTON_RIGHT; //Release!
						break;
					default: //Unknown button?
						break;
				}
				unlock(LOCK_INPUT);
			}
			break;
		case SDL_MOUSEBUTTONDOWN: //Button pressed?
			if (hasmousefocus) //Do we have mouse focus?
			{
				lock(LOCK_INPUT);
				switch (event->button.button) //What button?
				{
				case SDL_BUTTON_LEFT:
					mousebuttons |= 1; //Left pressed!
					if (Direct_Input) //Direct input enabled?
					{
						Mouse_buttons |= 1; //Left mouse button pressed!
					}
					break;
				case SDL_BUTTON_RIGHT:
					mousebuttons |= 2; //Right pressed!
					if (Direct_Input) //Direct input enabled?
					{
						Mouse_buttons |= 2; //Right mouse button pressed!
					}
					break;
				}
				unlock(LOCK_INPUT);
			}
			break;
		case SDL_MOUSEBUTTONUP: //Special mouse button action?
			if (hasmousefocus)
			{
				lock(LOCK_INPUT);
				switch (event->button.button) //What button?
				{
				case SDL_BUTTON_MIDDLE: //Middle released!
					toggleDirectInput(1); //Toggle direct input by middle button!
					break;
				case SDL_BUTTON_LEFT:
					if ((mousebuttons==3) && (!DirectInput_Middle)) //Were we both pressed? Special action when not enabled by middle mouse button!
					{
						toggleDirectInput(0); //Toggle direct input by both buttons!
					}
					mousebuttons &= ~1; //Left released!
					if (Direct_Input)
					{
						Mouse_buttons &= ~1; //button released!
					}
					break;
				case SDL_BUTTON_RIGHT:
					if ((mousebuttons == 3) && (!DirectInput_Middle)) //Were we both pressed? Special action when not enabled by middle mouse button!
					{
						toggleDirectInput(0); //Toggle direct input by both buttons!
					}
					mousebuttons &= ~2; //Right released!
					if (Direct_Input)
					{
						Mouse_buttons &= ~2; //button released!
					}
					break;
				}
				unlock(LOCK_INPUT);
			}
			break;
		case SDL_MOUSEMOTION: //Mouse moved?
			lock(LOCK_INPUT);
			if (Direct_Input && hasmousefocus) //Direct input?
			{
				mouse_xmove += event->motion.xrel; //Move the mouse horizontally!
				mouse_ymove += event->motion.yrel; //Move the mouse vertically!
			}
			unlock(LOCK_INPUT);
			break;
		case SDL_QUIT: //Quit?
			SDL_JoystickClose(joystick); //Finish our joystick!
			break;
		case SDL_ACTIVEEVENT: //Window event?
			if (event->active.state&SDL_APPMOUSEFOCUS)
			{
				hasmousefocus = event->active.gain; //Do we have mouse focus?
			}
			if (event->active.state&SDL_APPINPUTFOCUS) //Gain/lose keyboard focus?
			{
				hasinputfocus = event->active.gain; //Do we have input focus?
			}
			if (event->active.state&SDL_APPACTIVE) //Iconified/Restored?
			{
				haswindowactive = event->active.gain; //0=Iconified, 1=Restored.
			}
			break;
	}
}

//Check for timer occurrences.
void cleanKeyboard()
{
	cleanEMUKeyboard(); //Untick the timer!
}

void updateKeyboard()
{
	keyboard_type_handler(); //Tick the timer!
}

TicksHolder mouse_ticker;
float mouse_ticktiming;

//Check for timer occurrences.
void cleanMouse()
{
	getuspassed(&mouse_ticker); //Discard the amount of time passed!
}

void updateMouse()
{
	mouse_ticktiming += (float)getuspassed(&mouse_ticker); //Get the amount of time passed!
	if (mouse_ticktiming >= mouse_interval) //Enough time passed?
	{
		for (;mouse_ticktiming >= mouse_interval;) //All that's left!
		{
			mouse_handler(); //Tick mouse timer!
			mouse_ticktiming -= mouse_interval; //Decrease timer to get time left!
		}
	}
}

void psp_input_init()
{
	uint_32 i;
	#ifdef SDL_SYS_JoystickInit
		//Gotten initialiser for joystick?
		if (SDL_SYS_JoystickInit()==-1) quitemu(0); //No joystick present!
	#endif
	SDL_JoystickEventState(SDL_ENABLE);
	joystick = SDL_JoystickOpen(0); //Open our joystick!
	for (i = 0;i < NUMITEMS(emu_keys_sdl_rev);i++) //Initialise all keys!
	{
		emu_keys_sdl_rev[i] = -1; //Default to unused!
		++i; //Next!
	}
	for (i = 0;i < NUMITEMS(emu_keys_SDL);) //Process all keys!
	{
		emu_keys_sdl_rev[signed2unsigned16(emu_keys_SDL[i])] = i; //Reverse lookup of the table!
		++i; //Next!
	}
	SDL_EnableKeyRepeat(0,0); //Don't repeat pressed keys!
}

void psp_input_done()
{
	//Do nothing for now!
}