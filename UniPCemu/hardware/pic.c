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
	//Remaining variables? All memory that's stored in the APIC!
	uint_32 APIC_address; //Address register for the extended memory!
	uint_32 APIC_data; //Data register for the extended memory!

	//Now, the actual memory for the LAPIC!
	byte LAPIC_requirestermination[0x400]; //Dword requires termination?
	byte LAPIC_globalrequirestermination; //Is termination required at all for the Local APIC?
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
	uint_32 ISR[29]; //ISRs! 0100-0170
	uint_32 TMR[29]; //TMRs! 0180-01F0
	uint_32 IRR[29]; //IRRs! 0200-0270
	uint_32 ErrorStatusRegister; //0280
	uint_32 LVTCorrectedMachineCheckInterruptRegister; //02F0
	uint_32 InterruptCommandRegisterLo; //0300
	uint_32 InterruptCommandRegisterHi; //0310
	uint_32 LVTTimerRegister; //0320
	uint_32 LVTThermalSensorRegister; //0330
	uint_32 LVTPerformanceMonitoringCounterRegister; //0340
	uint_32 LVTLINT0Register; //0350
	uint_32 LVTLINT1Register; //0560
	uint_32 LVTErrorRegister; //0370
	uint_32 InitialCountRegister; //0380
	uint_32 CurrentCountRegistetr; //0390
	uint_32 DivideConfigurationRegister; //03E0

	//IO APIC address registers!
	uint_32 IOAPIC_Address; //Address register for the IOAPIC registers! 0000
	uint_32 IOAPIC_Value; //Value register for the IOAPIC registers! 0010

	//Now, the IO APIC registers
	uint_32 IOAPIC_ID; //00: ID
	uint_32 IOAPIC_version_numredirectionentries; //01: Version(bits 0-7), # of redirection entries(16-23).
	uint_32 IOAPIC_arbitrationpriority; //02: Arbitration priority(Bits 24-27), remainder is reserved.
	uint_32 redirectionentry[24][2]; //10-3F: 2 dwords for each redirection entry setting! Total 48 dwords!
	byte IOAPIC_requirestermination[0x40]; //Termination required for this entry?
	byte IOAPIC_globalrequirestermination; //Is termination required for the IO APIC?
} APIC; //The APIC that's emulated!

//i8259.irr is the complete status of all 8 interrupt lines at the moment. Any software having raised it's line, raises this. Otherwise, it's lowered(irr3 are all cleared)!
//i8259.irr2 is the live status of each of the parallel interrupt lines!
//i8259.irr3 is the identifier for request subchannels that are pending to be acnowledged(cleared when acnowledge and the interrupt is fired).

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
	APIC.enabled = 0; //Is the APIC enabled?
	APIC.needstermination = 0; //Doesn't need termination!
}

void APIC_handletermination() //Handle termination on the APIC!
{
	//Handle any writes to APIC addresses!
	if (likely(APIC.needstermination == 0)) return; //No termination needed?

	APIC.needstermination = 0; //No termination is needed anymore!
	//Now, handle the termination of the various registers!
}

extern byte memory_datawrittensize; //How many bytes have been written to memory during a write!
byte APIC_memIO_wb(uint_32 offset, byte value)
{
	uint_32 temp, tempoffset, storedvalue, ROMbits, address;
	uint_32* whatregister; //What register is addressed?
	tempoffset = offset; //Backup!
	if ((APIC.enabled == 0) || ((offset&0xFFFFFF000ULL) != APIC.baseaddr)) return 0; //Not the APIC memory space addressed?
	address = (offset & 0xFFC); //What address is addressed?

	ROMbits = ~0; //All bits are ROM bits by default?

	switch (address) //What is addressed?
	{
	case 0x0000: //IOAPIC address?
		whatregister = &APIC.APIC_address; //Address register!
		ROMbits = 0; //Fully writable!
		break;
	case 0x0010: //IOAPIC data?
		switch (APIC.APIC_address) //What address is selected?
		{
		case 0x00:
			whatregister = &APIC.IOAPIC_ID; //ID register!
			ROMbits = ~(0xF<<24); //Bits 24-27 writable!
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
			whatregister = &APIC.redirectionentry[(APIC.APIC_address - 0x10) >> 1][(APIC.APIC_address - 0x10) & 1]; //Redirection entry addressed!
			ROMbits = (1<<12)|(1<<14)|0xFFFFFFFFE00000ULL; //Fully writable, except bits 12, 14 and 17-55!
			break;
		default: //Unmapped?
			return 0; //Unmapped!
			break;
		}
		break;
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
	case 0x0104:
	case 0x0108:
	case 0x010C:
	case 0x0110:
	case 0x0114:
	case 0x0118:
	case 0x011C:
	case 0x0120:
	case 0x0124:
	case 0x0128:
	case 0x012C:
	case 0x0130:
	case 0x0134:
	case 0x0138:
	case 0x013C:
	case 0x0140:
	case 0x0144:
	case 0x0148:
	case 0x014C:
	case 0x0150:
	case 0x0154:
	case 0x0158:
	case 0x015C:
	case 0x0160:
	case 0x0164:
	case 0x0168:
	case 0x016C:
	case 0x0170:
	case 0x0174:
	case 0x0178:
	case 0x017C:
		whatregister = &APIC.ISR[((address - 0x100) >> 2)]; //ISRs! 0100-0170
		break;
	case 0x0180:
	case 0x0184:
	case 0x0188:
	case 0x018C:
	case 0x0190:
	case 0x0194:
	case 0x0198:
	case 0x019C:
	case 0x01A0:
	case 0x01A4:
	case 0x01A8:
	case 0x01AC:
	case 0x01B0:
	case 0x01B4:
	case 0x01B8:
	case 0x01BC:
	case 0x01C0:
	case 0x01C4:
	case 0x01C8:
	case 0x01CC:
	case 0x01D0:
	case 0x01D4:
	case 0x01D8:
	case 0x01DC:
	case 0x01E0:
	case 0x01E4:
	case 0x01E8:
	case 0x01EC:
	case 0x01F0:
	case 0x01F4:
	case 0x01F8:
	case 0x01FC:
		whatregister = &APIC.TMR[((address - 0x180) >> 2)]; //TMRs! 0180-01F0
		break;
	case 0x0200:
	case 0x0204:
	case 0x0208:
	case 0x020C:
	case 0x0210:
	case 0x0214:
	case 0x0218:
	case 0x021C:
	case 0x0220:
	case 0x0224:
	case 0x0228:
	case 0x022C:
	case 0x0230:
	case 0x0234:
	case 0x0238:
	case 0x023C:
	case 0x0240:
	case 0x0244:
	case 0x0248:
	case 0x024C:
	case 0x0250:
	case 0x0254:
	case 0x0258:
	case 0x025C:
	case 0x0260:
	case 0x0264:
	case 0x0268:
	case 0x026C:
	case 0x0270:
	case 0x0274:
	case 0x0278:
	case 0x027C:
		whatregister = &APIC.IRR[((address - 0x100) >> 2)]; //ISRs! 0200-0270
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
		ROMbits = 0; //Fully writable!
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
		whatregister = &APIC.CurrentCountRegistetr; //0390
		break;
	case 0x3E0:
		whatregister = &APIC.DivideConfigurationRegister; //03E0
		ROMbits = 0; //Fully writable!
		break;
	default: //Unmapped?
		return 0; //Unmapped!
		break;
	}

	//Get stored value!
	storedvalue = *whatregister; //What value is read at said address?

	//Create the value with adjusted data for storing it back!
	storedvalue = (storedvalue & ((~(0xFF << ((offset & 3) << 3))) | ROMbits)) | (value & ((0xFF << ((offset & 3) << 3)) & ~ROMbits)); //Stored value without the ROM bits!

	//Store the value back to the register!
	*whatregister = storedvalue; //Store the new value inside the register, if allowed to be changed!

	memory_datawrittensize = 1; //Only 1 byte written!
	return 1; //Data has been written!
}

extern uint_32 memory_dataread;
extern byte memory_datasize; //The size of the data that has been read!
byte APIC_memIO_rb(uint_32 offset, byte index)
{
	uint_32 temp, tempoffset, value, address;
	uint_32* whatregister; //What register is accessed?
	tempoffset = offset; //Backup!
	if ((APIC.enabled == 0) || ((offset & 0xFFFFFF000ULL) != APIC.baseaddr)) return 0; //Not the APIC memory space addressed?
	address = (offset & 0xFFC); //What address is addressed?
	switch (address) //What is addressed?
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
			whatregister = &APIC.redirectionentry[(APIC.APIC_address - 0x10) >> 1][(APIC.APIC_address - 0x10) & 1]; //Redirection entry addressed!
			break;
		default: //Unmapped?
			return 0; //Unmapped!
			break;
		}
		break;
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
	case 0x0104:
	case 0x0108:
	case 0x010C:
	case 0x0110:
	case 0x0114:
	case 0x0118:
	case 0x011C:
	case 0x0120:
	case 0x0124:
	case 0x0128:
	case 0x012C:
	case 0x0130:
	case 0x0134:
	case 0x0138:
	case 0x013C:
	case 0x0140:
	case 0x0144:
	case 0x0148:
	case 0x014C:
	case 0x0150:
	case 0x0154:
	case 0x0158:
	case 0x015C:
	case 0x0160:
	case 0x0164:
	case 0x0168:
	case 0x016C:
	case 0x0170:
	case 0x0174:
	case 0x0178:
	case 0x017C:
		whatregister = &APIC.ISR[((address-0x100)>>2)]; //ISRs! 0100-0170
		break;
	case 0x0180:
	case 0x0184:
	case 0x0188:
	case 0x018C:
	case 0x0190:
	case 0x0194:
	case 0x0198:
	case 0x019C:
	case 0x01A0:
	case 0x01A4:
	case 0x01A8:
	case 0x01AC:
	case 0x01B0:
	case 0x01B4:
	case 0x01B8:
	case 0x01BC:
	case 0x01C0:
	case 0x01C4:
	case 0x01C8:
	case 0x01CC:
	case 0x01D0:
	case 0x01D4:
	case 0x01D8:
	case 0x01DC:
	case 0x01E0:
	case 0x01E4:
	case 0x01E8:
	case 0x01EC:
	case 0x01F0:
	case 0x01F4:
	case 0x01F8:
	case 0x01FC:
		whatregister = &APIC.TMR[((address - 0x180) >> 2)]; //TMRs! 0180-01F0
		break;
	case 0x0200:
	case 0x0204:
	case 0x0208:
	case 0x020C:
	case 0x0210:
	case 0x0214:
	case 0x0218:
	case 0x021C:
	case 0x0220:
	case 0x0224:
	case 0x0228:
	case 0x022C:
	case 0x0230:
	case 0x0234:
	case 0x0238:
	case 0x023C:
	case 0x0240:
	case 0x0244:
	case 0x0248:
	case 0x024C:
	case 0x0250:
	case 0x0254:
	case 0x0258:
	case 0x025C:
	case 0x0260:
	case 0x0264:
	case 0x0268:
	case 0x026C:
	case 0x0270:
	case 0x0274:
	case 0x0278:
	case 0x027C:
		whatregister = &APIC.IRR[((address - 0x100) >> 2)]; //ISRs! 0200-0270
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
		whatregister = &APIC.CurrentCountRegistetr; //0390
		break;
	case 0x3E0:
		whatregister = &APIC.DivideConfigurationRegister; //03E0
		break;
	default: //Unmapped?
		return 0; //Unmapped!
		break;
	}

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

void acnowledgeirrs()
{
	byte nonedirty; //None are dirtied?
	byte recheck;
	recheck = 0;
performRecheck:
	if (recheck == 0) //Check?
	{
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
		lowerirq(0x802); //INTA lowers INTRQ!
		i8259.intreqtracking[1] = 0; //Not tracking INTREQ!
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

byte nextintr()
{
	if (__HW_DISABLED) return 0; //Abort!
	byte loopdet=1;
	byte IR;
	byte PICnr;
	byte masterIR;
	PICnr = 0; //First PIC is connected to CPU INTA!
	masterIR = 0xFF; //Default: Unknown(=master device only) IR!
	checkSlave:
	//First, process first PIC!
	for (IR=0; IR<8; IR++) //Process all IRs for this chip!
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
	lastinterrupt = getint(PICnr,7); //Unknown, dispatch through IR7 of the used PIC!
	interruptsaved = 1; //Gotten!
	return lastinterrupt; //No result: unk interrupt!
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
