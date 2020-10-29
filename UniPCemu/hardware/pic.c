/*

Copyright (C) 2019 - 2020  Superfury

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
#include "headers/mmu/mmuhandler.h" //Basic MMU handler support!
#include "headers/cpu/cpu.h" //Emulated CPU support!

//PIC Info: http://www.brokenthorn.com/Resources/OSDevPic.html

//Are we disabled?
#define __HW_DISABLED 0

PIC i8259;
byte irr3_dirty = 0; //IRR3/IRR3_a is changed?

struct
{
	//Basic information?
	byte enabled; //Is the APIC enabled by the CPU?
	byte needstermination; //APIC needs termination?
	//CPU MSR information!
	uint_32 windowMSRlo;
	uint_32 windowMSRhi; //Window register that's written in the CPU!
	//Runtime information!
	uint_64 baseaddr; //Base address of the APIC!
	uint_64 IObaseaddr; //Base address of the I/O APIC!
	//Remaining variables? All memory that's stored in the APIC!
	uint_32 APIC_address; //Address register for the extended memory!
	uint_32 APIC_data; //Data register for the extended memory!

	//Differential detection
	uint_32 prevSpuriousInterruptVectorRegister; //The previous value before the write!
	uint_64 LAPIC_timerremainder; //How much time remained?
	byte LAPIC_timerdivider; //The divider of the timer!

	//IRQ detection
	uint_32 IOAPIC_liveIRR; //Live IRR status!
	uint_32 IOAPIC_IRRreq; //Is the IRR requested, but masked(1-bit values)
	uint_32 IOAPIC_IRRset; //Is the IRR set(1-bit values)
	uint_32 IOAPIC_IMRset; //Is the IMR routine set(1-bit values)

	//Now, the actual memory for the LAPIC!
	byte LAPIC_requirestermination[0x400]; //Dword requires termination?
	byte LAPIC_globalrequirestermination; //Is termination required at all for the Local APIC?
	uint_32 LAPIC_arbitrationIDregister; //Arbitration ID, set at INIT deassert and RESET!
	uint_32 LAPIC_ID; //0020
	uint_32 LAPIC_version; //0030
	uint_32 TaskPriorityRegister; //0080
	uint_32 ArbitrationPriorityRegister; //0090
	uint_32 ProcessorPriorityRegister; //00A0
	uint_32 EOIregister; //00B0
	uint_32 RemoteReadRegister; //00C0
	uint_32 LogicalDestinationRegister; //00D0
	uint_32 DestinationFormatRegister; //00E0
	uint_32 SpuriousInterruptVectorRegister; //00F0
	uint_32 ISR[8]; //ISRs! 0100-0170
	uint_32 TMR[8]; //TMRs! 0180-01F0
	uint_32 IRR[8]; //IRRs! 0200-0270
	uint_32 ErrorStatusRegister; //0280
	uint_32 LVTCorrectedMachineCheckInterruptRegister; //02F0
	uint_32 InterruptCommandRegisterLo; //0300
	uint_32 InterruptCommandRegisterHi; //0310
	uint_32 LVTTimerRegister; //0320
	uint_32 LVTThermalSensorRegister; //0330
	uint_32 LVTPerformanceMonitoringCounterRegister; //0340
	uint_32 LVTLINT0Register; //0350. Connected to PIC master.
	uint_32 LVTLINT1Register; //0560. Connectd to NMI pin.
	uint_32 LVTErrorRegister; //0370
	uint_32 InitialCountRegister; //0380
	uint_32 CurrentCountRegister; //0390
	uint_32 DivideConfigurationRegister; //03E0

	//IO APIC address registers!
	uint_32 IOAPIC_Address; //Address register for the IOAPIC registers! 0000
	uint_32 IOAPIC_Value; //Value register for the IOAPIC registers! 0010

	//Now, the IO APIC registers
	uint_32 IOAPIC_ID; //00: ID
	uint_32 IOAPIC_version_numredirectionentries; //01: Version(bits 0-7), # of redirection entries(16-23).
	uint_32 IOAPIC_arbitrationpriority; //02: Arbitration priority(Bits 24-27), remainder is reserved.
	uint_32 IOAPIC_redirectionentry[24][2]; //10-3F: 2 dwords for each redirection entry setting! Total 48 dwords!
	byte IOAPIC_requirestermination[0x40]; //Termination required for this entry?
	byte IOAPIC_globalrequirestermination; //Is termination required for the IO APIC?
} APIC; //The APIC that's emulated!

byte addr22 = 0; //Address select of port 22h!
byte IMCR = 0; //Address selected. 00h=Connect INTR and NMI to the CPU. 01h=Disconnect INTR and NMI from the CPU.
extern byte NMIQueued; //NMI raised to handle? This can be handled by an Local APIC! This then clears said flag to acnowledge it!
extern byte APICNMIQueued; //APIC-issued NMI queued?

//i8259.irr is the complete status of all 8 interrupt lines at the moment. Any software having raised it's line, raises this. Otherwise, it's lowered(irr3 are all cleared)!
//i8259.irr2 is the live status of each of the parallel interrupt lines!
//i8259.irr3 is the identifier for request subchannels that are pending to be acnowledged(cleared when acnowledge and the interrupt is fired).

void updateLAPICTimerSpeed()
{
	byte divider;
	divider = (APIC.DivideConfigurationRegister & 3) | ((APIC.DivideConfigurationRegister & 8) >> 1); //Divider set!
	if (divider == 7) //Actually 1?
	{
		APIC.LAPIC_timerdivider = 0; //Divide by 1!
	}
	else //2^n
	{
		APIC.LAPIC_timerdivider = 1+divider; //Calculate it!
	}
}

//Handle everything that needs to be done when resetting the APIC!
void resetAPIC()
{
	byte IRQnr;
	for (IRQnr = 0; IRQnr < NUMITEMS(APIC.IOAPIC_redirectionentry); ++IRQnr) //Handle all IRQ handlers we support!
	{
		APIC.IOAPIC_redirectionentry[IRQnr][0] |= 0x10000; //Masked, nothing else set yet, edge mode, active high!
	}
	APIC.IOAPIC_IMRset = ~0; //Mask all set!
	APIC.IOAPIC_IRRreq = 0; //Remove all pending requests!
}

void updateLAPICArbitrationIDregister()
{
	APIC.LAPIC_arbitrationIDregister = APIC.LAPIC_ID & (0xFF << 24); //Load the Arbitration ID register from the Local APIC ID register! All 8-bits are loaded!
}

void init8259()
{
	if (__HW_DISABLED) return; //Abort!
	memset(&i8259, 0, sizeof(i8259));
	memset(&APIC, 0, sizeof(APIC));
	//Now the port handling!
	//PIC0!
	register_PORTOUT(&out8259);
	register_PORTIN(&in8259);
	//All set up!

	i8259.imr[0] = 0xFF; //Mask off all interrupts to start!
	i8259.imr[1] = 0xFF; //Mask off all interrupts to start!
	irr3_dirty = 0; //Default: not dirty!
	APIC.baseaddr = 0xFEE00000; //Default base address!
	APIC.IObaseaddr = 0xFEC00000; //Default base address!
	APIC.enabled = 0; //Is the APIC enabled?
	APIC.needstermination = 0; //Doesn't need termination!
	APIC.IOAPIC_version_numredirectionentries = 0x11 | ((24 - 1) << 16); //How many IRQs can we handle(24) and version number!
	APIC.LAPIC_version = 0x0010;
	APIC.DestinationFormatRegister = (0x11 << 24); //Cluster 0 APIC 0!
	APIC.IOAPIC_ID = 0x00; //Default IO APIC phyiscal ID!
	switch (EMULATED_CPU)
	{
	case CPU_PENTIUM:
		APIC.LAPIC_version |= 0x30000; //4 LVT entries
		break;
	case CPU_PENTIUMPRO:
	case CPU_PENTIUM2:
		APIC.LAPIC_version |= 0x40000; //5 LVT entries
		break;
	default:
		break;
	}
	resetAPIC(); //Reset the APIC as well!
	addr22 = IMCR = 0x00; //Default values after powerup for the IMCR and related register!
	updateLAPICTimerSpeed(); //Update the used timer speed!
	updateLAPICArbitrationIDregister(); //Update the Arbitration ID register with it's defaults!

	APIC.LVTLINT0Register = 0x10000; //Reset LINT0 register!
	APIC.LVTLINT1Register = 0x10000; //Reset LINT1 register!
}

void APIC_errorTrigger(); //Error has been triggered! Prototype!

void APIC_handletermination() //Handle termination on the APIC!
{
	word MSb, MSBleft;
	//Handle any writes to APIC addresses!
	if (likely((APIC.needstermination|APIC.IOAPIC_globalrequirestermination|APIC.LAPIC_globalrequirestermination) == 0)) return; //No termination needed?

	//Now, handle the termination of the various registers!
	if (APIC.needstermination & 1) //Needs termination due to possible reset?
	{
		if (((APIC.SpuriousInterruptVectorRegister & 0x100) == 0) && ((APIC.prevSpuriousInterruptVectorRegister & 0x100))) //Cleared?
		{
			resetAPIC(); //Reset the APIC!
		}
		else if (((APIC.prevSpuriousInterruptVectorRegister & 0x100) == 0) && ((APIC.SpuriousInterruptVectorRegister & 0x100))) //Set?
		{
			//APIC.IOAPIC_IRRreq = 0; //Remove all pending!
		}
	}

	if (APIC.needstermination & 2) //Needs termination due to possible EOI?
	{
		//if (APIC.EOIregister == 0) //Properly written 0?
		{
			if (APIC.ISR[0]|APIC.ISR[1]|APIC.ISR[2]|APIC.ISR[3]|APIC.ISR[4]|APIC.ISR[5]|APIC.ISR[6]|APIC.ISR[7]) //Anything set to acnowledge?
			{
				MSBleft = 256; //How many are left!
				for (MSb = 255; MSBleft; --MSBleft) //Check all possible interrupts!
				{
					if (APIC.ISR[MSb >> 5] & (1 << (MSb&0x1F))) //Highest IRQ found (MSb)?
					{
						APIC.ISR[MSb >> 5] &= ~(1 << (MSb & 0x1F)); //Clear said ISR!
						goto finishupEOI; //Only acnlowledge the MSb IRQ!
					}
					--MSb;
				}

			finishupEOI:
				MSBleft = 24; //How many are left!
				for (MSb = 23; MSBleft; --MSBleft)
				{
					APIC.IOAPIC_redirectionentry[MSb][0] &= ~(1 << 14); //EOI has been received!
					--MSb; //Next MSb to process!
				}
			}
		}
	}

	if (APIC.needstermination & 4) //Needs termination due to sending a command?
	{
		APIC.InterruptCommandRegisterLo |= (1 << 12); //Start to become pending!
	}

	if (APIC.needstermination & 8) //Error status register needs termination?
	{
		APIC.ErrorStatusRegister = 0; //Clear the status register for new errors to be reported!
	}

	if (APIC.needstermination & 0x10) //Initial count register is written?
	{
		if (APIC.InitialCountRegister == 0) //Stop the timer?
		{
			APIC.CurrentCountRegister = 0; //Stop the timer!
		}
		else //Timer started?
		{
			APIC.CurrentCountRegister = APIC.InitialCountRegister; //Load the current count and start timing!
		}
	}

	if (APIC.needstermination & 0x20) //Divide configuationregister is written?
	{
		updateLAPICTimerSpeed(); //Update the timer speed!
	}

	if (APIC.needstermination & 0x40) //Error Status Interrupt is written?
	{
		if (APIC.ErrorStatusRegister && ((APIC.LVTErrorRegister&0x10000)==0)) //Error marked and interrupt enabled?
		{
			APIC_errorTrigger(); //Error interrupt is triggered!
		}
	}

	APIC.needstermination = 0; //No termination is needed anymore!
	APIC.IOAPIC_globalrequirestermination = 0; //No termination is needed anymore!
	APIC.LAPIC_globalrequirestermination = 0; //No termination is needed anymore!
}

OPTINLINE byte getint(byte PIC, byte IR) //Get interrupt!
{
	if (__HW_DISABLED) return 0; //Abort!
	byte realir = IR; //Default: nothing changed!
	return ((i8259.icw[PIC][1] & 0xF8) | (realir & 0x7)); //Get interrupt!
}

byte isLAPIClogicaldestination(byte logicaldestination)
{
	byte ourid;
	switch (APIC.DestinationFormatRegister >> 28 & 0xFF) //What destination mode?
	{
	case 0: //Cluster model?
		//high 4 bits are encoded address of destination cluster
		//low 4 bits are the 4 APICs within the cluster.
		//the matching is done like with flat model, but on both the destination cluster and APIC number!
		ourid = ((APIC.DestinationFormatRegister >> 24) & (logicaldestination << 24)); //Simply logical AND on both the destination cluster and selected APIC!
		return (ourid != 0); //Received?
		break;
	case 0xF: //Flat model?
		ourid = ((APIC.DestinationFormatRegister >> 24) & (logicaldestination << 24)); //Simply logical AND!
		return (ourid != 0); //Received?
		break;
	default: //Unknown model?
		break;
	}
	return 0; //Default: not the selected destination!
}

//isLAPICorIOAPIC=0: LAPIC, 1=APIC! result: 0=No match. 1=Local APIC, 2=IO APIC.
byte isAPICPhysicaldestination(byte isLAPICorIOAPIC, byte physicaldestination)
{
	switch (isLAPICorIOAPIC) //Which chip is addressed?
	{
	case 0: //LAPIC?
		if (physicaldestination == 0xF) //Broadcast?
		{
			return 1; //Match!
		}
		else if (physicaldestination == ((APIC.LAPIC_ID >> 24) & 0xF)) //Match?
		{
			return 1;
		}
		else //No match!
		{
			return 0; //Not matched!
		}
		break;
	case 1: //IO APIC?
		if (physicaldestination == 0xF) //Broadcast?
		{
			return 2; //Match!
		}
		else if (physicaldestination == ((APIC.IOAPIC_ID >> 24) & 0xF)) //Match?
		{
			return 2; //Match!
		}
		else //No match!
		{
			return 0; //Not matched!
		}
		break;
	default: //Unknown?
		break;
	}
	return 0; //No match!
}

byte i8259_INTA(); //Prototype for the vector execution of the LAPIC for ExtINT modes!

//Execute a requested vector on the Local APIC! Specify IR=0xFF for no actual IR!
void LAPIC_executeVector(uint_32* vectorlo, byte IR)
{
	byte APIC_intnr;
	*vectorlo |= (1 << 14); //The IO or Local APIC has received the request!
	APIC_intnr = (*vectorlo & 0xFF); //What interrupt number?
	switch ((APIC.IOAPIC_redirectionentry[IR][0] >> 8) & 7) //What destination mode?
	{
	case 0: //Interrupt?
	case 1: //Lowest priority?
	//Now, we have selected the highest priority IR! Start using it!
		if (APIC_intnr < 0x10) //Invalid?
		{
			APIC.ErrorStatusRegister |= (1 << 6); //Report an illegal vector being received!
			APIC_errorTrigger(); //Error has been triggered!
			return; //Abort!
		}
		APIC.IRR[APIC_intnr >> 5] |= (1 << (APIC_intnr & 0x1F)); //Mark the interrupt requested to fire!
		//The IO APIC ignores the received message?
		break;
	case 2: //SMI?
		//Not implemented yet!
		//Can't be masked, bypasses IRR/ISR!
		break;
	case 4: //NMI?
		APICNMIQueued = 1; //APIC-issued NMI queued!
		//Can't be masked, bypasses IRR/ISR!
		break;
	case 5: //INIT or INIT deassert?
		resetCPU(0x80); //Special reset of the CPU: INIT only!
		break;
	case 7: //extINT?
		APIC_intnr = i8259_INTA(); //Perform an INTA-style interrupt retrieval!
		APIC.IRR[APIC_intnr >> 5] |= (1 << (APIC_intnr & 0x1F)); //Mark the interrupt requested to fire!
		break;
	default: //Unsupported yet?
		break;
	}
}

void updateAPIC(uint_64 clockspassed)
{
	uint_64 remainingclocks;
	if (!clockspassed) return; //Nothing passed?
	if (APIC.CurrentCountRegister) //Count busy?
	{
		//First, divide up!
		APIC.LAPIC_timerremainder += clockspassed; //How much more is passed!
		if (APIC.LAPIC_timerremainder >> APIC.LAPIC_timerdivider) //Something passed?
		{
			clockspassed = (APIC.LAPIC_timerremainder >> APIC.LAPIC_timerdivider); //How much passed!
			APIC.LAPIC_timerremainder -= clockspassed << APIC.LAPIC_timerdivider; //How much time is left!
		}
		else
		{
			return; //Nothing is ticked! So, abort!
		}
		//Now, the clocks

		if (APIC.CurrentCountRegister > clockspassed) //Still timing more than what's needed?
		{
			APIC.CurrentCountRegister -= clockspassed; //Time some clocks!
		}
		else //Finished counting?
		{
			clockspassed -= APIC.CurrentCountRegister; //Time until 0!

			for (; clockspassed >= APIC.InitialCountRegister;) //Multiple blocks?
			{
				clockspassed -= APIC.InitialCountRegister; //What is the remaining time?
			}
			APIC.CurrentCountRegister = clockspassed; //How many clocks are left!
			if (!(APIC.LVTTimerRegister & 0x20000)) //One-shot mode?
			{
				APIC.CurrentCountRegister = 0; //Stop(ped) counting!
			}
			else if (APIC.CurrentCountRegister == 0) //Needs to load a new value, otherwise already set! Otherwise, still counting on!
			{
				APIC.CurrentCountRegister = APIC.InitialCountRegister; //Reload the initial count!
			}

			if (APIC.LVTTimerRegister & 0x10000) //Not masked?
			{
				if ((APIC.LVTTimerRegister & (1 << 14)) == 0) //The IO or Local APIC can receive the request!
				{
					APIC.LVTTimerRegister |= (1 << 14); //Start pending!
				}
			}
		}
	}
}

void APIC_errorTrigger() //Error has been triggered!
{
	if ((APIC.LVTErrorRegister & 0x10000)==0) //Not masked?
	{
		if ((APIC.LVTErrorRegister & (1 << 14)) == 0) //The IO or Local APIC can receive the request!
		{
			APIC.LVTErrorRegister |= (1 << 14); //Start pending!
		}
	}
}

void IOAPIC_pollRequests()
{
	byte receiver;
	byte isLAPIC;
	byte logicaldestination;
	byte IR;
	byte APIC_intnr;
	int APIC_highestpriority; //-1=Nothing yet, otherwise, highest priority level detected
	byte APIC_highestpriorityIR; //Highest priority IR detected!
	uint_32 APIC_IRQsrequested, APIC_requestbit, APIC_requestsleft, APIC_requestbithighestpriority;
	APIC_IRQsrequested = APIC.IOAPIC_IRRset & (~APIC.IOAPIC_IMRset); //What can we handle!
	receiver = 0; //Initialize receivers of the packet!
	if (APIC.InterruptCommandRegisterLo & 0x1000) //Pending command being sent?
	{
		switch ((APIC.InterruptCommandRegisterLo >> 18) & 3) //What destination type?
		{
		case 0: //Destination field?
			if (APIC.InterruptCommandRegisterLo & 0x800) //Logical destination?
			{
				logicaldestination = ((APIC.InterruptCommandRegisterHi >> 24) & 0xFF); //What is the logical destination?
				if (isLAPIClogicaldestination(logicaldestination)) //Match on the logical destination?
				{
					receiver |= 1; //Received on LAPIC!
				}
			}
			else //Physical destination?
			{
				if (isAPICPhysicaldestination(0, ((APIC.InterruptCommandRegisterHi >> 24) & 0xF)) == 1) //Local APIC?
				{
					receiver |= 1; //Receive it on LAPIC!
				}
				if (isAPICPhysicaldestination(1, ((APIC.InterruptCommandRegisterHi >> 24) & 0xF)) == 2) //IO APIC?
				{
					receiver |= 2; //Received on the IO APIC!
				}
			}
			if (receiver & 1) //Received on the Local APIC?
			{
				goto receiveCommandRegister; //Receive it!
			}
			else if ((receiver & ~3) == 0) //No receiver?
			{
				APIC.ErrorStatusRegister |= (1 << 2); //Report an send accept error! Nothing responded on the bus!
				APIC_errorTrigger(); //Error has been triggered!
			}
			//Discard it!
			APIC.InterruptCommandRegisterLo &= ~0x1000; //We're receiving it somewhere!
			break;
		case 1: //To itself?
			receiver = 1; //Self received!
			goto receiveCommandRegister; //Receive it!
			break;
		case 2: //All processors?
			//Receive it!
			//Handle the request!
			receiver = 3; //All received!
		receiveCommandRegister:
			APIC.InterruptCommandRegisterLo &= ~0x1000; //We're receiving it somewhere!
			if (receiver & 1) //Received on LAPIC?
			{
				switch ((APIC.InterruptCommandRegisterLo >> 8) & 7) //What is requested?
				{
				case 0: //Interrupt raise?
				case 1: //Lowest priority?
					if ((APIC.InterruptCommandRegisterLo & 0xFF) < 0x10) //Invalid vector?
					{
						APIC.ErrorStatusRegister |= (1 << 5); //Report an illegal vector being sent!
						APIC_errorTrigger(); //Error has been triggered!
					}
					else if ((APIC.IRR[(APIC.InterruptCommandRegisterLo & 0xFF) >> 5] & (1 << ((APIC.InterruptCommandRegisterLo & 0xFF) & 0x1F))) == 0) //Ready to receive?
					{
						APIC.IRR[(APIC.InterruptCommandRegisterLo & 0xFF) >> 5] |= (1 << ((APIC.InterruptCommandRegisterLo & 0xFF) & 0x1F)); //Raise the interrupt on the Local APIC!
					}
					break;
				case 2: //SMI raised?
					break;
				case 4: //NMI raised?
					APICNMIQueued = 1; //Queue the APIC NMI!
					break;
				case 5: //INIT or INIT deassert?
					if (((APIC.InterruptCommandRegisterLo >> 14) & 3) == 2) //De-assert?
					{
						//Setup Arbitration ID registers on all APICs!
						//Operation on Pentium and P6: Arbitration ID register = APIC ID register.
						updateLAPICArbitrationIDregister(); //Update the register!
					}
					else //INIT?
					{
						resetCPU(0x80); //Special reset of the CPU: INIT only!
					}
					break;
				case 6: //SIPI?
					if ((APIC.InterruptCommandRegisterLo & 0xFF) < 0x10) //Invalid vector?
					{
						APIC.ErrorStatusRegister |= (1 << 5); //Report an illegal vector being sent!
						APIC_errorTrigger(); //Error has been triggered!
					}
					else //Valid vector!
					{
						CPU[activeCPU].SIPIreceived = 100 | (APIC.InterruptCommandRegisterLo & 0xFF); //We've received a SIPI!
					}
					break;
				default: //Unknown?
					//Don't handle it!
					break;
				}
			}
			break;
		case 3: //All but ourselves?
			//Don't handle the request!
			APIC.InterruptCommandRegisterLo &= ~0x1000; //We're receiving it somewhere!
			//Send no error because there are no other APICs to receive it! Only the IO APIC receives it, which isn't using it!
			break;
		}
	}
	if (NMIQueued) //NMI has been queued?
	{
		if ((APIC.LVTLINT1Register & (1 << 12)) == 0) //Not waiting to be delivered!
		{
			if ((APIC.LVTLINT1Register & 0x10000) == 0) //Not masked?
			{
				NMIQueued = 0; //Not queued anymore!
				APIC.LVTLINT1Register |= (1 << 12); //Start pending!
				//Edge: raised when set(done here already). Lowered has weird effects for level-sensitive modes? So ignore them!
			}
		}
	}

	if (likely(APIC_IRQsrequested == 0)) return; //Nothing to do?
//First, determine the highest priority IR to use!
	APIC_requestbit = 1; //What bit is requested first!
	APIC_requestsleft = 24; //How many are left!
	APIC_requestbithighestpriority = 0; //Default: no highest priority found yet!
	APIC_highestpriority = -1; //Default: no highest priority level found yet!
	APIC_highestpriorityIR = 0; //Default: No highest priority IR loaded yet!
	//Note: this way of handling the priority is done by the LAPIC as well(high nibble of the interrupt vector determines the priority)!
	for (IR = 0; APIC_requestsleft; ++IR) //Check all requests!
	{
		if (APIC_IRQsrequested & APIC_requestbit) //Are we requested to fire?
		{
			//Priority is based on the high nibble of the interrupt vector. The low nibble is ignored!
			if ((int)(APIC.IOAPIC_redirectionentry[IR][0] & 0xF0U) >= APIC_highestpriority) //Higher priority found?
			{
				//Determinate the interrupt number for the priority!
				APIC_intnr = (APIC.IOAPIC_redirectionentry[IR][0] & 0xFF); //What interrupt number?
				switch ((APIC.IOAPIC_redirectionentry[IR][0] >> 8) & 7) //What destination mode?
				{
				case 0: //Interrupt?
				case 1: //Lowest priority?
					if ((APIC.IRR[APIC_intnr >> 5] & (1 << (APIC_intnr & 0x1F))) == 0) //Not requested yet? Able to accept said message!
					{
						APIC_highestpriority = (int)(APIC.IOAPIC_redirectionentry[IR][0] & 0xF0U); //New highest priority!
						APIC_highestpriorityIR = IR; //What IR has the highest priority now!
						APIC_requestbithighestpriority = APIC_requestbit; //What bit was the highest priority?
					}
					break;
				case 2: //SMI?
				case 4: //NMI?
				case 5: //INIT or INIT deassert?
				case 7: //extINT?
					APIC_highestpriority = (int)(APIC.IOAPIC_redirectionentry[IR][0] & 0xF0U); //New highest priority!
					APIC_highestpriorityIR = IR; //What IR has the highest priority now!
					APIC_requestbithighestpriority = APIC_requestbit; //What bit was the highest priority?
					break;
				}
			}
		}
		APIC_requestbit <<= 1; //Next bit to check!
		--APIC_requestsleft; //One processed!
	}
	if (APIC_requestbithighestpriority) //Found anything to handle?
	{
		isLAPIC = 0; //Default: not the LAPIC!
		if (APIC.IOAPIC_redirectionentry[IR][0] & 0x800) //Logical destination?
		{
			logicaldestination = ((APIC.IOAPIC_redirectionentry[IR][0] >> 24) & 0xFF); //What is the logical destination?
			//Determine destination correct by destination format and logical destination register in the LAPIC!
			if (isLAPIClogicaldestination(logicaldestination)) //Match on the logical destination?
			{
				isLAPIC |= 1; //LAPIC!
				goto receiveIOLAPICCommandRegister; //Receive it!
			}
			else //No receivers?
			{
				APIC.ErrorStatusRegister |= (1 << 3); //Report an receive accept error!
				APIC_errorTrigger(); //Error has been triggered!
			}
		}
		else //Physical destination?
		{
			logicaldestination = ((APIC.InterruptCommandRegisterHi >> 24) & 0xF); //What destination!
			if (isAPICPhysicaldestination(0, logicaldestination) == 1) //Local APIC?
			{
				isLAPIC |= 1; //LAPIC received!
			}
			if (isAPICPhysicaldestination(1, logicaldestination)==2) //IO APIC?
			{
				isLAPIC |= 2; //IO APIC received!
			}
			if (isLAPIC) //Received?
			{
				goto receiveIOLAPICCommandRegister; //Receive it!
			}
			else //No receivers?
			{
				APIC.ErrorStatusRegister |= (1 << 3); //Report an receive accept error!
				APIC_errorTrigger(); //Error has been triggered!
			}
		}
		return; //Abort: invalid destination!
	receiveIOLAPICCommandRegister:
		//Received something from the IO APIC redirection targetting the main CPU?
		APIC_requestbit = APIC_requestbithighestpriority; //Highest priority IR bit
		IR = APIC_highestpriorityIR; //The IR for the highest priority!
		APIC_IRQsrequested &= ~APIC_requestbit; //Clear the request bit!
		APIC.IOAPIC_IRRset &= ~APIC_requestbit; //Clear the request, because we're firing it up now!
		if (isLAPIC&1) //Local APIC received?
		{
			LAPIC_executeVector(&APIC.IOAPIC_redirectionentry[IR][0], IR); //Execute this vector!
		}
		else //No receivers?
		{
			APIC.ErrorStatusRegister |= (1 << 3); //Report an receive accept error!
			APIC_errorTrigger(); //Error has been triggered!
		}
	}
}

sword LAPIC_pollRequests()
{
	byte IRgroup;
	byte IR;
	byte APIC_intnr;
	int APIC_highestpriority; //-1=Nothing yet, otherwise, highest priority level detected
	byte APIC_highestpriorityIR; //Highest priority IR detected!
	uint_32 APIC_IRQsrequested[8], APIC_requestbit, APIC_requestsleft, APIC_requestbithighestpriority;
	APIC_IRQsrequested[0] = APIC.IRR[0] & (~APIC.ISR[0]); //What can we handle!
	APIC_IRQsrequested[1] = APIC.IRR[1] & (~APIC.ISR[1]); //What can we handle!
	APIC_IRQsrequested[2] = APIC.IRR[2] & (~APIC.ISR[2]); //What can we handle!
	APIC_IRQsrequested[3] = APIC.IRR[3] & (~APIC.ISR[3]); //What can we handle!
	APIC_IRQsrequested[4] = APIC.IRR[4] & (~APIC.ISR[4]); //What can we handle!
	APIC_IRQsrequested[5] = APIC.IRR[5] & (~APIC.ISR[5]); //What can we handle!
	APIC_IRQsrequested[6] = APIC.IRR[6] & (~APIC.ISR[6]); //What can we handle!
	APIC_IRQsrequested[7] = APIC.IRR[7] & (~APIC.ISR[7]); //What can we handle!
	if (APIC.LVTErrorRegister & (1 << 12)) //Timer is pending?
	{
		LAPIC_executeVector(&APIC.LVTErrorRegister, 0xFF); //Start the timer interrupt!
	}
	if (APIC.LVTTimerRegister & (1 << 12)) //Timer is pending?
	{
		LAPIC_executeVector(&APIC.LVTTimerRegister, 0xFF); //Start the timer interrupt!
	}
	if (APIC.LVTLINT0Register & (1 << 12)) //LINT0 is pending?
	{
		LAPIC_executeVector(&APIC.LVTLINT0Register, 0xFF); //Start the LINT0 interrupt!
	}
	if (APIC.LVTLINT1Register & (1 << 12)) //LINT1 is pending?
	{
		LAPIC_executeVector(&APIC.LVTLINT1Register, 0xFF); //Start the LINT0 interrupt!
	}
	if (!(APIC_IRQsrequested[0] | APIC_IRQsrequested[1] | APIC_IRQsrequested[2] | APIC_IRQsrequested[3] | APIC_IRQsrequested[4] | APIC_IRQsrequested[5] | APIC_IRQsrequested[6] | APIC_IRQsrequested[7]))
	{
		return -1; //Nothing to do!
	}
	//Find the most prioritized interrupt to fire!
	for (IRgroup = 7;; --IRgroup) //Process all possible groups to handle!
	{
		if (APIC_IRQsrequested[IRgroup]) //Something requested here?
		{
			//First, determine the highest priority IR to use!
			APIC_requestbit = (1U << 31); //What bit is requested first!
			APIC_requestsleft = 32; //How many are left!
			APIC_requestbithighestpriority = 0; //Default: no highest priority found yet!
			APIC_highestpriority = -1; //Default: no highest priority level found yet!
			APIC_highestpriorityIR = 0; //Default: No highest priority IR loaded yet!
			//Note: this way of handling the priority is done by the LAPIC as well(high nibble of the interrupt vector determines the priority)!
			for (IR = 31; APIC_requestsleft; --IR) //Check all requests!
			{
				if (APIC_IRQsrequested[IRgroup] & APIC_requestbit) //Are we requested to fire?
				{
					//Priority is based on the high nibble of the interrupt vector. The low nibble is ignored!
					APIC_highestpriorityIR = IR; //What IR has the highest priority now!
					APIC_requestbithighestpriority = APIC_requestbit; //What bit was the highest priority?
					goto firePrioritizedIR; //handle it!
				}
				APIC_requestbit >>= 1; //Next bit to check!
				--APIC_requestsleft; //One processed!
			}
		}
	}

firePrioritizedIR: //Fire the IR that has the most priority!
//Now, we have selected the highest priority IR! Start using it!
	APIC_intnr = (IRgroup << 5) | IR; //The interrupt to fire!
	APIC.IRR[IRgroup] &= ~APIC_requestbit; //Mark the interrupt in-service!
	APIC.ISR[IRgroup] |= APIC_requestbit; //Mark the interrupt in-service!
	return (sword)APIC_intnr; //Give the interrupt number to fire!
}

uint_32 i440fx_ioapic_base_mask;
uint_32 i440fx_ioapic_base_match;

extern byte memory_datawrittensize; //How many bytes have been written to memory during a write!
extern uint_32 BIU_cachedmemoryaddr;
extern byte BIU_cachedmemorysize;
extern byte memory_datasize; //The size of the data that has been read!
byte APIC_memIO_wb(uint_32 offset, byte value)
{
	byte is_internalexternalAPIC;
	uint_32 tempoffset, storedvalue, ROMbits, address;
	uint_32* whatregister; //What register is addressed?
	byte updateredirection;
	updateredirection = 0; //Init!
	tempoffset = offset; //Backup!

	is_internalexternalAPIC = 0; //Default: no APIC chip!
	if (((offset & 0xFFFFFF000ULL) == APIC.baseaddr)) //LAPIC?
	{
		is_internalexternalAPIC |= 1; //LAPIC!
	}
	if ((((offset & 0xFFFFFF000ULL) == APIC.IObaseaddr))) //IO APIC?
	{
		is_internalexternalAPIC |= 2; //IO APIC!
	}
	else if (is_internalexternalAPIC==0) //Neither?
	{
		return 0; //Neither!
	}

	if (APIC.enabled == 0) return 0; //Not the APIC memory space enabled?
	address = (offset & 0xFFC); //What address is addressed?

	ROMbits = ~0; //All bits are ROM bits by default?

	if (((offset&i440fx_ioapic_base_mask)==i440fx_ioapic_base_match) && (is_internalexternalAPIC&2)) //I/O APIC?
	{
		switch (address&0x10) //What is addressed?
		{
		case 0x0000: //IOAPIC address?
			whatregister = &APIC.APIC_address; //Address register!
			ROMbits = ~0xFF; //Upper 24 bits are reserved!
			break;
		case 0x0010: //IOAPIC data?
			switch (APIC.APIC_address) //What address is selected?
			{
			case 0x00:
				whatregister = &APIC.IOAPIC_ID; //ID register!
				ROMbits = ~(0xFU<<24); //Bits 24-27 writable!
				break;
			case 0x01:
				whatregister = &APIC.IOAPIC_version_numredirectionentries; //Version/Number of direction entries!
				break;
			case 0x02:
				whatregister = &APIC.IOAPIC_arbitrationpriority; //Arbitration priority register!
				break;
			case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
			case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2E: case 0x2F:
			case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37: case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
				whatregister = &APIC.IOAPIC_redirectionentry[(APIC.APIC_address - 0x10) >> 1][(APIC.APIC_address - 0x10) & 1]; //Redirection entry addressed!
				if (((APIC.APIC_address - 0x10) & 1) != 0) //High dword?
				{
					ROMbits = 0xFFFFFFU;
				}
				else //Low DWord?
				{
					ROMbits = (1U << 12) | (1U << 14) | 0xFFFE0000U; //Fully writable, except bits 12, 14 and 17-55!
				}
				updateredirection = (((APIC.APIC_address - 0x10) & 1) == 0); //Update status when the first dword is updated!
				break;
			default: //Unmapped?
				if (is_internalexternalAPIC & 1) //LAPIC?
				{
					goto notIOAPICW;
				}
				else
				{
					return 0; //Unmapped!
				}
				break;
			}
			break;
		default: //Unmapped?
			if (is_internalexternalAPIC & 1) //LAPIC?
			{
				goto notIOAPICW;
			}
			else
			{
				return 0; //Unmapped!
			}
			break;
		}
	}
	else if (is_internalexternalAPIC&1) //LAPIC?
	{
		notIOAPICW:
		switch (address) //What is addressed?
		{
		case 0x0020:
			whatregister = &APIC.LAPIC_ID; //0020
			ROMbits = 0; //Fully writable!
			break;
		case 0x0030:
			whatregister = &APIC.LAPIC_version; //0030
			break;
		case 0x0080:
			whatregister = &APIC.TaskPriorityRegister; //0080
			ROMbits = 0; //Fully writable!
			break;
		case 0x0090:
			whatregister = &APIC.ArbitrationPriorityRegister; //0090
			break;
		case 0x00A0:
			whatregister = &APIC.ProcessorPriorityRegister; //00A0
			break;
		case 0x00B0:
			whatregister = &APIC.EOIregister; //00B0
			ROMbits = 0; //Fully writable!
			//Only writable with value 0! Otherwise, #GP(0) is encountered!
			break;
		case 0x00C0:
			whatregister = &APIC.RemoteReadRegister; //00C0
			break;
		case 0x00D0:
			whatregister = &APIC.LogicalDestinationRegister; //00D0
			ROMbits = 0; //Fully writable!
			break;
		case 0x00E0:
			whatregister = &APIC.DestinationFormatRegister; //00E0
			ROMbits = 0; //Fully writable!
			break;
		case 0x00F0:
			whatregister = &APIC.SpuriousInterruptVectorRegister; //00F0
			ROMbits = 0; //Fully writable!
			break;
		case 0x0100:
		case 0x0110:
		case 0x0120:
		case 0x0130:
		case 0x0140:
		case 0x0150:
		case 0x0160:
		case 0x0170:
			whatregister = &APIC.ISR[((address - 0x100) >> 4)]; //ISRs! 0100-0170
			break;
		case 0x0180:
		case 0x0190:
		case 0x01A0:
		case 0x01B0:
		case 0x01C0:
		case 0x01D0:
		case 0x01E0:
		case 0x01F0:
			whatregister = &APIC.TMR[((address - 0x180) >> 4)]; //TMRs! 0180-01F0
			break;
		case 0x0200:
		case 0x0210:
		case 0x0220:
		case 0x0230:
		case 0x0240:
		case 0x0250:
		case 0x0260:
		case 0x0270:
			whatregister = &APIC.IRR[((address - 0x200) >> 4)]; //ISRs! 0200-0270
			break;
		case 0x280:
			whatregister = &APIC.ErrorStatusRegister; //0280
			break;
		case 0x2F0:
			whatregister = &APIC.LVTCorrectedMachineCheckInterruptRegister; //02F0
			ROMbits = 0; //Fully writable!
			break;
		case 0x300:
			whatregister = &APIC.InterruptCommandRegisterLo; //0300
			ROMbits = (1<<12); //Fully writable! Pending to send isn't writable!
			break;
		case 0x310:
			whatregister = &APIC.InterruptCommandRegisterHi; //0310
			ROMbits = 0; //Fully writable!
			break;
		case 0x320:
			whatregister = &APIC.LVTTimerRegister; //0320
			ROMbits = 0; //Fully writable!
			break;
		case 0x330:
			whatregister = &APIC.LVTThermalSensorRegister; //0330
			ROMbits = 0; //Fully writable!
			break;
		case 0x340:
			whatregister = &APIC.LVTPerformanceMonitoringCounterRegister; //0340
			ROMbits = 0; //Fully writable!
			break;
		case 0x350:
			whatregister = &APIC.LVTLINT0Register; //0350
			ROMbits = 0; //Fully writable!
			break;
		case 0x360:
			whatregister = &APIC.LVTLINT1Register; //0560
			ROMbits = 0; //Fully writable!
			break;
		case 0x370:
			whatregister = &APIC.LVTErrorRegister; //0370
			ROMbits = 0; //Fully writable!
			break;
		case 0x380:
			whatregister = &APIC.InitialCountRegister; //0380
			ROMbits = 0; //Fully writable!
			break;
		case 0x390:
			whatregister = &APIC.CurrentCountRegister; //0390
			break;
		case 0x3E0:
			whatregister = &APIC.DivideConfigurationRegister; //03E0
			ROMbits = 0; //Fully writable!
			break;
		default: //Unmapped?
			return 0; //Unmapped!
			break;
		}
	}
	else
		return 0; //Abort!

	//Get stored value!
	storedvalue = *whatregister; //What value is read at said address?

	//Create the value with adjusted data for storing it back!
	storedvalue = (storedvalue & ((~(0xFF << ((offset & 3) << 3))) | ROMbits)) | ((value<<((offset&3)<<3)) & ((0xFF << ((offset & 3) << 3)) & ~ROMbits)); //Stored value without the ROM bits!

	//Store the value back to the register!
	*whatregister = storedvalue; //Store the new value inside the register, if allowed to be changed!

	if (is_internalexternalAPIC & 1) //LAPIC?
	{
		if (address == 0xF0) //Needs to handle resetting the APIC?
		{
			if ((APIC.needstermination & 1) == 0) //Not backed up yet?
			{
				APIC.prevSpuriousInterruptVectorRegister = APIC.SpuriousInterruptVectorRegister; //Backup the old value for change detection!
				APIC.needstermination |= 1; //We're in need of termination handling due to possible reset!
			}
		}
		else if (address == 0xB0) //Needs to handle EOI?
		{
			APIC.needstermination |= 2; //Handle an EOI?
		}
		else if (address == 0x300) //Needs to send a command?
		{
			APIC.InterruptCommandRegisterLo &= ~(1 << 12); //Not sent yet is kept cleared!
			APIC.needstermination |= 4; //Handle a command?
		}
		else if (address == 0x280) //Error status register?
		{
			APIC.needstermination |= 8; //Error status register is written!
		}
		else if (address == 0x380) //Initial count register?
		{
			APIC.needstermination |= 0x10; //Initial count register is written!
		}
		else if (address == 0x3E0) //Divide configuration register?
		{
			APIC.needstermination |= 0x20; //Divide configuration register is written!
		}
		else if (address == 0x370) //Error register?
		{
			APIC.needstermination |= 0x40; //Error register is written!
		}
	}

	if (updateredirection) //Update redirection?
	{
		if (APIC.IOAPIC_redirectionentry[(APIC.APIC_address - 0x10) >> 1][0] & 0x10000) //Mask set?
		{
			APIC.IOAPIC_IMRset |= (1 << ((APIC.APIC_address - 0x10) >> 1)); //Set the mask!
		}
		else //Mask cleared?
		{
			APIC.IOAPIC_IMRset &= ~(1 << ((APIC.APIC_address - 0x10) >> 1)); //Clear the mask!
		}
	}

	if (unlikely(isoverlappingw((uint_64)offset, 1, (uint_64)BIU_cachedmemoryaddr, BIU_cachedmemorysize))) //Cached?
	{
		memory_datasize = 0; //Invalidate the read cache to re-read memory!
		BIU_cachedmemorysize = 0; //Invalidate the BIU cache as well!
	}

	memory_datawrittensize = 1; //Only 1 byte written!
	return 1; //Data has been written!
}

extern uint_32 memory_dataread;
extern byte memory_datasize; //The size of the data that has been read!
byte APIC_memIO_rb(uint_32 offset, byte index)
{
	byte is_internalexternalAPIC;
	uint_32 temp, tempoffset, value, address;
	uint_32* whatregister; //What register is accessed?
	byte updateredirection;
	tempoffset = offset; //Backup!
	updateredirection = 0;

	is_internalexternalAPIC = 0; //Default: no APIC chip!
	if (((offset & 0xFFFFFF000ULL) == APIC.baseaddr)) //LAPIC?
	{
		is_internalexternalAPIC |= 1; //LAPIC!
	}
	if ((((offset & 0xFFFFFF000ULL) == APIC.IObaseaddr))) //IO APIC?
	{
		is_internalexternalAPIC |= 2; //IO APIC!
	}
	else if (is_internalexternalAPIC == 0) //Neither?
	{
		return 0; //Neither!
	}

	if (APIC.enabled == 0) return 0; //Not the APIC memory space enabled?
	address = (offset & 0xFFC); //What address is addressed?

	if (((offset&i440fx_ioapic_base_mask)==i440fx_ioapic_base_match) && (is_internalexternalAPIC & 2)) //I/O APIC?
	{
		switch (address&0x10) //What is addressed?
		{
		case 0x0000: //IOAPIC address?
			whatregister = &APIC.APIC_address; //Address register!
			break;
		case 0x0010: //IOAPIC data?
			switch (APIC.APIC_address) //What address is selected?
			{
			case 0x00:
				whatregister = &APIC.IOAPIC_ID; //ID register!
				break;
			case 0x01:
				whatregister = &APIC.IOAPIC_version_numredirectionentries; //Version/Number of direction entries!
				break;
			case 0x02:
				whatregister = &APIC.IOAPIC_arbitrationpriority; //Arbitration priority register!
				break;
			case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
			case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2E: case 0x2F:
			case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37: case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
				whatregister = &APIC.IOAPIC_redirectionentry[(APIC.APIC_address - 0x10) >> 1][(APIC.APIC_address - 0x10) & 1]; //Redirection entry addressed!
				break;
			default: //Unmapped?
				if (is_internalexternalAPIC & 1) //LAPIC?
				{
					goto notIOAPICR;
				}
				else
				{
					return 0; //Unmapped!
				}
				break;
			}
			break;
		default: //Unmapped?
			if (is_internalexternalAPIC & 1) //LAPIC?
			{
				goto notIOAPICR;
			}
			else
			{
				return 0; //Unmapped!
			}
			break;
		}
	}
	else if (is_internalexternalAPIC & 1) //LAPIC?
	{
		notIOAPICR:
		switch (address) //What is addressed?
		{
		case 0x0020:
			whatregister = &APIC.LAPIC_ID; //0020
			break;
		case 0x0030:
			whatregister = &APIC.LAPIC_version; //0030
			break;
		case 0x0080:
			whatregister = &APIC.TaskPriorityRegister; //0080
			break;
		case 0x0090:
			whatregister = &APIC.ArbitrationPriorityRegister; //0090
			break;
		case 0x00A0:
			whatregister = &APIC.ProcessorPriorityRegister; //00A0
			break;
		case 0x00B0:
			whatregister = &APIC.EOIregister; //00B0
			break;
		case 0x00C0:
			whatregister = &APIC.RemoteReadRegister; //00C0
			break;
		case 0x00D0:
			whatregister = &APIC.LogicalDestinationRegister; //00D0
			break;
		case 0x00E0:
			whatregister = &APIC.DestinationFormatRegister; //00E0
			break;
		case 0x00F0:
			whatregister = &APIC.SpuriousInterruptVectorRegister; //00F0
			break;
		case 0x0100:
		case 0x0110:
		case 0x0120:
		case 0x0130:
		case 0x0140:
		case 0x0150:
		case 0x0160:
		case 0x0170:
			whatregister = &APIC.ISR[((address - 0x100) >> 4)]; //ISRs! 0100-0170
			break;
		case 0x0180:
		case 0x0190:
		case 0x01A0:
		case 0x01B0:
		case 0x01C0:
		case 0x01D0:
		case 0x01E0:
		case 0x01F0:
			whatregister = &APIC.TMR[((address - 0x180) >> 4)]; //TMRs! 0180-01F0
			break;
		case 0x0200:
		case 0x0210:
		case 0x0220:
		case 0x0230:
		case 0x0240:
		case 0x0250:
		case 0x0260:
		case 0x0270:
			whatregister = &APIC.IRR[((address - 0x200) >> 4)]; //ISRs! 0200-0270
			break;
		case 0x280:
			whatregister = &APIC.ErrorStatusRegister; //0280
			break;
		case 0x2F0:
			whatregister = &APIC.LVTCorrectedMachineCheckInterruptRegister; //02F0
			break;
		case 0x300:
			whatregister = &APIC.InterruptCommandRegisterLo; //0300
			break;
		case 0x310:
			whatregister = &APIC.InterruptCommandRegisterHi; //0310
			break;
		case 0x320:
			whatregister = &APIC.LVTTimerRegister; //0320
			break;
		case 0x330:
			whatregister = &APIC.LVTThermalSensorRegister; //0330
			break;
		case 0x340:
			whatregister = &APIC.LVTPerformanceMonitoringCounterRegister; //0340
			break;
		case 0x350:
			whatregister = &APIC.LVTLINT0Register; //0350
			break;
		case 0x360:
			whatregister = &APIC.LVTLINT1Register; //0560
			break;
		case 0x370:
			whatregister = &APIC.LVTErrorRegister; //0370
			break;
		case 0x380:
			whatregister = &APIC.InitialCountRegister; //0380
			break;
		case 0x390:
			whatregister = &APIC.CurrentCountRegister; //0390
			break;
		case 0x3E0:
			whatregister = &APIC.DivideConfigurationRegister; //03E0
			break;
		default: //Unmapped?
			return 0; //Unmapped!
			break;
		}
	}
	else
		return 0; //Abort!

	value = *whatregister; //Take the register's value that's there!

	tempoffset = (offset & 3); //What DWord byte is addressed?
	temp = tempoffset;
	#ifdef USE_MEMORY_CACHING
	if ((index & 3) == 0)
	{
		temp &= 3; //Single DWord read only!
		tempoffset &= 3; //Single DWord read only!
		temp = tempoffset; //Backup address!
		tempoffset &= ~3; //Round down to the dword address!
		if (likely(((tempoffset | 3) < 0x1000))) //Enough to read a dword?
		{
			memory_dataread = SDL_SwapLE32(*((uint_32*)(&value))); //Read the data from the result!
			memory_datasize = tempoffset = 4 - (temp - tempoffset); //What is read from the whole dword!
			memory_dataread >>= ((4 - tempoffset) << 3); //Discard the bytes that are not to be read(before the requested address)!
			return 1; //Done: we've been read!
		}
		else
		{
			tempoffset = temp; //Restore the original address!
			tempoffset &= ~1; //Round down to the word address!
			if (likely(((tempoffset | 1) < 0x1000))) //Enough to read a word, aligned?
			{
				memory_dataread = SDL_SwapLE16(*((word*)(&value))); //Read the data from the result!
				memory_datasize = tempoffset = 2 - (temp - tempoffset); //What is read from the whole word!
				memory_dataread >>= ((2 - tempoffset) << 3); //Discard the bytes that are not to be read(before the requested address)!
				return 1; //Done: we've been read!				
			}
			else //Enough to read a byte only?
			{
				memory_dataread = value>>((tempoffset&3)<<3); //Read the data from the result!
				memory_datasize = 1; //Only 1 byte!
				return 1; //Done: we've been read!				
			}
		}
	}
	else //Enough to read a byte only?
	#endif
	{
		memory_dataread = value>>((tempoffset&3)<<3); //Read the data from the ROM, reversed!
		memory_datasize = 1; //Only 1 byte!
		return 1; //Done: we've been read!				
	}
	return 0; //Not implemented yet!
}

void APIC_updateWindowMSR(uint_32 lo, uint_32 hi)
{
	//Update the window MSR!
	APIC.windowMSRhi = hi; //High value of the MSR!
	APIC.windowMSRlo = lo; //Low value of the MSR!
	APIC.baseaddr = (uint_64)(APIC.windowMSRlo & 0xFFFFF000); //Base address for the APIC!
	if (EMULATED_CPU >= CPU_PENTIUMPRO) //4 more pins for the Pentium Pro!
	{
		APIC.baseaddr |= (((uint_64)(APIC.windowMSRhi & 0xF)) << 32); //Extra bits from the high MSR on Pentium II and up!
	}
	APIC.enabled = ((APIC.windowMSRlo & 0x800) >> 11); //APIC space enabled?
}

byte readPollingMode(byte pic); //Prototype!

byte in8259(word portnum, byte *result)
{
	if (__HW_DISABLED) return 0; //Abort!
	if (portnum == 0x22)
	{
		*result = addr22; //Read addr22
		return 1;
	}
	else if (portnum == 0x23)
	{
		if (addr22 == 0x70) //Selected IMCR?
		{
			*result = IMCR; //Give IMCR!
			return 1;
		}
		else
		{
			*result = 0xFF; //Unknown register!
			return 1;
		}
	}
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
	if (portnum == 0x22)
	{
		addr22 = value; //Write addr22
		return 1;
	}
	else if (portnum == 0x23)
	{
		if (addr22 == 0x70) //Selected IMCR?
		{
			if ((IMCR == 0) && (value == 1)) //Disconnect NMI and INTR from the CPU/
			{
				if (APIC.LVTLINT0Register & (1 << 12)) //Raised?
				{
					APIC.LVTLINT0Register &= ~(1 << 12); //Remove LINT0!
				}
			}
			else //Reconnect NMI and INTR to the CPU?
			{
				if (APIC.IOAPIC_liveIRR & 1) //Already raised?
				{
					if ((APIC.LVTLINT0Register & 0x10000) == 0) //Not masked?
					{
						APIC.LVTLINT0Register |= (1 << 12); //Perform LINT0!
					}
				}
				//NMI is handled automatically!
			}
			IMCR = value; //Set IMCR!
			return 1;
		}
		else
		{
			return 1; //Unknown register!
		}
	}
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

byte isSlave(byte PIC)
{
	return PIC; //The first PIC is not a slave, all others are!
}

byte startSlaveMode(byte PIC, byte IR) //For master only! Set slave mode masterIR processing on INTA?
{
	return (i8259.icw[PIC][2]&(1<<IR)) && (!isSlave(PIC)) && ((i8259.icw[PIC][0]&2)==0); //Process slaves and set IR on the slave UD instead of 0 while in cascade mode?
}

byte respondMasterSlave(byte PIC, byte masterIR) //Process this PIC as a slave for a set slave IR?
{
	return (((i8259.icw[PIC][2]&3)==masterIR) && (isSlave(PIC))) || ((masterIR==0xFF) && (!isSlave(PIC)) && (!PIC)); //Process this masterIR as a slave or Master in Master mode connected to INTRQ?
}

OPTINLINE byte getunprocessedinterrupt(byte PIC)
{
	byte result;
	result = i8259.irr[PIC];
	result &= ~i8259.imr[PIC];
	result &= ~i8259.isr[PIC];
	return result; //Give the result!
}

void IOAPIC_raisepending(); //Prototype!
void acnowledgeirrs()
{
	byte nonedirty; //None are dirtied?
	byte recheck;
	recheck = 0;

	//INTR on the APIC!
	if (getunprocessedinterrupt(0)) //INTR?
	{
		APIC_raisedIRQ(0, 2); //Raised INTR!
	}
	else //No INTR?
	{
		APIC_loweredIRQ(0, 2); //Raised INTR!
	}

performRecheck:
	if (recheck == 0) //Check?
	{
		IOAPIC_raisepending(); //Raise all pending!
		IOAPIC_pollRequests(); //Poll the APIC for possible requests!
		if (getunprocessedinterrupt(1)) //Slave connected to master?
		{
			raiseirq(0x802); //Slave raises INTRQ!
			i8259.intreqtracking[1] = 1; //Tracking INTREQ!
		}
		else if ((recheck == 0) && i8259.intreqtracking[1]) //Slave has been lowered and needs processing?
		{
			lowerirq(0x802); //Slave lowers INTRQ before being acnowledged!
			i8259.intreqtracking[1] = 0; //Not tracking INTREQ!
		}
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
		raiseirq(0x802); //Slave raises INTRQ!
		i8259.intreqtracking[1] = 1; //Tracking INTREQ!
		recheck = 1; //Check again!
		goto performRecheck; //Check again!
	}

	if (nonedirty) //None are dirty anymore?
	{
		irr3_dirty = 0; //Not dirty anymore!
	}

	//INTR on the APIC!
	if (getunprocessedinterrupt(0)) //INTR?
	{
		APIC_raisedIRQ(0, 2); //Raised INTR!
	}
	else //No INTR?
	{
		APIC_loweredIRQ(0, 2); //Raised INTR!
	}
}

sword APIC_intnr = -1;

byte PICInterrupt() //We have an interrupt ready to process? This is the primary PIC's INTA!
{
	if (__HW_DISABLED) return 0; //Abort!
	if ((APIC_intnr = LAPIC_pollRequests())!=-1) //APIC requested?
	{
		return 2; //APIC IRQ!
	}
	if (getunprocessedinterrupt(0) && (IMCR!=0x01)) //Primary PIC interrupt? This is also affected by the IMCR!
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
		lowerirq(0x802); //INTA lowers INTRQ!
		i8259.intreqtracking[1] = 0; //Not tracking INTREQ!
	}
}

byte readPollingMode(byte pic)
{
	byte IR;
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
					ACNIR(pic, realIR, srcIndex); //Acnowledge it!
					lastinterrupt = getint(pic, realIR); //Give the interrupt number!
					i8259.lastinterruptIR[pic] = realIR; //Last acnowledged interrupt line!
					interruptsaved = 1; //Gotten an interrupt saved!
					//Don't perform layering to any slave, this is done at ACNIR!
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

byte i8259_INTA()
{
	byte loopdet = 1;
	byte IR;
	byte PICnr;
	byte masterIR;
	//Not APIC, check the i8259 PIC now!
	PICnr = 0; //First PIC is connected to CPU INTA!
	masterIR = 0xFF; //Default: Unknown(=master device only) IR!
checkSlave:
	//First, process first PIC!
	for (IR = 0; IR < 8; IR++) //Process all IRs for this chip!
	{
		byte realIR = (IR & 7); //What IR within the PIC?
		byte srcIndex;
		for (srcIndex = 0; srcIndex < 0x10; ++srcIndex) //Check all indexes!
		{
			if (IRRequested(PICnr, realIR, srcIndex)) //Requested?
			{
				if (respondMasterSlave(PICnr, masterIR)) //PIC responds as a master or slave?
				{
					ACNIR(PICnr, realIR, srcIndex); //Acnowledge it!
					if (startSlaveMode(PICnr, realIR)) //Start slave processing for this? Let the slave give the IRQ!
					{
						if (loopdet) //Loop detection?
						{
							masterIR = IR; //Slave starts handling this IRQ!
							PICnr ^= 1; //What other PIC to check on the PIC bus?
							loopdet = 0; //Prevent us from looping on more PICs!
							goto checkSlave; //Check the slave instead!
						}
						else //Infinite loop detected!
						{
							goto unknownSlaveIR; //Unknown IR due to loop!
						}
					}

					lastinterrupt = getint(PICnr, realIR); //Give the interrupt number!
					interruptsaved = 1; //Gotten an interrupt saved!
					i8259.lastinterruptIR[PICnr] = realIR; //Last IR!
					return lastinterrupt;
				}
			}
		}
	}

unknownSlaveIR: //Slave has exited out to prevent looping!
	i8259.lastinterruptIR[PICnr] = 7; //Last IR!
	lastinterrupt = getint(PICnr, 7); //Unknown, dispatch through IR7 of the used PIC!
	interruptsaved = 1; //Gotten!
	return lastinterrupt; //No result: unk interrupt!
}

byte nextintr()
{
	sword result;
	if (__HW_DISABLED) return 0; //Abort!

	//Check APIC first!
	if (APIC_intnr!=-1) //APIC interrupt requested to fire?
	{
		result = APIC_intnr; //Accept!
		APIC_intnr = -1; //Invalidate!
		//Now that we've selected the highest priority IR, start firing it!
		return (byte)result; //Give the interrupt vector number!
	}

	return i8259_INTA(); //Perform a normal INTA and give the interrupt number!
}

void APIC_raisedIRQ(byte PIC, byte irqnum)
{
	//A line has been raised!
	if (irqnum == 0) irqnum = 2; //IRQ0 is on APIC line 2!
	else if (irqnum == 2) irqnum = 0; //INTR to APIC line 0!
	//INTR is also on APIC line 0!
	if (irqnum == 0) //Since we're also connected to the CPU, raise LINT properly!
	{
		//Always assume live doesn't match! So that the LINT0 register keeps being up-to-date!
		if (IMCR == 0) //Connected to the CPU?
		{
			if ((APIC.LVTLINT0Register & 0x10000) == 0) //Not masked?
			{
				switch ((APIC.LVTLINT0Register >> 8) & 7) //What mode?
				{
				case 0: //Interrupt? Also named Fixed!
				case 1: //Lowest priority?
					if ((APIC.LVTLINT0Register & 0x8000)==0) //Edge-triggered? Supported!
					{
						if ((APIC.IOAPIC_liveIRR & (1 << (irqnum & 0xF))) == 0) //Not yet raised? Rising edge!
						{
							APIC.LVTLINT0Register |= (1 << 12); //Perform LINT0!
						}
					}
					else
					{
						APIC.LVTLINT0Register |= (1 << 12); //Perform LINT0!
					}
					break;
				case 2: //SMI?
				case 4: //NMI?
				case 5: //INIT or INIT deassert?
					//Edge mode only! Don't do anything when lowered!
					if ((APIC.IOAPIC_liveIRR & (1 << (irqnum & 0xF))) == 0) //Not yet raised? Rising edge!
					{
						APIC.LVTLINT0Register |= (1 << 12); //Perform LINT0!
					}
					break;
				case 7: //extINT? Level only!
					//Always assume that the live IRR doesn't match to keep it live on the triggering!
					APIC.LVTLINT0Register |= (1 << 12); //Perform LINT0!
					break;
				}
			}
		}
	}
	if ((APIC.IOAPIC_redirectionentry[irqnum & 0xF][0] & 0x8000) == 0) //Edge-triggered? Supported!
	{
		APIC.IOAPIC_redirectionentry[irqnum & 0xF][0] |= (1 << 12); //Waiting to be delivered!
		if ((APIC.IOAPIC_redirectionentry[irqnum & 0xF][0] & 0x10000) == 0) //Not masked?
		{
			if (!(APIC.IOAPIC_IRRset & (1 << (irqnum & 0xF)))) //Not already pending?
			{
				APIC.IOAPIC_redirectionentry[irqnum & 0xF][0] &= ~(1 << 12); //Not waiting to be delivered!
				APIC.IOAPIC_IRRset |= (1 << (irqnum & 0xF)); //Set the IRR?
				APIC.IOAPIC_IRRreq &= ~(1 << (irqnum & 0xF)); //Acnowledged if pending!
			}
		}
		else
		{
			APIC.IOAPIC_IRRreq |= (1 << (irqnum & 0xF)); //Requested to fire!
		}
	}
	APIC.IOAPIC_liveIRR |= (1 << (irqnum & 0xF)); //Live status!
}

void IOAPIC_raisepending()
{
	if (!(APIC.IOAPIC_IRRreq&~(APIC.IOAPIC_IMRset)&~(APIC.IOAPIC_IRRset))) return; //Nothing to do?
	byte irqnum;
	for (irqnum=0;irqnum<24;++irqnum) //Check all!
	{
		if ((APIC.IOAPIC_redirectionentry[irqnum & 0xF][0] & 0x8000) == 0) //Edge-triggered? Supported!
		{
			if (APIC.IOAPIC_redirectionentry[irqnum & 0xF][0] & (1 << 12)) //Waiting to be delivered!
			{
				if ((APIC.IOAPIC_redirectionentry[irqnum & 0xF][0] & 0x10000) == 0) //Not masked?
				{
					if (!(APIC.IOAPIC_IRRset & (1 << (irqnum & 0xF)))) //Not already pending?
					{
						if ((APIC.IOAPIC_IRRreq & (1 << (irqnum & 0xF)))) //Pending requested?
						{
							APIC.IOAPIC_redirectionentry[irqnum & 0xF][0] &= ~(1 << 12); //Not waiting to be delivered!
							APIC.IOAPIC_IRRset |= (1 << (irqnum & 0xF)); //Set the IRR?
							APIC.IOAPIC_IRRreq &= ~(1 << (irqnum & 0xF)); //Requested to fire!
						}
					}
				}
			}
		}
	}
}

void APIC_loweredIRQ(byte PIC, byte irqnum)
{
	if (irqnum == 0) irqnum = 2; //IRQ0 is on APIC line 2!
	else if (irqnum == 2) irqnum = 0; //INTR to APIC line 0!
	//INTR is also on APIC line 0!
	//A line has been lowered!
	if ((APIC.IOAPIC_redirectionentry[irqnum & 0xF][0] & 0x8000) == 0) //Edge-triggered? Supported!
	{
		if (APIC.IOAPIC_IRRset & (1 << (irqnum & 0xF))) //Waiting to be delivered?
		{
			APIC.IOAPIC_redirectionentry[irqnum & 0xF][0] &= ~(1 << 12); //Not waiting to be delivered!
			APIC.IOAPIC_IRRset &= ~(1 << (irqnum & 0xF)); //Clear the IRR?
		}
	}
	if (irqnum == 0) //Since we're also connected to the CPU, raise LINT properly!
	{
		switch ((APIC.LVTLINT0Register >> 8) & 7) //What mode?
		{
		case 0: //Interrupt? Also named Fixed!
		case 1: //Lowest priority?
			if (APIC.LVTLINT0Register & 0x8000) //Level-triggered? Supported!
			{
				APIC.LVTLINT0Register &= ~(1 << 12); //Clear LINT0!
			}
			break;
		case 2: //SMI?
		case 4: //NMI?
		case 5: //INIT or INIT deassert?
			//Edge mode only! Don't do anything when lowered!
			break;
		case 7: //extINT? Level only!
				//Always assume that the live IRR doesn't match to keep it live on the triggering!
				APIC.LVTLINT0Register &= ~(1 << 12); //Clear LINT0!
				break;
		}
	}
	APIC.IOAPIC_liveIRR &= ~(1 << (irqnum & 0xF)); //Live status!
}

void raiseirq(word irqnum)
{
	if (__HW_DISABLED) return; //Abort!
	byte requestingindex=(irqnum&0xFF); //Save our index that's requesting!
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
		if (irqnum != 2) //Not valid to cascade on the APIC!
		{
			APIC_raisedIRQ(PIC, irqnum); //We're raised!
		}
		i8259.irr3[PIC][requestingindex] |= (1 << (irqnum & 7)); //Add the IRQ to request because of the rise! This causes us to be the reason during shared IR lines!
		irr3_dirty = 1; //Dirty!
	}
}

void lowerirq(word irqnum)
{
	if (__HW_DISABLED) return; //Abort!
	byte requestingindex = (irqnum&0xFF); //Save our index that's requesting!
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
		if (irqnum != 2) //Not valid to cascade on the APIC!
		{
			APIC_loweredIRQ(PIC, irqnum); //We're lowered!
		}
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
