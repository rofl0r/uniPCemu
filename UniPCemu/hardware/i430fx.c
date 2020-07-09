#define IS_I430FX
#include "headers/hardware/i430fx.h" //Our own types!
#include "headers/hardware/pci.h" //PCI support!
#include "headers/cpu/cpu.h" //CPU reset support!

byte is_i430fx = 0; //Are we an i430fx motherboard?
byte i430fx_memorymappings_read[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //All read memory/PCI! Set=DRAM, clear=PCI!
byte i430fx_memorymappings_write[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //All write memory/PCI! Set=DRAM, clear=PCI!
byte SMRAM_enabled = 0; //SMRAM enabled?
byte SMRAM_data = 1; //SMRAM responds to data accesses?
byte SMRAM_locked = 0; //Are we locked?
byte SMRAM_SMIACT = 0; //SMI activated
extern byte MMU_memoryholespec; //memory hole specification? 0=Normal, 1=512K, 2=15M.

byte i430fx_configuration[256]; //Full configuration space!

void i430fx_updateSMRAM()
{
	if ((i430fx_configuration[0x72] & 0x10) || SMRAM_locked) //Locked?
	{
		SMRAM_locked = 1; //Permanent lock!
		i430fx_configuration[0x72] &= ~0x40; //Bit is permanently cleared!
	}
	if (i430fx_configuration[0x72] & 0x40) //SMRAM enabled always?
	{
		SMRAM_enabled = (i430fx_configuration[0x72] & 0x08); //Enabled!
	}
	else
	{
		SMRAM_enabled = SMRAM_SMIACT && (i430fx_configuration[0x72] & 0x08); //Enabled for SMIACT!
	}
	SMRAM_data = (i430fx_configuration[0x72]&0x20)?0:1; //SMRAM responds to data accesses?
	MMU_RAMlayoutupdated(); //Update the RAM layout!
}

void i430fx__SMIACT(byte active)
{
	SMRAM_SMIACT = active; //SMIACT#?
	i430fx_updateSMRAM(); //Update the SMRAM mapping!
}

void i430fx_resetPCIConfiguration()
{
	i430fx_configuration[0x00] = 0x86;
	i430fx_configuration[0x01] = 0x80; //Intel
	i430fx_configuration[0x02] = 0x22;
	i430fx_configuration[0x03] = 0x01; //SB82437FX-66
	i430fx_configuration[0x04] = 0x06;
	i430fx_configuration[0x05] = 0x00;
	i430fx_configuration[0x06] = 0x00;
	i430fx_configuration[0x07] = 0x82;
	i430fx_configuration[0x07] = 0x02; //ROM set is a 430FX?
	i430fx_configuration[0x08] = 0x00; //A0 stepping
	i430fx_configuration[0x09] = 0x00;
	i430fx_configuration[0x0A] = 0x00;
	i430fx_configuration[0x0B] = 0x06;
	i430fx_configuration[0x52] = 0x40; //256kB PLB cache?
	i430fx_configuration[0x52] = 0x42; //ROM set is a 430FX?
	i430fx_configuration[0x53] = 0x14; //ROM set is a 430FX?
	i430fx_configuration[0x56] = 0x52; //ROM set is a 430FX? DRAM control
	i430fx_configuration[0x57] = 0x01;
	i430fx_configuration[0x69] = 0x03; //ROM set is a 430FX?
	i430fx_configuration[0x70] = 0x20; //ROM set is a 430FX?
	i430fx_configuration[0x72] = 0x02;
	i430fx_configuration[0x74] = 0x0E; //ROM set is a 430FX?
	i430fx_configuration[0x78] = 0x23; //ROM set is a 430FX?
}

void i430fx_map_read_memoryrange(byte start, byte size, byte maptoRAM)
{
	byte c, e;
	e = start + size; //How many entries?
	for (c = start; c < e; ++c) //Map all entries!
	{
		i430fx_memorymappings_read[c] = maptoRAM; //Set it to the RAM mapping(1) or PCI mapping(0)!
	}
}

void i430fx_map_write_memoryrange(byte start, byte size, byte maptoRAM)
{
	byte c,e;
	e = start + size; //How many entries?
	for (c = start; c < e; ++c) //Map all entries!
	{
		i430fx_memorymappings_write[c] = maptoRAM; //Set it to the RAM mapping(1) or PCI mapping(0)!
	}
}

void i430fx_mapRAMROM(byte start, byte size, byte setting)
{
	switch (setting&3) //What kind of mapping?
	{
	case 0: //Read=PCI, Write=PCI!
		i430fx_map_read_memoryrange(start, size, 0); //Map to PCI for reads!
		i430fx_map_write_memoryrange(start, size, 0); //Map to PCI for writes!
		break;
	case 1: //Read=RAM, write=PCI
		i430fx_map_read_memoryrange(start, size, 1); //Map to RAM for reads!
		i430fx_map_write_memoryrange(start, size, 0); //Map to PCI for writes!
		break;
	case 2: //Read=PCI, write=RAM
		i430fx_map_read_memoryrange(start, size, 0); //Map to PCI for reads!
		i430fx_map_write_memoryrange(start, size, 1); //Map to RAM for writes!
		break;
	case 3: //Read=RAM, Write=RAM
		i430fx_map_read_memoryrange(start, size, 1); //Map to RAM for reads!
		i430fx_map_write_memoryrange(start, size, 1); //Map to RAM for writes!
		break;
	default:
		break;
	}
}

void i430fx_PCIConfigurationChangeHandler(uint_32 address, byte device, byte function, byte size)
{
	i430fx_resetPCIConfiguration(); //Reset the ROM values!
	switch (address) //What configuration is changed?
	{
	case 0x57: //DRAMC - DRAM control register
		switch (((i430fx_configuration[0x57] >> 6) & 3)) //What memory hole to emulate?
		{
		case 0: //None?
			MMU_memoryholespec = 1; //Disabled!
			break;
		case 1: //512K-640K?
			MMU_memoryholespec = 2; //512K memory hole!
			break;
		case 2: //15-16MB?
			MMU_memoryholespec = 3; //15M memory hole!
			break;
		case 3: //Reserved?
			MMU_memoryholespec = 1; //Disabled!
			break;
		}
		break;
	case 0x59: //BIOS ROM at 0xF0000? PAM0
		i430fx_mapRAMROM(0xC, 4, (i430fx_configuration[0x59] >> 4)); //Set it up!
		//bit 4 sets some shadow BIOS setting? It's shadowing the BIOS in that case(Read=RAM setting)!
		break;
	case 0x5A: //PAM1
	case 0x5B: //PAM2
	case 0x5C: //PAM3
	case 0x5D: //PAM4
	case 0x5E: //PAM5
	case 0x5F: //RAM/PCI switches at 0xC0000-0xF0000? PAM6
		address -= 0x5A; //What PAM register number(0-based)?
		i430fx_mapRAMROM((address<<1), 1, (i430fx_configuration[address+0x5A] & 0xF)); //Set it up!
		i430fx_mapRAMROM(((address<<1)|1), 1, (i430fx_configuration[address+0x5A] >> 4)); //Set it up!
		break;
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x64:
	case 0x65:
	case 0x66:
	case 0x67: //DRAM module detection?
		//TODO
		i430fx_configuration[0x60] = i430fx_configuration[0x61] = i430fx_configuration[0x62] = i430fx_configuration[0x63] = i430fx_configuration[0x64] = 0x02; //
		i430fx_configuration[0x67] = 0x11; //ROM set is a 430FX?
		break;
	case 0x72: //SMRAM?
		i430fx_updateSMRAM();
		break;
	default: //Not emulated?
		break; //Ignore!
	}
}


extern uint_32 PCI_address; //What address register is currently set?
void i430fx_writeaddr(byte index, byte value) //Written an address?
{
	if (index == 1) //Written bit 2 of register CF9h?
	{
		if ((value & 4) && (PCI_address & 0x400)) //Set while not set yet?
		{
			//Should reset all PCI devices?
			if (value & 2) //Hard reset?
			{
				i430fx_configuration[0x59] = 0xF; //Reset this!
				i430fx_PCIConfigurationChangeHandler(0x49, 3, 0, 1); //Updated!
			}
			CPU[activeCPU].resetPending = 1; //Start pending reset!
		}
	}
}

void init_i430fx(byte enabled)
{
	byte address;
	is_i430fx = enabled; //Emulate a i430fx architecture!
	memset(&i430fx_memorymappings_read, 0, sizeof(i430fx_memorymappings_read)); //Default to PCI!
	memset(&i430fx_memorymappings_write, 0, sizeof(i430fx_memorymappings_write)); //Default to PCI!
	memset(&i430fx_configuration, 0, sizeof(i430fx_configuration)); //Initialize the configuration!

	i430fx_resetPCIConfiguration(); //Initialize/reset the configuration!

	//Initialize DRAM module detection!
	i430fx_configuration[0x60] = i430fx_configuration[0x61] = i430fx_configuration[0x62] = i430fx_configuration[0x63] = i430fx_configuration[0x64] = 0x02; //
	i430fx_configuration[0x67] = 0x11; //ROM set is a 430FX?

	MMU_memoryholespec = 0; //Default: normal behaviour!
	i430fx_configuration[0x59] = 0xF; //Default configuration setting when reset!

	//Initalize all mappings!
	for (address = 0x59; address < 0x5F; ++address) //Initialize us!
	{
		i430fx_PCIConfigurationChangeHandler(address, 3, 0, 1); //Initialize all required settings!
	}

	SMRAM_locked = 0; //Unlock SMRAM always!
	SMRAM_SMIACT = 0; //Default: not active!
	i430fx_updateSMRAM(); //Update the SMRAM setting!

	//Register PCI configuration space?
	if (enabled) //Are we enabled?
	{
		register_PCI(&i430fx_configuration, 3, 0, (sizeof(i430fx_configuration)>>2), &i430fx_PCIConfigurationChangeHandler); //Register ourselves to PCI!
		MMU_memoryholespec = 0; //Our specific specification!
	}
}

void done_i430fx()
{
	is_i430fx = 0; //Not a i430fx anymore!
}
