#include "headers/types.h" //Basic type support!
#include "headers/hardware/ports.h" //Basic PORT compatibility!
#include "headers/hardware/vga.h" //VGA data!
#include "headers/mmu/mmu.h" //For CPU passtrough!
#include "headers/hardware/vga_rest/textmodedata.h" //Text mode data for loading!
#include "headers/hardware/vga_rest/colorconversion.h" //Color conversion support!
#include "headers/hardware/vga_screen/vga_crtcontroller.h" //CRT controller!
#include "headers/support/log.h" //Logging support for debugging this!
#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer support for special actions!
#include "headers/support/zalloc.h" //Zero allocation (memprotect) support!
#include "headers/hardware/vga_screen/vga_vram.h" //VRAM support!

//VGA.VRAM is a pointer to the start of the VGA VRAM (256K large)

//COLOR MODES:
//VGA: 2, 3 (B/W/Bold), 4, 4 shades, 16, 256
//SVGA: 32k, 64k, True Colors

//We handle all input for writing to VRAM (CPU interrupts) and reading from VRAM (hardware) here!

//is_renderer determines special stuff for the renderer:
//bit 1 contains the "I'm a renderer" flag.

byte LOG_VRAM_WRITES = 0;

//Bit from left to right starts with 0(value 128) ends with 7(value 1)

//Retrieval function for single bits instead of full bytes!
byte getBitPlaneBit(VGA_Type *VGA, byte plane, word offset, byte bit, byte is_renderer)
{
	return GETBIT(readVRAMplane(VGA,plane,offset,is_renderer),7-bit); //Give the bit!
}

//Below patches input addresses for rendering only.
OPTINLINE word patch_map1314(VGA_Type *VGA, word addresscounter) //Patch full VRAM address!
{ //Check this!
	word memoryaddress = addresscounter; //New row scan to use!
	SEQ_DATA *Sequencer;
	Sequencer = (SEQ_DATA *)VGA->Sequencer; //The sequencer!

	register word rowscancounter = Sequencer->Scanline; //Load the row scan counter!
	rowscancounter = VGA->CRTC.charrowstatus[rowscancounter << 1]; //Take the row status as our source!
	rowscancounter >>= VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.SLDIV; //Apply scanline division!
	rowscancounter >>= VGA->precalcs.scandoubling; //Apply Scan Doubling here: we take effect on content!

	register word bit; //Load row scan counter!
	if (!VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.MAP13) //a13=Bit 0 of the row scan counter!
	{
		//Row scan counter bit 1 is placed on the memory bus bit 14 during active display time.
		//Bit 1, placed on memory address bit 14 has the effect of quartering the memory.
		bit = rowscancounter; //Current row scan counter!
		bit &= 1; //Bit0 only!
		bit <<= 13; //Shift to our position (bit 13)!
		memoryaddress &= 0xDFFF; //Clear bit13!
		memoryaddress |= bit; //Set bit13 if needed!
	}

	if (!VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.MAP14) //a14<=Bit 1 of the row scan counter!
	{
		bit = rowscancounter; //Current row scan counter!
		bit &= 2; //Bit1 only!
		bit <<= 13; //Shift to our position (bit 14)!
		memoryaddress &= 0xBFFF; //Clear bit14;
		memoryaddress |= bit; //Set bit14 if needed!
	}
	
	return memoryaddress; //Give the linear address!
}

OPTINLINE uint_32 addresswrap(VGA_Type *VGA, word memoryaddress) //Wraps memory arround 64k!
{
	register word address2; //Load the initial value for calculating!
	register word result;
	result = memoryaddress; //Default: don't change!
	if (VGA->precalcs.VRAMmemaddrsize==2) //Word mode?
	{
		address2 = memoryaddress; //Load the address for calculating!
		if (VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.AW) //MA15 has to be on MA0
		{
			address2 >>= 15;
		}
		else //MA13 has to be on MA0?
		{
			address2 >>= 13;
		}
		address2 &= 1; //Only load 1 bit!
		result &= 0xFFFE; //Clear bit 0!
		result |= address2; //Add bit MA15 at position 0!
	}
	return result; //Adjusted address!
}

//Planar access to VRAM
byte readVRAMplane(VGA_Type *VGA, byte plane, word offset, byte is_renderer) //Read from a VRAM plane!
{
	if (!VGA) return 0; //Invalid VGA!
	if (!VGA->VRAM_size) return 0; //No size!
	word patchedoffset = offset; //Default offset to use!

	if (is_renderer) //First address wrap, next map13&14!
	{
		//First, apply addressing mode!
		patchedoffset = addresswrap(VGA,patchedoffset); //Wrap first!
		patchedoffset = patch_map1314(VGA,patchedoffset); //Patch MAP13&14!
	}

	plane &= 3; //Only 4 planes are available! Wrap arround the planes if needed!

	register uint_32 fulloffset2;
	fulloffset2 = patchedoffset; //Load the offset!
	fulloffset2 <<= 2; //We cylce through the offsets!
	fulloffset2 |= plane; //The plane goes from low to high, through all indexes!

	if (fulloffset2<VGA->VRAM_size) //VRAM valid, simple check?
	{
		return VGA->VRAM[fulloffset2]; //Read the data from VRAM!
	}
	return 0; //Nothing there: invalid VRAM!
}

void writeVRAMplane(VGA_Type *VGA, byte plane, word offset, byte value) //Write to a VRAM plane!
{
	if (!VGA) return; //Invalid VGA!
	if (!VGA->VRAM_size) return; //No size!
	
	plane &= 3; //Only 4 planes are available!

	register uint_32 fulloffset2;
	fulloffset2 = offset; //Load the offset!
	fulloffset2 <<= 2; //We cycle through the offsets!
	fulloffset2 |= plane; //The plane goes from low to high, through all indexes!

	if (fulloffset2<VGA->VRAM_size) //VRAM valid, simple check?
	{
		VGA->VRAM[fulloffset2] = value; //Set the data in VRAM!
		if (plane==2) //Character RAM updated?
		{
			VGA_plane2updated(VGA,offset); //Plane 2 has been updated!	
		}
	}
}

/*void setBitPlaneBit(VGA_Type *VGA, int plane, uint_32 offset, byte bit, byte on) //For testing only. Read-Modify-Write!
{
	byte bits;
	bits = readVRAMplane(VGA,plane,offset,0); //Get original bits!
	if (on) //To turn bit on?
	{
		bits = SETBIT1(bits,7-bit); //Turn bit on!
	}
	else //To turn bit off?
	{
		bits = SETBIT0(bits,7-bit); //Turn bit off!
	}
	writeVRAMplane(VGA,plane,offset,bits); //Write the modified value back!
}*/

//END FLAG_OF VGA COLOR SUPPORT!

//SVGA color support

/*
union
{
	struct
	{
		union
		{
			struct
			{
				byte datalow;
				byte datahigh;
			};
			word data;
		};
	};
	struct
	{
		byte b : 5;
		byte g : 5;
		byte r : 5;
		byte u : 1;
	};
} decoder32k; //32K decoder!

/
32k colors: 1:5:5:5
/

uint_32 MEMGRAPHICS_get32kcolors(uint_32 startaddr, int x, int y)
{
	uint_32 pixelnumber;
	pixelnumber = startaddr+((y*xres)*2)+(x*2); //Starting pixel!
	decoder32k.datahigh = VRAM_readdirect(pixelnumber+1);
	decoder32k.datalow = VRAM_readdirect(pixelnumber);
	return getcolX(decoder32k.r,decoder32k.g,decoder32k.b,0x1F); //Give RGB!
}

void MEMGRAPHICS_put32kcolors(uint_32 startaddr, int x, int y, word color)
{
	uint_32 pixelnumber;
	pixelnumber = startaddr+((y*xres)*2)+(x*2); //Starting pixel!
	VRAM_writedirect(pixelnumber,(color&0xFF)); //Low
	VRAM_writedirect(pixelnumber+1,((color>>8)&0xFF)); //High!
}

union
{
	struct
	{
		union
		{
			struct
			{
				byte datalow;
				byte datahigh;
			};
			word data;
		};
	};
	struct
	{
		byte b : 5;
		byte g : 6;
		byte r : 5;
	};
} decoder64k; //64K decoder!


/
64k colors: 5:6:5
/

uint_32 MEMGRAPHICS_get64kcolors(uint_32 startaddr, int x, int y)
{
	uint_32 pixelnumber;
	pixelnumber = startaddr+((y*xres)*2)+(x*2); //Starting pixel!
	decoder64k.datahigh = VRAM_readdirect(pixelnumber+1);
	decoder64k.datalow = VRAM_readdirect(pixelnumber);
	return getcol64k(decoder32k.r,decoder32k.g,decoder32k.b); //Give RGB!
}

void MEMGRAPHICS_put64kcolors(uint_32 startaddr, int x, int y, word color)
{
	uint_32 pixelnumber;
	pixelnumber = startaddr+((y*xres)*2)+(x*2); //Starting pixel!
	VRAM_writedirect(pixelnumber,(color&0xFF)); //Low
	VRAM_writedirect(pixelnumber+1,((color>>8)&0xFF)); //High!
}


/
24 bits true color 8:8:8
/

uint_32 MEMGRAPHICS_getTruecolors(uint_32 startaddr, int x, int y)
{
	uint_32 pixelnumber;
	pixelnumber = startaddr+((y*xres)*3)+(x*3); //Starting pixel!
	return RGB(VRAM_readdirect(pixelnumber),VRAM_readdirect(pixelnumber+1),VRAM_readdirect(pixelnumber+2));
}

void MEMGRAPHICS_putTruecolors(uint_32 startaddr, int x, int y, uint_32 color)
{
	uint_32 pixelnumber;
	pixelnumber = startaddr+((y*xres)*3)+(x*3); //Starting pixel!
	VRAM_writedirect(pixelnumber,(color&0xFF)); //R
	VRAM_writedirect(pixelnumber+1,((color>>8)&0xFF)); //G
	VRAM_writedirect(pixelnumber+2,((color>>16)&0xFF)); //B
}*/