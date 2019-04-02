#ifndef __PCI_H
#define __PCI_H

#include "headers/types.h" //Basic types!

typedef void (*PCIConfigurationChangeHandler)(uint_32 address, byte device, byte function, byte size);

typedef struct
{
	word VendorID;
	word DeviceID;
	word Command;
	word Status;
	byte RevisionID;
	byte ProgIF;
	byte Subclass;
	byte ClassCode;
	byte CacheLineSize;
	byte LatencyTimer;
	byte HeaderType;
	byte BIST;
	uint_32 BAR[6]; //Our BARs!
	uint_32 CardBusCISPointer;
	word SubsystemVendorID;
	word SubsystemID;
	uint_32 ExpensionROMBaseAddress;
	byte CapabilitiesPointer;
	word ReservedLow;
	byte ReservedHigh;
	uint_32 Reserved;
	byte InterruptLine;
	byte InterruptPIN;
	byte MinGrant;
	byte MaxLatency;
} PCI_GENERALCONFIG; //The entire PCI data structure!

/*

Note on the BAR format:
bit0=1: I/O port. Bits 2+ are the port.
Bit0=0: Memory address:
	Bit1-2: Memory size(0=32-bit, 1=20-bit, 2=64-bit).
	Bit3: Prefetchable
	Bits 4-31: The base address. This has a special value when a BAR is written with all ones in it's address bits(value 0xFFFFFFF0):
		The value of the BAR becomes the negated size of the memory area this takes up((~x)+1). x must be 16-byte multiple due to the low 4 bits being ROM values(see bits 0-3 above)).

*/

void initPCI();
void register_PCI(void *config, byte device, byte function, byte size, PCIConfigurationChangeHandler configurationchangehandler); //Register a new device/function to the PCI configuration space!

#endif