#ifndef EMU_VGA_H
#define EMU_VGA_H

#include "headers/types.h"
#include "headers/hardware/vga_screen/vga_precalcs.h" //Precalculation support!
#include "headers/emu/gpu/gpu.h" //For max X!
#include "headers/support/locks.h" //Locking support!
//Emulate VGA?
#define EMU_VGA 1

//We're an VGA Architecture!
#define IS_VGA_ARCH 1

//4 planes of 64k totaling 256k (0x40000) of VRAM is a Minimum! Currently: 0x300000=1024x768x32;0x3C0000=1280x1024x24
#define VGA_VRAM_VGA 0x40000
#define VGA_VRAM_SVGA_1024_32 0x300000
#define VGA_VRAM_SVGA_1280_24 0x3C0000
//What size to use for a minimum: Should always be set to standard VGA.
#define VRAM_SIZE VGA_VRAM_VGA

//SRC for registers: http://www.osdever.net/FreeVGA/vga/vga.htm
//- Input/Output Register Information, before Indices.

//First, the registers:

//We're low endian, so low end first (DATA[0] is first item and First bit is lowest bit (so in 8-bits bit with value 1))

//Graphics Registers: OK

#include "headers/packed.h" //We're packed!
typedef union PACKED
{
	byte DATA[9]; //9 registers present!
	struct //Contains the registers itself!
	{
		//Set/Reset Register (index 00h)
		union
		{
			struct
			{
				byte SetReset : 4; //Set/reset!
				byte notused1 : 4; //Not used
			};
			byte SetResetFull; //Full Set/Reset register!
		} SETRESETREGISTER;

		//Enable Set/Reset register (index 01h)
		union
		{
			struct
			{
				byte EnableSetReset : 4; //Set/reset!
				byte notused2 : 4; //Not used
			};
			byte EnableSetResetFull; //Full Set/Reset register!
		} ENABLESETRESETREGISTER;

		//Color Compare register (index 02h)
		union
		{
			struct
			{
				byte ColorCompare : 4; //Set/reset!
				byte notused3 : 4; //Not used
			};
			byte ColorCompareFull; //Full Set/Reset register!
		} COLORCOMPAREREGISTER;

		//Data Rotate Register register (index 03h)
		union
		{
			struct
			{
				byte RotateCount : 3; //Rotate count!
				byte LogicalOperation : 2; //Logical operation!
				byte notused4 : 3; //Not used
			};
			byte DataRotateFull; //Full Set/Reset register!
		} DATAROTATEREGISTER;

		//Read Map Select Register (index 04h)

		union
		{
			struct
			{
				byte ReadMapSelect : 2; //Active memory plane for CPU (at location for VRAM)!
				byte notused5 : 6;
			};
			byte ReadMapSelectRegisterFull; //Full register!
		}  READMAPSELECTREGISTER;

		//Graphics Mode Register (Index 05h)
		union
		{
			struct
			{
				byte WriteMode : 2;
				byte notused6 : 1;
				byte ReadMode : 1;
				byte OddEvenMode : 1; //0=Normal VGA operating mode, 1=Host addresses access even/odd planes.
				byte ShiftRegisterInterleaveMode : 1; //Shift Register interleave mode (4 colors), else 4-bits (16 colors).
				byte Color256ShiftMode : 1; //1=support for 256-color, 0=Use Above bit.
				byte notused7 : 1;
			};
			struct
			{
				byte GMRegister_dummy1 : 5;
				byte ShiftRegister : 2; //The rendering mode!
				byte GMRegister_dummy2 : 1;
			};
			byte GraphicsModeRegisterFull; //Full register!
		} GRAPHICSMODEREGISTER;

		union
		{
			struct
			{
				byte AlphaNumericModeDisable : 1; //1 for graphics, 0 for text-mode.
				byte EnableOddEvenMode : 1; //1 for enabled, 0 for disabled.
				byte MemoryMapSelect : 2; //Where display memory resides in MMU:
				/*
				00b = A0000-BFFFFh (128k region)
				01b = A0000-AFFFFh (64k region)
				02b = B0000-B7FFFh (32k region)
				03b = B8000-BFFFFh (32k region)
				*/
				byte notusedmisc : 4; //Unused!
			};
			byte MiscellaneousGraphicsRegister;
		} MISCGRAPHICSREGISTER;


		union
		{
			struct
			{
			byte ColorDontCare0 : 1; //Plane 0: We care about this color?
			byte ColorDontCare1 : 1; //Plane 1: We care about this color?
			byte ColorDontCare2 : 1; //Plane 2: We care about this color?
			byte ColorDontCare3 : 1; //Plane 3: We care about this color?

			byte notused7 : 4;
			};
			byte ColorCare; //We care about these colors?
		} COLORDONTCAREREGISTER;

		byte BITMASKREGISTER; //Bit Mask Register (index 08h)
	} REGISTERS; //The registers itself!
} GRAPHREGS; //Graphics registers!
#include "headers/endpacked.h" //We're packed!





//Sequencer Registers: OK

#include "headers/packed.h" //We're packed!
typedef union PACKED
{
	byte DATA[0x8]; //5 registers present, 1 undocumented!

	struct //Contains the registers itself!
	{
		struct
		{
			byte AR : 1; //Asynchronous reset?
			byte SR : 1; //Synchronous reset?
			byte unused : 6; //Unused!
		} RESETREGISTER; //Reset Register (index 00h)

		struct
		{
			byte DotMode8 : 1; //8 Dots/Char? Else 9 Dots/Char!
			byte unused1 : 1; //Unused!
			byte SLR : 1; //Shift/Load Rate
			byte DCR : 1; //Dot Clock Rate
			byte S4 : 1; //Shift Fout Enable?
			byte ScreenDisable : 1; //Turn off display (Screen Disable)?
			byte unused2 : 2; //Unused!
		} CLOCKINGMODEREGISTER; //Clocking Mode Register (index 01h)

		struct
		{
			union
			{
				byte MemoryPlaneWriteEnable; //Enable writes to this plane? (Bit#=Plane#)
				struct
				{
					byte modifyplane0 : 1; //Modify this plane?
					byte modifyplane1 : 1; //Modify this plane?
					byte modifyplane2 : 1; //Modify this plane?
					byte modifyplane3 : 1; //Modify this plane?
					byte unused : 4; //Unused
				};
			}; //Just for this duality!
		} MAPMASKREGISTER; //Map Mask Register (index 02h)

		struct
		{
			byte CharacterSetBSelect_low : 2; //Low 2 bits of select
			byte CharacterSetASelect_low : 2; //Low 2 bits of select
			byte CharacterSetBSelect_high : 1; //High bit of select
			byte CharacterSetASelect_high : 1; //High bit of select
			byte unused : 2; //Unused!
			/*
			Values for Character set A&B: (A is used when bit 3 of the attribute byte for a character is set, else B)
			000b: 0000h-1FFFh
			001b: 4000h-4FFFh
			010b: 8000h-9FFFh
			011b: C000h-DFFFh
			100b: 2000h-3FFFh
			101b: 6000h-7FFFh
			110b: A000h-BFFFh
			111b: E000h-FFFFh
			*/
		} CHARACTERMAPSELECTREGISTER; //Character Map Select Register (index 03h)

		struct
		{
			byte unused1 : 1; //Unused!
			byte ExtendedMemory : 1; //Ext. Mem
			byte OEDisabled : 1; //Odd/Even Host Memory Disable
			byte Chain4Enable : 1; //Chain 4 Enable?
			byte unused : 4; //Unused!
		} SEQUENCERMEMORYMODEREGISTER; //Sequencer Memory Mode Register (index 04h)

		byte unused[2];
		
		byte DISABLERENDERING; //Just here for compatibility (actual disable check is external). The CPU can read/write to this freely. Only writes to this register are counted.
	} REGISTERS;
} SEQUENCERREGS;
#include "headers/endpacked.h" //We're packed!


//Attribute Controller Registers: OK

#include "headers/packed.h" //We're packed!
typedef union PACKED
{
	byte DATA[0x15]; //10h color registers + 5 registers present!

	struct //Contains the registers itself!
	{
		union
		{
			byte DATA;
			struct
			{
				byte InternalPaletteIndex : 6;
				byte unused : 2;
			};
		} PALETTEREGISTERS[0x10];

		struct
		{
			byte AttributeControllerGraphicsEnable : 1;
			byte MonochromeEmulation : 1;
			byte LineGraphicsEnable : 1;
			byte BlinkEnable : 1;
			byte unused : 1;
			byte PixelPanningMode : 1;
			byte ColorEnable8Bit : 1;
			byte PaletteBits54Select : 1;
		} ATTRIBUTEMODECONTROLREGISTER;

		byte OVERSCANCOLORREGISTER;

		union
		{
			byte DATA; //The Data!
			struct
			{
				byte ColorPlaneEnable1 : 1;
				byte ColorPlaneEnable2 : 1;
				byte ColorPlaneEnable3 : 1;
				byte ColorPlaneEnable4 : 1;
				byte unused : 4;
			};
		} COLORPLANEENABLEREGISTER;

		struct
		{
			byte PixelShiftCount : 4;
			byte unused : 4;
		} HORIZONTALPIXELPANNINGREGISTER;

		struct
		{
			byte ColorSelect54 : 2;
			byte ColorSelect76 : 2;
			byte unused : 4;
		} COLORSELECTREGISTER;
	} REGISTERS;
} ATTRIBUTECONTROLLERREGS;
#include "headers/endpacked.h" //We're packed!


//CRT Controller Registers

#include "headers/packed.h" //We're packed!
typedef union PACKED
{
	byte DATA[0x25]; //25h registers present, up to 0x40 are undocumented!

	struct //Contains the registers itself!
	{
		/*

		Officially documented registers:

		*/
		byte HORIZONTALTOTALREGISTER; //00: # of char. clocks per scan line index #0

		byte ENDHORIZONTALDISPLAYREGISTER; //01:When to stop outputting pixels from display memory. index #1

		byte STARTHORIZONTALBLANKINGREGISTER; //02:At which character clock does the horizontal blanking period begin? index #2

		struct
		{
			byte EndHorizontalBlanking : 5; //Bits 4-0 of the End Horizontal Blanking field which specifies the end of the Horizontal Blanking Period.
			byte DisplayEnableSkew : 2; //???
			byte EVRA : 1; //Enable Vertical Retrace Access; When set to 0 (forced 1 on VGA), index 10h&11h are read-only
		} ENDHORIZONTALBLANKINGREGISTER; //index #3

		byte STARTHORIZONTALRETRACEREGISTER; //When to move to the left side of the screen. index #4

		struct
		{
			byte EndHorizontalRetrace : 5; //End of the Horizontal Retrace Period, which begins at the char. clock specified in the Start Horizontal Retrace field.
			byte HorizontalRetraceSkew : 2;  //Delays the start of the Horizontal Retrace period by the number of char. clocks equal to the value of this field. (Always 0. 40 column text mode have 1).
			byte EHB5 : 1; //Bit 5 of End Horizontal Blanking.
		} ENDHORIZONTALRETRACEREGISTER; //index #5

		byte VERTICALTOTALREGISTER; //Lower 8 bits of the Vertical Total field. Bits 9-8 are located in the Overflow Register. index #6

		struct
		{
			byte VerticalTotal8 : 1; //Vertical Total (bit 8)
			byte VerticalDisplayEnd8 : 1; //Vertical Display End (bit 8)
			byte VerticalRetraceStart8 : 1; //Vertical Retrace Start (bit 8)
			byte StartVerticalBlanking8 : 1; //Start Vertical Blanking (bit 8)
			byte LineCompare8 : 1; //Line Compare (bit 8)
			byte VerticalTotal9 : 1; //Vertical total (bit 9)
			byte VerticalDisplayEnd9 : 1; //Vertical Display End (bit 9)
			byte VerticalRetraceStart9 : 1; //Vertical Retrace Start (bit 9)
		} OVERFLOWREGISTER; //index #7

		struct
		{
			byte PresetRowScan : 5; //Used when using text mode or mode with non-0 Maximum Scan Line to provide more precise scrolling than the Start Address Register provides. How many scan lines to scroll the display upwards. Values:0-Maximum Scan Line field.
			byte BytePanning : 2; //Added to Start Address Register when calculating the display mmory address for the upper-left hand pixel or character of the screen.
			byte unused : 1;
		} PRESETROWSCANREGISTER; //index #8

		struct
		{
			byte MaximumScanLine : 5; //In text mode =Character Height-1 (0-based). In graphics mode, non-0 value causes each scan line to be repeated by the value+1.
			byte StartVerticalBlanking9 : 1; //Vertical Blanking field (bit 9)
			byte LineCompare9 : 1; //Line compare field (bit 9)
			byte ScanDoubling : 1; //When set to 1, 200-scan-line video data is converted to 400-scan line output. (Doubling every scan line).
		} MAXIMUMSCANLINEREGISTER; //index #9

		struct
		{
			byte CursorScanLineStart : 5; //Start scan line of the cursor.
			byte CursorDisable : 1; //When set, the cursor is disabled.
			byte Unused : 2;
		} CURSORSTARTREGISTER; //index #a

		struct
		{
			byte CursorScanLineEnd : 5; //End scan line of the cursor.
			byte CursorSkew : 2; //Added to the Scan line start and end.
			byte unused : 1;
		} CURSORENDREGISTER; //index #b

		byte STARTADDRESSHIGHREGISTER; //Bits 15-8 of the Start Address field. index #e
		byte STARTADDRESSLOWREGISTER; //Bits 7-0 of the Start Address field. Specifies 0,0 location of the display memory. index #f

		byte CURSORLOCATIONHIGHREGISTER; //Bits 15-8 of the Cursor Location field. index #c
		byte CURSORLOCATIONLOWREGISTER; //Bits 7-0 of the Cursor Location field. When displaying text-mode and the cursor is enabled, compare current display address with the sum of this field and the Cursor Skew field. If it equals then display cursor depending on scan lines. index #d

		byte VERTICALRETRACESTARTREGISTER; //Index #10:Bits 7-0 of Vertical Retrace Start field. Bits 9-8 are in the Overflow Register. When to move up to the beginning of display memory. Contains the value of the vertical scanline counter at the beginning of the 1st scanline where the signal is asserted.

		struct
		{
			byte VerticalRetraceEnd : 4; //Determines the end of the vertical retrace pulse, and this it's length. Lower 4 bits of Vertical Retrace End register.
			byte VerticalInterrupt_Enabled : 1; //Set to 0 clears a pending VRetrace interrupt. Set to enable generation of VBlank interrupt.
			byte VerticalInterrupt_Disabled : 1; //1: Disable the generation of an interrupt of each VRetrace.
			byte Bandwidth : 1; //Memory Refresh Bandwidth: Unused by VGA.
			byte Protect : 1; //Protect CRTC Registers? 1=Ignore writes to indexes 00h-07h, except bit 4 of the Overflow Register.
		} VERTICALRETRACEENDREGISTER; //Index #11

		byte VERTICALDISPLAYENDREGISTER; //Index #12: Bits 7-0 of the Vertical Display End field. Bits 9-8 are located in the Overflow Register.

		byte OFFSETREGISTER; //Index #13: Specifies the address difference between two scan lines (graphics) or character (text).

		/*
		Text modes: Width/(MemoryAddressSize*2).
				MemoryAddressSize =1:byte,2:word,4:dword.
		Graphics modes: Width/(PixelsPerAddress*MemoryAddressSize*2)
				Width=Width in pixels of the screen
				PixelsPerAddress=Number of pixels stored in one display memory address.
				MemoryAddressSize is the current memory addressing size.
		*/

		struct
		{
			byte UnderlineLocation : 5; //Horizontal Scan line of a character row on which an underline occurs. =Scanline-1.
			byte DIV4 : 1; //Divide Memory Address Clock by 4 (when dword addresses are used)
			byte DW : 1; //Double-Word Addressing: When set to 1, memory addresses are dword addresses. See bit 6 of the Mode Control Register.
			byte unused : 1;
		} UNDERLINELOCATIONREGISTER; //Index #14

		byte STARTVERTICALBLANKINGREGISTER; //Index #15: Bits 7-0 of the Start Vertical Blanking field. Bit 8 is located in the Overflow Register, and bit 9 is located in the Maximum Scan Line register. Determines when the vblanking period begins, and contains the value of the vertical scanline counter at the beginning of the first vertical scanline of blanking.

		struct
		{
			byte EndVerticalBlanking : 7; //Determines when the vblanking period ends and contins the value of the vertical scanline counter at the beginning of the vertical scanline immediately after the last scanline of blanking.
			byte unused : 1;
		} ENDVERTICALBLANKINGREGISTER; //Index #16


		struct
		{
			byte MAP13 : 1; //Map Display Address 13: When 0, bit 0 of the row scan counter is used as bit 13. Normal operation otherwise.
			//For 640x200 graphics, CRTC is programmed for 100 horizontal scan lines with 2 scan-line addresses per char. row. Row san address bit 0 becomes the most-significant address bit to the display buffer. Successive scan lines are placed in 8KB of memory.
			byte MAP14 : 1; //Map Display Address 14: 1= Normal operations. 0=Row scan counter bit 1 is used for memory bus address bit 14.
			byte SLDIV : 1; //Divide Scan Line clock by 2.
			byte DIV2 : 1; //Divide Memory Address clock by 2: When set to 0, the address counter uses the character clock. When set to 1, the address counter uses the character clock input divided by 2. Used to create either byte or word refresh address for display memory.
			byte unused : 1;
			byte AW : 1; //Address Wrap Select: Selects the memory-address bit, bit MA 13 or MA 15, that appears on output pin MA 0, in the words address mode. ...
			byte UseByteMode : 1; //1=Use byte mode, 0=Use word mode.
			byte SE : 1; //1=Enable horizontal and vertical retrace signals. Doesn't reset any other registers or signal outputs.
		} CRTCMODECONTROLREGISTER; //Index #17


		byte LINECOMPAREREGISTER; //Index 18h: Bits 7-0 of the Line Compare field. Bit 9 is located in the Maximum Scan Line Register and bit 8 is located in the Overflow Register. Specifies the scan line at which a horizontal division can occur, providing for split-screen operation. If no horizontal division is required, this field sohuld be set to 3FFh. When the scan line counter reaches the value of the Line Compare field, the current scan line address is reset to 0 and the Preset Row Scan is presumed to be 0. If the Pixel Panning Mode field is set to 1 then the Pixel Shift Count and the Byte Panning fields are reset to 0 for the remainder of the display cycle.

		//Some undocumented registers (index 22,24,3X)

		byte notused[10]; //Unused registers
		
		union
		{
			struct
			{
				byte bit0 : 1; //Data latch n, bit 0
				byte bit1 : 1;
				byte bit2 : 1;
				byte bit3 : 1;
				byte bit4 : 1;
				byte bit5 : 1;
				byte bit6 : 1;
				byte bit7 : 1; //Data latch n, bit 7
			};
			byte LatchN; //Must match data latch 'n', determined by Graphics Register 4 bit 0-1
		} GraphicsControllerDataLatches; //Index 22 data latch 'n', controlled by Graphics Register 4 bits 0-1; range 0-3; READ ONLY!

		byte unused2; //Unused: index 23!

		struct
		{
			byte CurrentIndex : 5; //Current palette index!
			byte PAL : 1; //Palette Address Source!
			byte reserved : 1; //Reserved!
			byte DataState : 1; //DATA Mode (1); Index mode (0)
		} ATTRIBUTECONTROLLERTOGGLEREGISTER; //Index 24 Attribute Controller Toggle Register (CR24). READ ONLY at 3B5/3D5 index 24h!
	} REGISTERS;
} CRTCONTROLLERREGS;
#include "headers/endpacked.h" //We're packed!

//Attribute controller toggle register location for precalcs!
#define VGA_CRTC_ATTRIBUTECONTROLLERTOGGLEREGISTER 24



#include "headers/packed.h" //We're packed!
typedef struct PACKED
{
	byte DAC_ADDRESS_WRITE_MODE_REGISTER; //Port 3C8
	byte DAC_ADDRESS_READ_MODE_REGISTER; //Port 3C7 Write only
	byte DAC_ADDRESS_DATA_REGISTER; //Port 3C9 immediately processed if state is OK (on R/W).

	union
	{
		byte DATA;
		struct
		{
			byte DACState : 2; //0:Prepared for reads, 3:Prepared for writes.
			byte unused : 6;
		};
	} DAC_STATE_REGISTER; //Port 3C7 Read only

} COLORREGS;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{
	union
	{
		byte DATA;
		struct
		{
			byte IO_AS : 1; //Input/Output Address Select (Select CRT Addresses. 0=03Bx; Input Status Register 1=3BA. 1=03Dx; Input Status Register=3DA)
			byte RAM_Enable : 1; //1=Enable VRAM System Access (see MISC Graphics Register); 0=Disabled.
			byte ClockSelect : 2; //0=25MHz;1=28MHz;2-3=Undefined (possible ext. clock)
			byte unused : 1; //EGA Only: Disable internal video drivers if set.
			byte OE_HighPage : 1; //Select upper/lower 64K page when system in odd/even mode. 0=Low;1=High.
			byte HSyncP : 1; //Horizontal Sync Polarity of Horizontal Sync Pulse. 0=Select Positive Horizontal Retrace Sync Pulse
			byte VSyncP : 1; //See HSyncP, but Vertical.
		};
	} MISCOUTPUTREGISTER; //Read at 3CCh, Write at 3C2h

	union
	{
		byte DATA;
		struct
		{
			byte FC0 : 1; //Unknown! Set to 0 for CGA mode!
			byte FC1 : 1; //Set to 1 for CGA mode!
			byte dummy : 6;
		};
	} FEATURECONTROLREGISTER; //Read at 3CAh, write at 3BAh(mono)/3DAh(color) Bit 1 is lightpen triggered when on!

	union
	{
		byte DATA;
		struct
		{
			byte Reserved0_1 : 4;
			byte SwitchSense : 1;
			byte Reserved0_2 : 2;
			byte CRTInterruptPending: 1; //0: Not pending, 1: Pending.
		};
	} INPUTSTATUS0REGISTER; //Input status #0 register (Read-only at 3C2h)

	union
	{
		byte DATA;
		struct
		{
			byte DisplayDisabled : 1; //Horizontal/vertical interval taking place? 1 during HBlank/VBlank, 0 else.
			byte LightpenTriggered : 1; //Light pen triggered?
			byte LightPenSwitchIsOpen : 1; //Light Pen switch is open?
			byte VRetrace : 1; //VSync Active: Is a VRetrace interval taking place?
			byte VideoFeedback10 : 2; //Color plane enable bits which are selected. EGA Compatibility only.
			byte Reserved : 1; //0's only!
			byte CRTInterruptPending : 1; //When enabled (see CR11 bit 4&5), turned on with interrupt on VBlank!
		};
	} INPUTSTATUS1REGISTER; //Read at 3BAh(mono), Read at 3DAh(color)

	union
	{
		uint_32 latch; //The full data latch!
		byte latchplane[4]; //All 4 plane latches!
	} DATALATCH;
} EXTERNALREGS;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{
	byte r;
	byte g;
	byte b;
} DACEntry; //A DAC Entry!
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{
	byte VideoMode; //Active video mode!

//Next, our registers:

//First, the indexed registers:
	GRAPHREGS GraphicsRegisters; //Graphics registers!
	byte GraphicsRegisters_Index; //Current index!

	SEQUENCERREGS SequencerRegisters; //Sequencer registers!
	byte SequencerRegisters_Index; //Current index!

	ATTRIBUTECONTROLLERREGS AttributeControllerRegisters; //Attribute controller registers!

	CRTCONTROLLERREGS CRTControllerRegisters; //CRT Controller registers!
	byte CRTControllerDontRender; //No rendering?
	byte CRTControllerRegisters_Index; //Current index!

//Now the normal registers, by group:

	COLORREGS ColorRegisters; //Color registers!
	EXTERNALREGS ExternalRegisters; //External registers!

	byte DACMaskRegister; //DAC Mask Register (port 0x3C6 for read/write; default is 0xFF)
	byte DAC[1024]; //All DAC values (4 bytes an entry)!

//For colors:
	byte colorIndex; //Written to port 0x3C8; 3 bytes to 3C9 in order R, G, B
	byte current_3C9; //Current by out of 3 from writing to 3C9.

//Index registers:
//Set by writing to 0x3C0, set address or data depending on 3C0_Current. See CRTController Register 24!

	byte ModeStep; //Current mode step (used for mode 0&1)!
	word Scanline; //Current scan line updating!

	byte VerticalDisplayTotalReached; //Set when Scanline>max. Setting this causes Scanline to reset!
	byte lightpen_high; //Lightpen high register!
	byte lightpen_low; //Lightpen low register!
} VGA_REGISTERS;
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{
	word rowstatus[0x800]; //Row status!
	word charrowstatus[0x1000]; //Character row status (double the row status, for character and inner)
	word colstatus[0x1000]; //Column status!
	word charcolstatus[0x2000]; //Character column status (double the row status, for character and inner)
	//Current processing coordinates on-screen!
	word x; //X coordinate on the screen!
	word y; //Y coordinate on the screen!
} VGA_CRTC; //CRTC information!
#include "headers/endpacked.h" //We're packed!

#include "headers/packed.h" //We're packed!
typedef struct PACKED
{
//First, VRAM and registers:
	byte *VRAM; //The VRAM: 64K of 32-bit values, byte align!
	uint_32 VRAM_size; //The size of the VRAM!
//Active video mode:

	VGA_REGISTERS *registers; //The registers itself!

	uint_32 CurrentScanLine[32]; //A scan line's data for the currently rendering scanline pixel!
	byte LinesToRender; //Value between 1-32, to determine how many scanlines to render!
	word CurrentScanLineStart; //Start line of CurrentScanLine!
	byte currentScanline; //Start line within the buffer we're at!
	uint_32 ExpandTable[256]; //Expand Table (originally 32-bit) for VRAM read and write by CPU!
	uint_32 FillTable[16]; //Fill table for memory writes!
	byte getcharxy_values[0x4000]; //All getcharxy values!
	byte CursorOn; //Cursor on?
	byte TextBlinkOn; //Text blink on? (Every 2 CursorOn changed)

	int initialised; //Are we initialised (row timer active? After VGAalloc is called, this is 1, row timer sets this to 0!)

//For rendering:
	int frameDone; //We have a frame ready for rendering? (for GPU refresh)
	byte CharacterRAMUpdated; //Character RAM updated?	
	
	//VGA Termination request and active state!
	int Request_Termination; //Request VGA termination: we're going to stop etc.
	int Terminated; //We're terminated?
	
//VBlank request and occurred.
	int wait_for_vblank; //Waiting for VBlank?
	int VGA_vblank; //VBlank occurred?
	VGA_PRECALCS precalcs; //Precalcs, for speedup!

	byte frameskip; //Frameskip!
	uint_32 framenr; //Current frame number (for Frameskip, kept 0 elsewise.)
	
	void *Sequencer; //The active sequencer to use!
	VGA_CRTC CRTC; //The CRTC information!
} VGA_Type; //VGA dataset!
#include "headers/endpacked.h" //We're packed!

#ifndef IS_VGA
extern VGA_Type *ActiveVGA; //Currently active VGA chipset!
#endif

/*

Read and write ports:

Info:						Type:
3B4h: CRTC Controller Address Register		ADDRESS
3B5h: CRTC Controller Data Register		DATA

3BAh Read: Input Status #1 Register (mono)	DATA
3BAh Write: Feature Control Register		DATA

3C0h: Attribute Address/Data register		ADDRESS/DATA
3C1h: Attribute Data Read Register		DATA

3C2h Read: Input Status #0 Register		DATA
3C2h Write: Miscellaneous Output Register	DATA

3C4h: Sequencer Address Register		ADDRESS
3C5h: Sequencer Data Register			DATA

3C7h Read: DAC State Register			DATA
3C7h Write: DAC Address Read Mode Register	ADDRESS

3C8h: DAC Address Write Mode Register		ADDRESS
3C9h: DAC Data Register				DATA

3CAh Read: Feature Control Register (mono Read)	DATA

3CCh Read: Miscellaneous Output Register	DATA

3CEh: Graphics Controller Address Register	ADDRESS
3CFh: Graphics Controller Data Register		DATA

3D4h: CRTC Controller Address Register		ADDRESS
3D5h: CRTC Controller Data Register		DATA

3DAh Read: Input Status #1 Register (color)	DATA
3DAh Write: Feature Control Register (color)	DATA

*/

/*

All functions!

*/









VGA_Type *VGAalloc(uint_32 custom_vram_size, int update_bios); //Allocates VGA and gives the current set!
void setupVGA(); //Sets the VGA up for PC usage (CPU access etc.)!
VGA_Type *getActiveVGA(); //Get the active VGA Chipset!
void setActiveVGA(VGA_Type *VGA); //Sets the active VGA chipset!
void terminateVGA(); //Terminate running VGA and disable it! Only to be used by root processes (non-VGA processes!)
void startVGA(); //Starts the current VGA! (See terminateVGA!)
void doneVGA(VGA_Type **VGA); //Cleans up after the VGA operations are done.


//CPU specific: ***
byte VRAM_readdirect(uint_32 offset);
void VRAM_writedirect(uint_32 offset, byte value); //Direct read/write!

//CPU read/write functionality (contained in BDA)
byte VRAM_readcpu(uint_32 offset);
void VRAM_writecpu(uint_32 offset, byte value);

byte PORT_readVGA(word port, byte *result); //Read from a port/register!
byte PORT_writeVGA(word port, byte value); //Write to a port/register!
//End of CPU specific! ***

//DAC (for rendering)
void readDAC(VGA_Type *VGA,byte entrynumber,DACEntry *entry); //Read a DAC entry
void writeDAC(VGA_Type *VGA,byte entrynumber,DACEntry *entry); //Write a DAC entry

//CRTC Controller renderer&int10:
void VGA_LoadVideoMode(byte mode); //Loads a preset video mode to be active!
void VGA_VBlankHandler(VGA_Type *VGA); //VBlank handler for VGA!

void VGA_waitforVBlank(); //Wait for a VBlank to happen?

void changeRowTimer(VGA_Type *VGA, word lines); //Change the VGA row processing timer the ammount of lines on display: should be in the emulator itself!

//Concerning rendering the VGA for the GPU!
void setVGAFrameskip(VGA_Type *VGA, byte Frameskip); //Set frameskip or 0 for none!

void dumpVRAM(); //Diagnostic dump of VRAM!

void VGAmemIO_reset(); //Reset/initialise memory mapped I/O for VGA!

void VGA_dumpFonts(); //Dump all VGA fonts!

void VGA_plane2updated(VGA_Type *VGA, uint_32 address); //Plane 2 has been updated?

void setVGA_NMIonPrecursors(byte enabled); //Trigger an NMI when our precursors are called?

#define lockVGA() lock(LOCK_VGA)
#define unlockVGA() unlock(LOCK_VGA)
//Give the active VGA!
#define getActiveVGA() ActiveVGA
#endif