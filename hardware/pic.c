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
}

byte in8259(word portnum, byte *result)
{
	if (__HW_DISABLED) return 0; //Abort!
	byte pic = ((portnum&0xFE)==0xA0)?1:(((portnum&0xFE)==0x20)?0:2); //PIC0/1/unknown!
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

OPTINLINE void EOI(byte PIC) //Process and (Automatic) EOI send to an PIC!
{
	if (__HW_DISABLED) return; //Abort!
	byte i;
	for (i=0; i<8; i++)
		if ((i8259.isr[PIC] >> i) & 1)
		{
			i8259.isr[PIC] ^= (1 << i);
			byte IRQ;
			IRQ = (PIC << 3) | i; //The IRQ we've finished!
			if (i8259.finishirq[IRQ]) //Gotten a handler?
			{
				i8259.finishirq[IRQ](IRQ); //We're done with this IRQ!
			}
			return;
		}
}

byte out8259(word portnum, byte value)
{
	if (__HW_DISABLED) return 0; //Abort!
	byte pic = ((portnum & 0xFE) == 0xA0) ? 1 : (((portnum & 0xFE) == 0x20) ? 0 : 2); //PIC0/1/unknown!
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
			EOI(pic); //Send an EOI!
		}
		break;
	case 1:
		if ((i8259.icwstep[pic] == 2) && (i8259.icw[pic][0] & 2))
		{
			++i8259.icwstep[pic]; //single mode, so don't read ICW3
			if (EMULATED_CPU <= CPU_NECV30) //PC/XT hack?
			{
				if (!pic) //PIC0?
				{
					i8259.icw[0][2] = 4; //Use IR Master!
					i8259.icw[1][2] = 2; //Use IR Slave!
				}
			}
		}
		if ((i8259.icwstep[pic] == 3) && (i8259.icw[pic][0] & 1))
		{
			++i8259.icwstep[pic]; //no ICW4 expected, so don't read ICW4
			if (EMULATED_CPU <= CPU_NECV30) //PC/XT hack?
			{
				if (!pic) //PIC0?
				{
					i8259.icw[0][3] = 1; //Set ICW4!
					i8259.icw[1][3] = 1; //Set ICW4!
				}
			}
		}
		if (i8259.icwstep[pic]<4)
		{
			if (i8259.icwstep[pic] == 1) //Interrupt number?
			{
				if (EMULATED_CPU <= CPU_NECV30) //PC/XT hack?
				{
					if (!pic) //PIC0?
					{
						i8259.icw[1][1] = 0x70; //Set ICW2 interrupt base vector!
					}
				}
			}
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
	return !((i8259.icw[0][0] & 2) || //Only one PIC?
		(i8259.icw[0][2] != 4) || //Wrong IR to connect?
		(i8259.icw[1][2] != 2)); //Wrong IR to connect?
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
		return 1;
	}

	if (getunprocessedinterrupt(1)) //Secondary PIC interrupt?
	{
		return 1;
	}

	return 0; //No interrupt to process!
}

OPTINLINE byte IRRequested(byte PIC, byte IR) //We have this requested?
{
	if (__HW_DISABLED) return 0; //Abort!
	return ((getunprocessedinterrupt(PIC) >> IR) & 1); //Interrupt requested?
}

OPTINLINE void ACNIR(byte PIC, byte IR) //Acnowledge request!
{
	if (__HW_DISABLED) return; //Abort!
	i8259.irr[PIC] ^= (1 << IR); //Turn IRR off!
	i8259.isr[PIC] |= (1 << IR); //Turn in-service on!
	byte IRQ;
	IRQ = (PIC << 3) | IR; //The IRQ we're accepting!
	if (i8259.acceptirq[IRQ]) //Gotten a handler?
	{
		i8259.acceptirq[IRQ](IRQ); //We're accepting th
	}
	if ((i8259.icw[PIC][3]&2)==2) //Automatic EOI?
	{
		EOI(PIC); //Send an EOI!
	}
}

OPTINLINE byte getint(byte PIC, byte IR) //Get interrupt!
{
	if (__HW_DISABLED) return 0; //Abort!
	byte realir = IR; //Default: nothing changed!
	if ((realir == 1) && PIC) //IRQ9?
	{
		PIC = 0; //PIC1!
		realir = 2; //Reroute IRQ 9 to 2!
	}
	return i8259.icw[PIC][1]+realir; //Get interrupt!
}

byte nextintr()
{
	if (interruptsaved) //Re-requested?
	{
		return lastinterrupt; //Give the same as the last time!
	}
	if (__HW_DISABLED) return 0; //Abort!
	byte i;

	//First, process first PIC!
	for (i=0; i<16; i++) //Process all IRs!
	{
		byte IR = i8259.IROrder[i]; //Get the prioritized IR!
		byte PICnr = ((IR>>3)&1); //What pic?
		byte realIR = (IR&7); //What IR within the PIC?
		if (IRRequested(PICnr,realIR)) //Requested?
		{
			ACNIR(PICnr, realIR); //Acnowledge it!
			lastinterrupt = getint(PICnr, realIR); //Give the interrupt number!
			interruptsaved = 1; //Gotten an interrupt saved!
			return lastinterrupt;
		}
	}
	lastinterrupt = 0; //Unknown!
	interruptsaved = 1; //Gotten!
	return lastinterrupt; //No result: unk interrupt!
}

void doirq(byte irqnum)
{
	if (__HW_DISABLED) return; //Abort!
	byte PIC = (irqnum>>3); //IRQ8+ is high PIC!
	i8259.irr[PIC] |= (1 << (irqnum&7)); //Add the IRQ to request!
}

void removeirq(byte irqnum)
{
	if (__HW_DISABLED) return; //Abort!
	byte PIC = (irqnum>>3); //IRQ8+ is high PIC!
	i8259.irr[PIC] &= ~(1 << (irqnum&7)); //Remove the IRQ from request!
}

void registerIRQ(byte IRQ, IRQHandler acceptIRQ, IRQHandler finishIRQ)
{
	//Register the handlers!
	i8259.acceptirq[IRQ] = acceptIRQ;
	i8259.finishirq[IRQ] = finishIRQ;
}