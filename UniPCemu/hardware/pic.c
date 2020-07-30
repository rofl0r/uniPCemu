/*

Copyright (C) 2019  Superfury

This file is part of UniPCemu.

UniPCemu is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

UniPCemu is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with UniPCemu.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "headers/types.h" //Basic type support!
#include "headers/hardware/pic.h" //Basic data!
#include "headers/hardware/ports.h" //Port support!

//PIC Info: http://www.brokenthorn.com/Resources/OSDevPic.html


//Are we disabled?
#define __HW_DISABLED 0

PIC i8259;
byte irr3_dirty = 0; //IRR3/IRR3_a is changed?

//i8259.irr is the complete status of all 8 interrupt lines at the moment. Any software having raised it's line, raises this. Otherwise, it's lowered(irr3 are all cleared)!
//i8259.irr2 is the live status of each of the parallel interrupt lines!
//i8259.irr3 is the identifier for request subchannels that are pending to be acnowledged(cleared when acnowledge and the interrupt is fired).

void init8259()
{
	if (__HW_DISABLED) return; //Abort!
	memset(&i8259, 0, sizeof(i8259));
	//Now the port handling!
	//PIC0!
	register_PORTOUT(&out8259);
	register_PORTIN(&in8259);
	//All set up!

	i8259.imr[0] = 0xFF; //Mask off all interrupts to start!
	i8259.imr[1] = 0xFF; //Mask off all interrupts to start!
	irr3_dirty = 0; //Default: not dirty!
}

byte readPollingMode(byte pic); //Prototype!

byte in8259(word portnum, byte *result)
{
	if (__HW_DISABLED) return 0; //Abort!
	byte pic = ((portnum&~1)==0xA0)?1:(((portnum&~1)==0x20)?0:2); //PIC0/1/unknown!
	if (pic == 2) return 0; //Not our PIC!
	switch (portnum & 1)
	{
	case 0:
		if (i8259.pollingmode[pic]) //Polling mode enabled?
		{
			*result = readPollingMode(pic); //Read the polling mode!
			i8259.pollingmode[pic] = 0; //Not anymore!
		}
		else //Normal mode?
		{
			if (i8259.readmode[pic] == 0) *result = i8259.irr[pic];
			else *result = i8259.isr[pic];
		}
		break;
	case 1: //read mask register
		if (i8259.pollingmode[pic]) //Polling mode enabled?
		{
			*result = readPollingMode(pic); //Read the polling mode!
			i8259.pollingmode[pic] = 0; //Not anymore!
		}
		else //Normal mode?
		{
			*result = i8259.imr[pic];
		}
		break;
	default:
		break;
	}
	return 1; //The result is given!
}

OPTINLINE void EOI(byte PIC, byte source) //Process and (Automatic) EOI send to an PIC!
{
	if (__HW_DISABLED) return; //Abort!
	byte i;
	for (i = 0; i < 8; i++)
	{
		if ((i8259.isr[PIC] >> i) & 1)
		{
			i8259.isr[PIC] ^= (1 << i);
			byte IRQ;
			IRQ = (PIC << 3) | i; //The IRQ we've finished!
			byte currentsrc;
			currentsrc = source; //Check the specified source!
			if (i8259.isr2[PIC][currentsrc]&(1<<(IRQ&7))) //We've finished?
			{
				if (i8259.finishirq[IRQ][currentsrc]) //Gotten a handler?
				{
					i8259.finishirq[IRQ][currentsrc](IRQ|(currentsrc<<4)); //We're done with this IRQ!
				}
				i8259.isr2[PIC][currentsrc] ^= (1 << i); //Not in service anymore!
			}
			return;
		}
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
		if (value & 0x10)   //begin initialization sequence(OCS)
		{
			i8259.icwstep[pic] = 0; //Init ICWStep!
			memset(&i8259.irr[pic],0,sizeof(i8259.irr[pic])); //Reset IRR raised sense!
			memset(&i8259.irr3[pic],0,sizeof(i8259.irr3[pic])); //Reset IRR shared raised sense!
			memset(&i8259.irr3_a[pic],0,sizeof(i8259.irr3_a[pic])); //Reset IRR shared raised sense!
			memset(&i8259.irr3_b[pic], 0, sizeof(i8259.irr3_b[pic])); //Reset IRR shared raised sense!
			irr3_dirty = 0; //Not dirty anymore!
			i8259.imr[pic] = 0; //clear interrupt mask register
			i8259.icw[pic][i8259.icwstep[pic]++] = value; //Set the ICW1!
			i8259.icw[pic][2] = 7; //Slave mode address is set to 7?
			if ((i8259.icw[pic][0] & 1)==0) //ICW4 not sent?
			{
				i8259.icw[pic][3] = 0; //ICW4 has all it's functions set to zero!
			}
			i8259.readmode[pic] = 0; //Default to IRR reading after a reset!
			return 1;
		}
		if ((value & 0x98)==0x08) //it's an OCW3
		{
			i8259.pollingmode[pic] = ((value & 4) >> 2); //Enable polling mode?
			if (value & 2) i8259.readmode[pic] = value & 1; //Read ISR instead of IRR on reads? Only modify this setting when setting this setting(bit 2 is set)!
			return 1;
		}
		if ((value & 0x18) == 0) //it's an OCW2
		{
			//We're a OCW2!
			//if (((value & 0xE0)==0x20) || ((value&0xE0)==0x60)) //EOI command
			if ((value&0xE0)!=0x40) //Ignore type! Not a NOP?
			{
				if (value & 0x20) //It's an EOI-type command(non-specific, specific, rotate on non-specific, rotate on specific)?
				{
					for (source = 0; source < 0x10; ++source) //Check all sources!
					{
						EOI(pic, source); //Send an EOI from this source!
					}
				}
			}
		}
		return 1;
		break;
	case 1:
		if (i8259.icwstep[pic]<4) //Not sent all ICW yet?
		{
			i8259.icw[pic][i8259.icwstep[pic]++] = value;
			if ((i8259.icwstep[pic] == 2) && (i8259.icw[pic][0] & 2)) //Next is not ICW3?
			{
				++i8259.icwstep[pic]; //single mode, so don't read ICW3
			}
			if ((i8259.icwstep[pic] == 3) && ((i8259.icw[pic][0] & 1)==0)) //Next is not ICW4?
			{
				++i8259.icwstep[pic]; //no ICW4 expected, so don't read ICW4
			}
			return 1;
		}
		//OCW1!
		//if we get to this point, this is just a new IMR value
		i8259.imr[pic] = value;
		break;
	default:
		break;
	}
	return 1; //We're processed!
}

byte interruptsaved = 0; //Have we gotten a primary interrupt (first PIC)?
byte lastinterrupt = 0; //Last interrupt requested!

byte startSlaveMode(byte PIC, byte IR) //For master only! Set slave mode masterIR processing on INTA?
{
	return (i8259.icw[pic][2]&(1<<IR)) && (i8259.icw[pic][3]&4); //Process slaves and set IR on the slave UD instead of 0?
}

byte respondMasterSlave(byte PIC, byte masterIR) //Process this PIC as a slave for a set slave IR?
{
	return (((i8259.icw[pic][2]&3)==masterIR) && ((i8259.icw[pic][3]&4)==0)) || ((masterIR==0xFF) && (i8259.icw[pic][3]&4) && (!PIC)); //Process this masterIR as a slave or Master in Master mode connected to INTRQ?
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

void acnowledgeirrs()
{
	byte nonedirty; //None are dirtied?
	byte recheck;
	recheck = 0;
	performRecheck:
	if (getunprocessedinterrupt(1) && (recheck==0)) //Slave connected to master?
	{
		raiseirq(0x402); //Slave raises INTRQ!
	}

	if (likely(irr3_dirty == 0)) return; //Nothing to do?
	nonedirty = 1; //None are dirty!
	//Move IRR3 to IRR and acnowledge!
	byte IRQ, source, PIC, IR;
	for (PIC=0;PIC<2;++PIC)
		for (IR=0;IR<8;++IR)
		{
			IRQ = (PIC << 3) | IR; //The IRQ we're accepting!
			if (((i8259.irr[PIC]&(1<<IR))==0) || (is_XT==0)) //Nothing acnowledged yet?
			{
				for (source = 0;source < 0x10;++source) //Verify if anything is left!
				{
					if (((i8259.irr3_a[PIC][source]&(1 << IR))==0) && (i8259.irr3[PIC][source] & (1 << IR))) //Not acnowledged yet and high?
					{
						if (i8259.acceptirq[IRQ][source]) //Gotten a handler?
						{
							i8259.acceptirq[IRQ][source](IRQ|(source<<4)); //We're accepting the IRQ from this source!
						}
						i8259.irr3_a[PIC][source] |= (1 << IR); //Add the IRQ to request because of the rise!
						i8259.irr3_b[PIC][source] |= (1 << IR); //Second line for handling the interrupt itself!
						i8259.irr[PIC] |= (1 << IR); //Add the IRQ to request because of the rise!
						nonedirty = 0; //Acnowledged one!
					}
				}
			}
		}
	if (getunprocessedinterrupt(1) && (recheck==0)) //Slave connected to master?
	{
		raiseirq(0x402); //Slave raises INTRQ!
		recheck = 1; //Check again!
		goto performRecheck; //Check again!
	}

	if (nonedirty) //None are dirty anymore?
	{
		irr3_dirty = 0; //Not dirty anymore!
	}
}

byte PICInterrupt() //We have an interrupt ready to process? This is the primary PIC's INTA!
{
	if (__HW_DISABLED) return 0; //Abort!
	if (getunprocessedinterrupt(0)) //Primary PIC interrupt?
	{
		return 1;
	}
	//Slave PICs are handled when encountered from the Master PIC!
	return 0; //No interrupt to process!
}

OPTINLINE byte IRRequested(byte PIC, byte IR, byte source) //We have this requested?
{
	if (__HW_DISABLED) return 0; //Abort!
	return (((getunprocessedinterrupt(PIC) & (i8259.irr3_b[PIC&1][source]))>> IR) & 1); //Interrupt requested on the specified source?
}

OPTINLINE void ACNIR(byte PIC, byte IR, byte source) //Acnowledge request!
{
	if (__HW_DISABLED) return; //Abort!
	i8259.irr3[PIC][source] &= ~(1 << IR); //Turn source IRR off!
	i8259.irr3_a[PIC][source] &= ~(1 << IR); //Turn source IRR off!
	i8259.irr3_b[PIC][source] &= ~(1 << IR); //Turn source IRR off!
	irr3_dirty = 1; //Dirty!
	i8259.irr[PIC] &= ~(1<<IR); //Clear the request!
	//Clearing IRR only for edge-triggered interrupts!
	i8259.isr[PIC] |= (1 << IR); //Turn in-service on!
	i8259.isr2[PIC][source] |= (1 << IR); //Turn the source on!
	if ((i8259.icw[PIC][3]&2)==2) //Automatic EOI?
	{
		EOI(PIC,source); //Send an EOI!
	}
	if (PIC) //Slave connected to Master?
	{
		lowerirq(0x402); //INTA lowers INTRQ!
	}
}

OPTINLINE byte getint(byte PIC, byte IR) //Get interrupt!
{
	if (__HW_DISABLED) return 0; //Abort!
	byte realir = IR; //Default: nothing changed!
	return ((i8259.icw[PIC][1]&0xF8)|(realir&0x7)); //Get interrupt!
}

byte readPollingMode(byte pic)
{
	if (getunprocessedinterrupt(pic)) //Interrupt requested?
	{
		if (__HW_DISABLED) return 0; //Abort!
		//First, process the PIC!
		for (IR=0;IR<8;++IR)
		{
			byte realIR = (IR & 7); //What IR within the PIC?
			byte srcIndex;
			for (srcIndex = 0; srcIndex < 0x10; ++srcIndex) //Check all indexes!
			{
				if (IRRequested(pic, realIR, srcIndex)) //Requested?
				{
					ACNIR(PICnr, realIR, srcIndex); //Acnowledge it!
					lastinterrupt = getint(PICnr, realIR); //Give the interrupt number!
					i8259.lastinterruptIR[PICnr] = realIR; //Last acnowledged interrupt line!
					interruptsaved = 1; //Gotten an interrupt saved!
					//Don't perform layering to any slave!
					return 0x80 | realIR; //Give the raw IRQ number on the PIC!
				}
			}
		}

		i8259.lastinterruptIR[i8259.activePIC] = 7; //Last acnowledged interrupt line!
		lastinterrupt = getint(i8259.activePIC, 7); //Unknown, dispatch through IR7 of the used PIC!
		interruptsaved = 1; //Gotten!
		return i8259.lastinterruptIR[i8259.activePIC]; //No result: unk interrupt!
	}
	return 0x00; //No interrupt available!
}

byte nextintr()
{
	if (__HW_DISABLED) return 0; //Abort!
	byte i;
	byte PICnr;
	byte masterIR;
	PICnr = 0; //First PIC is connected to CPU INTA!
	masterIR = 0xFF; //Default: Unknown(=master device only) IR!
	checkSlave:
	//First, process first PIC!
	for (IR=0; IR<8; i++) //Process all IRs for this chip!
	{
		byte realIR = (IR&7); //What IR within the PIC?
		byte srcIndex;
		for (srcIndex=0;srcIndex<0x10;++srcIndex) //Check all indexes!
		{
			if (IRRequested(PICnr,realIR,srcIndex)) //Requested?
			{
				if (respondMasterSlave(PICnr,masterIR)) //PIC responds as a master or slave?
				{
					ACNIR(PICnr, realIR,srcIndex); //Acnowledge it!
					if (startSlaveMode(PICnr,realIR)) //Start slave processing for this? Let the slave give the IRQ!
					{
						masterIR = IR; //Slave starts handling this IRQ!
						PICnr ^= 1; //What other PIC to check on the PIC bus?
						goto checkSlave; //Check the slave instead!
					}

					lastinterrupt = getint(PICnr, realIR); //Give the interrupt number!
					interruptsaved = 1; //Gotten an interrupt saved!
					i8259.lastinterruptIR[PICnr] = realIR; //Last IR!
					return lastinterrupt;
				}
			}
		}
	}

	i8259.lastinterruptIR[i8259.activePIC] = 7; //Last IR!
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
	irr2index = requestingindex; //What is requested?
	//Handle edge-triggered IRR!
	hasirr = 0; //Init IRR state!
	//for (irr2index = 0;irr2index < 0x10;++irr2index) //Verify if anything is left!
	{
		if (i8259.irr2[PIC][irr2index] & (1 << (irqnum & 7))) //Request still set?
		{
			hasirr = 1; //We still have an IRR!
			//break; //Stop searching!
		}
	}
	oldIRR = hasirr; //Old IRR state!

	i8259.irr2[PIC][requestingindex] |= (1 << (irqnum & 7)); //Add the IRQ to request!
	//hasirr = 0; //Init IRR state!
	//for (irr2index = 0;irr2index < 0x10;++irr2index) //Verify if anything is left!
	{
		//if (i8259.irr2[PIC][irr2index] & (1 << (irqnum & 7))) //Request still set?
		{
			hasirr = 1; //We still have an IRR!
			//break; //Stop searching!
		}
	}

	if (hasirr && ((hasirr^oldIRR)&1)) //The line is actually raised?
	{
		i8259.irr3[PIC][requestingindex] |= (1 << (irqnum & 7)); //Add the IRQ to request because of the rise! This causes us to be the reason during shared IR lines!
		irr3_dirty = 1; //Dirty!
	}
}

void lowerirq(byte irqnum)
{
	if (__HW_DISABLED) return; //Abort!
	byte requestingindex = irqnum; //Save our index that's requesting!
	irqnum &= 0xF; //Only 16 IRQs!
	requestingindex >>= 4; //What index is requesting?
	byte PIC = (irqnum>>3); //IRQ8+ is high PIC!
	byte lowerirr, lowerirr2;
	if (i8259.irr2[PIC][requestingindex]&(1 << (irqnum & 7))) //Were we raised?
	{
		i8259.irr2[PIC][requestingindex] &= ~(1 << (irqnum & 7)); //Lower the IRQ line to request!
		lowerirr = i8259.irr3[PIC][requestingindex]; //What has been lowered!
		lowerirr2 = i8259.irr3_a[PIC][requestingindex]; //What has been lowered!
		i8259.irr3[PIC][requestingindex] &= ~(1 << (irqnum & 7)); //Remove the request being used itself!
		i8259.irr3_a[PIC][requestingindex] &= ~(1 << (irqnum & 7)); //Remove the acnowledge!
		i8259.irr3_b[PIC][requestingindex] &= ~(1 << (irqnum & 7)); //Remove the acnowledge!
		irr3_dirty = 1; //Dirty!
		if ((lowerirr&lowerirr2)&(1 << (irqnum & 7))) //Were we acnowledged and loaded?
		{
			i8259.irr[PIC] &= ~(1<<(irqnum&7)); //Remove the request, if any! New requests can be loaded!
		} //Is this only for level triggered interrupts?
	}
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
