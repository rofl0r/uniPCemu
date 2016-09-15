#include "headers/types.h" //Basic types!
#include "headers/hardware/8237A.h" //DMA0 support!
#include "headers/hardware/8253.h" //PIT1 support!

byte DRAM_DREQ = 0; //Our DREQ signal!
extern byte SystemControlPortB; //System control port B!

void DRAM_DMADREQ() //For checking any new DREQ signals of DRAM!
{
	DMA_SetDREQ(0,DRAM_DREQ); //Set the current DREQ0: DRAM Refresh!
}

void DRAM_setDREQ(byte output)
{
	DRAM_DREQ = output; //PIT1 is connected to the DREQ signal!
	if (EMULATED_CPU>=CPU_80286)) //We show PIT1 status at port 0x61 bit 4?
	{
		SystemControlPortB = (SystemControlPortB&0xEF)|(output<<4); //Toggle the refresh register to let know we're active!
	}
}

void DRAM_access(uint_32 address) //Accessing DRAM?
{
	//Tick the part of RAM affected! Clear RAM on timeout(lower bits are specified)!
}

void initDRAM()
{
	registerPIT1Ticker(&DRAM_setDREQ); //Register our ticker for timing DRAM ticks!
	registerDMATick(0, &DRAM_DMADREQ, NULL, NULL); //Our handlers for DREQ, DACK and TC of the DRAM refresh! Don't handle DACK and TC!
	DRAM_DREQ = 0; //Init us!
}
