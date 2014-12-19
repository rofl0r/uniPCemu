#include "headers/types.h" //Types!
#include "headers/hardware/vga.h" //VGA!

void readDAC(VGA_Type *VGA, byte entrynumber,DACEntry *entry) //Read a DAC entry
{
	entry->r = VGA->registers->DAC[entrynumber<<2]; //R
	entry->g = VGA->registers->DAC[(entrynumber<<2)|1]; //G
	entry->b = VGA->registers->DAC[(entrynumber<<2)|2]; //B
}

void writeDAC(VGA_Type *VGA, byte entrynumber,DACEntry *entry) //Write a DAC entry
{
	VGA->registers->DAC[entrynumber<<2] = entry->r; //R
	VGA->registers->DAC[(entrynumber<<2)|1] = entry->g; //G
	VGA->registers->DAC[(entrynumber<<2)|2] = entry->b; //B
	VGA_calcprecalcs(VGA,WHEREUPDATED_DAC|entrynumber); //We've been updated!
}