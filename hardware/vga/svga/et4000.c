#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //Basic VGA!
#include "headers/hardware/vga/svga/et4000.h" //Our own typedefs!
#include "headers/support/zalloc.h" //Memory allocation support!
#include "headers/hardware/vga/vga_precalcs.h" //Precalculation typedefs etc.
#include "headers/hardware/ports.h" //I/O support!
#include "headers/bios/bios.h" //Basic BIOS support!
#include "headers/hardware/vga/vga_attributecontroller.h" //Attribute controller support!
#include "headers/hardware/vga/vga_sequencer_graphicsmode.h" //Graphicas mode support!

float ET4K_clockFreq[16] = {
	0.0f, //VGA defined!
	0.0f, //VGA defined!
	32400000.0f, //ET4000 clock!
	35900000.0f, //ET4000 clock!
	39900000.0f, //ET4000 clock!
	44700000.0f, //ET4000 clock!
	31400000.0f, //ET4000 clock!
	37500000.0f, //ET4000 clock!
	50000000.0f, //ET4000 clock!
	56500000.0f, //ET4000 clock!
	64900000.0f, //ET4000 clock!
	71900000.0f, //ET4000 clock!
	79900000.0f, //ET4000 clock!
	89600000.0f, //ET4000 clock!
	62800000.0f, //ET4000 clock!
	74800000.0f //ET4000 clock!
	};

extern uint_32 VGA_MemoryMapBankRead, VGA_MemoryMapBankWrite; //The memory map bank to use!

byte Tseng4K_writeIO(word port, byte val)
{
	SVGA_ET4K_DATA *et4kdata = et4k_data; //The et4k data!
// Tseng ET4K implementation
	switch (port) //What port?
	{
	case 0x3D5: //CRTC data register?
//void 3d5_et4k(Bitu reg,Bitu val,Bitu iolen) {
	if(!et4kdata->extensionsEnabled && getActiveVGA()->registers->CRTControllerRegisters_Index !=0x33)
		return 0;

	switch(getActiveVGA()->registers->CRTControllerRegisters_Index)
	{
		/*
		3d4h index 31h (R/W):  General Purpose
		bit  0-3  Scratch pad
			 6-7  Clock Select bits 3-4. Bits 0-1 are in 3C2h/3CCh bits 2-3.
		*/
		STORE_ET4K(3d4, 31,WHEREUPDATED_CRTCONTROLLER);

		// 3d4h index 32h - RAS/CAS Configuration (R/W)
		// No effect on emulation. Should not be written by software.
		STORE_ET4K(3d4, 32,WHEREUPDATED_CRTCONTROLLER);

		case 0x33:
			// 3d4 index 33h (R/W): Extended start Address
			// 0-1 Display Start Address bits 16-17
			// 2-3 Cursor start address bits 16-17
			// Used by standard Tseng ID scheme
			et4kdata->store_3d4_33 = val;
			et4kdata->display_start_high = ((val & 0x03)<<16);
			et4kdata->cursor_start_high = ((val & 0x0c)<<14);
			VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CRTCONTROLLER|0x33); //Update all precalcs!
			break;

		/*
		3d4h index 34h (R/W): 6845 Compatibility Control Register
		bit    0  Enable CS0 (alternate clock timing)
			   1  Clock Select bit 2.  Bits 0-1 in 3C2h bits 2-3, bits 3-4 are in 3d4h
				  index 31h bits 6-7
			   2  Tristate ET4000 bus and color outputs if set
			   3  Video Subsystem Enable Register at 46E8h if set, at 3C3h if clear.
			   4  Enable Translation ROM for reading CRTC and MISCOUT if set
			   5  Enable Translation ROM for writing CRTC and MISCOUT if set
			   6  Enable double scan in AT&T compatibility mode if set
			   7  Enable 6845 compatibility if set
		*/
		// TODO: Bit 6 may have effect on emulation
		STORE_ET4K(3d4, 34,WHEREUPDATED_CRTCONTROLLER);

		case 0x35: 
		/*
		3d4h index 35h (R/W): Overflow High
		bit    0  Vertical Blank Start Bit 10 (3d4h index 15h).
			   1  Vertical Total Bit 10 (3d4h index 6).
			   2  Vertical Display End Bit 10 (3d4h index 12h).
			   3  Vertical Sync Start Bit 10 (3d4h index 10h).
			   4  Line Compare Bit 10 (3d4h index 18h).
			   5  Gen-Lock Enabled if set (External sync)
			   6  (4000) Read/Modify/Write Enabled if set. Currently not implemented.
			   7  Vertical interlace if set. The Vertical timing registers are
				programmed as if the mode was non-interlaced!!
		*/
			et4kdata->store_3d4_35 = val;
			et4kdata->line_compare_high = ((val&0x10)<<6);
			VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CRTCONTROLLER|0x35); //Update all precalcs!
			break;

		// 3d4h index 36h - Video System Configuration 1 (R/W)
		// VGADOC provides a lot of info on this register, Ferraro has significantly less detail.
		// This is unlikely to be used by any games. Bit 4 switches chipset into linear mode -
		// that may be useful in some cases if there is any software actually using it.
		// TODO (not near future): support linear addressing
		STORE_ET4K(3d4, 36,WHEREUPDATED_CRTCONTROLLER);

		// 3d4h index 37 - Video System Configuration 2 (R/W)
		// Bits 0,1, and 3 provides information about memory size:
		// 0-1 Bus width (1: 8 bit, 2: 16 bit, 3: 32 bit)
		// 3   Size of RAM chips (0: 64Kx, 1: 256Kx)
		// Other bits have no effect on emulation.
		case 0x37:
			if (val != et4kdata->store_3d4_37) {
				et4kdata->store_3d4_37 = val;
				et4kdata->memwrap = (((64*1024)<<((val&8)>>2))<<((val&3)-1))-1; //The mask to use for memory!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CRTCONTROLLER|0x37); //Update all precalcs!
			}
			return 1;
			break;

		case 0x3f:
		/*
		3d4h index 3Fh (R/W):
		bit    0  Bit 8 of the Horizontal Total (3d4h index 0)
			   2  Bit 8 of the Horizontal Blank Start (3d4h index 3)
			   4  Bit 8 of the Horizontal Retrace Start (3d4h index 4)
			   7  Bit 8 of the CRTC offset register (3d4h index 13h).
		*/
		// The only unimplemented one is bit 7
			et4kdata->store_3d4_3f = val;
		// Abusing s3 ex_hor_overflow field which very similar. This is
		// to be cleaned up later
			VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CRTCONTROLLER|0x3F); //Update all precalcs!
			return 1;
			break;
		default:
			//LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:CRTC:ET4K:Write to illegal index %2X", reg);
			break;
		}
		break;
	case 0x3C5: //Sequencer data register?
	//void write_p3c5_et4k(Bitu reg,Bitu val,Bitu iolen) {
		switch(getActiveVGA()->registers->SequencerRegisters_Index) {
		/*
		3C4h index  6  (R/W): TS State Control
		bit 1-2  Font Width Select in dots/character
				If 3C4h index 4 bit 0 clear:
					0: 9 dots, 1: 10 dots, 2: 12 dots, 3: 6 dots
				If 3C4h index 5 bit 0 set:
					0: 8 dots, 1: 11 dots, 2: 7 dots, 3: 16 dots
				Only valid if 3d4h index 34h bit 3 set.
		*/
		// TODO: Figure out if this has any practical use
		STORE_ET4K(3c4, 06,WHEREUPDATED_SEQUENCER);
		// 3C4h index  7  (R/W): TS Auxiliary Mode
		// Unlikely to be used by games (things like ROM enable/disable and emulation of VGA vs EGA)
		STORE_ET4K(3c4, 07,WHEREUPDATED_SEQUENCER);
		default:
			//LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:SEQ:ET4K:Write to illegal index %2X", reg);
			break;
		}
		break;
	/*
	3CDh (R/W): Segment Select
	bit 0-3  64k Write bank number (0..15)
	4-7  64k Read bank number (0..15)
	*/
	//void write_p3cd_et4k(Bitu port, Bitu val, Bitu iolen) {
	case 0x3CD: //Segment select?
		et4kdata->bank_write = val & 0x0f;
		et4kdata->bank_read = (val >> 4) & 0x0f;
		//Apply correct memory banks!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CRTCONTROLLER|0x36); //Update from the CRTC controller registers!
		//VGA_SetupHandlers();
		//}
		return 1;
		break;
	case 0x3C0: //Attribute controller?
		//void write_p3c0_et4k(Bitu reg, Bitu val, Bitu iolen) {
		switch (VGA_3C0_INDEX) {
			// 3c0 index 16h: ATC Miscellaneous
			// VGADOC provides a lot of information, Ferarro documents only two bits
			// and even those incompletely. The register is used as part of identification
			// scheme.
			// Unlikely to be used by any games but double timing may be useful.
			// TODO: Figure out if this has any practical use
			STORE_ET4K(3c0, 16,WHEREUPDATED_ATTRIBUTECONTROLLER);
			/*
			3C0h index 17h (R/W):  Miscellaneous 1
			bit   7  If set protects the internal palette ram and redefines the attribute
			bits as follows:
			Monochrome:
			bit 0-2  Select font 0-7
			3  If set selects blinking
			4  If set selects underline
			5  If set prevents the character from being displayed
			6  If set displays the character at half intensity
			7  If set selects reverse video
			Color:
			bit 0-1  Selects font 0-3
			2  Foreground Blue
			3  Foreground Green
			4  Foreground Red
			5  Background Blue
			6  Background Green
			7  Background Red
			*/
			// TODO: Figure out if this has any practical use
			STORE_ET4K(3c0, 17,WHEREUPDATED_ATTRIBUTECONTROLLER);
		default:
			//LOG(LOG_VGAMISC, LOG_NORMAL)("VGA:ATTR:ET4K:Write to illegal index %2X", reg);
			break;
		}
		break;
	default: //Unknown port?
		return 0;
		break;
	}
	return 0; //Unsupported port!
}

byte Tseng4K_readIO(word port, byte *result)
{
	SVGA_ET4K_DATA *et4kdata = et4k_data; //The et4k data!
	switch (port)
	{
	case 0x3D5: //CRTC data register?
	//Bitu read_p3d5_et4k(Bitu reg,Bitu iolen) {
		if (!et4kdata->extensionsEnabled && getActiveVGA()->registers->CRTControllerRegisters_Index !=0x33)
			return 0x0;
		switch(getActiveVGA()->registers->CRTControllerRegisters_Index) {
		RESTORE_ET4K(3d4, 31);
		RESTORE_ET4K(3d4, 32);
		RESTORE_ET4K(3d4, 33);
		RESTORE_ET4K(3d4, 34);
		RESTORE_ET4K(3d4, 35);
		RESTORE_ET4K(3d4, 36);
		RESTORE_ET4K(3d4, 37);
		RESTORE_ET4K(3d4, 3f);
		default:
			//LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:CRTC:ET4K:Read from illegal index %2X", reg);
			return 0;
			break;
		}
	case 0x3C5: //Sequencer data register?
	//Bitu read_p3c5_et4k(Bitu reg,Bitu iolen) {
		switch(getActiveVGA()->registers->SequencerRegisters_Index) {
		RESTORE_ET4K(3c4, 06);
		RESTORE_ET4K(3c4, 07);
		default:
			//LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:SEQ:ET4K:Read from illegal index %2X", reg);
			break;
		}
		break;
	case 0x3CD: //Segment select?
	//Bitu read_p3cd_et4k(Bitu port, Bitu iolen) {
		*result = (et4kdata->bank_read << 4) | et4kdata->bank_write;
		return 1; //Supported!
		break;
	case 0x3C1: //Attribute controller read?
	//Bitu read_p3c1_et4k(Bitu reg, Bitu iolen) {
		switch (VGA_3C0_INDEX) {
			RESTORE_ET4K(3c0, 16);
			RESTORE_ET4K(3c0, 17);
		default:
			//LOG(LOG_VGAMISC, LOG_NORMAL)("VGA:ATTR:ET4K:Read from illegal index %2X", reg);
			break;
		}
		break;
	default: //Unknown port?
		break;
	}
	return 0; //Unsupported port!
}

/*
These ports are used but have little if any effect on emulation:
	3BFh (R/W): Hercules Compatibility Mode
	3CBh (R/W): PEL Address/Data Wd
	3CEh index 0Dh (R/W): Microsequencer Mode
	3CEh index 0Eh (R/W): Microsequencer Reset
	3d8h (R/W): Display Mode Control
	3DEh (W);  AT&T Mode Control Register
*/

OPTINLINE static byte get_clock_index_et4k(VGA_Type *VGA) {
	// Ignoring bit 4, using "only" 16 frequencies. Looks like most implementations had only that
	return ((VGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.DATA>>2)&3) | ((et4k(VGA)->store_3d4_34<<1)&4) | ((et4k(VGA)->store_3d4_31>>3)&8);
}

/*static void set_clock_index_et4k(VGA_Type *VGA, byte index) {
	// Shortwiring register reads/writes for simplicity
	et4k_data->store_3d4_34 = (et4k(VGA)->store_3d4_34&~0x02)|((index&4)>>1);
	et4k_data->store_3d4_31 = (et4k(VGA)->store_3d4_31&~0xc0)|((index&8)<<3); // (index&0x18) if 32 clock frequencies are to be supported
	PORT_write_MISC_3C2((VGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.DATA&~0x0c)|((index&3)<<2));
}*/ //Currently it isn't manually set!

extern byte EMU_VGAROM[0x10000];

uint_32 Tseng4k_VRAMSize = 0; //Setup VRAM size?

extern BIOS_Settings_TYPE BIOS_Settings; //Current BIOS settings to be updated!

void Tseng4k_init()
{
	byte *Tseng_VRAM = NULL; //The new VRAM to use with our card!
	if (getActiveVGA()) //Gotten active VGA? Initialise the full hardware if needed!
	{
		if (getActiveVGA()->enable_SVGA==1) //Are we enabled as SVGA?
		{
			//Handle all that needs to be initialized for the Tseng 4K!
			// Default to 1M of VRAM
			if (Tseng4k_VRAMSize == 0)
				Tseng4k_VRAMSize = 1024 * 1024;

			if (Tseng4k_VRAMSize < 512 * 1024)
				Tseng4k_VRAMSize = 256 * 1024;
			else if (Tseng4k_VRAMSize < 1024 * 1024)
				Tseng4k_VRAMSize = 512 * 1024;
			else
				Tseng4k_VRAMSize = 1024 * 1024;
			debugrow("VGA: Allocating VGA VRAM...");
			Tseng_VRAM = (byte *)zalloc(Tseng4k_VRAMSize, "VGA_VRAM", getLock(LOCK_VGA)); //The VRAM allocated to 0!
			if (Tseng_VRAM) //VRAM allocated?
			{
				freez((void **)&getActiveVGA()->VRAM,getActiveVGA()->VRAM_size,"VGA_VRAM"); //Release the original VGA VRAM!
				getActiveVGA()->VRAM = Tseng_VRAM; //Assign the new Tseng VRAM instead!
				getActiveVGA()->VRAM_size = Tseng4k_VRAMSize; //Assign the Tseng VRAM size!
			}
			BIOS_Settings.VRAM_size = getActiveVGA()->VRAM_size; //Update VRAM size in BIOS!
			forceBIOSSave(); //Force save of BIOS!

			// Tseng ROM signature
			EMU_VGAROM[0x0075] = ' ';
			EMU_VGAROM[0x0076] = 'T';
			EMU_VGAROM[0x0077] = 's';
			EMU_VGAROM[0x0078] = 'e';
			EMU_VGAROM[0x0079] = 'n';
			EMU_VGAROM[0x007a] = 'g';
			EMU_VGAROM[0x007b] = ' ';

			et4k(getActiveVGA())->extensionsEnabled = 1; //Enable the extensions!

			VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_ALL); //Update all precalcs!
		}
	}
}

//ET4K precalcs updating functionality.
void Tseng4k_calcPrecalcs(void *useVGA, uint_32 whereupdated)
{
	VGA_Type *VGA = (VGA_Type *)useVGA; //The VGA to work on!
	SVGA_ET4K_DATA *et4kdata = et4k(VGA); //The et4k data!
	byte updateCRTC = 0; //CRTC updated?
	byte et4k_tempreg;
	uint_32 tempdata; //Saved data!
	if (!et4k(VGA)) return; //No extension registered?
	if (!et4k(VGA)->extensionsEnabled) return; //Abort when we're disabled!
	//Bits 4-5 of the Attribute Controller register 0x16(Miscellaneous) determine the mode to be used when decoding pixels:
	/*
	00=Normal power-up/default(VGA mode)
	01=Reserved
	10=High-resolution mode (up to 256 colors)
	11=High-color 16-bits/pixel
	*/
	if ((whereupdated == WHEREUPDATED_ALL) || (whereupdated == (WHEREUPDATED_ATTRIBUTECONTROLLER|0x16))) //Attribute misc. register?
	{
		et4k_tempreg = (et4k_reg(et4kdata,3c0,16)>>4)&3; //The mode to use when decoding!
		if ((VGA->precalcs.AttributeController_16bitDAC&1)==0)
		{
			et4k_tempreg = 0; //Ignore the reserved value, forcing VGA mode in that case!
		}
		else if (et4k_tempreg & 1) //Valid to process an extended mode?
		{
			et4k_tempreg = (et4k_tempreg>>1) | ((et4k_tempreg&1)<<1); //Mode 1=2 and mode 3=3(convert by switching the bits).
		}
		VGA->precalcs.AttributeController_16bitDAC = et4k_tempreg; //Set the new mode to use (mode 2/3 or 0)!
		//Modes 2&3 set forced 8-bit and 16-bit Attribute modes!
		updateVGAAttributeController_Mode(VGA); //Update the attribute controller mode, which might have changed!
		updateVGAGraphics_Mode(VGA);
	}

	if ((whereupdated == WHEREUPDATED_ALL) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER|0x33)) || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0xC)) || (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0xD))) //Extended start address?
	{
		VGA->precalcs.startaddress[0] = (VGA->precalcs.startaddress[0]&0xFFFF)|et4k(VGA)->cursor_start_high;
	}

	if ((whereupdated == WHEREUPDATED_ALL) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x33)) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0xE)) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0xF))) //Extended cursor location?
	{
		VGA->precalcs.cursorlocation = (VGA->precalcs.cursorlocation & 0xFFFF) | et4k(VGA)->cursor_start_high;
	}

	et4k_tempreg = et4k_reg(et4kdata,3d4,35); //The overflow register!

	if ((whereupdated == WHEREUPDATED_ALL) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x35)) //Extended bits of the overflow register!
		|| (whereupdated==(WHEREUPDATED_CRTCONTROLLER|0x7)) || //Overflow register itself
		//Finally, bits needed by the overflow register itself(of which we are an extension)!
		(whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x12)) //Vertical display end
		) //Extended bits of the overflow register!
	{
		//bit2=Vertical display end bit 10
		tempdata = VGA->precalcs.verticaldisplayend; //Load old data!
		--tempdata; //One later!
		updateCRTC |= ((((et4k_tempreg&4)<<8))|(tempdata&0x3FF))!=tempdata; //To be updated?
		tempdata = ((et4k_tempreg & 4) << 9) | (tempdata & 0x3FF); //Add/replace the new/changed bits!
		++tempdata; //One later!
		VGA->precalcs.verticaldisplayend = tempdata; //Save the new data!
	}

	if ((whereupdated == WHEREUPDATED_ALL) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x35)) //Extended bits of the overflow register!
		|| (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x7)) || //Overflow register itself
		//Finally, bits needed by the overflow register itself(of which we are an extension)!
		(whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x15)) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x9)) //Vertical blanking start
		)
	{
		//bit0=Vertical blank bit 10
		tempdata = VGA->precalcs.verticalblankingstart; //Load old data!
		updateCRTC |= (((et4k_tempreg & 1) << 10) | (tempdata & 0x3FF)) != tempdata; //To be updated?
		tempdata = ((et4k_tempreg & 1) << 10) | (tempdata & 0x3FF); //Add/replace the new/changed bits!
		VGA->precalcs.verticalblankingstart = tempdata; //Save the new data!
	}

	if ((whereupdated == WHEREUPDATED_ALL) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x35)) //Extended bits of the overflow register!
		|| (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x7)) || //Overflow register itself
		//Finally, bits needed by the overflow register itself(of which we are an extension)!
		(whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x10)) //Vertical retrace start
		)
	{
		//bit3=Vertical sync start bit 10
		tempdata = VGA->precalcs.verticalretracestart;
		updateCRTC |= (((et4k_tempreg & 8) << 7) | (tempdata & 0x3FF)) != tempdata; //To be updated?
		tempdata = ((et4k_tempreg & 8) << 7) | (tempdata & 0x3FF); //Add/replace the new/changed bits!
		VGA->precalcs.verticalretracestart = tempdata; //Save the new data!
	}

	if ((whereupdated == WHEREUPDATED_ALL) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x35)) //Extended bits of the overflow register!
		|| (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x7)) || //Overflow register itself
		//Finally, bits needed by the overflow register itself(of which we are an extension)!
		(whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x6)) //Vertical total
		)
	{
		//bit1=Vertical total bit 10
		tempdata = VGA->precalcs.verticaltotal;
		--tempdata; //One later!
		updateCRTC |= (((et4k_tempreg & 2) << 9) | (tempdata & 0x3FF)) != tempdata; //To be updated?
		tempdata = ((et4k_tempreg & 2) << 9) | (tempdata & 0x3FF); //Add/replace the new/changed bits!
		++tempdata; //One later!
		VGA->precalcs.verticaltotal = tempdata; //Save the new data!
	}

	if ((whereupdated == WHEREUPDATED_ALL) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x35)) //Extended bits of the overflow register!
		|| (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x7)) || //Overflow register itself
		//Finally, bits needed by the overflow register itself(of which we are an extension)!
		(whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x18)) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x9)) //Line compare
		)
	{
		//bit4=Line compare bit 10
		tempdata = VGA->precalcs.topwindowstart; //Load!
		--tempdata; //One later!
		updateCRTC |= (((et4k_tempreg & 0x10) << 6) | (tempdata & 0x3FF)) != tempdata; //To be updated?
		tempdata = ((et4k_tempreg & 0x10) << 6) | (tempdata & 0x3FF); //Add/replace the new/changed bits!
		++tempdata; //One later!
		VGA->precalcs.topwindowstart = tempdata; //Save the new data!
	}

	if ((whereupdated == WHEREUPDATED_ALL) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x34)) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x31))) //Clock frequency might have been updated?
	{
		if (VGA==getActiveVGA()) //Active VGA?
		{
			changeRowTimer(VGA,VGA->precalcs.clockselectrows); //Make sure the display scanline refresh rate is OK!
		}		
	}

	//Misc settings
	if ((whereupdated == WHEREUPDATED_ALL) || (whereupdated == (WHEREUPDATED_CRTCONTROLLER | 0x36))) //Video system configuration #1!
	{
		et4k_tempreg = et4k_reg(et4kdata, 3d4, 36); //The overflow register!
		if ((et4k_tempreg & 0x10)==0x00) //Segment configuration?
		{
			VGA_MemoryMapBankRead = et4kdata->bank_read << 16; //Read bank!
			VGA_MemoryMapBankWrite = et4kdata->bank_write << 16; //Write bank!
			VGA->precalcs.linearmode &= ~2; //Use normal data addresses!
		}
		else //Linear system configuration? Disable the segment and enable linear mode (high 4 bits of the address select the bank)!
		{
			VGA->precalcs.linearmode |= 2; //Linear mode, use high 4-bits!
		}
		if (et4k_tempreg & 0x20) //Continuous memory?
		{
			VGA->precalcs.linearmode |= 1; //Enable contiguous memory!
		}
		else //Normal memory addressing?
		{
			VGA->precalcs.linearmode &= ~1; //Use VGA-mapping of memory!
		}
		VGA->precalcs.linearmode |= 4; //Enable the new linear and contiguous modes to affect memory!
	}

	if (updateCRTC) //Update CRTC?
	{
		VGA_calcprecalcs_CRTC(VGA); //Update the CRTC timing data!
		adjustVGASpeed(); //Auto-adjust our VGA speed!
	}
}

float Tseng4k_getClockRate(VGA_Type *VGA)
{
	byte clock_index;
	if (!et4k(VGA)) return 0.0f; //Unregisterd ET4K!
	clock_index = get_clock_index_et4k(VGA); //Retrieve the ET4K clock index!
	if (clock_index>=2) //New clocks for ET4K that aren't the same as a normal VGA?
	{
		return ET4K_clockFreq[clock_index&0xF]; //Give the ET4K clock index rate!
	}
	return 0.0f; //Not an ET4K clock rate, default to VGA rate!
}

void SVGA_Setup_TsengET4K(uint_32 VRAMSize) {
	// From the depths of X86Config, probably inexact
	VGA_registerExtension(&Tseng4K_readIO, &Tseng4K_writeIO, &Tseng4k_init,&Tseng4k_calcPrecalcs,&Tseng4k_getClockRate);
	Tseng4k_VRAMSize = VRAMSize; //Set this VRAM size to use!
	getActiveVGA()->SVGAExtension = zalloc(sizeof(SVGA_ET4K_DATA),"SVGA_ET4K_DATA",getLock(LOCK_VGA)); //Our SVGA extension data!
	if (!getActiveVGA()->SVGAExtension)
	{
		raiseError("ET4000","Couldn't allocate SVGA card ET4000 data! Ran out of memory!");
	}
}