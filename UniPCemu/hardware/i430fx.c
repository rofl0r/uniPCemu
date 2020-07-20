#define IS_I430FX
#include "headers/hardware/i430fx.h" //Our own types!
#include "headers/hardware/pci.h" //PCI support!
#include "headers/cpu/cpu.h" //CPU reset support!
#include "headers/cpu/biu.h" //CPU reset support!
#include "headers/hardware/ports.h" //Port support!
#include "headers/mmu/mmuhandler.h" //RAM layout updating support!
#include "headers/hardware/ide.h" //IDE PCI support!

byte is_i430fx = 0; //Are we an i430fx motherboard?
byte i430fx_memorymappings_read[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //All read memory/PCI! Set=DRAM, clear=PCI!
byte i430fx_memorymappings_write[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //All write memory/PCI! Set=DRAM, clear=PCI!
byte SMRAM_enabled = 0; //SMRAM enabled?
byte SMRAM_data = 1; //SMRAM responds to data accesses?
byte SMRAM_locked = 0; //Are we locked?
byte SMRAM_SMIACT = 0; //SMI activated
extern byte MMU_memoryholespec; //memory hole specification? 0=Normal, 1=512K, 2=15M.
byte i430fx_previousDRAM[5]; //Previous DRAM values
byte i430fx_DRAMsettings[5]; //Previous DRAM values
typedef struct
{
	byte DRAMsettings[8]; //All 5 DRAM settings to load!
	byte maxmemorysize; //Maximum memory size to use, in MB!
} DRAMInfo;
DRAMInfo i430fx_DRAMsettingslookup[8] = {
	{{0x02,0x02,0x02,0x02,0x02,0x00,0x00,0x00},8}, //up to 8MB
	{{0x02,0x04,0x04,0x04,0x04,0x00,0x00,0x00},16}, //up to 16MB
	{{0x02,0x04,0x06,0x06,0x06,0x00,0x00,0x00},24}, //up to 24MB
	{{0x04,0x08,0x08,0x08,0x08,0x00,0x00,0x00},32}, //up to 32MB
	{{0x04,0x08,0x0C,0x00,0x00,0x00,0x00,0x00},48}, //up to 48MB
	{{0x08,0x10,0x10,0x10,0x10,0x00,0x00,0x00},64}, //up to 64MB
	{{0x04,0x08,0x10,0x18,0x18,0x00,0x00,0x00},96}, //up to 96MB
	{{0x10,0x20,0x20,0x20,0x20,0x00,0x00,0x00},255} //up to 128MB. Since it's capped at 128 MB, take it for larger values as well!
};
byte effectiveDRAMsettings = 0; //Effective DRAM settings!

byte i430fx_configuration[256]; //Full configuration space!
byte i430fx_piix_configuration[256]; //Full configuration space!
byte i430fx_ide_configuration[256]; //IDE configuration!
extern PCI_GENERALCONFIG* activePCI_IDE; //For hooking the PCI IDE into a i430fx handler!

byte APMcontrol = 0;
byte APMstatus = 0;

byte ELCRhigh = 0;
byte ELCRlow = 0;

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
	i430fx_configuration[0x07] = 0x02; //ROM set is a 430FX?
	i430fx_configuration[0x08] = 0x00; //A0 stepping
	i430fx_configuration[0x09] = 0x00;
	i430fx_configuration[0x0A] = 0x00;
	i430fx_configuration[0x0B] = 0x06;
}

void i430fx_update_piixstatus()
{
	i430fx_piix_configuration[0x0E] = (i430fx_piix_configuration[0x0E] & ~0x7F) | ((i430fx_piix_configuration[0x6A] & 0x04) << 5); //Set the bit from the settings!
	i430fx_ide_configuration[0x0E] = (i430fx_ide_configuration[0x0E] & ~0x7F) | ((i430fx_piix_configuration[0x6A] & 0x04) << 5); //Set the bit from the settings!
}

void i430fx_piix_resetPCIConfiguration()
{
	i430fx_piix_configuration[0x00] = 0x86;
	i430fx_piix_configuration[0x01] = 0x80; //Intel
	i430fx_piix_configuration[0x02] = 0x2E;
	i430fx_piix_configuration[0x03] = 0x12; //PIIX
	i430fx_piix_configuration[0x04] = 0x07;
	i430fx_piix_configuration[0x05] = 0x00;
	i430fx_piix_configuration[0x06] = 0x00;
	i430fx_piix_configuration[0x07] = 0x02; //ROM set is a 430FX?
	i430fx_piix_configuration[0x08] = 0x02; //A-1 stepping
	i430fx_piix_configuration[0x09] = 0x00;
	i430fx_piix_configuration[0x0A] = 0x01;
	i430fx_piix_configuration[0x0B] = 0x06;
	i430fx_update_piixstatus(); //Update the status register bit!
}

void i430fx_ide_resetPCIConfiguration()
{
	i430fx_ide_configuration[0x00] = 0x86;
	i430fx_ide_configuration[0x01] = 0x80; //Intel
	i430fx_ide_configuration[0x02] = 0x30;
	i430fx_ide_configuration[0x03] = 0x12; //PIIX
	i430fx_ide_configuration[0x04] = 0x05&1; //Limited use(bit 2=Bus master function, which is masked off to be disabled)
	i430fx_ide_configuration[0x05] = 0x00;
	i430fx_ide_configuration[0x06] = 0x80;
	i430fx_ide_configuration[0x07] = 0x02; //ROM set is a 430FX?
	i430fx_ide_configuration[0x08] = 0x02; //A-1 stepping
	i430fx_ide_configuration[0x09] = 0x80|(i430fx_ide_configuration[0x09]&0xF); //Not capable of IDE-bus master yet, so mask it off! Keep the configuation intact!
	i430fx_ide_configuration[0x0A] = 0x01; //Sub-class
	i430fx_ide_configuration[0x0B] = 0x01; //Base-class
	i430fx_update_piixstatus(); //Update the status register bit!
}

void i430fx_map_read_memoryrange(byte start, byte size, byte maptoRAM)
{
	byte c, e;
	e = start + size; //How many entries?
	for (c = start; c < e; ++c) //Map all entries!
	{
		i430fx_memorymappings_read[c] = maptoRAM; //Set it to the RAM mapping(1) or PCI mapping(0)!
	}
	MMU_RAMlayoutupdated(); //Update the RAM layout!
}

void i430fx_map_write_memoryrange(byte start, byte size, byte maptoRAM)
{
	byte c,e;
	e = start + size; //How many entries?
	for (c = start; c < e; ++c) //Map all entries!
	{
		i430fx_memorymappings_write[c] = maptoRAM; //Set it to the RAM mapping(1) or PCI mapping(0)!
	}
	MMU_RAMlayoutupdated(); //Update the RAM layout!
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

extern byte PCI_transferring;

void i430fx_PCIConfigurationChangeHandler(uint_32 address, byte device, byte function, byte size)
{
	PCI_GENERALCONFIG* config = (PCI_GENERALCONFIG*)&i430fx_configuration; //Configuration generic handling!
	//byte old_DRAMdetect;
	i430fx_resetPCIConfiguration(); //Reset the ROM values!
	switch (address) //What configuration is changed?
	{
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
	case 0x18:
	case 0x19:
	case 0x1A:
	case 0x1B:
	case 0x1C:
	case 0x1D:
	case 0x1E:
	case 0x1F:
	case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x24:
	case 0x25:
	case 0x26:
	case 0x27: //BAR?
		if (PCI_transferring == 0) //Finished transferring data for an entry?
		{
			config->BAR[0] = 0xFFFFFFFD; //Unused!
			config->BAR[1] = 0xFFFFFFFD; //Unused!
			config->BAR[2] = 0xFFFFFFFD; //Unused!
			config->BAR[3] = 0xFFFFFFFD; //Unused!
			config->BAR[4] = 0xFFFFFFFD; //Unused!
			config->BAR[5] = 0xFFFFFFFD; //Unused!
		}
		break;
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
	//case 0x65:
	//case 0x66:
	//case 0x67:
		//DRAM module detection?
		i430fx_configuration[address] &= 0x3F; //Only 6 bits/row!
		/*
		if (((i430fx_configuration[0x60] +
			((i430fx_configuration[0x61] > i430fx_configuration[0x60])?i430fx_configuration[0x61] - i430fx_configuration[0x60]:0) +
			((i430fx_configuration[0x62] > i430fx_configuration[0x61])?i430fx_configuration[0x62] - i430fx_configuration[0x61]:0) +
			((i430fx_configuration[0x63] > i430fx_configuration[0x62])?i430fx_configuration[0x63] - i430fx_configuration[0x62]:0) +
			((i430fx_configuration[0x64] > i430fx_configuration[0x63])?i430fx_configuration[0x64] - i430fx_configuration[0x63]:0)
			)<<22) > *getarchmemory()) //Too much detected?
		{
			i430fx_configuration[address] = i430fx_previousDRAM[address-0x60]; //Reset back to the default: nothing!
		}
		i430fx_previousDRAM[address - 0x60] = i430fx_configuration[address]; //Change detection!
		*/
		//DRAM auto detection!
		memcpy(&i430fx_configuration[0x60], &i430fx_DRAMsettings, 5); //Set all DRAM setting registers to the to be detected value!
		break;
	case 0x72: //SMRAM?
		i430fx_updateSMRAM();
		break;
	default: //Not emulated?
		break; //Ignore!
	}
}

void i430fx_piix_PCIConfigurationChangeHandler(uint_32 address, byte device, byte function, byte size)
{
	PCI_GENERALCONFIG* config = (PCI_GENERALCONFIG*)&i430fx_piix_configuration; //Configuration generic handling!
	i430fx_piix_resetPCIConfiguration(); //Reset the ROM fields!
	switch (address) //What address has been updated?
	{
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
	case 0x18:
	case 0x19:
	case 0x1A:
	case 0x1B:
	case 0x1C:
	case 0x1D:
	case 0x1E:
	case 0x1F:
	case 0x20:
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x24:
	case 0x25:
	case 0x26:
	case 0x27: //BAR?
		if (PCI_transferring == 0) //Finished transferring data for an entry?
		{
			config->BAR[0] = 0xFFFFFFFD; //Unused!
			config->BAR[1] = 0xFFFFFFFD; //Unused!
			config->BAR[2] = 0xFFFFFFFD; //Unused!
			config->BAR[3] = 0xFFFFFFFD; //Unused!
			config->BAR[4] = 0xFFFFFFFD; //Unused!
			config->BAR[5] = 0xFFFFFFFD; //Unused!

			/*
			config->BAR[0] = ((config->BAR[0] & ((~offsetwidth) & 0xFFFFU)) | 1); //IO BAR! This is disabled, so set the mask fully!
			*/
		}
		break;
	case 0x4C: //ISA Recovery I/O timer register
		//Bit 7 set: alias ports 80h, 84-86h, 88h, 8c-8eh to 90-9fh.
		break;
	case 0x4E: //X-bus chip select register
		//bit 6 set: alias PCI FFF80000-FFFDFFFF at F80000-FDFFFF (extended bios).
		//bit 5 set: alias PCI FFFE0000-FFFFFFFF at FE0000-FFFFFF (lower bios).
		//bit 1 set: Enable keyboard Chip-Select for address 60h and 64h.
		//bit 0 set: Enable RTC for addresses 70-77h.
		break;
	case 0x60:
	case 0x61:
	case 0x62:
	case 0x63: //IRQA-IRQD PCI interrupt routing control
		//bit 7 set: disable
		//bits 3-0: IRQ number, except 0-2, 8 and 13.
		break;
	case 0x69: //Top of memory register
		//bits 7-4: Top of memory, in MB-1.
		//bit 3: forward lower bios to PCI(register 4E isn't set for the lower BIOS)? 0=Contain to ISA.
		//bit 1: forward 512-640K region to PCI instead of ISA. 0=Contain to ISA.
		break;
	case 0x6A: //Miscellaneous Status Register
		i430fx_update_piixstatus(); //Update the Misc Status!
		break;
	case 0x70: //MBIRQ0
	case 0x71:  //MBIRQ1
		//bit 7: Interrupt routing enable
		//bit 6: MIRQx/IRQx sharing enable. When 0 and Interrupt routine Enable is cleared, the IRQ is masked.
		//bits 3-0: IRQ line to connect to: 0-3. 8 and 13 are invalid.
		break;
	case 0x78:
	case 0x79: //Programmable Chip-Select control register
		//bit 15-2: 16-bit I/O address (dword accessed) that causes PCS# to be asserted.
		//bit 1-0: Address mask? 0=4 bytes, 1=8 bytes, 2=Disabled, 3=16 bytes.
		break;
	case 0xA0: //SMI control register
		//bit 4-3: Fast off timer count granularity. 1=Disabled.
		//it's 1(at 33MHz PCICLK or 1.1 at 30MHz or 1.32 at 25MHz) minute(when 0), disabled(when 1), 1 PCICLK(when 2), 1(or 1.1 at 33MHz PCICLK, 1.32 at 25MHz
		//bit 2: STPCLK# controlled by high and low timer registers
		//bit 1: APMC read causes assertion of STPCLK#.
		//bit 0: SMI Gate. 1=Enabled, 0=Disable.
		break;
	case 0xA2: //SMI Enable register
		//What triggers an SMI:
		//bit 7: APMC
		//bit 6: EXTSMI#
		//bit 5: Fast Off Timer
		//bit 4: IRQ12(PS/2 mouse)
		//bit 3: IRQ8(RTC Alarm)
		//bit 2: IRQ4(COM2/COM4)
		//bit 1: IRQ3(COM1/COM3)
		//bit 0: IRQ1(PS/2 keyboard)
		break;
	case 0xA4:
	case 0xA5:
	case 0xA6:
	case 0xA7: //System Event Enable Register
		//bit 31: Fast off SMI enable
		//bit 29: fast off NMI enable
		//bit 15-3: Fast off IRQ #<bit> enable
		//bit 1-0: Fast off IRQ #<bit> enable.
		break;
	case 0xA8: //Fast off timer register
		//Reload value of N+1, a read gives the last value written. Countdown to 0 reloads with N+1 and triggers an SMI.
		break;
	case 0xAA:
	case 0xAB: //SMI Request Register
		//What caused an SMI:
		//bit 7: write to APM control register
		//bit 6: extSM#
		//bit 5: Fast off timer
		//bit 4: IRQ12
		//bit 3: IRQ8
		//bit 2: IRQ4
		//bit 1: IRQ3
		//bit 0: IRQ1
		break;
	case 0xAC: //STPCLK# low timer
	case 0xAE: //STPCLK high timer
		//Number of clocks for each STPCLK# transition to/from high,low. PCI clocks=1+(1056*(n+1))
		break;
	}
}

void i430fx_ide_PCIConfigurationChangeHandler(uint_32 address, byte device, byte function, byte size)
{
	ATA_ConfigurationSpaceChanged(address, device, function, size); //Normal ATA/ATAPI handler passthrough!
	i430fx_ide_resetPCIConfiguration(); //Reset the ROM fields!
}

void i430fx_hardreset()
{
	byte address;
	memset(&i430fx_memorymappings_read, 0, sizeof(i430fx_memorymappings_read)); //Default to PCI!
	memset(&i430fx_memorymappings_write, 0, sizeof(i430fx_memorymappings_write)); //Default to PCI!
	memset(&i430fx_configuration, 0, sizeof(i430fx_configuration)); //Initialize the configuration!
	memset(&i430fx_piix_configuration, 0, sizeof(i430fx_piix_configuration)); //Initialize the configuration!

	i430fx_resetPCIConfiguration(); //Initialize/reset the configuration!
	i430fx_piix_resetPCIConfiguration(); //Initialize/reset the configuration!
	i430fx_ide_resetPCIConfiguration(); //Initialize/reset the configuration!

	//Initialize DRAM module detection!
	memset(&i430fx_configuration[0x60], 2, 5); //Initialize the DRAM settings!

	MMU_memoryholespec = 1; //Default: disabled!
	i430fx_configuration[0x59] = 0xF; //Default configuration setting when reset!
	i430fx_configuration[0x57] = 0x01; //Default memory hole setting!
	i430fx_configuration[0x72] = 0x02; //Default SMRAM setting!

	//Known and unknown registers:
	i430fx_configuration[0x52] = 0x02; //0x40: 256kB PLB cache(originally 0x42)? 0x00: No cache installed? 0x02: No cache installed and force cache miss?
	i430fx_configuration[0x53] = 0x14; //ROM set is a 430FX?
	i430fx_configuration[0x56] = 0x52; //ROM set is a 430FX? DRAM control
	i430fx_configuration[0x57] = 0x01;
	i430fx_configuration[0x69] = 0x03; //ROM set is a 430FX?
	i430fx_configuration[0x70] = 0x20; //ROM set is a 430FX?
	i430fx_configuration[0x72] = 0x02;
	i430fx_configuration[0x74] = 0x0E; //ROM set is a 430FX?
	i430fx_configuration[0x78] = 0x23; //ROM set is a 430FX?

	//Initalize all mappings!
	for (address = 0x59; address < 0x5F; ++address) //Initialize us!
	{
		i430fx_PCIConfigurationChangeHandler(address, 3, 0, 1); //Initialize all required settings!
	}

	i430fx_piix_configuration[0x6A] = 0x04; //Default value: PCI Header type bit enable set!
	i430fx_piix_PCIConfigurationChangeHandler(0x6A, 3, 0, 1); //Initialize all required settings!

	SMRAM_locked = 0; //Unlock SMRAM always!
	SMRAM_SMIACT = 0; //Default: not active!
	i430fx_updateSMRAM(); //Update the SMRAM setting!

	APMcontrol = APMstatus = 0; //Initialize APM registers!
	ELCRhigh = ELCRlow = 0; //Initialize the ELCR registers!
}

extern uint_32 PCI_address; //What address register is currently set?
extern BIU_type BIU[NUMCPUS]; //BIU definition!
void i430fx_writeaddr(byte index, byte *value) //Written an address?
{
	if (index == 1) //Written bit 2 of register CF9h?
	{
		if ((*value & 4) && ((PCI_address & 0x400)==0) && (BIU[activeCPU].newtransfer<=1) && (BIU[activeCPU].newtransfer_size==1)) //Set while not set yet during a direct access?
		{
			//Should reset all PCI devices?
			if (*value & 2) //Hard reset?
			{
				i430fx_hardreset(); //Perform a hard reset of the hardware!
				i430fx_configuration[0x59] = 0xF; //Reset this!
				i430fx_PCIConfigurationChangeHandler(0x59, 3, 0, 1); //Updated!
			}
			CPU[activeCPU].resetPending = 1; //Start pending reset!
			*value &= ~4; //Cannot be read as a 1, according to documentation!
		}
	}
}

void i430fx_postwriteaddr(byte index)
{
}

byte readAPM(word port, byte* value)
{
	if (likely((port < 0xB2) || (port > 0xB3))) return 0; //Not us!
	switch (port)
	{
	case 0xB2: //APM Control(APMC)
		*value = APMcontrol; //Give the control register!
		break;
	case 0xB3: //APM Status (APMS)
		*value = APMstatus; //Give the status!
		break;
	}
	return 1; //Give the value!
}

byte writeAPM(word port, byte value)
{
	if (likely((port < 0xB2) || (port > 0xB3))) return 0; //Not us!
	switch (port)
	{
	case 0xB2: //APM Control(APMC)
		APMcontrol = value; //Store the value for reading later!
		//Write: can generate an SMI, depending on bit 7 of SMI Enable register and bit 0 of SMI control register both being set.
		break;
	case 0xB3: //APM Status (APMS)
		APMstatus = value; //Store the value for reading by the handler!
		break;
	}
	return 1; //Give the value!
}

byte readELCR(word port, byte* value)
{
	if (likely((port < 0x4D0) || (port > 0x4D1))) return 0; //Not us!
	switch (port)
	{
	case 0x4D0: //ELCR1
		*value = ELCRlow; //Low!
		break;
	case 0x4D1: //ELCR2
		*value = ELCRhigh; //High!
		break;
	}
	return 1; //Give the value!
}

byte writeELCR(word port, byte value)
{
	if (likely((port < 0x4D0) || (port > 0x4D1))) return 0; //Not us!
	switch (port)
	{
	case 0x4D0: //ELCR1
		ELCRlow = value; //Low!
		break;
	case 0x4D1: //ELCR2
		ELCRhigh = value; //High!
		break;
	}
	return 1; //Give the value!
}

byte i430fx_piix_portremapper(word *port, byte size, byte isread)
{
	if (size != 1) return 1; //Passthrough with invalid size!
	return 1; //Passthrough by default!
}

void i430fx_MMUready()
{
	byte memorydetection;
	//First, detect the DRAM settings to use!
	effectiveDRAMsettings = 0; //Default DRAM settings is the first entry!
	for (memorydetection = 0; memorydetection < NUMITEMS(i430fx_DRAMsettingslookup); ++memorydetection) //Check all possible memory sizes!
	{
		if (MEMsize() <= (i430fx_DRAMsettingslookup[memorydetection].maxmemorysize<<20)) //Within the limits of the maximum memory size?
		{
			effectiveDRAMsettings = memorydetection; //Use this memory size information!
		}
	}
	//effectiveDRAMsettings now points to the DRAM information to use!
	memcpy(&i430fx_DRAMsettings, &i430fx_DRAMsettingslookup[effectiveDRAMsettings], sizeof(i430fx_DRAMsettings)); //Setup the DRAM settings to use!
}

void init_i430fx(byte enabled)
{
	is_i430fx = enabled; //Emulate a i430fx architecture!

	i430fx_hardreset(); //Perform a hard reset of the hardware!
	effectiveDRAMsettings = 0; //Effective DRAM settings to take effect! Start at the first entry, which is the minimum!

	//Register PCI configuration space?
	if (is_i430fx) //Are we enabled?
	{
		register_PCI(&i430fx_configuration, 3, 0, (sizeof(i430fx_configuration)>>2), &i430fx_PCIConfigurationChangeHandler); //Register ourselves to PCI!
		MMU_memoryholespec = 1; //Our specific specification!
		register_PCI(&i430fx_piix_configuration, 4, 0, (sizeof(i430fx_piix_configuration) >> 2), &i430fx_piix_PCIConfigurationChangeHandler); //Register ourselves to PCI!
		register_PCI(&i430fx_ide_configuration, 4, 1, (sizeof(i430fx_ide_configuration) >> 2), &i430fx_ide_PCIConfigurationChangeHandler); //Register ourselves to PCI!
		activePCI_IDE = (PCI_GENERALCONFIG *)&i430fx_ide_configuration; //Use our custom handler!
		//APM registers
		register_PORTIN(&readAPM);
		register_PORTOUT(&writeAPM);
		//ECLR registers
		register_PORTIN(&readELCR);
		register_PORTOUT(&writeELCR);
		//Port remapping itself!
		register_PORTremapping(&i430fx_piix_portremapper);
	}
}

void done_i430fx()
{
	is_i430fx = 0; //Not a i430fx anymore!
}
