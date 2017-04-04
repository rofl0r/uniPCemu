#include "headers/types.h" //Basic types!
#include "headers/hardware/8237A.h" //DMA0 support!
#include "headers/hardware/8253.h" //PIT1 support!

byte DRAM_DREQ = 0; //Our DREQ signal!
byte DRAM_Pending = 0; //DRAM tick is pending?
extern byte SystemControlPortB; //System control port B!

void DRAM_DMADREQ() //For checking any new DREQ signals of DRAM!
{
	DMA_SetDREQ(0,DRAM_Pending); //Set the current DREQ0: DRAM Refresh!
}

void DRAM_DACK()
{
	//We're to acnowledge the DACK!
	DRAM_Pending = 0; //Lower the DREQ signal now!
}

void DRAM_setDREQ(byte output)
{
	if ((output!=DRAM_DREQ) && output) //DREQ raised?
	{
		DRAM_Pending = 1; //Start pending!
	}
	DRAM_DREQ = output; //PIT1 is connected to the DREQ signal!
}

void DRAM_access(uint_32 address) //Accessing DRAM?
{
	//Tick the part of RAM affected! Clear RAM on timeout(lower bits are specified)!
}

void initDRAM()
{
	registerPIT1Ticker(&DRAM_setDREQ); //Register our ticker for timing DRAM ticks!
	registerDMATick(0, &DRAM_DMADREQ, &DRAM_DACK, NULL); //Our handlers for DREQ, DACK and TC of the DRAM refresh! Don't handle DACK and TC!
	DRAM_DREQ = 0; //Init us!
}
