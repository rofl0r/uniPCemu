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

uint_32 *configurationspaces[0x100]; //All possible configuation spaces!
byte configurationsizes[0x100]; //The size of the configuration!

uint_32 PCI_address, PCI_data, PCI_status; //Address data and status buffers!

uint_32 PCI_read_data(uint_32 address) //Read data from the PCI space!
{
	word device;
	device = (address & 0xFFFF00) >> 8; //What device?
	if (device >= NUMITEMS(configurationspaces))
	{
		PCI_status = 0xFFFFFFFF; //Error!
		return 0xFFFFFFFF; //Non-existant device number!
	}
	if (!configurationspaces[device])
	{
		PCI_status = 0xFFFFFFFF; //Error!
		return 0xFFFFFFFF; //Unregistered device number!
	}
	address &= 0xFC; //What address are we requesting within the PCI device!
	PCI_status = 0x80000000; //OK!
	return configurationspaces[device][address]; //Give the configuration entry!
}

void PCI_write_data(uint_32 address, uint_32 value) //Write data to the PCI space!
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
	if (address > 1) //Not write protected data (identification and status)?
	{
		configurationspaces[device][address] = value; //Set the data!
	}
}

extern byte singlestep; //Enforce single step by CPU/hardware special debugging effects?

byte inPCI(word port, byte *result)
{
	byte bitpos = ((port & 3) << 3); //0,8,16,24!
	switch (port)
	{
	case 0xCF8: //Status low word low part?
	case 0xCF9: //Status low word high part?
	case 0xCFA: //Status high high low part?
	case 0xCFB: //Status high word high part?
		*result = ((PCI_status>>bitpos) & 0xFF); //Read the current status byte!
		return 1;
		break;
	case 0xCFC: //Data low word low part?
	case 0xCFD: //Data low word high part?
	case 0xCFE: //Data high word low part?
	case 0xCFF: //Data high word high part?
		*result = ((PCI_read_data(PCI_address)>>bitpos) & 0xFF); //Read the current status byte!
		return 1;
		break;
	}
	return 0; //Not supported yet!
}

byte outPCI(word port, byte value)
{
	byte bitpos = ((port & 3) << 3); //0,8,16,24!
	uint_32 temp_value; //Old value!
	switch (port)
	{
	case 0xCF8: //Address low word low part?
	case 0xCF9: //Address low word high part?
	case 0xCFA: //Address high word low part?
	case 0xCFB: //Address high word high part?
		PCI_address &= ~((0xFF)<<bitpos); //Clear the old address bits!
		PCI_address |= value << bitpos; //Set the new address bits!
		return 1;
		break;
	case 0xCFC: //Data low word low part?
	case 0xCFD: //Data low word high part?
	case 0xCFE: //Data high word low part?
	case 0xCFF: //Data high word high part?
		temp_value = PCI_read_data(PCI_address); //Read the old value!
		temp_value &= ~(0xFF << bitpos); //Clear the old data bits!
		temp_value |= value << bitpos; //Set the new data bits!
		PCI_write_data(PCI_address, temp_value); //Write the DWORD to the configuration space if allowed!
		return 1;
		break;
	}
	return 0; //Not supported yet!
}

void register_PCI(uint_32 *config, byte size)
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