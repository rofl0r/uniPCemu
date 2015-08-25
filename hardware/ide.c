//ATA-1 harddisk emulation

#include "headers/types.h" //Basic types!
#include "headers/bios/io.h" //I/O support!
#include "headers/hardware/ports.h" //I/O port support!
#include "headers/emu/timers.h" //Timer support!
#include "headers/hardware/pci.h" //PCI support!
#include "headers/support/highrestimer.h" //High resolution timer support!

//Use WD BIOS by default assignment setting for compatibility?
#define WD_ATA

//Hard disk IRQ!
#define ATA_PRIMARYIRQ 14
#define ATA_SECONDARYIRQ 15

extern byte singlestep; //Enable single stepping when called?

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
} PCI_DEVICE; //The entire PCI data structure!

PCI_DEVICE PCI_IDE;

struct
{
	byte longop; //Long operation instead of a normal one?
	uint_32 datapos; //Data position?
	uint_32 datablock; //How large is a data block to be refreshed?
	uint_32 datasize; //Data size?
	byte data[512]; //Full sector data!
	word resultpos; //Result position?
	word resultsize; //Result size?
	byte result[512]; //Full result data!
	byte command;
	byte commandstatus; //Do we have a command?
	struct
	{
		union
		{
			struct
			{
				byte noaddressmark : 1;
				byte track0notfound : 1;
				byte commandaborted : 1;
				byte mediachangerequested : 1;
				byte idmarknotfound : 1;
				byte mediachanged : 1;
				byte uncorrectabledata : 1;
				byte badsector : 1;
			};
			byte data;
		} ERRORREGISTER;
		union
		{
			struct
			{
				byte error : 1; //An error has occurred when 1!
				byte index : 1; //Set once per disk revolution.
				byte correcteddata : 1; //Data has been corrected.
				byte datarequestready : 1; //Ready to transfer a word or byte of data between the host and the drive.
				byte driveseekcomplete : 1; //Drive heads are settled on a track.
				byte drivewritefault : 1; //Write fault status.
				byte driveready : 1; //Ready to accept a command?
				byte busy : 1; //The drive has access to the Command Block Registers.
			};
			byte data;
		} STATUSREGISTER;

		struct
		{
			union
			{
				struct
				{
					byte sectornumber; //LBA bits 0-7!
					byte cylinderhigh; //LBA bits 8-15!
					byte cylinderlow; //LBA bits 16-23!
					union
					{
						byte drivehead; //LBA 24-27!
						struct
						{
							byte head : 4; //What head?
							byte slavedrive : 1; //What drive?
							byte always1_1 : 1;
							byte LBAMode_2 : 1; //LBA mode?
							byte always1_2 : 1;
						};
						struct
						{
							byte LBAhigh : 6; //High 6 bits!
							byte LBAMode : 1; //LBA mode?
							byte always1_3 : 1;
						};
					};
				};
				uint_32 LBA; //LBA address in LBA mode (28 bits value)!
			};
			byte features;
			byte sectorcount;
		} PARAMETERS;
		word driveparams[0x100]; //All drive parameters for a drive!
		uint_32 current_LBA_address; //Current LBA address!
	} Drive[2]; //Two drives!

	union
	{
		struct
		{
			byte unused : 1;
			byte nIEN : 1; //Disable interrupts when set or not the drive selected!
			byte SRST : 1; //Reset!
		};
		byte data;
	} DriveControlRegister;
	union
	{
		byte data;
	} DriveAddressRegister;

	byte activedrive; //What drive are we currently?
	byte DMAPending; //DMA pending?
	byte TC; //Terminal count occurred in DMA transfer?
} ATA[2]; //Two channels of ATA drives!

uint_32 ATA_CHS2LBA(byte channel, byte slave, word cylinder, byte head, byte sector)
{
	return ((cylinder*ATA[channel].Drive[slave].driveparams[55]) + head)*ATA[channel].Drive[slave].driveparams[56]; //Give the LBA value!
}

void ATA_LBA2CHS(byte channel, byte slave, uint_32 LBA, word *cylinder, byte *head, byte *sector)
{
	uint_32 temp;
	temp = (ATA[channel].Drive[slave].driveparams[55] * ATA[channel].Drive[slave].driveparams[56]); //Sectors per cylinder!
	*cylinder = (word)(LBA / temp); //Cylinder!
	LBA -= temp; //Decrease LBA to get heads&sectors!
	temp = ATA[channel].Drive[slave].driveparams[57]; //SPT!
	*head = (LBA / temp) & 0xF; //The head!
	LBA -= temp; //Decrease LBA to get sectors!
	*sector = (LBA & 0xFF); //The sector!
}

int ATA_Drives[2][2]; //All ATA mounted drives to disk conversion!
byte ATA_DrivesReverse[4][2]; //All Drive to ATA mounted drives conversion!

void ATA_IRQ(byte channel, byte slave)
{
	if (!ATA[channel].DriveControlRegister.nIEN) //Allow interrupts?
	{
		switch (channel)
		{
		case 0: //Primary channel?
			doirq(ATA_PRIMARYIRQ); //Execute the IRQ!
			break;
		case 1:
			doirq(ATA_PRIMARYIRQ); //Execute the IRQ!
			break;
		default: //Unknown channel?
			break;
		}
	}
}

TicksHolder ATATicks;
struct
{
	uint_64 ATA_tickstiming;
	uint_64 ATA_tickstimeout; //How big a timeout are we talking about?
	byte type; //Type of timer!
} IRQtimer[4]; //IRQ timer!

void updateATA() //ATA timing!
{
	uint_64 passed = getuspassed_k(&ATATicks); //Get us passed!
	if (passed) //Anything passed?
	{
		getuspassed(&ATATicks); //Passed some time!
		int i;
		for (i = 0;i < 4;i++) //Process all timers!
		{
			if (IRQtimer[i].ATA_tickstimeout) //Ticking?
			{
				IRQtimer[i].ATA_tickstiming += passed; //We've passed some!
				if (IRQtimer[i].ATA_tickstiming >= IRQtimer[i].ATA_tickstimeout) //Expired?
				{
					IRQtimer[i].ATA_tickstimeout = 0; //Finished!
					ATA_IRQ(i & 2, i & 1); //Do an IRQ from the source!
					byte drive = (i & 1); //Drive!
					byte channel = ((i & 2) >> 1); //Channel!
					if (IRQtimer[i].type == 1) //Calibrate?
					{
						ATA[channel].Drive[drive].PARAMETERS.cylinderhigh = ATA[channel].Drive[drive].PARAMETERS.cylinderlow = 0; //Clear cylinder #!
						ATA[channel].Drive[drive].STATUSREGISTER.driveseekcomplete = 1; //We've completed seeking!
						ATA[channel].Drive[drive].ERRORREGISTER.data = 0; //No error!
						ATA[channel].commandstatus = 0; //Reset status!
					}
					else if (IRQtimer[i].type == 2) //Seek?
					{
						ATA[channel].Drive[drive].STATUSREGISTER.driveseekcomplete = 1; //We've completed seeking!
						ATA[channel].Drive[drive].ERRORREGISTER.data = 0; //No error!
						ATA[channel].commandstatus = 0; //Reset status!
					}
				}
			}
		}
	}
}

uint_32 getPORTaddress(byte channel)
{
	switch (channel)
	{
	case 0: //First?
		return (PCI_IDE.BAR[0] > 1) ? PCI_IDE.BAR[0] : 0x1F0; //Give the BAR!
		break;
	case 1: //Second?
		return (PCI_IDE.BAR[2] > 1) ? PCI_IDE.BAR[2] : 0x170; //Give the BAR!
		break;
	default:
		return ~0; //Error!
	}
}

uint_32 getControlPORTaddress(byte channel)
{
	switch (channel)
	{
	case 0: //First?
		return (PCI_IDE.BAR[1] > 1) ? PCI_IDE.BAR[1] : 0x3F6; //Give the BAR!
		break;
	case 1: //Second?
		return (PCI_IDE.BAR[3] > 1) ? PCI_IDE.BAR[3] : 0x376; //Give the BAR!
		break;
	default:
		return ~0; //Error!
	}
}

word get_cylinders(uint_64 disk_size)
{
	return floor(disk_size / (63 * 16)); //How many cylinders!
}

word get_heads(uint_64 disk_size)
{
	return 16;
}

word get_SPT(uint_64 disk_size)
{
	return 63;
}

byte ATA_activeDrive(byte channel)
{
	return ATA[channel].activedrive; //Give the drive or 0xFF if invalid!
}

byte ATA_resultIN(byte channel)
{
	byte result;
	switch (ATA[channel].command)
	{
	case 0xEC: //Identify?
		result = ATA[channel].result[ATA[channel].resultpos++]; //Read the result byte!
		if (ATA[channel].resultpos == ATA[channel].resultsize) //Fully read?
		{
			ATA[channel].commandstatus = 0; //Reset command!
		}
		return result; //Give the result byte!
		break;
	default: //Unknown?
		break;
	}
	return 0; //Unknown data!
}

void ATA_increasesector(byte channel) //Increase the current sector to the next sector!
{
	++ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address; //Increase the current sector!
}

void ATA_updatesector(byte channel) //Update the current sector!
{
	word cylinder;
	byte head, sector;
	if (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBAMode) //LBA mode?
	{
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBA &= 0xFFFFFFF; //Clear the LBA part!
		ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address &= 0xFFFFFFF; //Truncate the address to it's size!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBA |= ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address; //Set the LBA part only!
	}
	else
	{
		uint_64 disk_size = ((ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[61] << 8) | ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[60]); //The size of the disk in sectors!
		ATA_LBA2CHS(channel,ATA_activeDrive(channel),ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address, &cylinder, &head, &sector); //Convert the current LBA address into a CHS value!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh = ((cylinder >> 8) & 0xFF); //Cylinder high!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow = (cylinder&0xFF); //Cylinder low!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.head = head; //Head!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber = sector; //Sector number!
	}
}

byte ATA_readsector(byte channel) //Read the current sector set up!
{
	uint_64 disk_size = ((ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[61] << 8) | ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[60]); //The size of the disk in sectors!
	if (ATA[channel].commandstatus == 1) //We're reading already?
	{
		if (!--ATA[channel].datasize) //Finished?
		{
			ATA_updatesector(channel); //Update the current sector!
			ATA[channel].commandstatus = 0; //We're back in command mode!
			return 1; //We're finished!
		}
	}
	if (ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address >= disk_size) //Past the end of the disk?
	{
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.idmarknotfound = 1; //Not found!
		ATA_updatesector(channel); //Update the current sector!
		ATA[channel].commandstatus = 0xFF; //Error!
		return 0; //Stop!
	}

	if (readdata(ATA_Drives[channel][ATA_activeDrive(channel)], &ATA[channel].data, (ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address << 9), 0x200)) //Read the data from disk?
	{
		ATA_increasesector(channel); //Increase the current sector!

		ATA[channel].datablock = 0x200; //We're refreshing after this many bytes!
		ATA[channel].datapos = 0; //Initialise our data position!
		ATA[channel].commandstatus = 1; //Transferring data IN!
		return 1; //Process the block!
	}
	else //Error reading?
	{
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.idmarknotfound = 1; //Not found!
		ATA_updatesector(channel); //Update the current sector!
		ATA[channel].commandstatus = 0xFF; //Error!
		return 0; //Stop!
	}
	return 1; //We're finished!
}

byte ATA_writesector(byte channel)
{
	uint_64 disk_size = ((ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[61] << 8) | ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[60]); //The size of the disk in sectors!
	if (ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address >= disk_size) //Past the end of the disk?
	{
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.idmarknotfound = 1; //Not found!
		ATA_updatesector(channel); //Update the current sector!
		ATA[channel].commandstatus = 0xFF; //Error!
		return 1; //We're finished!
	}

	if (writedata(ATA_Drives[channel][ATA_activeDrive(channel)], &ATA[channel].data, (ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address << 9), 0x200)) //Write the data to the disk?
	{
		ATA_increasesector(channel); //Increase the current sector!

		if (!--ATA[channel].datasize) //Finished?
		{
			ATA_updatesector(channel); //Update the current sector!
			ATA[channel].commandstatus = 0; //We're back in command mode!
			return 1; //We're finished!
		}

		//Process next sector!
		ATA[channel].datablock = 0x200; //We're refreshing after this many bytes!
		ATA[channel].datapos = 0; //Initialise our data position!
		ATA[channel].commandstatus = 1; //Transferring data IN!
		return 1; //Process the block!
	}
	else //Write failed?
	{
		if (drivereadonly(ATA_Drives[channel][ATA_activeDrive(channel)])) //R/O drive?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.drivewritefault = 1; //Write fault!
		}
		else
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.uncorrectabledata = 1; //Not found!
		}
		ATA_updatesector(channel); //Update the current sector!
		ATA[channel].commandstatus = 0xFF; //Error!
	}
	return 0; //Finished!
}

byte ATA_dataIN(byte channel) //Byte read from data!
{
	byte result;
	switch (ATA[channel].command) //What command?
	{
	case 0x20:
	case 0x21: //Read sectors?
		result = ATA[channel].data[ATA[channel].datapos++]; //Read the data byte!
		if (ATA[channel].datapos == ATA[channel].datablock) //Full block read?
		{
			if (ATA_readsector(channel)) //Next sector read?
			{
				ATA_IRQ(channel, ATA_activeDrive(channel)); //Give our requesting IRQ!
			}
		}
		break;
	default: //Unknown?
		break;
	}
	return 0; //Unknown data!
}

void ATA_dataOUT(byte channel, byte data) //Byte written to data!
{
	switch (ATA[channel].command) //What command?
	{
	case 0x30: //Write sector(s) (w/retry)?
	case 0x31: //Write sectors (w/o retry)?
		ATA[channel].data[ATA[channel].datapos++] = data; //Write the data byte!
		if (ATA[channel].datapos == ATA[channel].datablock) //Full block read?
		{
			if (ATA_writesector(channel)) //Sector written and to write another sector?
			{
				ATA_IRQ(channel, ATA_activeDrive(channel)); //Give our requesting IRQ!
			}
		}
		break;
	default: //Unknown?
		break;
	}
}

void ATA_executeCommand(byte channel, byte command) //Execute a command!
{
	ATA[channel].longop = 0; //Default: no long operation!
	uint_64 disk_size;
	int drive;
	byte temp;
	switch (command) //What command?
	{
	case 0x90: //Execute drive diagnostic?
		dolog("ATA", "DIAGNOSTICS:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
		ATA[channel].Drive[0].ERRORREGISTER.data = 0x1; //OK!
		ATA[channel].Drive[1].ERRORREGISTER.data = 0x1; //OK!
		ATA[channel].commandstatus = 0; //Reset status!
		ATA_IRQ(channel, ATA_activeDrive(channel)); //IRQ!
		break;
	case 0xDB: //Acnowledge media change?
		dolog("ATA", "ACNMEDIACHANGE:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
		switch (ATA_Drives[channel][ATA_activeDrive(channel)]) //What kind of drive?
		{
		case CDROM0:
		case CDROM1: //CD-ROM?
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.mediachanged = 0; //Not changed anymore!
			ATA[channel].commandstatus = 0; //Reset status!
			break;
		default:
			goto invalidcommand;
		}
		break;
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
	case 0x1F: //Recalibrate?
		dolog("ATA", "RECALIBRATE:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
		temp = (command & 0xF); //???
		if (has_drive(ATA_Drives[channel][ATA_activeDrive(channel)])) //Gotten drive?
		{
			memset(&IRQtimer[(channel << 1) | ATA_activeDrive(channel)], 0, sizeof(IRQtimer[(channel << 1) | ATA_activeDrive(channel)])); //Init timer!
			ATA[channel].commandstatus = 4; //Waiting for completion!
			IRQtimer[(channel << 1) | ATA_activeDrive(channel)].type = 1; //Execute an IRQ and clear the status and set drive seek complete!
			IRQtimer[(channel << 1) | ATA_activeDrive(channel)].ATA_tickstimeout = 1; //100us timeout!
		}
		else
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.track0notfound = 1; //Track 0 couldn't be found!
			ATA[channel].commandstatus = 0xFF; //Error!
		}
		break;
	case 0x70:
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:
	case 0x78:
	case 0x79:
	case 0x7A:
	case 0x7B:
	case 0x7C:
	case 0x7D:
	case 0x7E:
	case 0x7F: //Seek?
		dolog("ATA", "SEEK:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
		temp = (command & 0xF); //The head to select!
		if (((ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh << 8) | ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow) < ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[54]) //Cylinder correct?
		{
			if (temp < ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[55]) //Head within range?
			{
				memset(&IRQtimer[(channel << 1) | ATA_activeDrive(channel)], 0, sizeof(IRQtimer[(channel << 1) | ATA_activeDrive(channel)])); //Init timer!
				ATA[channel].commandstatus = 4; //Waiting for completion!
				IRQtimer[(channel << 1) | ATA_activeDrive(channel)].type = 2; //Execute an IRQ and clear the status and set drive seek complete!
				IRQtimer[(channel << 1) | ATA_activeDrive(channel)].ATA_tickstimeout = 100; //1us timeout!
			}
			else //Head not found!
			{
				ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //Initialise error register!
				ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.idmarknotfound = 1; //Not found!
				ATA[channel].commandstatus = 0xFF; //Error!
				ATA_IRQ(channel, ATA_activeDrive(channel)); //Execute an IRQ!
			}
		}
		else //Cylinder incorrect?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //Initialise error register!
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.idmarknotfound = 1; //Not found!
			ATA_IRQ(channel, ATA_activeDrive(channel)); //Execute an IRQ!
		}
		break;
	case 0x22: //Read long (w/retry)?
	case 0x23: //Read long (w/o retry)?
		ATA[channel].longop = 1; //Long operation!
	case 0x20: //Read sector(s) (w/retry)?
	case 0x21: //Read sector(s) (w/o retry)?
		dolog("ATA", "READ(long):%i,%i=%02X", channel, ATA_activeDrive(channel), command);
		ATA[channel].datasize = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount; //Load sector count!
		disk_size = ((ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[61] << 8) | ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[60]); //The size of the disk in sectors!
		if (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBAMode) //Are we in LBA mode?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address = (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBA & 0xFFFFFFF); //The LBA address!
		}
		else //Normal CHS address?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address = ATA_CHS2LBA(channel,ATA_activeDrive(channel),
				((ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh << 8) | (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow)),
				ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.head,
				ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber); //The LBA address based on the CHS address!

		}
		if (ATA_readsector(channel)) //OK?
		{
			ATA_IRQ(channel, ATA_activeDrive(channel)); //Give our requesting IRQ!
		}
		break;
	case 0x32: //Write long (w/retry)?
	case 0x33: //Write long (w/o retry)?
		ATA[channel].longop = 1; //Long operation!
	case 0x30: //Write sector(s) (w/retry)?
	case 0x31: //Write sectors (w/o retry)?
		dolog("ATA", "WRITE(LONG):%i,%i=%02X", channel, ATA_activeDrive(channel), command);
		ATA[channel].datasize = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount; //Load sector count!
		disk_size = ((ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[61] << 8) | ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[60]); //The size of the disk in sectors!
		if (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBAMode) //Are we in LBA mode?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address = (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBA & 0xFFFFFFF); //The LBA address!
		}
		else //Normal CHS address?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address = ATA_CHS2LBA(
				((ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh << 8) | (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow)),
				ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.head,
				ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber,
				ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[55],
				(uint_64)disk_size); //The LBA address based on the CHS address!

		}
		ATA_IRQ(channel, ATA_activeDrive(channel)); //Give our requesting IRQ!
		ATA[channel].commandstatus = 2; //Transferring data OUT!
		break;
	case 0x91: //Initialise device parameters?
		dolog("ATA", "INITDRVPARAMS:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
		ATA[channel].commandstatus = 0; //Requesting command again!
		ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[55] = (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.head + 1); //Set the current maximum head!
		ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[56] = (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount); //Set the current sectors per track!
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //No errors!
		break;
	case 0xEC: //Identify drive?
		dolog("ATA", "IDENTIFY:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
		ATA[channel].command = 0xEC; //We're running this command!
		memcpy(&ATA[channel].result, &ATA[channel].Drive[ATA_activeDrive(channel)].driveparams, sizeof(ATA[channel].Drive[ATA_activeDrive(channel)].driveparams)); //Set drive parameters currently set!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.data = 0; //Clear any errors!
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //No errors!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow = 0; //Needs to be 0 to detect!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh = 0; //Needs to be 0 to detect!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 0; //Not an error!
		//Finish up!
		ATA[channel].resultpos = 0; //Initialise data position for the result!
		ATA[channel].resultsize = sizeof(ATA[channel].Drive[ATA_activeDrive(channel)].driveparams); //512 byte result!
		ATA[channel].commandstatus = 3; //We're requesting data to be read!
		ATA_IRQ(channel, ATA_activeDrive(channel)); //Execute an IRQ from us!
		break;
	case 0xDA: //Get media status?
		dolog("ATA", "GETMEDIASTATUS:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
		if (ATA_Drives[channel][ATA_activeDrive(channel)] >= CDROM0) //CD-ROM drive?
		{
			drive = ATA_Drives[channel][ATA_activeDrive(channel)]; //Load the drive identifier!
			if (has_drive(drive)) //Drive inserted?
			{
				ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.uncorrectabledata = drivereadonly(drive); //Are we read-only!
			}
			else
			{
				ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.uncorrectabledata = 0; //Are we read-only!
			}
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.mediachanged = 0; //Not anymore!
			ATA_IRQ(channel, ATA_activeDrive(channel)); //Raise IRQ!
			ATA[channel].commandstatus = 0; //Reset status!
		}
		else goto invalidcommand;
		break;
	case 0xDC: //BIOS - post-boot?
	case 0xDD: //BIOS - pre-boot?
	case 0x50: //Format track?
	case 0x97:
	case 0xE3: //Idle?
	case 0x95:
	case 0xE1: //Idle immediate?
	case 0x00: //NOP?
	case 0xE4: //Read buffer?
	case 0xC8: //Read DMA (w/retry)?
	case 0xC9: //Read DMA (w/o retry)?
	case 0xC4: //Read multiple?
	case 0x40: //Read verify sector(s) (w/retry)?
	case 0x41: //Read verify sector(s) (w/o retry)?
	case 0xEF: //Set features?
	case 0xC6: //Set multiple mode?
	case 0x99:
	case 0xE6: //Sleep?
	case 0x96:
	case 0xE2: //Standby?
	case 0x94:
	case 0xE0: //Standby immediate?
	case 0xE8: //Write buffer?
	case 0xCA: //Write DMA (w/retry)?
	case 0xCB: //Write DMA (w/o retry)?
	case 0xC5: //Write multiple?
	case 0xE9: //Write same?
	case 0x3C: //Write verify?
	default: //Unknown command?
		//Invalid command?
		invalidcommand:
		dolog("ATA", "Invalid command,%i,%i: %02X",channel,ATA_activeDrive(channel), command);
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 1; //Reset error register!
		ATA[channel].commandstatus = 0xFF; //Move to error mode!
		break;
	}
}

void ATA_updateStatus(byte channel)
{
	switch (ATA[channel].commandstatus) //What command status?
	{
	case 0: //Ready for command?
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.busy = 0; //Not busy! You can write to the CBRs!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveready = 1; //We're ready to process a command!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.datarequestready = 0; //Not requesting data to transfer!
		break;
	case 1: //Transferring data IN?
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.busy = 0; //Not busy! You can write to the CBRs!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveready = 1; //We're ready to process a command!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.datarequestready = 1; //We're requesting data to transfer!
		break;
	case 2: //Transferring data OUT?
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.busy = 0; //Not busy! You can write to the CBRs!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveready = 1; //We're ready to process a command!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.datarequestready = 1; //We're requesting data to transfer!
		break;
	case 3: //Transferring result?
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.busy = 0; //Not busy! You can write to the CBRs!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveready = 1; //We're ready to process a command!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.datarequestready = 1; //We're requesting data to transfer!
		break;
	case 4: //Busy waiting?
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.busy = 1; //Busy! You can write to the CBRs!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveready = 0; //We're ready to process a command!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.datarequestready = 0; //We're requesting data to transfer!
		break;
	default: //Unknown?
	case 0xFF: //Error?
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.busy = 0; //Not busy! You can write to the CBRs!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveready = 1; //We're ready to process a command!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.datarequestready = 0; //We're not requesting data to transfer!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 1; //Error!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.correcteddata = 0; //Not corrected data!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.index = 0;
		break;
	}
}

void ATA_writedata(byte channel, byte value)
{
	if (!ATA_Drives[channel][ATA_activeDrive(channel)]) //Invalid drive?
	{
		return; //OK!
	}
	switch (ATA[channel].commandstatus) //Current status?
	{
	case 2: //DATA OUT?
		ATA_dataOUT(channel,value); //Read data!
		break;
	default: //Unknown status?
		break;
	}
}

byte outATA16(word port, word value)
{
	byte channel = 0; //What channel?
	if (port != getPORTaddress(channel)) //Primary channel?
	{
		channel = 1; //Try secondary channel!
		if (port != getPORTaddress(channel)) //Secondary channel?
		{
			return 0; //Not our port?
		}
	}
	ATA_writedata(channel, (value&0xFF)); //Write the data low!
	ATA_writedata(channel, ((value >> 8) & 0xFF)); //Write the data high!
	return 1;
}

byte outATA8(word port, byte value)
{
	byte channel = 0; //What channel?
	if ((port<getPORTaddress(channel)) || (port>getPORTaddress(channel)+0xD)) //Primary channel?
	{
		if ((port >= getControlPORTaddress(channel)) && (port <= getControlPORTaddress(channel)+1)) goto port3_write;
		channel = 1; //Try secondary channel!
		if ((port<getPORTaddress(channel)) || (port>getPORTaddress(channel) + 0xD)) //Secondary channel?
		{
			if ((port >= getControlPORTaddress(channel)) && (port <= getControlPORTaddress(channel) + 1)) goto port3_write;
			return 0; //Not our port?
		}
	}
	port -= getPORTaddress(channel); //Get the port from the base!
	switch (port) //What port?
	{
	case 0: //DATA?
		ATA_writedata(channel, value); //Write the data!
		return 1;
		break;
	case 1: //Features?
		//ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.features = value; //Use the set data! Ignore!
		return 1; //OK!
		break;
	case 2: //Sector count?
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount = value; //Set sector count!
		return 1; //OK!
		break;
	case 3: //Sector number?
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber = value; //Set sector number!
		return 1; //OK!
		break;
	case 4: //Cylinder low?
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow = value; //Set cylinder low!
		return 1; //OK!
		break;
	case 5: //Cylinder high?
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh = value; //Set cylinder high!
		return 1; //OK!
		break;
	case 6: //Drive/head?
		ATA[channel].activedrive = (value >> 4) & 1; //The active drive!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.drivehead = value; //Set drive head!
		return 1; //OK!
		break;
	case 7: //Command?
		ATA_executeCommand(channel,value); //Execute a command!
		return 1; //OK!
		break;
	default: //Unsupported!
		break;
	}
	return 0; //Safety!
port3_write: //Special port #3?
	port -= getControlPORTaddress(channel); //Get the port from the base!
	if (!ATA_Drives[channel][ATA_activeDrive(channel)]) //Invalid drive?
	{
		return 1; //OK!
	}
	switch (port) //What port?
	{
	case 0: //Control register?
		ATA[channel].DriveControlRegister.data = value; //Give the drive control register!
		return 1; //OK!
		break;
	default: //Unsupported!
		break;
	}
	return 0; //Unsupported!
}

void ATA_readdata(byte channel, byte *result)
{
	if (!ATA_Drives[channel][ATA_activeDrive(channel)])
	{
		*result = 0; //No result!
		return; //Abort!
	}
	switch (ATA[channel].commandstatus) //Current status?
	{
	case 1: //DATA IN?
		*result = ATA_dataIN(channel); //Read data!
		break;
	case 3: //Result IN?
		*result = ATA_resultIN(channel); //Read result!
		break;
	default: //Unknown status?
		*result = 0; //Unsupported for now!
		break;
	}
}

byte inATA16(word port, word *result)
{
	byte channel = 0; //What channel?
	if (port!=getPORTaddress(channel)) //Primary channel?
	{
		channel = 1; //Try secondary channel!
		if (port!=getPORTaddress(channel)) //Secondary channel?
		{
			return 0; //Not our port?
		}
	}
	byte buffer;
	word resultbuffer;
	ATA_readdata(channel, &buffer); //Read the low data!
	resultbuffer = buffer; //Load the low byte!
	ATA_readdata(channel, &buffer); //Read the high data!
	resultbuffer |= (buffer << 8); //Load the high byte!
	*result = resultbuffer; //Set the result!
	return 1;
}

byte inATA8(word port, byte *result)
{
	byte channel = 0; //What channel?
	if ((port<getPORTaddress(channel)) || (port>getPORTaddress(channel) + 0xD)) //Primary channel?
	{
		if ((port >= getControlPORTaddress(channel)) && (port <= getControlPORTaddress(channel) + 1)) goto port3_read;
		channel = 1; //Try secondary channel!
		if ((port<getPORTaddress(channel)) || (port>getPORTaddress(channel) + 0xD)) //Secondary channel?
		{
			if ((port >= getControlPORTaddress(channel)) && (port <= getControlPORTaddress(channel) + 1)) goto port3_read;
			return 0; //Not our port?
		}
	}
	if (!ATA_Drives[channel][ATA_activeDrive(channel)]) //Invalid drive?
	{
		*result = 0; //Give 0: we're not present!
		return 1; //OK!
	}
	port -= getPORTaddress(channel); //Get the port from the base!
	switch (port) //What port?
	{
	case 0: //DATA?
		ATA_readdata(channel, result); //Read the data!
		return 1;
		break;
	case 1: //Error register?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data; //Error register!
		return 1;
		break;
	case 2: //Sector count?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount; //Get sector count!
		return 1;
		break;
	case 3: //Sector number?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber; //Get sector number!
		return 1; //OK!
		break;
	case 4: //Cylinder low?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow; //Get cylinder low!
		return 1; //OK!
		break;
	case 5: //Cylinder high?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh; //Get cylinder high!
		return 1; //OK!
		break;
	case 6: //Drive/head?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.drivehead; //Get drive/head!
		return 1; //OK!
		break;
	case 7: //Status?
		ATA_updateStatus(channel); //Update the status register if needed!
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.data; //Get status!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.drivewritefault = 0; //Reset write fault flag!
		return 1; //OK!
		break;
	default: //Unsupported?
		break;
	}
	return 0; //Unsupported!
port3_read: //Special port #3?
	port -= getControlPORTaddress(channel); //Get the port from the base!
	if (!ATA_Drives[channel][ATA_activeDrive(channel)]) //Invalid drive?
	{
		*result = 0; //Give 0: we're not present!
		return 1; //OK!
	}
	switch (port) //What port?
	{
	case 0: //Alternate status register?
		ATA_updateStatus(channel); //Update the status register if needed!
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.data; //Get status!
		return 1; //OK!
		break;
	case 1: //Drive address register?
		*result = ATA[channel].DriveAddressRegister.data; //Give the data!
		return 1; //OK!
		break;
	default: //Unsupported!
		break;
	}
	return 0; //Unsupported!
}

byte CDROM_DiskChanged = 0;

void ATA_DiskChanged(int disk)
{
	byte disk_ATA, disk_channel, disk_nr;
	switch (disk) //What disk?
	{
	//Four disk numbers!
	case HDD0:
		disk_nr = 0;
		break;
	case HDD1:
		disk_nr = 1;
		break;
	case CDROM0:
		disk_nr = 2;
		break;
	case CDROM1:
		disk_nr = 3;
		break;
	default: //Unsupported?
		return; //Abort!
	}
	disk_channel = ATA_DrivesReverse[disk_nr][0]; //The channel of the disk!
	disk_ATA = ATA_DrivesReverse[disk_nr][1]; //The master/slave of the disk!
	if ((disk_nr >= 2) && CDROM_DiskChanged) //CDROM changed?
	{
		ATA[disk_channel].Drive[disk_ATA].ERRORREGISTER.mediachanged = 1; //We've changed media!
	}
	if ((disk_channel == 0xFF) || (disk_ATA == 0xFF)) return; //Not mounted!
	uint_64 disk_size;
	switch (disk)
	{
	case HDD0: //HDD0 changed?
	case HDD1: //HDD1 changed?
	case CDROM0: //CDROM0 changed?
	case CDROM1: //CDROM1 changed?
		if (has_drive(disk)) //Do we even have this drive?
		{
			disk_size = disksize(disk); //Get the disk's size!
			disk_size >>= 9; //Get the disk size in sectors!
			ATA[disk_channel].Drive[disk_ATA].driveparams[0] = ((disk==CDROM0)||(disk==CDROM1))?0x80:0x40; //Hard sectored, Fixed drive/CDROM drive!
			ATA[disk_channel].Drive[disk_ATA].driveparams[1] = ATA[disk_channel].Drive[disk_ATA].driveparams[54] = get_cylinders(disk_size); //1=Number of cylinders
			ATA[disk_channel].Drive[disk_ATA].driveparams[2] = ATA[disk_channel].Drive[disk_ATA].driveparams[55] = get_heads(disk_size); //3=Number of heads
			ATA[disk_channel].Drive[disk_ATA].driveparams[6] = ATA[disk_channel].Drive[disk_ATA].driveparams[56] = get_SPT(disk_size); //6=Sectors per track
			ATA[disk_channel].Drive[disk_ATA].driveparams[20] = 1; //One transfer at a time!
			ATA[disk_channel].Drive[disk_ATA].driveparams[21] = 1; //512 byte buffer!
			ATA[disk_channel].Drive[disk_ATA].driveparams[49] = 0x200; //LBA supported, DMA unsupported!
			ATA[disk_channel].Drive[disk_ATA].driveparams[53] = 1; //Using soft-sectoring!
			ATA[disk_channel].Drive[disk_ATA].driveparams[60] = (disk_size & 0xFFFF); //Number of addressable sectors, low word!
			ATA[disk_channel].Drive[disk_ATA].driveparams[61] = (disk_size >> 16); //Number of addressable sectors, high word!
		}
		else //Drive not inserted?
		{
			memset(ATA[disk_channel].Drive[disk_ATA].driveparams, 0, sizeof(ATA[disk_channel].Drive[disk_ATA].driveparams)); //Clear the information on the drive: it's non-existant!
		}
		break;
	default: //Unknown?
		break;
	}
}

void initATA()
{
	initTicksHolder(&ATATicks);
	memset(&ATA, 0, sizeof(ATA)); //Initialise our data!
	memset(&IRQtimer, 0, sizeof(IRQtimer)); //Init timers!
	//We don't register a disk change handler, because ATA doesn't change disks when running!
	//8-bits ports!
	register_PORTIN(&inATA8);
	register_PORTOUT(&outATA8);
	//16-bits port!
	register_PORTINW(&inATA16);
	register_PORTOUTW(&outATA16);

	//We don't implement DMA: this is done by our own DMA controller!
	//First, detect HDDs!
	memset(ATA_Drives, 0, sizeof(ATA_Drives)); //Init drives to unused!
	memset(ATA_DrivesReverse, 0, sizeof(ATA_DrivesReverse)); //Init reverse drives to unused!
	byte CDROM_channel = 1; //CDROM is the second channel by default!
	if (has_drive(HDD0)) //Have HDD0?
	{
		ATA_Drives[0][0] = HDD0; //Mount HDD0!
		if (has_drive(HDD1)) //Have HDD1?
		{
			ATA_Drives[0][1] = HDD1; //Mount HDD1!
		}
	}
	else if (has_drive(HDD1)) //Have HDD1?
	{
		ATA_Drives[0][0] = HDD1; //Mount HDD1!
	}
	else
	{
		CDROM_channel = 0; //Move CDROM to primary channel!
	}
	ATA_Drives[CDROM_channel][0] = CDROM0; //CDROM0 always present as master!
	ATA_Drives[CDROM_channel][1] = CDROM1; //CDROM1 always present as master!
	int i,j,k;
	int disk_reverse[4] = { HDD0,HDD1,CDROM0,CDROM1 }; //Our reverse lookup information values!
	for (i = 0;i < 4;i++) //Check all drives mounted!
	{
		ATA_DrivesReverse[i][0] = 0xFF; //Unassigned!
		ATA_DrivesReverse[i][1] = 0xFF; //Unassigned!
		for (j = 0;j < 2;j++)
		{
			for (k = 0;k < 2;k++)
			{
				if (ATA_Drives[j][k] == disk_reverse[i]) //Found?
				{
					ATA_DrivesReverse[i][0] = j;
					ATA_DrivesReverse[i][1] = k; //Load reverse lookup!
				}
			}
		}
	}
	//Now, apply the basic disk information (disk change/initialisation of parameters)!
	register_DISKCHANGE(HDD0, &ATA_DiskChanged);
	register_DISKCHANGE(HDD1, &ATA_DiskChanged);
	register_DISKCHANGE(CDROM0, &ATA_DiskChanged);
	register_DISKCHANGE(CDROM1, &ATA_DiskChanged);
	CDROM_DiskChanged = 0; //Init!
	ATA_DiskChanged(HDD0); //Init HDD0!
	ATA_DiskChanged(HDD1); //Init HDD1!
	ATA_DiskChanged(CDROM0); //Init HDD0!
	ATA_DiskChanged(CDROM1); //Init HDD1!
	CDROM_DiskChanged = 1; //We're changing when updating!
	memset(&PCI_IDE, 0, sizeof(PCI_IDE)); //Initialise to 0!
	register_PCI(&PCI_IDE, sizeof(PCI_IDE)); //Register the PCI data area!
	//Initialise our data area!
	PCI_IDE.DeviceID = 1;
	PCI_IDE.VendorID = 1; //DEVICEID::VENDORID: We're a ATA device!
	PCI_IDE.ProgIF = 0x80; //We use our own set interrupts and we're a parallel ATA controller!

#ifdef WD_ATA
	PCI_IDE.BAR[0] = 0x300; //Single controller only!
#endif
}