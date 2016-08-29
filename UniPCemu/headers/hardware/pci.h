#ifndef __PCI_H
#define __PCI_H

#include "headers/types.h" //Basic types!

typedef void (*PCIConfigurationChangeHandler)(uint_32 address, byte size);

typedef struct
{
	word DeviceID;
	word VendorID;
	word Status;
	word Command;
	byte ClassCode;
	byte Subclass;
	byte ProgIF;
	byte RevisionID;
	byte BIST;
	byte HeaderType;
	byte LatencyTimer;
	byte CacheLineSize;
	uint_32 BAR[6]; //Our BARs!
	uint_32 CardBusCISPointer;
	word SubsystemID;
	word SubsystemVendorID;
	uint_32 ExpensionROMBaseAddress;
	word ReservedLow;
	byte ReservedHigh;
	byte CapabilitiesPointer;
	uint_32 Reserved;
	byte MaxLatency;
	byte MinGrant;
	byte InterruptPIN;
	byte InterruptLine;
} PCI_CONFIG; //The entire PCI data structure!

void initPCI();
void register_PCI(void *config, byte size, PCIConfigurationChangeHandler configurationchangehandler);

#endif