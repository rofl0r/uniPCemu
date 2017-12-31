#ifndef INPUT_H
#define INPUT_H

//Minimal delay to check for keypressed input, also minimal inexact step rate!

#include "headers/types.h"

enum input_button_emulator { //All buttons we support in our emulator!
BUTTON_TRIANGLE=1, BUTTON_CIRCLE=2, BUTTON_CROSS=4, BUTTON_SQUARE=8,
BUTTON_LTRIGGER=0x10, BUTTON_RTRIGGER=0x20,BUTTON_DOWN=0x40, BUTTON_LEFT=0x80,
BUTTON_UP=0x100, BUTTON_RIGHT=0x200,BUTTON_SELECT=0x400, BUTTON_START=0x800,
BUTTON_HOME=0x1000, BUTTON_HOLD=0x2000, //End of PSP buttons!
BUTTON_PLAY=0x4000, BUTTON_STOP=0x8000};

//All available PSP Keys indexes for input during GAME MODE.
//No SELECT: We use this to break out of it!
#define GAMEMODE_LEFT 0
#define GAMEMODE_UP 1
#define GAMEMODE_RIGHT 2
#define GAMEMODE_DOWN 3
#define GAMEMODE_LTRIGGER 4
#define GAMEMODE_RTRIGGER 5
#define GAMEMODE_TRIANGLE 6
#define GAMEMODE_CIRCLE 7
#define GAMEMODE_CROSS 8
#define GAMEMODE_SQUARE 9
#define GAMEMODE_START 10
//Analog for gaming mode: if any set, use keyboard, if none are set use mouse.
#define GAMEMODE_ANALOGLEFT 11
#define GAMEMODE_ANALOGUP 12
#define GAMEMODE_ANALOGRIGHT 13
#define GAMEMODE_ANALOGDOWN 14

//Different shift statuses (gamemodemappings_alt) bits:
#define SHIFTSTATUS_CTRL 1
#define SHIFTSTATUS_ALT 2
#define SHIFTSTATUS_SHIFT 4

typedef struct
{
	int gamingmode; //Gaming mode (buttons are ALL OR-ed! Exit using start, enter using down arrow in normal modes)
	int analogdirection_keyboard_x; //X direction of analog (-1 for left, 0 center, 1 for right)
	int analogdirection_keyboard_y; //Y direction of analog (see X)
	int analogdirection_mouse_x; //Mouse X direction (0=None)
	int analogdirection_mouse_y; //Mouse Y direction (0=None)
	int analogdirection2_x; //X direction of second analog(if supported)
	int analogdirection2_y; //Y direction of second analog(if supported)
	int analogdirection_rudder; //X direction of the rudder(if supported)
	uint_32 buttonpress; //Which button are pressed (0=None,1=Square,2=Triangle,4=Circle,8=Cross,16=Left,32=Up,64=Right,64=Down,128=L,256=R)
	byte mode; //Which mode: 0=Mouse, 1=Keyboard!
} PSP_INPUTSTATE; //Contains the state of PSP input buttons!

#include "headers/packed.h" //Packed support!
typedef struct PACKED
{
	byte analog_minrange; //Minimum adjustment x&y(0,0) for keyboard&mouse to change states (from center)
	sword keyboard_gamemodemappings[15]; //11 Buttons to map: START, LEFT, UP, RIGHT, DOWN, L, R, TRIANGLE, CIRCLE, CROSS, SQUARE, Then LEFT,UP,RIGHT,DOWN for analog stick (when not assigned, assign mouse movement) -1 for not assigned!
	byte keyboard_gamemodemappings_alt[15]; //Ctrl,alt,shift states!
	byte mouse_gamemodemappings[15]; //Mouse button mappings!
	union
	{
		struct
		{
			byte fontcolor; //Font color (0-15) for text on the keyboard!
			byte bordercolor; //Border color for text on the keyboard!
			byte activecolor; //Active color for text on the keyboard!
			byte specialcolor; //Special font color!
			byte specialbordercolor; //Special border color!
			byte specialactivecolor; //Special active color!
		};
		byte colors[6]; //All our colors!
	};
	byte gamingmode_joystick; //Use the joystick input instead of mapped input during gaming mode?
} INPUT_SETTINGS; //Settings for the above!
#include "headers/endpacked.h" //Finished packed data!

//Minimal step before override!
#define INPUTKEYDELAYSTEP 500000

//Timing for the CPU!
void cleanKeyboard();
void updateKeyboard(double timepassed);
void cleanEMUKeyboard(); //Internal use only!
void cleanMouse();
void updateMouse(double timepassed);

int psp_inputkey(); //Current pressed key(s)?
int psp_inputkeydelay(uint_32 delay); //Keypress and release, can return no keys!
int psp_readkey(); //Wait for keypress and release!
int psp_keypressed(int key); //Key pressed?

void psp_keyboard_init(); //Initialise the on-screen keyboard!
void psp_keyboard_done(); //Finishes the on-screen keyboard!
void keyboard_renderer(); //Render the keyboard on-screen (must be called to update on-screen)!

void disableKeyboard(); //Disables the keyboard/mouse functionnality!
void enableKeyboard(byte bufferinput); //Enables the keyboard/mouse functionnality param: to buffer into input_buffer?!

void keyboard_loadDefaults(); //Load the defaults for the keyboard!

void initKeyboardOSK(); //Initialise keyboard (GPU init only)
void doneKeyboardOSK(); //See above, but done!

void setMouseRate(float packetspersecond); //Set the mouse refresh rate!

void load_keyboard_status(); //Load keyboard status from memory!
void save_keyboard_status(); //Save keyboard status to memory!

int is_gamingmode(); //We're in gaming mode (select isn't mapped to BIOS?)

/*

EMU internal!

*/

void EMU_stopInput(); //Stop input from EMU!
void EMU_startInput(); //Start input from EMU!

void keyboard_loadDefaultColor(byte color); //Load a default color!

//Input support!
void psp_input_init();
void psp_input_done();

void updateInput(SDL_Event *event); //Update all input for SDL!

#ifndef SDLK_AUDIOSTOP
#define SDLK_AUDIOSTOP 0x40000104
#endif

#ifndef SDLK_AUDIOPLAY
#define SDLK_AUDIOPLAY 0x40000105
#endif

void tickPendingKeys(double timepassed); //Handle all pending keys from our emulation! Updating every 1/1000th second!
void reconnectJoystick0(); //For non-SDL2 compilations!

#endif
