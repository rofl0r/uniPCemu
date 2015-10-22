#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA VBlank support!
#include "headers/emu/gpu/gpu_text.h" //Text support!
#include "headers/cpu/cpu.h" //CPU!
#include "headers/interrupts/interrupt10.h" //INT10 support!
#include "headers/emu/emu_vga_bios.h" //VGA misc functionality for INT10!

//Debugger functions!
#include "headers/emu/timers.h" //Timer support!
#include "headers/hardware/vga/vga_precalcs.h" //For the CRT precalcs dump!
#include "headers/hardware/vga/vga_dac.h" //DAC support!
//To make a screen capture of all of the debug screens active?
#define LOG_VGA_SCREEN_CAPTURE 0
//For text-mode debugging! 40 and 80 character modes!
#define VIDEOMODE_TEXTMODE_40 0x00
#define VIDEOMODE_TEXTMODE_80 0x02
//To log the first rendered line after putting pixels?
#define LOG_VGA_FIRST_LINE 0
//To debug text modes too in below or BIOS setting?
#define TEXTMODE_DEBUGGING 1
//Always sleep after debugging?
#define ALWAYS_SLEEP 1

extern byte LOG_MMU_WRITES; //Log MMU writes?

extern byte ENABLE_VRAM_LOG; //Enable VRAM logging?
extern byte SCREEN_CAPTURE; //Log a screen capture?

extern VGA_Type *MainVGA; //Main VGA!

extern GPU_type GPU; //For x&y initialisation!

void debugTextModeScreenCapture()
{
	//VGA_DUMPDAC(); //Make sure the DAC is dumped!
	SCREEN_CAPTURE = LOG_VGA_SCREEN_CAPTURE; //Screen capture next frame?
	VGA_waitforVBlank(); //Log one screen!
	for (; SCREEN_CAPTURE;) //Busy?
	{
		VGA_waitforVBlank(); //Wait for VBlank!
	}
}

extern GPU_TEXTSURFACE *frameratesurface; //The framerate surface!

void DoDebugVGAGraphics(byte mode, word xsize, word ysize, word maxcolor, int allequal, byte centercolor, byte usecenter, byte screencapture)
{
	stopTimers(0); //Stop all timers!
	CPU[activeCPU].registers->AX = (word)mode; //Switch to graphics mode!
	BIOS_int10();
	CPU[activeCPU].registers->AH = 0xB;
	CPU[activeCPU].registers->BH = 0x0; //Set overscan color!
	CPU[activeCPU].registers->BL = 0x1; //Blue overscan!
	BIOS_int10();
	//VGA_DUMPDAC(); //Dump the current DAC and rest info!

	int x,y; //X&Y coordinate!
	int color; //The color for the coordinate!

	GPU_text_locksurface(frameratesurface);
	GPU_textgotoxy(frameratesurface,0,2); //Goto third row!
	GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"Surface for mode %02X(Colors %03i): Rendering...",mode,maxcolor);
	GPU_text_releasesurface(frameratesurface);
	//VGA_waitforVBlank(); //Make sure we're ending drawing!

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
	
	GPU_text_locksurface(frameratesurface);
	GPU_textgotoxy(frameratesurface,33,2); //Goto Rendering... text!
	GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"Rendered.   ",mode);
	GPU_text_releasesurface(frameratesurface);

	/*
	startTimers(0); //Start timers again!
	VGA_waitforVBlank(); //Make sure we're ending drawing!
	VGA_waitforVBlank(); //Make sure we've drawn the picture!
	if (screencapture) //Screen capture?
	{
		debugTextModeScreenCapture(); //Debug a screen capture!
	}
	*/
	
	//dolog("VGA", "CRTC of mode %02X", mode); //Log what mode we're dumping!
	//VGA_LOGCRTCSTATUS(); //Dump the current CRTC status!
	//dump_CRTCTiming(); //Dump the current CRTC timing!
	startTimers(0); //Start the timers!
	if (screencapture) //To create a screen capture?
	{
		debugTextModeScreenCapture(); //Debug a screen capture!
	}
	delay(5000000); //Wait a bit!
}

/*

VGA Full Debug routine!

*/

extern byte VGA_LOGPRECALCS; //Log precalcs after this ammount of scanlines!

void DoDebugTextMode(byte waitforever) //Do the text-mode debugging!
{
	enableKeyboard(0); //Allow to test the keyboard!
	if (TEXTMODE_DEBUGGING) //Debug text mode too?
	{
		stopTimers(0); //Make sure we've stopped!
		int i; //For further loops!

		CPU[activeCPU].registers->AX = VIDEOMODE_TEXTMODE_40;
		BIOS_int10(); //Text mode operations!
		CPU[activeCPU].registers->AH = 0xB;
		CPU[activeCPU].registers->BH = 0x0; //Set overscan color!
		CPU[activeCPU].registers->BL = 0x4; //Blue overscan!
		BIOS_int10(); //Set overscan!

		VGA_LOGCRTCSTATUS(); //Log our full status!
		VGA_LOGPRECALCS = 5; //Log after 5 scanlines!
		//VGA_DUMPDAC(); //Dump the active DAC!
		//debugTextModeScreenCapture(); //Make a screen capture!
		//sleep(); //Wait forever to debug!

		MMU_wb(-1,0xB800,0,'a');
		MMU_wb(-1,0xB800,1,0x1);
		MMU_wb(-1,0xB800,2,'b');
		MMU_wb(-1,0xB800,3,0x2);
		MMU_wb(-1,0xB800,4,'c');
		MMU_wb(-1,0xB800,5,0x3);
		MMU_wb(-1,0xB800,6,'d');
		MMU_wb(-1,0xB800,7,0x4);
		MMU_wb(-1,0xB800,8,'e');
		MMU_wb(-1,0xB800,9,0x5);
		MMU_wb(-1,0xB800,10,'f');
		MMU_wb(-1,0xB800,11,0x6);
		MMU_wb(-1,0xB800,12,'g');
		MMU_wb(-1,0xB800,13,0x7);
		MMU_wb(-1,0xB800,14,'h');
		MMU_wb(-1,0xB800,15,0x8);
		MMU_wb(-1,0xB800,16,'i');
		MMU_wb(-1,0xB800,17,0x9);
		MMU_wb(-1,0xB800,18,'j');
		MMU_wb(-1,0xB800,19,0xA);
		MMU_wb(-1,0xB800,20,'k');
		MMU_wb(-1,0xB800,21,0xB);
		MMU_wb(-1,0xB800,22,'l');
		MMU_wb(-1,0xB800,23,0xC);
		MMU_wb(-1,0xB800,24,'m');
		MMU_wb(-1,0xB800,25,0xD);
		MMU_wb(-1,0xB800,26,'n');
		MMU_wb(-1,0xB800,27,0xE);
		MMU_wb(-1,0xB800,28,'o');
		MMU_wb(-1,0xB800,29,0xF);
		/*
		MMU_wb(-1,0xB800,156,'-');
		MMU_wb(-1,0xB800,157,0xF);
		MMU_wb(-1,0xB800,158,'E');
		MMU_wb(-1,0xB800,159,0xF); //Green at the full width!
		*/ //80x25
		//SCREEN_CAPTURE = 2; //Enable a screen capture please, on the second frame (first is incomplete, since we've changed VRAM)!
		
		GPU_text_locksurface(frameratesurface);
		GPU_textgotoxy(frameratesurface,0,2); //Goto third debug row!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"Direct VRAM access 40x25-0...");

		//VGA_DUMPATTR(); //Dump attribute controller info!

		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"Ready.");
		GPU_text_releasesurface(frameratesurface);
		/*debugTextModeScreenCapture(); //Debug a screen capture!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"SCREENCAPTURE CREATEN.");
		*/
		startTimers(0); //Start timers up!
		delay(5000000); //Wait a bit!
	
		CPU[activeCPU].registers->AH = 0x0B; //Advanced:!
		CPU[activeCPU].registers->BH = 0x00; //Set background/border color!
		CPU[activeCPU].registers->BL = 0x0E; //yellow!
		BIOS_int10(); //Show the border like this!
	
		debugTextModeScreenCapture(); //Debug a screen capture!
		delay(5000000); //Wait 5 seconds!
	
		CPU[activeCPU].registers->AX = 0x01; //40x25 TEXT mode!
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
		GPU_text_locksurface(frameratesurface);
		GPU_textgotoxy(frameratesurface,0,2); //Goto third debug row!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"40x25-0 Alltextcolors...");
		GPU_text_releasesurface(frameratesurface);
		debugTextModeScreenCapture(); //Debug a screen capture!
		delay(10000000); //Wait 10 seconds!

		CPU[activeCPU].registers->AX = 0x81; //40x25, same, but with grayscale!
		BIOS_int10();
		GPU_text_locksurface(frameratesurface);
		GPU_textgotoxy(frameratesurface,0,2); //Goto third debug row!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"80x25-1 Alltextcolors...");
		GPU_text_releasesurface(frameratesurface);
		debugTextModeScreenCapture(); //Debug a screen capture!
		delay(10000000); //Wait 10 seconds!
	
		CPU[activeCPU].registers->AX = VIDEOMODE_TEXTMODE_80; //80x25 TEXT mode!
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
		GPU_text_locksurface(frameratesurface);
		GPU_textgotoxy(frameratesurface,0,2); //Goto third debug row!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"80x25-2 WidthRows...");
		GPU_text_releasesurface(frameratesurface);
		debugTextModeScreenCapture(); //Debug a screen capture!
		delay(10000000); //Wait 1 seconds!
	
		CPU[activeCPU].registers->AX = VIDEOMODE_TEXTMODE_80; //Reset to 80x25 text mode!
		BIOS_int10(); //Reset!
	
		for (i=0; i<0x100; i++) //Verify all colors!
		{
			CPU[activeCPU].registers->AX = 0x0E41+(i%26); //Character A-Z!
			CPU[activeCPU].registers->BX = (word)(i%0x100); //Attribute at page 0!
			BIOS_int10(); //Show the color!
		}
	
		GPU_text_locksurface(frameratesurface);
		GPU_textgotoxy(frameratesurface,0,2); //Goto third debug row!
		GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"80x25-2 Alltextcolors...");
		GPU_text_releasesurface(frameratesurface);
		debugTextModeScreenCapture(); //Debug a screen capture!
		delay(10000000); //Wait 1 seconds!
	
		CPU[activeCPU].registers->AX = 0x02; //80x25 b/w!
		BIOS_int10(); //Switch video modes!
	
		CPU[activeCPU].registers->AL = 0; //Reset character!
	
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
	
		CPU[activeCPU].registers->AH = 2; //Set cursor x,y
		CPU[activeCPU].registers->BH = 0; //Display page #0!
		CPU[activeCPU].registers->DL = 0; //X
		CPU[activeCPU].registers->DH = 0; //Y
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
	DoDebugVGAGraphics(0x04,320,200,0x04,0,0x3,1,0); //Debug 320x200x4!
	DoDebugVGAGraphics(0x05,320,200,0x04,0,0x0,1,0); //Debug 320x200x4(B/W)! 
	//B/W mode!
	
	//TODO:
	DoDebugVGAGraphics(0x06,640,200,0x02,0,0x1,1,0); //Debug 640x200x2(B/W)!
	
	DoDebugVGAGraphics(0x0F,640,350,0x02,0,0x1,1,0); //Debug 640x350x2(Monochrome)!
	//16 color mode!
	DoDebugVGAGraphics(0x0D,320,200,0x10,0,0xF,0,0); //Debug 320x200x16!

	DoDebugVGAGraphics(0x0E,640,200,0x10,0,0xF,1,0); //Debug 640x200x16!
	DoDebugVGAGraphics(0x10,640,350,0x10,0,0xF,1,0); //Debug 640x350x16!
	//16 color b/w mode!
	DoDebugVGAGraphics(0x11,640,480,0x10,0,0x1,1,0); //Debug 640x480x16(B/W)! 
	//16 color maxres mode!
	DoDebugVGAGraphics(0x12,640,480,0x10,0,0xF,1,0); //Debug 640x480x16! VGA+!
	//VGA_DUMPDAC(); //Dump the DAC!
	//256 color mode!
	DoDebugVGAGraphics(0x13,320,200,0x100,0,0xF,1,0); //Debug 320x200x256! MCGA,VGA! works, but 1/8th screen width?
	//debugTextModeScreenCapture(); //Log screen capture!
	//dumpVGA(); //Dump VGA data&display!
	//delay(10000000); //Wait 10 sec!
	//halt(); //Stop!
	if (waitforever) //Waiting forever?
	{
		sleep(); //Wait forever till user Quits the game!
	}
}

/*

VGA Graphics debugging routine!

*/

void dumpVGA()
{
	GPU_text_locksurface(frameratesurface);
	GPU_textgotoxy(frameratesurface,0,0);
	GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0x00,0x00,0x00),"Dumping VGA data...");
	GPU_text_releasesurface(frameratesurface);
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