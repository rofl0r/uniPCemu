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

void initPCI();
void register_PCI(void *config, byte device, byte function, byte size, PCIConfigurationChangeHandler configurationchangehandler); //Register a new device/function to the PCI configuration space!

#endif