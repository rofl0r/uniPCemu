#include "headers/types.h"
#include "headers/emu/input.h" //Own typedefs.
#include "headers/bios/bios.h" //BIOS Settings for keyboard input!
#include "headers/hardware/vga.h" //VGA for simple 8x8 font!
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
byte input_buffer_input = 0; //To buffer, instead of straigt into emulation (giving the below value the key index)?
int input_buffer_shift = -1; //Ctrl-Shift-Alt Status for the pressed key!
int input_buffer = -1; //To contain the pressed key!

SceCtrlData input; //Current input status!

PSP_INPUTSTATE curstat; //Current status!

void updateInput() //Update all input!
{
	/*for (;;)
	{*/
		SceCtrlData temp; //Current input status!
		if (sceCtrlReadBufferPositive(&temp, 1)) //Anything happened!
		{
			memcpy(&input,&temp,sizeof(input)); //Copy to actived input!
		}
		/*delay(100000); //Refresh 10x/second.
	}*/
}

int psp_inputkey() //Simple key sampling!
{
	updateInput(); //Update the input!
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
			if (counter>INPUTKEYDELAYSTEP) //Big block?
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
	result = psp_inputkeydelay(1); //Check for input!
	while (result==0) //No input?
	{
		result = psp_inputkeydelay(1); //Check for input!
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
	updateInput(); //Make sure our input is up-to-date!
	//Clear all we set!
	state->analogdirection_mouse_x = 0; //No mouse X movement!
	state->analogdirection_mouse_y = 0; //No mouse Y movement!
	state->buttonpress = 0; //No buttons pressed!
	state->analogdirection_keyboard_x = 0; //No keyboard X movement!
	state->analogdirection_keyboard_y = 0; //No keyboard Y movement!
	//We preserve the input mode and gaming mode flags, to be able to go back when using gaming mode!

	
	int x; //Analog x!
	int y; //Analog y!
	x = input.Lx-128; //Convert to signed!
	y = input.Ly-128; //Convert to signed!
	
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

		if ((input.Buttons&PSP_CTRL_TRIANGLE)>0) //Triangle?
		{
			state->buttonpress |= 2; //Triangle!
		}
		if ((input.Buttons&PSP_CTRL_SQUARE)>0) //Square?
		{
			state->buttonpress |= 1; //Square!
		}
		if ((input.Buttons&PSP_CTRL_CROSS)>0) //Cross?
		{
			state->buttonpress |= 8; //Cross!
		}
		if ((input.Buttons&PSP_CTRL_CIRCLE)>0) //Circle?
		{
			state->buttonpress |= 4; //Circle!
		}

		if ((input.Buttons&PSP_CTRL_LEFT)>0) //Left?
		{
			state->buttonpress |= 16; //Left!
		}

		if ((input.Buttons&PSP_CTRL_UP)>0) //Up?
		{
			state->buttonpress |= 32; //Up!
		}

		if ((input.Buttons&PSP_CTRL_RIGHT)>0) //Right?
		{
			state->buttonpress |= 64; //Right!
		}

		if ((input.Buttons&PSP_CTRL_DOWN)>0) //Down?
		{
			state->buttonpress |= 128; //Down!
		}

		if ((input.Buttons&PSP_CTRL_LTRIGGER)>0) //L?
		{
			state->buttonpress |= 256; //L!
		}

		if ((input.Buttons&PSP_CTRL_RTRIGGER)>0) //R?
		{
			state->buttonpress |= 512; //R!
		}

		if ((input.Buttons&PSP_CTRL_START)>0) //START?
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
			if ((input.Buttons&PSP_CTRL_TRIANGLE)>0) //Triangle?
			{
				state->buttonpress |= 2; //Triangle!
			}
			else if ((input.Buttons&PSP_CTRL_SQUARE)>0) //Square?
			{
				state->buttonpress |= 1; //Square!
			}
			else if ((input.Buttons&PSP_CTRL_CROSS)>0) //Cross?
			{
				state->buttonpress |= 8; //Cross!
			}
			else if ((input.Buttons&PSP_CTRL_CIRCLE)>0) //Circle?
			{
				state->buttonpress |= 4; //Circle!
			}
	
			if ((input.Buttons&PSP_CTRL_LEFT)>0) //Left?
			{
				state->buttonpress |= 16; //Left!
			}
	
			if ((input.Buttons&PSP_CTRL_UP)>0) //Up?
			{
				state->buttonpress |= 32; //Up!
			}
	
			if ((input.Buttons&PSP_CTRL_RIGHT)>0) //Right?
			{
				state->buttonpress |= 64; //Right!
			}
	
			if ((input.Buttons&PSP_CTRL_DOWN)>0) //Down?
			{
				state->buttonpress |= 128; //Down!
			}
			
			if ((input.Buttons&PSP_CTRL_LTRIGGER)>0) //L?
			{
				state->buttonpress |= 256; //L!
			}
	
			if ((input.Buttons&PSP_CTRL_RTRIGGER)>0) //R?
			{
				state->buttonpress |= 512; //R!
			}		
			
			if ((input.Buttons&PSP_CTRL_START)) //START?
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
			if ((input.Buttons&PSP_CTRL_TRIANGLE)>0) //Triangle?
			{
				state->buttonpress |= 2; //Triangle!
			}
			else if ((input.Buttons&PSP_CTRL_SQUARE)>0) //Square?
			{
				state->buttonpress |= 1; //Square!
			}
			else if ((input.Buttons&PSP_CTRL_CROSS)>0) //Cross?
			{
				state->buttonpress |= 8; //Cross!
			}
			else if ((input.Buttons&PSP_CTRL_CIRCLE)>0) //Circle?
			{
				state->buttonpress |= 4; //Circle!
			}
	
			if ((input.Buttons&PSP_CTRL_LEFT)>0) //Left?
			{
				state->buttonpress |= 16; //Left!
			}
	
			if ((input.Buttons&PSP_CTRL_UP)>0) //Up?
			{
				state->buttonpress |= 32; //Up!
			}
	
			if ((input.Buttons&PSP_CTRL_RIGHT)>0) //Right?
			{
				state->buttonpress |= 64; //Right!
			}
	
			if ((input.Buttons&PSP_CTRL_DOWN)>0) //Down?
			{
				state->buttonpress |= 128; //Down!
			}
	
			if ((input.Buttons&PSP_CTRL_LTRIGGER)>0) //L?
			{
				state->buttonpress |= 256; //L!
			}
	
			if ((input.Buttons&PSP_CTRL_RTRIGGER)>0) //R?
			{
				state->buttonpress |= 512; //R!
			}
			
			if ((input.Buttons&PSP_CTRL_START)) //START?
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
		if (useMouseTimer()) //We're using the mouse?
		{
			MOUSE_PACKET *mousepacket = (MOUSE_PACKET *)zalloc(sizeof(MOUSE_PACKET),"Mouse_Packet"); //Allocate a mouse packet!
		
			if (mousepacket) //Not running out of memory?
			{
				//Fill the mouse data!
				if (!curstat.gamingmode || 
					(curstat.gamingmode &&
					 (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGLEFT]==-1) &&
					 (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGUP]==-1) &&
					 (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGRIGHT]==-1) &&
					 (BIOS_Settings.input_settings.keyboard_gamemodemappings[GAMEMODE_ANALOGDOWN]==-1)
					)) //Not gaming mode OR gaming mode with no keyboard mappings: use mouse movement!
				{
					mousepacket->xmove = curstat.analogdirection_mouse_x; //X movement!
					mousepacket->ymove = curstat.analogdirection_mouse_y; //Y movement!
				}
				else
				{
					mousepacket->xmove = 0; //No movement!
					mousepacket->ymove = 0; //No movement!
				}
				mousepacket->buttons = 0; //Default: no buttons!
				
				if (!curstat.gamingmode) //Not in gaming mode and in mouse mode? emulate left,right,middle mouse button too!
				{
					if (curstat.buttonpress&256) //Left mouse button?
					{
						mousepacket->buttons |= 1; //Left button pressed!
					}
			
					if (curstat.buttonpress&512) //Right mouse button?
					{
						mousepacket->buttons |= 2; //Right button pressed!
					}
			
					if (curstat.buttonpress&2) //Middle mouse button?
					{
						mousepacket->buttons |= 4; //Middle button pressed!
					}
				}
				mouse_packet_handler(mousepacket); //Add the mouse packet!
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

/*
byte keyboard_attribute[KEYBOARD_NUMY][KEYBOARD_NUMX]; //The attribute for each part of display: 0=Font std, 1=Font active (Pressed)!
byte keyboard_display[KEYBOARD_NUMY][KEYBOARD_NUMX]; //The characters to display on-screen!
*/
//Valid values: 0:Display, 1=Font, 2=Border, 3=Active

PSP_TEXTSURFACE *keyboardsurface = NULL; //Framerate surface!

void initKeyboardOSK()
{
	keyboardsurface = alloc_GPUtext(); //Allocate GPU text surface for us to use!
	if (!keyboardsurface) //Couldn't allocate?
	{
		raiseError("GPU","Error allocating OSK layer!");
	}
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

	if (curstat.mode==1 && !curstat.gamingmode) //Keyboard mode?
	{
		if (strcmp((char *)getkeyboard(0,currentset,sety,setx,0),"enable")==0) //Set enabled?
		{
			char *left = getkeyboard(currentshift,currentset,sety,setx,1);
			char *right = getkeyboard(currentshift,currentset,sety,setx,2);
			char *up = getkeyboard(currentshift,currentset,sety,setx,3);
			char *down = getkeyboard(currentshift,currentset,sety,setx,4);

			byte leftloc = CALCMIDDLE(CALCMIDDLE(KEYBOARD_NUMX,0),safe_strlen(left,0)); //Left location!
			byte rightloc = CALCMIDDLE(KEYBOARD_NUMX,0); //Start at the right half!
			rightloc += CALCMIDDLE(CALCMIDDLE(KEYBOARD_NUMX,0),safe_strlen(right,0)); //Right location!
			byte uploc = CALCMIDDLE(KEYBOARD_NUMX,safe_strlen(up,0)); //Up location!
			byte downloc = CALCMIDDLE(KEYBOARD_NUMX,safe_strlen(down,0)); //Down location!

			memcpy(&keyboard_display[0][uploc],up,safe_strlen(up,0)); //Set up!
			if (currentkey==1) //Got a key pressed?
			{
				memset(&keyboard_attribute[0][uploc],1,safe_strlen(up,0)); //Active key!
			}

			memcpy(&keyboard_display[1][leftloc],left,safe_strlen(left,0)); //Set left!
			if (currentkey==2) //Got a key pressed?
			{
				memset(&keyboard_attribute[1][leftloc],1,safe_strlen(left,0)); //Active key!
			}

			memcpy(&keyboard_display[1][rightloc],right,safe_strlen(right,0)); //Set up!
			if (currentkey==4) //Got a key pressed?
			{
				memset(&keyboard_attribute[1][rightloc],1,safe_strlen(right,0)); //Active key!
			}

			memcpy(&keyboard_display[2][downloc],down,safe_strlen(down,0)); //Set up!
			if (currentkey==3) //Got a key pressed?
			{
				memset(&keyboard_attribute[2][downloc],1,safe_strlen(down,0)); //Active key!
			}
		}
	} //Keyboard mode?

	//LEDs on keyboard are always visible!
	
	if (!curstat.gamingmode) //Not gaming mode (both mouse and keyboard mode)?
	{
		keyboard_display[KEYBOARD_NUMY-3][KEYBOARD_NUMX-3] = 'C'; //Screen capture!
		keyboard_display[KEYBOARD_NUMY-3][KEYBOARD_NUMX-2] = 'A'; //Screen capture!
		keyboard_display[KEYBOARD_NUMY-3][KEYBOARD_NUMX-1] = 'P'; //Screen capture!

		if (SCREEN_CAPTURE) //Screen capture status?
		{
			keyboard_attribute[KEYBOARD_NUMY-3][KEYBOARD_NUMX-3] = 3; //Special shift color active!
			keyboard_attribute[KEYBOARD_NUMY-3][KEYBOARD_NUMX-2] = 3; //Special shift color active!
			keyboard_attribute[KEYBOARD_NUMY-3][KEYBOARD_NUMX-1] = 3; //Special shift color active!
		}
		else
		{
			keyboard_attribute[KEYBOARD_NUMY-3][KEYBOARD_NUMX-3] = 2; //Special shift color inactive!
			keyboard_attribute[KEYBOARD_NUMY-3][KEYBOARD_NUMX-2] = 2; //Special shift color inactive!
			keyboard_attribute[KEYBOARD_NUMY-3][KEYBOARD_NUMX-1] = 2; //Special shift color inactive!
		}
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

	if (!curstat.mode && !curstat.gamingmode) //Mouse mode?
	{
		keyboard_display[KEYBOARD_NUMY-1][KEYBOARD_NUMX-4] = 'M'; //Mouse mode!
		keyboard_attribute[KEYBOARD_NUMY-1][KEYBOARD_NUMX-4] = 2; //Special shift color inactive!
	}

	if (!curstat.gamingmode) //Not gaming mode?
	{
		keyboard_display[KEYBOARD_NUMY-1][KEYBOARD_NUMX-3] = 'C'; //Ctrl!
		if (currentctrl)
		{
			keyboard_attribute[KEYBOARD_NUMY-1][KEYBOARD_NUMX-3] = 3; //Special shift color active!
		}
		else
		{
			keyboard_attribute[KEYBOARD_NUMY-1][KEYBOARD_NUMX-3] = 2; //Special shift color inactive!
		}

		keyboard_display[KEYBOARD_NUMY-1][KEYBOARD_NUMX-2] = 'A'; //Alt!
		if (currentalt)
		{
			keyboard_attribute[KEYBOARD_NUMY-1][KEYBOARD_NUMX-2] = 3; //Special shift color active!
		}
		else
		{
			keyboard_attribute[KEYBOARD_NUMY-1][KEYBOARD_NUMX-2] = 2; //Special shift color inactive!
		}

		keyboard_display[KEYBOARD_NUMY-1][KEYBOARD_NUMX-1] = 'S'; //Shift!
		if (currentshift)
		{
			keyboard_attribute[KEYBOARD_NUMY-1][KEYBOARD_NUMX-1] = 3; //Special shift color active!
		}
		else
		{
			keyboard_attribute[KEYBOARD_NUMY-1][KEYBOARD_NUMX-1] = 2; //Special shift color inactive!
		}
	}
	else //Gaming mode?
	{
		keyboard_display[KEYBOARD_NUMY-1][KEYBOARD_NUMX-1] = 'G'; //Gaming mode!
		keyboard_attribute[KEYBOARD_NUMY-1][KEYBOARD_NUMX-1] = 2; //Special shift color inactive!
	}
}

uint_32 keyboard_rendertime; //Time for framerate rendering!

void keyboard_renderer() //Render the keyboard on-screen!
{
	if (!KEYBOARD_ENABLED) return; //Disabled?
	if (!input_enabled) //Keyboard disabled atm OR Gaming mode?
	{
		return; //Keyboard disabled: don't show!
	}

	keyboard_loadDefaults(); //Enfore defaults for now!
	fill_keyboarddisplay(); //Fill the keyboard display!

	int ybase,xbase;
	ybase = GPU_TEXTSURFACE_HEIGHT-KEYBOARD_NUMY; //Base Y on GPU's text screen!
	xbase = GPU_TEXTSURFACE_WIDTH-KEYBOARD_NUMX; //Base X on GPU's text screen!

	int x;
	int y; //The coordinates in the buffer!

	for (y=ybase;y<GPU_TEXTSURFACE_HEIGHT;y++)
	{
		for (x=xbase;x<GPU_TEXTSURFACE_WIDTH;x++)
		{
			uint_32 fontcolor = getemucol16(BIOS_Settings.input_settings.fontcolor); //Use font color by default!
			uint_32 bordercolor = getemucol16(BIOS_Settings.input_settings.bordercolor); //Use border color by default!
			switch (keyboard_attribute[y-ybase][x-xbase]) //What attribute?
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
			GPU_textsetxy(keyboardsurface,x,y,keyboard_display[y-ybase][x-xbase],fontcolor,bordercolor);
		}
	}
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

void keyboard_swap_handler() //Swap handler for keyboard!
{
	//while (1)
	{
		if (input_enabled) //Input enabled?
		{
		if (curstat.gamingmode) //Gaming mode?
		{
			if (psp_inputkey()&PSP_CTRL_SELECT) //Quit gaming mode?
			{
				curstat.gamingmode = 0; //Disable gaming mode!
			}
		}
		else if (curstat.mode==1 && input_enabled) //Keyboard active and on-screen?
		{
			int curkey;
			curkey = psp_inputkey(); //Read current keys with delay!
			if ((curkey&PSP_CTRL_LTRIGGER) && (!(curkey&PSP_CTRL_RTRIGGER))) //Not L&R (which is CAPS LOCK) special?
			{
				currentset = (currentset+1)%3; //Next set!
				currentkey = 0; //No keys pressed!
				//Disable all output still standing!
				ReleaseKeys(); //Release all keys!
			}
			else if (curkey&PSP_CTRL_DOWN) //Down pressed: swap to gaming mode!
			{
				currentkey = 0; //No keys pressed!
				ReleaseKeys(); //Release all keys!
				curstat.gamingmode = 1; //Enable gaming mode!
			}
			else if (curkey&PSP_CTRL_START) //Swap to mouse mode!
			{
				currentkey = 0; //No keys pressed!
				ReleaseKeys(); //Release all keys!
				curstat.mode = 0; //Swap to mouse mode!
			}
		}
		else if (curstat.mode==0 && input_enabled) //Mouse active?
		{
			if (psp_inputkey()&PSP_CTRL_DOWN) //Down pressed: swap to gaming mode!
			{
				curstat.gamingmode = 1; //Enable gaming mode!
			}
			else if (psp_inputkey()&PSP_CTRL_START) //Swap to keyboard mode!
			{
				curstat.mode = 1; //Swap to keyboard mode!
			}
		}
		}
		//delay(KEYSWAP_DELAY); //Wait a bit, for as precise as we can simply (1ms), slower is never worse!
	}
}

byte oldshiftstatus = 0; //Old shift status, used for keyboard/gaming mode!
byte shiftstatus = 0; //New shift status!

void handleKeyboardMouse() //Handles keyboard input during mouse operations!
{
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
	} //Not buffering?
}

void handleKeyboard() //Handles keyboard input!
{
	static int lastkey=0, lastx=0, lasty=0, lastset=0; //Previous key that was pressed!
	setx = curstat.analogdirection_keyboard_x; //X in keyboard set!
	sety = curstat.analogdirection_keyboard_y; //Y in keyboard set!

	//Now handle current keys!
	currentkey = 0; //Default: no key pressed!
	//Order of currentkey: Up left down right.
	if (curstat.buttonpress&1) //Left?
	{
		currentkey = 2; //Pressed square!
	}
	else if (curstat.buttonpress&2) //Up?
	{
		currentkey = 1; //Pressed triangle!
	}
	else if (curstat.buttonpress&4) //Right?
	{
		currentkey = 4; //Circle pressed!
	}
	else if (curstat.buttonpress&8) //Down?
	{
		currentkey = 3; //Cross pressed!
	}

	//Now, process the keys!

	shiftstatus = 0; //Init shift status!
	shiftstatus |= ((curstat.buttonpress&512)>0)*SHIFTSTATUS_SHIFT; //Apply shift status!
	if ((curstat.buttonpress&0x300)==0x300) //L&R hold?
	{
		shiftstatus &= ~SHIFTSTATUS_SHIFT; //Shift isn't pressed: it's CAPS LOCK special case!
	}
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
		
		if ((curstat.buttonpress&0x300)==0x300) //L&R hold? CAPS LOCK PRESSED! (Special case)
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
			if ((lastkey!=currentkey) || (lastx!=setx) || (lasty!=sety) || (lastset!=currentset)) //We had a last key that's different?
			{
				onKeyRelease(getkeyboard(shiftstatus,lastset,lasty,lastx,displaytokeyboard[lastkey])); //Release the last key!
			}
			onKeyPress(getkeyboard(0,currentset,sety,setx,displaytokeyboard[currentkey]));
			//Save the active key information!
			lastset = currentset;
			lastx = setx;
			lasty = sety;
			lastkey = currentkey;
		}
		else if (lastkey) //We have a last key with nothing pressed?
		{
			onKeyRelease(getkeyboard(0,lastset,lasty,lastx,displaytokeyboard[lastkey])); //Release the last key!
			lastkey = 0; //We didn't have a last key!			
		}
	} //Not buffering?
	else //Buffering?
	{
		if (currentkey) //Key pressed?
		{
			//Save the active key information!
			lastset = currentset;
			lastx = setx;
			lasty = sety;
			lastkey = currentkey;
		}
		else if (((lastkey!=currentkey) || (lastx!=setx) || (lasty!=sety) || (lastset!=currentset))) //Key released?
		{
			int key;
			key = EMU_keyboard_handler_nametoid(getkeyboard(0,lastset,lasty,lastx,displaytokeyboard[lastkey])); //Our key?
			if (key!=-1) //Found as a valid key to press?
			{
				input_buffer_shift = shiftstatus; //Set shift status!
				input_buffer = key; //Last key!
			
			}
			//Update current information!
			lastkey = 0; //Update current information!
			lastx = setx;
			lasty = sety;
			lastset = currentset;
		}
		//Key presses aren't buffered: we only want to know the key, nothing more!
	}
}

void handleGaming() //Handles gaming mode input!
{
	//Test for all keys and process!
	if (input_buffer_input) return; //Don't handle buffered input, we don't allow mapping gaming mode to gaming mode!

	int keys[15]; //Key index for this key, -1 for none/nothing happened!
	byte keys_pressed[15] = {0,0,0,0,0,0,0,0,0,0,0}; //We have pressed the key?
	//Order: START, LEFT, UP, RIGHT, DOWN, L, R, TRIANGLE, CIRCLE, CROSS, SQUARE, ANALOGLEFT, ANALOGUP, ANALOGRIGHT, ANALOGDOWN

	int i;
	for (i=0;i<10;i++)
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

int request_type_term = 0;

void keyboard_type_handler() //Handles keyboard typing: we're an interrupt!
{
	//for(;;)
	{
		/*if (request_type_term) //Request termination?
		{
			request_type_term = 0; //Terminated!
			return; //Terminate!
		}*/

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
		} //Input enabled?

		/*if (HWkeyboard_getrepeatrate()) //Got a repeat rate?
		{
			delay((uint_32)(1000000/HWkeyboard_getrepeatrate())); //Wait for the set timespan, depending on the set keyboard by the CPU!
		}
		else
		{
			delay(100000); //Wait for the minimum!
		}*/
	} //While loop, muse be infinite to prevent closing!
}

void psp_keyboard_refreshrate()
{
	float repeatrate = HWkeyboard_getrepeatrate();
	if (!repeatrate) repeatrate = 10.0f; //10 times a second sampling!
	addtimer(repeatrate,&keyboard_type_handler,"Keyboard PSP Type",1); //Our type handler!
}

int KEYBOARD_STARTED = 0; //Default not started yet!
void psp_keyboard_init()
{
	if (__HW_DISABLED) return; //Abort!
	//dolog("osk","Keyboard init...");
	input_enabled = 0; //Default: input disabled!
	oldshiftstatus = 0; //Init!
	curstat.mode = DEFAULT_KEYBOARD; //Keyboard mode enforced by default for now!

	if (!KEYBOARD_ENABLED) //Keyboard disabled?
	{
		//dolog("osk","Keyboard disabled!");
		return; //Keyboard disabled?
	}
	
	
	
	//dolog("osk","Starting OSK when enabled...");
	/*if (!KEYBOARD_STARTED) //Not started yet?
	{*/
		//dolog("osk","Starting type handler");
		psp_keyboard_refreshrate(); //Handles keyboard typing: we're an interrupt!
		//dolog("osk","Starting swap handler");
		addtimer(3.0f,&keyboard_swap_handler,"Keyboard PSP Swap",1); //Handles keyboard set swapping: we're an interrupt!
		//dolog("osk","Starting mouse handler");
		addtimer(256.0f,&mouse_handler,"PSP Mouse",10); //Handles mouse input: we're a normal timer!
		KEYBOARD_STARTED = 1; //Started!
	//}
	//dolog("osk","keyboard&mouse ready.");
}

void psp_keyboard_done()
{
	if (__HW_DISABLED) return; //Abort!
	/*if (KEYBOARD_STARTED) //Stil started?
	{
		if (!request_type_term)
		{
			request_type_term = 1; //Request termination!
		}
		while (request_type_term) //Requesting termination?
		{
			delay(1); //Wait to be terminated!
		}
		KEYBOARD_STARTED = 0; //Not started anymore!
		//Keyboard has been terminated!
	}*/
	removetimer("Keyboard PSP Type"); //No typing!
	removetimer("Keyboard PSP Swap"); //No swapping!
	removetimer("PSP Mouse"); //No mouse!
}

void keyboard_loadDefaults() //Load the defaults for the keyboard font etc.!
{
	BIOS_Settings.input_settings.analog_minrange = (int)(127/2); //Default to half to use!
	//Default: no game mode mappings!
	//Standard keys:
	BIOS_Settings.input_settings.fontcolor = 0x1; //Blue font!
	BIOS_Settings.input_settings.bordercolor = 0x8; //Dark gray border inactive!
	BIOS_Settings.input_settings.activecolor = 0xE; //Yellow border active!
	BIOS_Settings.input_settings.specialcolor = 0x7; //Special: Brown font!
	BIOS_Settings.input_settings.specialbordercolor = 0x6; //Special: Dark gray border inactive!
	BIOS_Settings.input_settings.specialactivecolor = 0xE; //Special: Yellow border active!
	int i;
	for (i=0;i<NUMITEMS(BIOS_Settings.input_settings.keyboard_gamemodemappings);i++) //Process all keymappings!
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
	SAVED_KEYBOARD_STATUS.input_buffer_shift = input_buffer_shift;
	SAVED_KEYBOARD_STATUS.input_buffer = input_buffer;
	SAVED_KEYBOARD_STATUS.input_buffer_input = input_buffer_input;
	SAVED_KEYBOARD_STATUS.input_enabled = input_enabled; //Save all!
}

void load_keyboard_status() //Load keyboard status from memory!
{
	input_buffer_shift = SAVED_KEYBOARD_STATUS.input_buffer_shift;
	input_buffer = SAVED_KEYBOARD_STATUS.input_buffer;
	input_buffer_input = SAVED_KEYBOARD_STATUS.input_buffer_input;
	input_enabled = SAVED_KEYBOARD_STATUS.input_enabled; //Load all that was saved!
}

void disableKeyboard() //Disables the keyboard/mouse functionnality!
{
	input_enabled = 0; //Disable input!
}

void enableKeyboard(int bufferinput) //Enables the keyboard/mouse functionnality param: to buffer into input_buffer?!
{
	disableKeyboard(); //Make sure the keyboard if off to start with!
	input_buffer_shift = 0; //Shift status!
	input_buffer = -1; //Nothing pressed yet!
	input_buffer_input = bufferinput; //To buffer?
	input_enabled = ALLOW_INPUT; //Enable input!
}

/* All update functionality for input */

//ThreadParams_p input_thread = NULL;

void psp_input_init()
{
	/*(if (!input_thread) //Nothing yet?
	{*/
		sceCtrlSetSamplingCycle(0); //Polling ourselves!
		sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG); //We need buttons and analog, not analog only!
		/*input_thread = startThread(&updateInput,"X86EMU_Input refresh",DEFAULT_PRIORITY);
	}*/
}

void psp_input_done()
{
	/*if (input_thread) //Started?
	{
		terminateThread(input_thread->threadID); //Terminate input!
		input_thread = 0; //No thread anymore!
	}*/ //Nothing to finish: nothing used!
}