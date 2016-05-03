#include "headers/types.h" //Basic type support!
#include "headers/hardware/ports.h" //Basic PORT compatibility!
#include "headers/hardware/vga/vga.h" //VGA data!
#include "headers/mmu/mmu.h" //For CPU passtrough!
#include "headers/hardware/vga/vga_crtcontroller.h" //CRT controller!
#include "headers/support/log.h" //Logging support for debugging this!
#include "headers/hardware/vga/vga_sequencer.h" //Sequencer support for special actions!
#include "headers/support/zalloc.h" //Zero allocation (memprotect) support!
#include "headers/hardware/vga/vga_vram.h" //VRAM support!
#include "headers/hardware/vga/vga_cga_mda.h" //CGA/MDA support for it's display memory!

//VGA.VRAM is a pointer to the start of the VGA VRAM (256K large)

//COLOR MODES:
//VGA: 2, 3 (B/W/Bold), 4, 4 shades, 16, 256
//SVGA: 32k, 64k, True Colors

//We handle all input for writing to VRAM (CPU interrupts) and reading from VRAM (hardware) here!

//Bit from left to right starts with 0(value 128) ends with 7(value 1)

//Planar access to VRAM
byte readVRAMplane(VGA_Type *VGA, byte plane, word offset) //Read from a VRAM plane!
{
	if (!VGA) return 0; //Invalid VGA!
	if (!VGA->VRAM_size) return 0; //No size!
	plane &= 3; //Only 4 planes are available! Wrap arround the planes if needed!

	INLINEREGISTER uint_32 fulloffset2;
	fulloffset2 = offset; //Default offset to use!
	fulloffset2 <<= 2; //We cycle through the offsets!
	fulloffset2 |= plane; //The plane goes from low to high, through all indexes!

	fulloffset2 &= 0x3FFFF; //Wrap arround memory! Maximum of 256K memory!

	if (!VGA->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.ExtendedMemory) fulloffset2 &= 0xFFFF; //Only 64K memory available, so wrap arround it!

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

	INLINEREGISTER uint_32 fulloffset2;
	fulloffset2 = offset; //Load the offset!
	fulloffset2 <<= 2; //We cycle through the offsets!
	fulloffset2 |= plane; //The plane goes from low to high, through all indexes!

	fulloffset2 &= 0x3FFFF; //Wrap arround memory!

	if (!VGA->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.ExtendedMemory) fulloffset2 &= 0xFFFF; //Only 64K memory available, so wrap arround it!

	if (fulloffset2<VGA->VRAM_size) //VRAM valid, simple check?
	{
		VGA->VRAM[fulloffset2] = value; //Set the data in VRAM!
		if (plane==2) //Character RAM updated?
		{
			VGA_plane2updated(VGA,offset); //Plane 2 has been updated!	
		}
	}
}