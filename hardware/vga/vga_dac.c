#include "headers/types.h" //Types!
#include "headers/hardware/vga.h" //VGA!

void readDAC(VGA_Type *VGA, byte entrynumber,DACEntry *entry) //Read a DAC entry
{
	word entryi;
	entryi = entrynumber;
	entryi <<= 2; //Multiply by 4!
	entry->r = VGA->registers->DAC[entryi]; //R
	entry->g = VGA->registers->DAC[entryi|1]; //G
	entry->b = VGA->registers->DAC[entryi|2]; //B
}

void writeDAC(VGA_Type *VGA, byte entrynumber,DACEntry *entry) //Write a DAC entry
{
	word entryi;
	entryi = entrynumber;
	entryi <<= 2; //Multiply by 4!
	VGA->registers->DAC[entryi] = entry->r; //R
	VGA->registers->DAC[entryi|1] = entry->g; //G
	VGA->registers->DAC[entryi|2] = entry->b; //B
	VGA_calcprecalcs(VGA,WHEREUPDATED_DAC|entrynumber); //We've been updated!
}