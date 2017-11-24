#include "headers/types.h" //Basic type support!
#include "headers/hardware/vga/vga.h" //VGA data!
#include "headers/hardware/vga/vga_vram.h" //VRAM support!

//We handle all input for writing to VRAM and reading from VRAM directly here!

//Bit from left to right starts with 0(value 128) ends with 7(value 1)

//Planar access to VRAM
byte readVRAMplane(VGA_Type *VGA, byte plane, uint_32 offset, uint_32 bank) //Read from a VRAM plane!
{
	if (unlikely(VGA==0)) return 0; //Invalid VGA!
	plane &= 3; //Only 4 planes are available! Wrap arround the planes if needed!

	INLINEREGISTER uint_32 fulloffset2;
	fulloffset2 = offset; //Default offset to use!
	fulloffset2 <<= 2; //We cycle through the offsets!
	fulloffset2 |= plane; //The plane goes from low to high, through all indexes!
	fulloffset2 += bank; //Add the bank directly!

	fulloffset2 &= VGA->precalcs.VMemMask; //Only 64K memory available, so wrap arround it when needed!

	if (unlikely(fulloffset2>=VGA->VRAM_size)) return 0; //VRAM valid, simple check?
	return VGA->VRAM[fulloffset2]; //Read the data from VRAM!
}

void writeVRAMplane(VGA_Type *VGA, byte plane, uint_32 offset, uint_32 bank, byte value) //Write to a VRAM plane!
{
	if (unlikely(VGA==0)) return; //Invalid VGA!
	plane &= 3; //Only 4 planes are available!

	INLINEREGISTER uint_32 fulloffset2;
	fulloffset2 = offset; //Load the offset!
	fulloffset2 <<= 2; //We cycle through the offsets!
	fulloffset2 |= plane; //The plane goes from low to high, through all indexes!
	fulloffset2 += bank; //Add the bank directly!

	fulloffset2 &= VGA->precalcs.VMemMask; //Only 64K memory available, so wrap arround it when needed!

	if (unlikely(fulloffset2>=VGA->VRAM_size)) return; //VRAM valid, simple check?
	VGA->VRAM[fulloffset2] = value; //Set the data in VRAM!
	if (unlikely(plane==2)) //Character RAM updated?
	{
		VGA_plane2updated(VGA,offset); //Plane 2 has been updated!	
	}
}