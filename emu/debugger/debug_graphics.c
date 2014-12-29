#include "headers/types.h" //Basic types!
#include "headers/hardware/vga.h" //VGA VBlank support!
#include "headers/emu/gpu/gpu_text.h" //Text support!
#include "headers/cpu/cpu.h" //CPU!
#include "headers/interrupts/interrupt10.h" //INT10 support!
#include "headers/emu/emu_vga_bios.h" //VGA misc functionality for INT10!

//To make a screen capture of all of the debug screens active?
#define LOG_VGA_SCREEN_CAPTURE 1
//For text-mode debugging!
#define VIDEOMODE_TEXTMODE 0x02
//To log the first rendered line after putting pixels?
#define LOG_VGA_FIRST_LINE 0
//To debug text modes too in below or BIOS setting?
#define TEXTMODE_DEBUGGING 1
//Always sleep after debugging?
#define ALWAYS_SLEEP 1

extern byte LOG_VRAM_WRITES; //Log VRAM writes?
extern byte LOG_MMU_WRITES; //Log MMU writes?

extern byte LOG_RENDER_BYTES; //See VRAM, but the rendering!

extern byte ENABLE_VRAM_LOG; //Enable VRAM logging?
extern byte SCREEN_CAPTURE; //Log a screen capture?

extern VGA_Type *MainVGA; //Main VGA!

extern GPU_type GPU; //For x&y initialisation!
extern CPU_type CPU; //CPU!

void debugTextModeScreenCapture()
{
	VGA_DUMPDAC(); //Make sure the DAC is dumped!
	VGA_waitforVBlank(); //Wait for VBlank!
	SCREEN_CAPTURE = LOG_VGA_SCREEN_CAPTURE; //Screen capture next frame?
	LOG_RENDER_BYTES = LOG_VGA_FIRST_LINE; //Log it!
	VGA_waitforVBlank(); //Log one screen!
}

extern PSP_TEXTSURFACE *frameratesurface; //The framerate surface!

void DoDebugVGAGraphics(byte mode, word xsize, word ysize, word maxcolor, int allequal, byte centercolor, byte usecenter, byte screencapture)
{
	CPU.registers->AX = (word)mode; //Switch to graphics mode!
	BIOS_int10();
	VGA_DUMPDAC(); //Dump the current DAC and rest info!

	int x,y; //X&Y coordinate!
	int color; //The color for the coordinate!

	GPU_textgotoxy(frameratesurface,0,2); //Goto third row!
	GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"Surface for mode %02X(Colors %03i): Rendering...",mode,maxcolor);
	VGA_waitforVBlank(); //Make sure we're ending drawing!
	stopTimers(); //Stop all timers!

	y = 0; //Init Y!
	nexty:
	{
		if (y>=ysize) goto finishy;
		x = 0; //Init x!
		nextx:
		{
			if (x>=xsize) goto finishx;
			color = convertrel(x,xsize,maxcolor); //Convert relative to get all colors from left to right on the screen!
			if (color>(maxcolor-1)) color = maxcolor-1; //MAX limit!
			if (allequal) //All equal filling?
			{
				color = (maxcolor&0xFF);
			}

			if (y>=(int)((ysize/2)-(usecenter/2)) && 
				(y<=(int)((ysize/2)+(usecenter/2))) && usecenter) //Half line horizontally?
			{
				GPU_putpixel(x,y,0,centercolor); //Plot color!
			}
			else
			{
				if (x>=(int)((xsize/2)-(usecenter/2)) && 
					(x<=(int)((xsize/2)+(usecenter/2))) && usecenter) //Half line vertically?
				{
					color = (byte)SAFEMOD(((int)convertrel(y,ysize,maxcolor)),maxcolor); //Flow Y!
				}
				GPU_putpixel(x,y,0,color); //Plot color!
			}
			++x; //Next X!
			goto nextx;
		}
		finishx: //Finish our line!
		++y; //Next Y!
		goto nexty;
	}
	
	finishy: //Finish our operations!

	GPU_textgotoxy(frameratesurface,33,2); //Goto Rendering... text!
	GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"Rendered.   ",mode);

	startTimers(); //Start timers again!
	VGA_waitforVBlank(); //Make sure we're ending drawing!
	VGA_waitforVBlank(); //Make sure we've drawn the picture!
	if (screencapture) //Screen capture?
	{
		debugTextModeScreenCapture(); //Debug a screen capture!
	}
	delay(5000000); //Wait a bit!
}

/*

VGA Full Debug routine!

*/

void DoDebugTextMode(byte waitforever) //Do the text-mode debugging!
{
	enableKeyboard(0); //Allow to test the keyboard!
	if (TEXTMODE_DEBUGGING) //Debug text mode too?
	{
		int i; //For further loops!

		/*CPU.registers->AX = VIDEOMODE_TEXTMODE;
		BIOS_int10(); //Text mode operations!
		CPU.registers->AH = 0xB;
		CPU.registers->BH = 0x0; //Set overscan color!
		CPU.registers->BL = 0x4; //Blue overscan!
		BIOS_int10(); //Set overscan!
		//VGA_LOGCRTCSTATUS(); //Log our full status!
		VGA_DUMPDAC(); //Dump the active DAC!
		//debugTextModeScreenCapture(); //Make a screen capture!
		//sleep(); //Wait forever to debug!

		//LOG_VRAM_WRITES = 1; //Enable log!
		MMU_wb(-1,0xB800,0,'F');
		MMU_wb(-1,0xB800,1,0x1);
		MMU_wb(-1,0xB800,2,'i');
		MMU_wb(-1,0xB800,3,0x2);
		MMU_wb(-1,0xB800,4,'r');
		MMU_wb(-1,0xB800,5,0x3);
		MMU_wb(-1,0xB800,6,'s');
		MMU_wb(-1,0xB800,7,0x4);
		MMU_wb(-1,0xB800,8,'t');
		MMU_wb(-1,0xB800,9,0x5);
		MMU_wb(-1,0xB800,10,'6');
		MMU_wb(-1,0xB800,11,0x6);
		MMU_wb(-1,0xB800,12,'r');
		MMU_wb(-1,0xB800,13,0x7);
		MMU_wb(-1,0xB800,14,'o');
		MMU_wb(-1,0xB800,15,0x8);
		MMU_wb(-1,0xB800,16,'w');
		MMU_wb(-1,0xB800,17,0x9);
		MMU_wb(-1,0xB800,18,'!');
		MMU_wb(-1,0xB800,19,0xA);
		MMU_wb(-1,0xB800,20,'B');
		MMU_wb(-1,0xB800,21,0xB);
		MMU_wb(-1,0xB800,22,'C');
		MMU_wb(-1,0xB800,23,0xC);
		MMU_wb(-1,0xB800,24,'D');
		MMU_wb(-1,0xB800,25,0xD);
		MMU_wb(-1,0xB800,26,'E');
		MMU_wb(-1,0xB800,27,0xE);
		MMU_wb(-1,0xB800,28,'F');
		MMU_wb(-1,0xB800,29,0xF);
		
		//LOG_VRAM_WRITES = 0; //Disable log!

		GPU_textgotoxy(frameratesurface,0,2); //Goto third debug row!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"Direct VRAM access 40x25-0...");

		//VGA_DUMPATTR(); //Dump attribute controller info!

		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"Ready.");
		debugTextModeScreenCapture(); //Debug a screen capture!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"SCREENCAPTURE CREATEN.");
		delay(5000000); //Wait a bit!
		*/
	
		CPU.registers->AH = 0x0B; //Advanced:!
		CPU.registers->BH = 0x00; //Set background/border color!
		CPU.registers->BL = 0x0E; //yellow!
		BIOS_int10(); //Show the border like this!
	
		/*CPU.registers->AH = 1; //Set cursor shape!
		CPU.registers->CH = 7; //Scan line 7-
		CPU.registers->CL = 8; //8!
		BIOS_int10(); //Set cursor shape!
		*/

		debugTextModeScreenCapture(); //Debug a screen capture!
		delay(5000000); //Wait 5 seconds!
	
		CPU.registers->AX = 0x00; //40x25 TEXT mode!
		BIOS_int10(); //Switch modes!
	
		printmsg(0xF,"This is 40x25 TEXT MODE!");
		printCRLF();
		printmsg(0xF,"S"); //Start!
		for (i=0; i<38; i++) //38 columns!
		{
			printmsg(0x2,"X");
		}
		printmsg(0xF,"E"); //End!
		//printCRLF(); //Newline!
		printmsg(0xF,"Third row!");
		GPU_textgotoxy(frameratesurface,0,2); //Goto third debug row!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"40x25-0 Alltextcolors...");

		debugTextModeScreenCapture(); //Debug a screen capture!
		delay(10000000); //Wait 10 seconds!

		CPU.registers->AX = 0x81; //40x25, same, but with grayscale!
		BIOS_int10();
		GPU_textgotoxy(frameratesurface,0,2); //Goto third debug row!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"80x25-1 Alltextcolors...");
		debugTextModeScreenCapture(); //Debug a screen capture!
		delay(10000000); //Wait 10 seconds!
	
		CPU.registers->AX = VIDEOMODE_TEXTMODE; //80x25 TEXT mode!
		BIOS_int10(); //Switch modes!
		printmsg(0xF,"This is 80x25 TEXT MODE!");
		printCRLF();
		printmsg(0xF,"S"); //Start!
		for (i=0; i<78; i++) //78 columns!
		{
			printmsg(0x2,"X");
		}
		printmsg(0xF,"E"); //End!
		printmsg(0xF,"Third row!");
		GPU_textgotoxy(frameratesurface,0,2); //Goto third debug row!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"80x25-2 WidthRows...");
		debugTextModeScreenCapture(); //Debug a screen capture!
		delay(10000000); //Wait 1 seconds!
	
		CPU.registers->AX = VIDEOMODE_TEXTMODE; //Reset!
		BIOS_int10(); //Reset!
	
		for (i=0; i<0x100; i++) //Verify all colors!
		{
			CPU.registers->AX = 0x0E41+(i%26); //Character A-Z!
			CPU.registers->BX = (word)(i%0x100); //Attribute at page 0!
			BIOS_int10(); //Show the color!
		}
	
		GPU_textgotoxy(frameratesurface,0,2); //Goto third debug row!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"80x25-2 Alltextcolors...");
		debugTextModeScreenCapture(); //Debug a screen capture!
		delay(10000000); //Wait 1 seconds!
	
		CPU.registers->AX = 0x02; //80x25 b/w!
		BIOS_int10(); //Switch video modes!
	
		CPU.registers->AL = 0; //Reset character!
	
		for (i=0; i<0x100; i++) //Verify all characters!
		{
			if (i==12) //Special blink?
			{
				int10_internal_outputchar(0,(i&0xFF),0x8F); //Output&update with blink!
			}
			else //Normal character?
			{
				int10_internal_outputchar(0,(i&0xFF),0xF); //Output&update!
			}
		}
	
		CPU.registers->AH = 2; //Set cursor x,y
		CPU.registers->BH = 0; //Display page #0!
		CPU.registers->DL = 0; //X
		CPU.registers->DH = 0; //Y
		BIOS_int10(); //Show!
		debugTextModeScreenCapture(); //Debug a screen capture!
		delay(5000000); //Wait 5 seconds!
	}

	//Text modes work!
	//DoDebugVGAGraphics(0x08,160,200,0x10,0,0xE,0,0); //Debug 160x200x16! NOT VGA COMPAT.
	//DoDebugVGAGraphics(0x09,320,200,0x10,0,0xE,0,0); //Debug 320x200x16! Bijna OK NOT VGA COMPAT.
	//DoDebugVGAGraphics(0x0A,640,200,0x04,0,0xE,0,0); //Debug 640x200x4! None! NOT VGA COMPAT.
	//Graphics should be OK!
	//4-color modes: TODO!
	//DoDebugVGAGraphics(0x04,320,200,0x04,0,0x0,1,0); //Debug 320x200x4! NOT WORKING FULLY YET!
	//DoDebugVGAGraphics(0x05,320,200,0x04,0,0x0,1,0); //Debug 320x200x4(B/W)! 
	//B/W mode!
	
	//TODO:
	DoDebugVGAGraphics(0x06,640,200,0x02,0,0x1,1,1); //Debug 640x200x2(B/W)! NOT WORKING YET!
	
	//DoDebugVGAGraphics(0x0F,640,350,0x02,0,0x1,1,0); //Debug 640x350x2(Monochrome)!
	//16 color mode!
	//DoDebugVGAGraphics(0x0D,320,200,0x10,0,0xF,0,0); //Debug 320x200x16!

	//DoDebugVGAGraphics(0x0E,640,200,0x10,0,0xF,1,0); //Debug 640x200x16!
	//DoDebugVGAGraphics(0x10,640,350,0x10,0,0xF,1,0); //Debug 640x350x16!
	//16 color b/w mode!
	//DoDebugVGAGraphics(0x11,640,480,0x10,0,0x1,1,0); //Debug 640x480x16(B/W)! 
	//16 color maxres mode!
	//DoDebugVGAGraphics(0x12,640,480,0x10,0,0xF,1,0); //Debug 640x480x16! VGA+!
	//256 color mode!
	//DoDebugVGAGraphics(0x13,320,200,0x100,0,0xF,1,0); //Debug 320x200x256! MCGA,VGA! works, but 1/4th screen height?
	//dumpVGA(); //Dump VGA data&display!
	//delay(10000000); //Wait 10 sec!
	//halt(); //Stop!

	if (waitforever || ALWAYS_SLEEP) //Normal debugging with keyboard?
	{
		sleep(); //Wait forever!
	}
	disableKeyboard(); //Disable the keyboard!
	return; //Go to reset!

	CPU.registers->AH = 2; //Set cursor x,y
	CPU.registers->BH = 0; //Display page #0!
	CPU.registers->DL = 0; //X
	CPU.registers->DH = 0; //Y
	BIOS_int10(); //Show!

//Now, debug cursor movement etc!
	while (1)
	{
		int key;
		key = psp_inputkeydelay(500000); //Wait for keypress with delay!
		CPU.registers->AH = 3; //Get cursor position and size!
		CPU.registers->BH = 0; //Display page #0!
		BIOS_int10(); //Get location!
		switch (key)
		{
		case PSP_CTRL_SQUARE: //Backspace?
			CPU.registers->AH = 0xE; //Enter character!
			CPU.registers->AL = 0x7; //One back!
			CPU.registers->BH = 0; //Page!
			CPU.registers->BL = 0xF; //Color!
			BIOS_int10(); //Execute!

			CPU.registers->AH = 0xE; //Enter character!
			CPU.registers->AL = ' '; //Clear!
			CPU.registers->BH = 0; //Page!
			CPU.registers->BL = 0xF; //Color!
			BIOS_int10(); //Execute!

			CPU.registers->AH = 0xE; //Enter character!
			CPU.registers->AL = 7; //One back!
			CPU.registers->BH = 0; //Page!
			CPU.registers->BL = 0xF; //Color!
			BIOS_int10(); //Execute!
			break;
		case PSP_CTRL_CROSS: //Enter X?
			CPU.registers->AH = 0xE; //Enter character!
			CPU.registers->AL = 'x'; //Our cross!
			CPU.registers->BH = 0; //Page!
			CPU.registers->BL = 0x2; //Color!
			BIOS_int10(); //Execute!
			break;
		case PSP_CTRL_UP:
			if (CPU.registers->DH>0) //Below top?
			{
				CPU.registers->AH = 2; //Set cursor position!
				--CPU.registers->DH; //Up one row!
				BIOS_int10(); //Update!
			}
			break;
		case PSP_CTRL_DOWN:
			if (CPU.registers->DH<24) //Above bottom?
			{
				CPU.registers->AH = 2; //Set cursor position!
				++CPU.registers->DH; //Up one row!
				BIOS_int10(); //Update!
			}
			break;
		case PSP_CTRL_LEFT:
			if (CPU.registers->DL>0) //Not leftmost?
			{
				CPU.registers->AH = 2; //Set cursor position!
				--CPU.registers->DL; //Up one row!
				BIOS_int10(); //Update!
			}
			break;
		case PSP_CTRL_RIGHT:
			if (CPU.registers->DL<39) //Not rightmost?
			{
				CPU.registers->AH = 2; //Set cursor position!
				++CPU.registers->DL; //Up one row!
				BIOS_int10(); //Update!
			}
			break;
		default: //Default?
			//Unknown keypress!
			break;
		}
	}

	sleep(); //Wait forever till user Quits the game!
}

/*

VGA Graphics debugging routine!

*/

void dumpVGA()
{
	GPU_textgotoxy(frameratesurface,0,0);
	GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"Dumping VGA data...");
	FILE *f;
	f = fopen("VGA.DAT","wb"); //Open it!
	byte *b = (byte *)MainVGA->VRAM;
	uint_32 i=0;
	for (;i<MainVGA->VRAM_size;)
	{
		fwrite(&b[i],1,1,f); //Write VRAM!
		++i; //Next byte!
	}
	fclose(f); //Close it!
	f = fopen("DISPLAY.DAT","wb"); //Display!
	fwrite(&GPU.xres,1,sizeof(GPU.xres),f); //X size!
	fwrite(&GPU.yres,1,sizeof(GPU.yres),f); //Y size!
	fwrite(&GPU.emu_screenbuffer,1,1024*sizeof(GPU.emu_screenbuffer[0])*GPU.yres,f); //Video data!
	fclose(f); //Close it!
	raiseError("Debugging","Main VGA&Display dumped!");
}