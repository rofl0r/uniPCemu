#include "headers/types.h" //Basic type support!
#include "headers/hardware/pic.h" //Basic data!
#include "headers/cpu/cpu.h" //CPU support!

/* Note: This is not a very complete i8259 interrupt controller
   implementation, but for the purposes of a PC, it's acceptable. */

//PIC Info: http://www.brokenthorn.com/Resources/OSDevPic.html

#include "headers/hardware/pic.h" //Own typedefs etc.
#include "headers/hardware/ports.h" //Port support!

//Are we disabled?
#define __HW_DISABLED 0

PIC i8259;

//i8259.irr is the complete status of all 8 interrupt lines at the moment. Any software having raised it's line, raises this. Otherwise, it's lowered!
//i8259.irr2 is the live status of each of the parallel interrupt lines!
//i8259.irr3 is the identifier for request subchannels that are pending to be acnowledged(cleared when acnowledge and the interrupt is fired).

byte defaultIROrder[16] = { 0,1,2,8,9,10,11,12,13,14,15,3,4,5,6,7 }; //The order of IRQs!

void init8259()
{
	if (__HW_DISABLED) return; //Abort!
	memset(&i8259, 0, sizeof(i8259));
	byte c;
	for (c=0;c<16;c++)
	{
		i8259.IROrder[c] = defaultIROrder[c]; //Set default IR order!
	}
	//Now the port handling!
	//PIC0!
	register_PORTOUT(&out8259);
	register_PORTIN(&in8259);
	//All set up!

	i8259.imr[0] = 0xFF; //Mask off all interrupts to start!
	i8259.imr[1] = 0xFF; //Mask off all interrupts to start!
}

byte in8259(word portnum, byte *result)
{
	if (__HW_DISABLED) return 0; //Abort!
	byte pic = ((portnum&~1)==0xA0)?1:(((portnum&~1)==0x20)?0:2); //PIC0/1/unknown!
	if (pic == 2) return 0; //Not our PIC!
	switch (portnum & 1)
	{
	case 0:
		if (i8259.readmode[pic]==0) *result = i8259.irr[pic];
		else *result = i8259.isr[pic];
		break;
	case 1: //read mask register
		*result = i8259.imr[pic];
		break;
	}
	return 1; //The result is given!
}

OPTINLINE void EOI(byte PIC, byte source) //Process and (Automatic) EOI send to an PIC!
{
	if (__HW_DISABLED) return; //Abort!
	byte i;
	for (i=0; i<8; i++)
		if ((i8259.isr[PIC] >> i) & 1)
		{
			i8259.isr[PIC] ^= (1 << i);
			byte IRQ;
			IRQ = (PIC << 3) | i; //The IRQ we've finished!
			byte currentsrc;
			for (currentsrc=0;currentsrc<0x10;++currentsrc) //Check all sources!
			{
				if (i8259.finishirq[IRQ][currentsrc]) //Gotten a handler?
				{
					if (i8259.isr2[IRQ][currentsrc]) //We've finished?
					{
						i8259.finishirq[IRQ][currentsrc](IRQ); //We're done with this IRQ!
						i8259.isr2[IRQ][currentsrc] ^= (1<<i); //Not in service anymore!
					}
				}
			}
			return;
		}
}

extern byte is_XT; //Are we emulating a XT architecture?

byte out8259(word portnum, byte value)
{
	byte source;
	if (__HW_DISABLED) return 0; //Abort!
	byte pic = ((portnum & ~1) == 0xA0) ? 1 : (((portnum & ~1) == 0x20) ? 0 : 2); //PIC0/1/unknown!
	if (pic == 2) return 0; //Not our PIC!
	switch (portnum & 1)
	{
	case 0:
		if (value & 0x10)   //begin initialization sequence
		{
			i8259.icwstep[pic] = 0; //Init ICWStep!
			i8259.imr[pic] = 0; //clear interrupt mask register
			i8259.icw[pic][i8259.icwstep[pic]++] = value;
			return 1;
		}
		if ((value & 0x98)==8)   //it's an OCW3
		{
			if (value & 2) i8259.readmode[pic] = value & 2;
		}
		if (value & 0x20)   //EOI command
		{
			for (source=0;source<0x10;++source) //Check all sources!
			{
				EOI(pic,source); //Send an EOI from this source!
			}
		}
		break;
	case 1:
		if ((i8259.icwstep[pic] == 2) && (i8259.icw[pic][0] & 2))
		{
			++i8259.icwstep[pic]; //single mode, so don't read ICW3
		}
		if ((i8259.icwstep[pic] == 3) && (i8259.icw[pic][0] & 1))
		{
			++i8259.icwstep[pic]; //no ICW4 expected, so don't read ICW4
		}
		if (i8259.icwstep[pic]<4)
		{
			i8259.icw[pic][i8259.icwstep[pic]++] = value;
			return 1;
		}
		else if (i8259.icw[0][0]&2) //Second PIC disabled?
		{
			i8259.icw[0][0] &= ~2; //Enable second PIC always!
		}
		//if we get to this point, this is just a new IMR value
		i8259.imr[pic] = value;
		break;
	}
	return 1; //We're processed!
}

byte interruptsaved = 0; //Have we gotten a primary interrupt (first PIC)?
byte lastinterrupt = 0; //Last interrupt requested!

OPTINLINE byte enablePIC(byte PIC)
{
	if (!PIC) return 1; //PIC0 always enabled!
	return ((!((i8259.icw[0][0] & 2) || //Only one PIC?
		(i8259.icw[0][2] != 4) || //Wrong IR to connect?
		(i8259.icw[1][2] != 2))) //Wrong IR to connect?
		&& ((i8259.isr[0]&4)==0)); //ISR on PIC0 is keeping the second PIC functioning sending status!
}

OPTINLINE byte getunprocessedinterrupt(byte PIC)
{
	if (!enablePIC(PIC)) return 0; //PIC disabled?
	byte result;
	result = i8259.irr[PIC];
	result &= ~i8259.imr[PIC];
	result &= ~i8259.isr[PIC];
	return result; //Give the result!
}

byte PICInterrupt() //We have an interrupt ready to process?
{
	if (__HW_DISABLED) return 0; //Abort!
	if (getunprocessedinterrupt(0) || interruptsaved) //Primary PIC interrupt?
	{
		i8259.activePIC = 0; //From PIC0!
		return 1;
	}

	if (getunprocessedinterrupt(1)) //Secondary PIC interrupt?
	{
		i8259.activePIC = 1; //From PIC1!
		return 1;
	}

	return 0; //No interrupt to process!
}

OPTINLINE byte IRRequested(byte PIC, byte IR, byte source) //We have this requested?
{
	if (__HW_DISABLED) return 0; //Abort!
	return (((getunprocessedinterrupt(PIC) & (i8259.irr3[PIC&1][source]))>> IR) & 1); //Interrupt requested on the specified source?
}

OPTINLINE void ACNIR(byte PIC, byte IR, byte source) //Acnowledge request!
{
	if (__HW_DISABLED) return; //Abort!
	i8259.irr3[PIC][source] &= ~(1 << IR); //Turn source IRR off!
	i8259.irr[PIC] &= ~(1<<IR); //Clear the request!
	i8259.isr[PIC] |= (1 << IR); //Turn in-service on!
	i8259.isr2[PIC][source] |= (1 << IR); //Turn the source on!
	byte IRQ;
	IRQ = (PIC << 3) | IR; //The IRQ we're accepting!
	if (i8259.acceptirq[IRQ][source]) //Gotten a handler?
	{
		i8259.acceptirq[IRQ][source](IRQ); //We're accepting the IRQ from this source!
	}
	if ((i8259.icw[PIC][3]&2)==2) //Automatic EOI?
	{
		EOI(PIC,source); //Send an EOI!
	}
	if (PIC) //Second PIC?
	{
		ACNIR(0,2,source); //Acnowledging request on first PIC too! This keeps us from firing until acnowledged properly!
	}
}

OPTINLINE byte getint(byte PIC, byte IR) //Get interrupt!
{
	if (__HW_DISABLED) return 0; //Abort!
	byte realir = IR; //Default: nothing changed!
	return i8259.icw[PIC][1]+realir; //Get interrupt!
}

byte nextintr()
{
	if (__HW_DISABLED) return 0; //Abort!
	byte i;

	//First, process first PIC!
	for (i=0; i<16; i++) //Process all IRs!
	{
		byte IR = i8259.IROrder[i]; //Get the prioritized IR!
		byte PICnr = ((IR>>3)&1); //What pic?
		byte realIR = (IR&7); //What IR within the PIC?
		byte srcIndex;
		for (srcIndex=0;srcIndex<0x10;++srcIndex) //Check all indexes!
		{
			if (IRRequested(PICnr,realIR,srcIndex)) //Requested?
			{
				ACNIR(PICnr, realIR,srcIndex); //Acnowledge it!
				lastinterrupt = getint(PICnr, realIR); //Give the interrupt number!
				interruptsaved = 1; //Gotten an interrupt saved!
				return lastinterrupt;
			}
		}
	}

	lastinterrupt = getint(i8259.activePIC,7); //Unknown, dispatch through IR7 of the used PIC!
	interruptsaved = 1; //Gotten!
	return lastinterrupt; //No result: unk interrupt!
}

void raiseirq(byte irqnum)
{
	if (__HW_DISABLED) return; //Abort!
	byte requestingindex=irqnum; //Save our index that's requesting!
	irqnum &= 0xF; //Only 16 IRQs!
	requestingindex >>= 4; //What index is requesting?
	byte PIC = (irqnum>>3); //IRQ8+ is high PIC!
	byte irr2index;
	byte hasirr = 0;
	byte oldIRR = 0;
	//Handle edge-triggered IRR!
	hasirr = 0; //Init IRR state!
	for (irr2index = 0;irr2index < 8;++irr2index) //Verify if anything is left!
	{
		if (i8259.irr2[PIC][irr2index] & (1 << (irqnum & 7))) //Request still set?
		{
			hasirr = 1; //We still have an IRR!
			break; //Stop searching!
		}
	}
	oldIRR = hasirr; //Old IRR state!

	i8259.irr2[PIC][requestingindex] |= (1 << (irqnum & 7)); //Add the IRQ to request!
	hasirr = 0; //Init IRR state!
	for (irr2index = 0;irr2index < 8;++irr2index) //Verify if anything is left!
	{
		if (i8259.irr2[PIC][irr2index] & (1 << (irqnum & 7))) //Request still set?
		{
			hasirr = 1; //We still have an IRR!
			break; //Stop searching!
		}
	}

	if (hasirr && ((hasirr^oldIRR)&1)) //The line is actually raised?
	{
		i8259.irr[PIC] |= (1 << (irqnum & 7)); //Add the IRQ to request because of the rise!
		i8259.irr3[PIC][requestingindex] |= (1 << (irqnum & 7)); //Add the IRQ to request because of the rise! This causes us to be the reason during shared IR lines!
	}
}

void lowerirq(byte irqnum)
{
	if (__HW_DISABLED) return; //Abort!
	byte requestingindex = irqnum; //Save our index that's requesting!
	irqnum &= 0xF; //Only 16 IRQs!
	requestingindex >>= 4; //What index is requesting?
	byte PIC = (irqnum>>3); //IRQ8+ is high PIC!
	if (i8259.irr2[PIC][requestingindex]) //Were we raised? We're lowered!
	{
		i8259.irr[PIC] &= ~(1<<(irqnum&7)); //Remove the request, if any!
	}
	i8259.irr2[PIC][requestingindex] &= ~(1 << (irqnum & 7)); //Remove the IRQ to request!
	i8259.irr3[PIC][requestingindex] &= ~(1 << (irqnum & 7)); //Remove the request being used itself!
}

void acnowledgeIRQrequest(byte irqnum)
{
	//byte requestingindex = irqnum; //Save our index that's requesting!
	//irqnum &= 0xF; //Only 16 IRQs!
	//requestingindex >>= 4; //What index is requesting?
	//byte PIC = (irqnum >> 3); //IRQ8+ is high PIC!
	//i8259.irr[PIC] &= ~(1 << (irqnum & 7)); //Remove the IRQ from request! Don't affect the signal we receive, just acnowledge it so that no more interrupts are fired!
	//We don't lower raised interrupts!
}

void registerIRQ(byte IRQ, IRQHandler acceptIRQ, IRQHandler finishIRQ)
{
	//Register the handlers!
	i8259.acceptirq[IRQ&0xF][IRQ>>4] = acceptIRQ;
	i8259.finishirq[IRQ&0xF][IRQ>>4] = finishIRQ;
}
