#include "headers/types.h" //Basic type support!
#include "headers/hardware/pic.h" //Basic data!

/* Note: This is not a very complete i8259 interrupt controller
   implementation, but for the purposes of a PC, it's acceptable. */

//PIC Info: http://www.brokenthorn.com/Resources/OSDevPic.html

#include "headers/hardware/pic.h" //Own typedefs etc.
#include "headers/hardware/ports.h" //Port support!

//Are we disabled?
#define __HW_DISABLED 0

PIC i8259;

byte defaultIROrder[16] = { 0,1,2,8,9,10,11,12,13,14,15,3,4,5,6,7 }; //The order of IRQs!
extern uint_32 makeupticks;

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
	register_PORTOUT(0x20,&out8259);
	register_PORTOUT(0x21,&out8259);
	register_PORTIN(0x20,&in8259);
	register_PORTIN(0x21,&in8259);
	//PIC1!
	register_PORTOUT(0xA0,&out8259);
	register_PORTOUT(0xA1,&out8259);
	register_PORTIN(0xA0,&in8259);
	register_PORTIN(0xA1,&in8259);

	//All set up!
}

byte in8259(word portnum)
{
	if (__HW_DISABLED) return 0; //Abort!
	byte pic = ((portnum&0xFE)==0xA0)?1:0; //PIC0/1!
	switch (portnum & 1)
	{
	case 0:
		if (i8259.readmode[pic]==0) return(i8259.irr[pic]);
		else return(i8259.isr[pic]);
	case 1: //read mask register
		return(i8259.imr[pic]);
	}
	return 0; //Shouldn't be here: unknown port number!
}

void EOI(byte PIC) //Process and (Automatic) EOI send to an PIC!
{
	if (__HW_DISABLED) return; //Abort!
	byte i;
	for (i=0; i<8; i++)
		if ((i8259.isr[PIC] >> i) & 1)
		{
			i8259.isr[PIC] ^= (1 << i);
			if ((i==0) && (makeupticks>0))
			{
				makeupticks = 0;
				i8259.irr[PIC] |= 1;
			}
			return;
		}

}

void out8259(word portnum, byte value)
{
	if (__HW_DISABLED) return; //Abort!
	byte pic = ((portnum & 0xFE) == 0xA0) ? 1 : 0; //PIC0/1!
	switch (portnum & 1)
	{
	case 0:
		if (value & 0x10)   //begin initialization sequence
		{
			i8259.icwstep[pic] = 1;
			i8259.imr[pic] = 0; //clear interrupt mask register
			i8259.icw[pic][i8259.icwstep[pic]++] = value;
			return;
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
		if ((i8259.icwstep[pic]==3) && (i8259.icw[pic][1] & 2)) i8259.icwstep[pic] = 4; //single mode, so don't read ICW3
		if ((i8259.icwstep[pic] == 4) && (i8259.icw[pic][1] & 1)) i8259.icwstep[pic] = 5; //no ICW4 expected, so don't read ICW4
		if (i8259.icwstep[pic]<5)
		{
			i8259.icw[pic][i8259.icwstep[pic]++] = value;
			return;
		}
		//if we get to this point, this is just a new IMR value
		i8259.imr[pic] = value;
		break;
	}
}

byte interruptsaved = 0; //Have we gotten a primary interrupt (first PIC)?
byte lastinterrupt = 0; //Last interrupt requested!

byte PICInterrupt() //We have an interrupt ready to process?
{
	if (__HW_DISABLED) return 0; //Abort!
	if ((i8259.irr[0] & (~i8259.imr[0])) || interruptsaved) //Primary PIC interrupt?
	{
		return 1;
	}

	if (
		(i8259.icw[0][1]&2)|| //Only one PIC?
		(i8259.icw[0][2]!=4)|| //Wrong IR to connect?
		(i8259.icw[1][2]!=2) //Wrong IR to connect?
		)
	{
		return 0; //No second PIC, so no interrupt!
	}
	
	if (i8259.irr[1] & (~i8259.imr[1])) //Secondary PIC interrupt?
	{
		return 1;
	}

	return 0; //No interrupt to process!
}

byte IRRequested(byte PIC, byte IR) //We have this requested?
{
	if (__HW_DISABLED) return 0; //Abort!
	if (PIC==2 && //Second PIC addressed?
			(
			(i8259.icw[0][1]&2)|| //Only one PIC?
			(i8259.icw[0][2]!=4)|| //Wrong IR to connect?
			(i8259.icw[1][2]!=2) //Wrong IR to connect?
			)
			) //Disabled second PIC?
	{
		return 0; //Disable interrupt!	
	}
	byte tmpirr = i8259.irr[0] & (~i8259.imr[0]); //XOR request register with inverted mask register
	return ((tmpirr >> IR) & 1); //Interrupt requested?
}

void ACNIR(byte PIC, byte IR) //Acnowledge request!
{
	if (__HW_DISABLED) return; //Abort!
	i8259.irr[PIC] ^= (1 << IR); //Turn IRR off!
	i8259.isr[PIC] |= (1 << IR); //Turn in-service on!
	if ((i8259.icw[PIC][3]&2)==2) //Automatic EOI?
	{
		EOI(PIC); //Send an EOI!
	}
}

byte getint(byte PIC, byte IR) //Get interrupt!
{
	if (__HW_DISABLED) return 0; //Abort!
	byte realir = IR; //Default: nothing changed!
	if (realir==2 && PIC==1) realir = 9; //Reroute interrupt 2 to 9!
	return i8259.icw[PIC][2]+realir; //Get interrupt!
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
		byte PICnr = (IR>>3); //What pic?
		byte realIR = (IR&7); //What IR within the PIC?
		if (IRRequested(PICnr,realIR)) //Requested?
		{
			ACNIR(PICnr,realIR); //Acnowledge it!
			lastinterrupt = getint(PICnr,realIR); //Give the interrupt number!
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