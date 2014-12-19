#include "headers/types.h"
#include "headers/cpu/cpu.h"
#include "headers/cpu/interrupts.h" //BOOT loader!
#include "headers/cpu/easyregs.h" //Easy registers!
#include "headers/cpu/callback.h" //Callback support!
#include "headers/cpu/80286/protection.h" //Protection support!
//Keyboard interrupt!

void int16_readkey()
{
	/*
	Read (Wait for) Next Keystroke
	returns:
		AH=Scan code
		AL=ASCII character code or extended ASCII keystroke(AL=0)
	*/
	sceKernelSleepThread(); //No input!
}

void int16_querykeyb_status_previewkey()
{
	/*
	Query Keyboard Status/Preview Key
	returns:
		ZF: No keys in buffer
		NZ: key is ready

		AH (if not ZF): scan code
		AL (if not ZF): ASCII character code or extended ASCII keystroke
	*/
	ZF = 1; //No keys to be read atm!
}

void int16_querykeyb_shiftflags()
{
	/*
		Query keyboard Shift Status
		returns:
			AL: Status of Ctl, Alt etc. (Same as 0040:0017)

			Info on bits AL:
				0: Alpha-shift (right side) DOWN
				1: Alpha-shift (left side) DOWN
				2: Ctrl-shift (either side) DOWN
				3: Alt-shift (either side) DOWN
				4: ScrollLock State
				5: NumLock State
				6: CapsLock State
				7: Insert state.

			Extra: 0040:0018:
				0: Ctrl-shift (left side) DOWN (only 101-key enhanced keyboard)
				1: Alt-shift (left side) DOWN (see above)
				2: SysReq DOWN
				3: hold/pause state
				4: ScrollLock DOWN
				5: NumLock DOWN (ON)
				6: CapsLock DOWN (ON)
				7: Insert DOWN
	*/
	AL = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0x0040,0x0017,0); //Reset!
}

void int16_set_typeamaticrate_and_delay()
{
	/*
		Input:
			AL=05h
			BH=Delay code
			BL=typematic rate code
		BL Delay code:
			00h: 250ms
			01h: 500ms
			02h: 750ms
			03h: 1000ms
			else: reserved
		BH repeat code:
			00h: 30 repeats per second
			01h: 26.7 repeats per second
			02h: 24 repeats per second
			03h: 21.8 repeats per second
			04h: 20.0 repeats per second
			05h: 18.5 repeats per second
			06h: 17.1 repeats per second
			07h: 16.0 repeats per second
			08h: 15.0 repeats per second
			09h: 13.3 repeats per second
			0Ah: 12.0 repeats per second
			0Bh: 10.9 repeats per second
			0Ch: 10.0 repeats per second
			0Dh: 9.2 repeats per second
			0Eh: 8.6 repeats per second
			0Fh: 8.0 repeats per second
			10h: 7.5 repeats per second
			11h: 6.7 repeats per second
			12h: 6.0 repeats per second
			13h: 5.5 repeats per second
			14h: 5.0 repeats per second
			15h: 4.6 repeats per second
			16h: 4.3 repeats per second
			17h: 4.0 repeats per second
			18h: 3.7 repeats per second
			19h: 3.3 repeats per second
			1Ah: 3.0 repeats per second
			1Bh: 2.7 repeats per second
			1Ch: 2.5 repeats per second
			1Dh: 2.3 repeats per second
			1Eh: 2.1 repeats per second
			1Fh: 2 repeats per second
	*/
	//Delay=(rate+1)*250ms
	//Rate=(8+A)*(2**B)*4.17
	//Default: 10.9 characters/second; 500ms delay!
	//When typed, first character is send, after pressed for delay time, next repeat x characters/second!
}

void int16_storekeydata()
{
	/*
		Input:
			CH=Scan code to store
			CL=ASCII character or extended ACII keystroke
		Output:
			AL=0: Successfully stored
			AL=1: Not stored (no room in buffer)
	*/
	AL = 1; //Not stored: not implemented yet!
}

void int16_readextendedkeybinput()
{
}

void int16_queryextendedkeybstatus()
{
}

void int16_queryextendedkeybshiftflags()
{
}

Handler int16_functions[0x13] =
{
	int16_readkey, //0x00
	int16_querykeyb_status_previewkey, //0x01
	int16_querykeyb_shiftflags, //0x02
	int16_set_typeamaticrate_and_delay, //0x03
	NULL, //0x04: reserved
	int16_storekeydata, //0x05
	NULL, //0x06
	NULL, //0x07
	NULL, //0x08
	NULL, //0x09
	NULL, //0x0A
	NULL, //0x0B
	NULL, //0x0C
	NULL, //0x0D
	NULL, //0x0E
	NULL, //0x0F
	NULL, //int16_readextendedkeybinput, //0x10
	NULL, //int16_queryextendedkeybstatus, //0x11
	NULL //int16_queryextendedkeybshiftflags //0x12
};

void BIOS_int16()
{
//Not implemented yet!
	int dohandle = 0;
	dohandle = (AH<(sizeof(int16_functions)/sizeof(Handler))); //handle?
	if (CPU.blocked) //No output?
	{
		dohandle = 0; //No handling!
	}

	if (!dohandle) //Not within list to execute?
	{
		AX = 0; //Break!
	}
	else //To handle?
	{
		if (int16_functions[AH]) //Set?
		{
			int16_functions[AH](); //Run the function!
		}
		else
		{
			AX = 0; //Error: unknown command!
		}
	}
}