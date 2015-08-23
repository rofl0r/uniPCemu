//PCI emulation

#include "headers/types.h" //Basic types!
#include "headers/hardware/ports.h" //I/O port support!

#define PCI_VENDOR_ID 0x00	/* 16 bits */
#define PCI_DEVICE_ID 0x02	/* 16 bits */
#define PCI_COMMAND 0x04	/* 16 bits */
#define PCI_BASE_ADDRESS_0 0x10	/* 32 bits */
#define PCI_BASE_ADDRESS_1 0x14	/* 32 bits [htype 0,1 only] */
#define PCI_BASE_ADDRESS_2 0x18	/* 32 bits [htype 0 only] */
#define PCI_BASE_ADDRESS_3 0x1c	/* 32 bits */
#define PCI_BASE_ADDRESS_4 0x20	/* 32 bits */
#define PCI_BASE_ADDRESS_5 0x24	/* 32 bits */
#define PCI_INTERRUPT_LINE 0x3c	/* 8 bits */
#define PCI_CLASS_REVISION 0x08	/* High 24 bits are class, low 8 revision */

byte *configurationspaces[0x100]; //All possible configuation spaces!
byte configurationsizes[0x100]; //The size of the configuration!

uint_32 PCI_address, PCI_data, PCI_status; //Address data and status buffers!

byte PCI_read_data(uint_32 address, byte index) //Read data from the PCI space!
{
	word device;
	device = (address & 0xFFFF00) >> 8; //What device?
	if (device >= NUMITEMS(configurationspaces))
	{
		PCI_status = 0xFFFFFFFF; //Error!
		return 0xFF; //Non-existant device number!
	}
	if (!configurationspaces[device])
	{
		PCI_status = 0xFFFFFFFF; //Error!
		return 0xFF; //Unregistered device number!
	}
	address &= 0xFC; //What address are we requesting within the PCI device!
	PCI_status = 0x80000000; //OK!
	return configurationspaces[device][address+index]; //Give the configuration entry!
}

void PCI_write_data(uint_32 address, byte index, byte value) //Write data to the PCI space!
{
	word device;
	device = (address & 0xFFFF00) >> 8; //What device?
	if (device >= NUMITEMS(configurationspaces))
	{
		PCI_status = 0xFFFFFFFF; //Error!
		return; //Non-existant device number!
	}
	if (!configurationspaces[device])
	{
		PCI_status = 0xFFFFFFFF; //Error!
		return; //Unregistered device number!
	}
	PCI_status = 0x80000000; //OK!
	if ((address+index) > 4) //Not write protected data (identification and status)?
	{
		configurationspaces[device][address+index] = value; //Set the data!
	}
}

extern byte singlestep; //Enforce single step by CPU/hardware special debugging effects?

byte inPCI(word port, byte *result)
{
	switch (port)
	{
	case 0xCF8: //Status low word low part?
	case 0xCF9: //Status low word high part?
	case 0xCFA: //Status high high low part?
	case 0xCFB: //Status high word high part?
		*result = ((PCI_status>> ((port & 3) << 3)) & 0xFF); //Read the current status byte!
		return 1;
		break;
	case 0xCFC: //Data low word low part?
	case 0xCFD: //Data low word high part?
	case 0xCFE: //Data high word low part?
	case 0xCFF: //Data high word high part?
		*result = PCI_read_data(PCI_address,port&3); //Read the current status byte!
		return 1;
		break;
	}
	return 0; //Not supported yet!
}

byte outPCI(word port, byte value)
{
	byte bitpos; //0,8,16,24!
	switch (port)
	{
	case 0xCF8: //Address low word low part?
	case 0xCF9: //Address low word high part?
	case 0xCFA: //Address high word low part?
	case 0xCFB: //Address high word high part?
		bitpos = ((port & 3) << 3); //Get the bit position!
		PCI_address &= ~((0xFF)<<bitpos); //Clear the old address bits!
		PCI_address |= value << bitpos; //Set the new address bits!
		return 1;
		break;
	case 0xCFC: //Data low word low part?
	case 0xCFD: //Data low word high part?
	case 0xCFE: //Data high word low part?
	case 0xCFF: //Data high word high part?
		PCI_write_data(PCI_address,port&3, value); //Write the byte to the configuration space if allowed!
		return 1;
		break;
	}
	return 0; //Not supported yet!
}

void register_PCI(void *config, byte size)
{
	int i;
	for (i = 0;i < NUMITEMS(configurationspaces);i++) //Check for available configuration space!
	{
		if (configurationspaces[i] == config) //Already registered?
		{
			return; //Abort: we've already been registered!
		}
	}
	for (i = 0;i < NUMITEMS(configurationspaces);i++) //Check for available configuration space!
	{
		if (!configurationspaces[i]) //Not set yet?
		{
			configurationspaces[i] = config; //Set up the configuration!
			configurationsizes[i] = size; //What size (in dwords)!
			return; //We've registered!
		}
	}
}

void initPCI()
{
	register_PORTIN(&inPCI);
	register_PORTOUT(&outPCI);
	//We don't implement DMA: this is done by our own DMA controller!
	memset(configurationspaces, 0, sizeof(configurationspaces)); //Clear all configuration spaces set!
}