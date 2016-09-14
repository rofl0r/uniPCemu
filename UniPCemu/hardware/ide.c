//ATA-1 harddisk emulation

#include "headers/types.h" //Basic types!
#include "headers/basicio/io.h" //I/O support!
#include "headers/hardware/ports.h" //I/O port support!
#include "headers/hardware/pci.h" //PCI support!
#include "headers/hardware/pic.h" //PIC support!
#include "headers/support/log.h" //Logging support for debugging!

//#define ATA_LOG

//Hard disk IRQ!
#define ATA_PRIMARYIRQ 14
#define ATA_SECONDARYIRQ 15

extern byte singlestep; //Enable single stepping when called?

PCI_CONFIG PCI_IDE;

struct
{
	byte longop; //Long operation instead of a normal one?
	uint_32 datapos; //Data position?
	uint_32 datablock; //How large is a data block to be refreshed?
	uint_32 datasize; //Data size?
	byte data[4096]; //Full sector data!
	byte command;
	byte commandstatus; //Do we have a command?
	struct
	{
		byte ATAPI_processingPACKET; //Are we processing a packet or data for the ATAPI device?
		byte ATAPI_PACKET[12]; //Full ATAPI packet!
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
		byte Enable8BitTransfers; //Enable 8-bit transfers?
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

OPTINLINE byte ATA_activeDrive(byte channel)
{
	return ATA[channel].activedrive; //Give the drive or 0xFF if invalid!
}

OPTINLINE uint_32 read_LBA(byte channel)
{
	return (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBA & 0xFFFFFFF); //Give the LBA register contents!
}

OPTINLINE uint_32 ATA_CHS2LBA(byte channel, byte slave, word cylinder, byte head, byte sector)
{
	return ((cylinder*ATA[channel].Drive[slave].driveparams[55]) + head)*ATA[channel].Drive[slave].driveparams[56] + sector - 1; //Give the LBA value!
}

OPTINLINE void ATA_LBA2CHS(byte channel, byte slave, uint_32 LBA, word *cylinder, byte *head, byte *sector)
{
	uint_32 temp;
	temp = (ATA[channel].Drive[slave].driveparams[55] * ATA[channel].Drive[slave].driveparams[56]); //Sectors per cylinder!
	*cylinder = (word)(LBA / temp); //Cylinder!
	LBA -= *cylinder*temp; //Decrease LBA to get heads&sectors!
	temp = ATA[channel].Drive[slave].driveparams[56]; //SPT!
	*head = (LBA / temp) & 0xF; //The head!
	LBA -= *head*temp; //Decrease LBA to get sectors!
	*sector = ((LBA+1) & 0xFF); //The sector!
}

int ATA_Drives[2][2]; //All ATA mounted drives to disk conversion!
byte ATA_DrivesReverse[4][2]; //All Drive to ATA mounted drives conversion!

OPTINLINE void ATA_IRQ(byte channel, byte slave)
{
	if ((!ATA[channel].DriveControlRegister.nIEN) && (!ATA[channel].DriveControlRegister.SRST) && (ATA_activeDrive(channel)==slave)) //Allow interrupts?
	{
		switch (channel)
		{
		case 0: //Primary channel?
			raiseirq(ATA_PRIMARYIRQ); //Execute the IRQ!
			break;
		case 1:
			raiseirq(ATA_SECONDARYIRQ); //Execute the IRQ!
			break;
		default: //Unknown channel?
			break;
		}
	}
}

OPTINLINE void ATA_removeIRQ(byte channel, byte slave)
{
	if ((!ATA[channel].DriveControlRegister.nIEN) && (!ATA[channel].DriveControlRegister.SRST) && (ATA_activeDrive(channel) == slave)) //Allow interrupts?
	{
		switch (channel)
		{
		case 0: //Primary channel?
			lowerirq(ATA_PRIMARYIRQ); //Execute the IRQ!
			break;
		case 1:
			lowerirq(ATA_SECONDARYIRQ); //Execute the IRQ!
			break;
		default: //Unknown channel?
			break;
		}
	}
}

struct
{
	double ATA_tickstiming;
	double ATA_tickstimeout; //How big a timeout are we talking about?
	byte type; //Type of timer!
} IRQtimer[4]; //IRQ timer!

void cleanATA()
{
}

void updateATA(double timepassed) //ATA timing!
{
	/*return; //Don't handle any timers, since we're not used atm!
	if (timepassed) //Anything passed?
	{
		int i;
		for (i = 0;i < 4;i++) //Process all timers!
		{
			if (IRQtimer[i].ATA_tickstimeout) //Ticking?
			{
				IRQtimer[i].ATA_tickstiming += timepassed; //We've passed some!
				if (IRQtimer[i].ATA_tickstiming >= IRQtimer[i].ATA_tickstimeout) //Expired?
				{
					IRQtimer[i].ATA_tickstimeout = 0; //Finished!
					ATA_IRQ(i & 2, i & 1); //Do an IRQ from the source!
					byte drive = (i & 1); //Drive!
					byte channel = ((i & 2) >> 1); //Channel!
					switch (ATA[channel].commandstatus)
					{
					case 3: //Waiting for completion of a command?
						switch (ATA[channel].command) //What command are we executing?
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
						case 0x1F: //Recalibrate?
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
							break;
						} //Unused atm!
					}
				}
			}
		}
	}*/ //Not used atm!
}

OPTINLINE uint_32 getPORTaddress(byte channel)
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

OPTINLINE uint_32 getControlPORTaddress(byte channel)
{
	switch (channel)
	{
	case 0: //First?
		return (PCI_IDE.BAR[1] > 1) ? PCI_IDE.BAR[1]+2 : 0x3F6; //Give the BAR!
		break;
	case 1: //Second?
		return (PCI_IDE.BAR[3] > 1) ? PCI_IDE.BAR[3]+2 : 0x376; //Give the BAR!
		break;
	default:
		return ~0; //Error!
	}
}

void ATA_updateCapacity(byte channel, byte slave)
{
	uint_32 sectors;
	sectors = ATA[channel].Drive[slave].driveparams[54]; //Current cylinders!
	sectors *= ATA[channel].Drive[slave].driveparams[55]; //Current heads!
	sectors *= ATA[channel].Drive[slave].driveparams[56]; //Current sectors per track!
	ATA[channel].Drive[slave].driveparams[57] = (word)(sectors&0xFFFF);
	sectors >>= 16;
	ATA[channel].Drive[slave].driveparams[58] = (word)(sectors&0xFFFF);
}

word get_cylinders(uint_64 disk_size)
{
	uint_32 cylinders=0;
	cylinders = (uint_32)(disk_size / (63 * 16)); //How many cylinders!
	return (cylinders>=0x3FFF)?0x3FFF:cylinders; //Give the maximum amount of cylinders allowed!
}

OPTINLINE word get_heads(uint_64 disk_size)
{
	return 16;
}

OPTINLINE word get_SPT(uint_64 disk_size)
{
	return 63;
}

OPTINLINE void ATA_increasesector(byte channel) //Increase the current sector to the next sector!
{
	++ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address; //Increase the current sector!
}

OPTINLINE void ATA_updatesector(byte channel) //Update the current sector!
{
	word cylinder;
	byte head, sector;
	if (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBAMode) //LBA mode?
	{
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBA &= ~0xFFFFFFF; //Clear the LBA part!
		ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address &= 0xFFFFFFF; //Truncate the address to it's size!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBA |= ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address; //Set the LBA part only!
	}
	else
	{
		ATA_LBA2CHS(channel,ATA_activeDrive(channel),ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address, &cylinder, &head, &sector); //Convert the current LBA address into a CHS value!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh = ((cylinder >> 8) & 0xFF); //Cylinder high!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow = (cylinder&0xFF); //Cylinder low!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.head = head; //Head!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber = sector; //Sector number!
	}
}

OPTINLINE byte ATA_readsector(byte channel, byte command) //Read the current sector set up!
{
	uint_32 disk_size = ((ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[61] << 16) | ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[60]); //The size of the disk in sectors!
	if (ATA[channel].commandstatus == 1) //We're reading already?
	{
		if (!--ATA[channel].datasize) //Finished?
		{
			ATA_updatesector(channel); //Update the current sector!
			ATA[channel].commandstatus = 0; //We're back in command mode!
			EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 0); //We're not reading anymore!
			return 1; //We're finished!
		}
	}
	if (ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address > disk_size) //Past the end of the disk?
	{
#ifdef ATA_LOG
		dolog("ATA", "Read Sector out of range:%i,%i=%08X/%08X!", channel, ATA_activeDrive(channel), ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address, disk_size);
#endif
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.idmarknotfound = 1; //Not found!
		ATA_updatesector(channel); //Update the current sector!
		ATA[channel].commandstatus = 0xFF; //Error!
		EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 0); //We're not reading anymore!
		return 0; //Stop!
	}

	if (readdata(ATA_Drives[channel][ATA_activeDrive(channel)], &ATA[channel].data, ((uint_64)ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address << 9), 0x200)) //Read the data from disk?
	{
		EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 1); //We're reading!
		ATA_increasesector(channel); //Increase the current sector!

		ATA[channel].datablock = 0x200; //We're refreshing after this many bytes!
		ATA[channel].datapos = 0; //Initialise our data position!
		ATA[channel].commandstatus = 1; //Transferring data IN!
		ATA[channel].command = command; //Set the command to use when reading!
		return 1; //Process the block!
	}
	else //Error reading?
	{
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.idmarknotfound = 1; //Not found!
		ATA_updatesector(channel); //Update the current sector!
		ATA[channel].commandstatus = 0xFF; //Error!
		EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 0); //We're doing nothing!
		return 0; //Stop!
	}
	return 1; //We're finished!
}

OPTINLINE byte ATA_writesector(byte channel)
{
	uint_32 disk_size = ((ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[61] << 16) | ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[60]); //The size of the disk in sectors!
	if (ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address > disk_size) //Past the end of the disk?
	{
#ifdef ATA_LOG
		dolog("ATA", "Write Sector out of range:%i,%i=%08X/%08X!",channel,ATA_activeDrive(channel), ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address,disk_size);
#endif
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.idmarknotfound = 1; //Not found!
		ATA_updatesector(channel); //Update the current sector!
		ATA[channel].commandstatus = 0xFF; //Error!
		EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 0); //We're doing nothing!
		return 1; //We're finished!
	}

#ifdef ATA_LOG
	dolog("ATA", "Writing sector #%i!", ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address); //Log the sector we're writing to!
#endif
	EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 2); //We're writing!
	if (writedata(ATA_Drives[channel][ATA_activeDrive(channel)], &ATA[channel].data, ((uint_64)ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address << 9), 0x200)) //Write the data to the disk?
	{
		ATA_increasesector(channel); //Increase the current sector!

		if (!--ATA[channel].datasize) //Finished?
		{
			ATA_updatesector(channel); //Update the current sector!
			ATA[channel].commandstatus = 0; //We're back in command mode!
#ifdef ATA_LOG
			dolog("ATA", "All sectors to be written written! Ready.");
#endif
			EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 0); //We're doing nothing!
			return 1; //We're finished!
		}

#ifdef ATA_LOG
		dolog("ATA", "Process next sector...");
#endif
		//Process next sector!
		ATA[channel].datablock = 0x200; //We're refreshing after this many bytes!
		ATA[channel].datapos = 0; //Initialise our data position!
		ATA[channel].commandstatus = 2; //Transferring data OUT!
		return 1; //Process the block!
	}
	else //Write failed?
	{
#ifdef ATA_LOG
		dolog("ATA", "Write failed!"); //Log the sector we're writing to!
#endif
		if (drivereadonly(ATA_Drives[channel][ATA_activeDrive(channel)])) //R/O drive?
		{
#ifdef ATA_LOG
			dolog("ATA", "Because the drive is readonly!"); //Log the sector we're writing to!
#endif
			ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.drivewritefault = 1; //Write fault!
		}
		else
		{
#ifdef ATA_LOG
			dolog("ATA", "Because there was an error with the mounted disk image itself!"); //Log the sector we're writing to!
#endif
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.uncorrectabledata = 1; //Not found!
		}
		ATA_updatesector(channel); //Update the current sector!
		ATA[channel].commandstatus = 0xFF; //Error!
		EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 0); //We're doing nothing!
	}
	return 0; //Finished!
}

OPTINLINE byte ATAPI_readsector(byte channel) //Read the current sector set up!
{
	uint_32 disk_size = ((ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[61] << 16) | ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[60]); //The size of the disk in sectors!
	if (ATA[channel].commandstatus == 1) //We're reading already?
	{
		if (!--ATA[channel].datasize) //Finished?
		{
			ATA[channel].commandstatus = 0; //We're back in command mode!
			EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 0); //We're not reading anymore!
			return 1; //We're finished!
		}
	}
	if (ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address > disk_size) //Past the end of the disk?
	{
#ifdef ATA_LOG
		dolog("ATA", "Read Sector out of range:%i,%i=%08X/%08X!", channel, ATA_activeDrive(channel), ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address, disk_size);
#endif
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.idmarknotfound = 1; //Not found!
		ATA[channel].commandstatus = 0xFF; //Error!
		EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 0); //We're not reading anymore!
		return 0; //Stop!
	}

	if (readdata(ATA_Drives[channel][ATA_activeDrive(channel)], &ATA[channel].data, ((uint_64)ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address << 9), 0x800)) //Read the data from disk?
	{
		EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 1); //We're reading!
		ATA_increasesector(channel); //Increase the current sector!

		ATA[channel].datablock = 0x800; //We're refreshing after this many bytes!
		ATA[channel].datapos = 0; //Initialise our data position!
		ATA[channel].commandstatus = 1; //Transferring data IN!
		return 1; //Process the block!
	}
	else //Error reading?
	{
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.idmarknotfound = 1; //Not found!
		ATA[channel].commandstatus = 0xFF; //Error!
		EMU_setDiskBusy(ATA_Drives[channel][ATA_activeDrive(channel)], 0); //We're doing nothing!
		return 0; //Stop!
	}
	return 1; //We're finished!
}

OPTINLINE byte ATA_dataIN(byte channel) //Byte read from data!
{
	byte result;
	switch (ATA[channel].command) //What command?
	{
	case 0x20:
	case 0x21: //Read sectors?
	case 0x22: //Read long (w/retry)?
	case 0x23: //Read long (w/o retry)?
		result = ATA[channel].data[ATA[channel].datapos++]; //Read the data byte!
		if (ATA[channel].datapos == ATA[channel].datablock) //Full block read?
		{
			if (ATA_readsector(channel,ATA[channel].command)) //Next sector read?
			{
				ATA_IRQ(channel, ATA_activeDrive(channel)); //Give our requesting IRQ!
			}
		}
        return result; //Give the result!
		break;
	case 0xA0: //PACKET?
		if (!ATA[channel].Drive[ATA_activeDrive(channel)].ATAPI_processingPACKET) //Sending data?
		{
			switch (ATA[channel].Drive[ATA_activeDrive(channel)].ATAPI_PACKET[0]) //What command?
			{
			case 0x25: //Read capacity?
				result = ATA[channel].data[ATA[channel].datapos++]; //Read the data byte!
				if (ATA[channel].datapos == ATA[channel].datablock) //Full block read?
				{

				}
				break;
			case 0xA8: //Read sector?
				result = ATA[channel].data[ATA[channel].datapos++]; //Read the data byte!
				if (ATA[channel].datapos==ATA[channel].datablock) //Full block read?
				{
					if (ATAPI_readsector(channel)) //Next sector read?
					{
						ATA_IRQ(channel,ATA_activeDrive(channel)); //Raise an IRQ: we're needing attention!
					}
				}
				break;
			default: //Unknown command?
				break;
			}
		}
		break;
	case 0xEC: //Identify?
	case 0xA1: //IDENTIFY PACKET DEVICE?
		result = ATA[channel].data[ATA[channel].datapos++]; //Read the result byte!
		if (ATA[channel].datapos == ATA[channel].datablock) //Fully read?
		{
			ATA[channel].commandstatus = 0; //Reset command!
		}
		return result; //Give the result byte!
	default: //Unknown?
		break;
	}
	return 0; //Unknown data!
}

void ATAPI_executeCommand(byte channel); //Prototype for ATAPI execute Command!
void ATAPI_executeData(byte channel); //Prototype for ATAPI data processing!

OPTINLINE void ATA_dataOUT(byte channel, byte data) //Byte written to data!
{
	switch (ATA[channel].command) //What command?
	{
	case 0x30: //Write sector(s) (w/retry)?
	case 0x31: //Write sectors (w/o retry)?
	case 0x32: //Write long (w/retry)?
	case 0x33: //Write long (w/o retry)?
		ATA[channel].data[ATA[channel].datapos++] = data; //Write the data byte!
		if (ATA[channel].datapos == ATA[channel].datablock) //Full block read?
		{
			if (ATA_writesector(channel)) //Sector written and to write another sector?
			{
				ATA_IRQ(channel, ATA_activeDrive(channel)); //Give our requesting IRQ!
			}
		}
		break;
	case 0xA0: //ATAPI: PACKET!
		if (ATA[channel].Drive[ATA_activeDrive(channel)].ATAPI_processingPACKET) //Are we processing a packet?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].ATAPI_PACKET[ATA[channel].datapos++] = data; //Add the packet byte!
			if (ATA[channel].datapos==12) //Full packet written?
			{
				ATA[channel].Drive[ATA_activeDrive(channel)].ATAPI_processingPACKET = 0; //We're not processing a packet anymore, from now on we're data only!
				ATA_IRQ(channel,ATA_activeDrive(channel)); //We're not DMA, so execute an IRQ: we're ready to process data!
				ATAPI_executeCommand(channel); //Execute the ATAPI command!
			}
		}
		else //We're processing data for an ATAPI packet?
		{
			ATA[channel].data[ATA[channel].datapos++] = data; //Write the data byte!
			if (ATA[channel].datapos == ATA[channel].datablock) //Full block read?
			{
				ATAPI_executeData(channel); //Execute the data process!
			}
		}
		break;
	default: //Unknown?
		break;
	}
}

void ATAPI_executeData(byte channel) //Prototype for ATAPI data processing!
{
	switch (ATA[channel].Drive[ATA_activeDrive(channel)].ATAPI_PACKET[0]) //What command?
	{
	case 0xA8: //Read sectors command!
		ATA[channel].commandstatus = 0; //Reset status: we're done!
		ATA_IRQ(channel,ATA_activeDrive(channel)); //Raise an IRQ: we're done!
		break;
	}	
}

void ATAPI_executeCommand(byte channel) //Prototype for ATAPI execute Command!
{
	byte drive;
	drive = ATA_activeDrive(channel); //The current drive!
	uint_32 disk_size,LBA;
	disk_size = (ATA[channel].Drive[drive].driveparams[61]<<16) | ATA[channel].Drive[drive].driveparams[60]; //Disk size in 512 byte sectors!
	disk_size >>= 2; //We're 4096 byte sectors instead of 512 byte sectors!
	switch (ATA[channel].Drive[drive].ATAPI_PACKET[0]) //What command?
	{
	case 0x00: //TEST UNIT READY(Mandatory)?
	case 0x01: //REZERO UNIT(Mandatory)?
	case 0x03: //REQUEST SENSE(Mandatory)?
	case 0x12: //INQUIRY(Mandatory)?
	case 0x46: //GET CONFIGURATION(Mandatory)?
	case 0x4A: //GET EVENT STATUS NOTIFICATION(Mandatory)?
	case 0x55: //MODE SELECT(Mandatory)?
	case 0x5A: //MODE SENSE(Mandatory)?
		break;
	case 0xA8: //Read sectors command!
		if (!has_drive(ATA_Drives[channel][drive])) goto ATAPI_invalidcommand; //Error out if not present!
		ATA[channel].Drive[ATA_activeDrive(channel)].ATAPI_processingPACKET = 0; //Not processing anymore!
																				 //[9]=Amount of sectors, [2-5]=LBA address, LBA mid/high=2048.
		LBA = (((((ATA[channel].Drive[drive].ATAPI_PACKET[2]<<8) | ATA[channel].Drive[drive].ATAPI_PACKET[3])<<8)| ATA[channel].Drive[drive].ATAPI_PACKET[4]) << 8)| ATA[channel].Drive[drive].ATAPI_PACKET[5]; //The LBA address!
		if (LBA>disk_size) goto ATAPI_invalidcommand; //Error out when invalid sector!
		ATA[channel].datapos = 0; //Start of data!
		ATA[channel].datablock = 0x800; //Size of a sector to transfer!
		ATA[channel].datasize = ATA[channel].Drive[drive].ATAPI_PACKET[9]; //How many sectors to transfer
		if (ATAPI_readsector(channel)) //Sector read?
		{
			ATA_IRQ(channel,ATA_activeDrive(channel)); //Raise an IRQ: we're needing attention!
		}
		break;
	case 0x25: //Read capacity?
		if (!has_drive(ATA_Drives[channel][drive])) goto ATAPI_invalidcommand; //Error out if not present!
		ATA[channel].Drive[ATA_activeDrive(channel)].ATAPI_processingPACKET = 0; //Not processing anymore!
		ATA[channel].datapos = 0; //Start of data!
		ATA[channel].datablock = 8; //Size of a block of information to transfer!
		ATA[channel].data[0] = (disk_size&0xFF);
		ATA[channel].data[1] = ((disk_size>>8) & 0xFF);
		ATA[channel].data[2] = ((disk_size>>16) & 0xFF);
		ATA[channel].data[3] = ((disk_size>>24) & 0xFF);
		ATA[channel].data[4] = 0;
		ATA[channel].data[5] = 8;
		ATA[channel].data[6] = 0;
		ATA[channel].data[7] = 0; //We're 4096 byte sectors!
		ATA_IRQ(channel,ATA_activeDrive(channel)); //Raise an IRQ: we're finished!
		break;
	default:
		dolog("ATAPI","Executing unknown SCSI command: %02X", ATA[channel].Drive[drive].ATAPI_PACKET[0]); //Error: invalid command!
		ATAPI_invalidcommand:
		ATA[channel].Drive[drive].ERRORREGISTER.data = 4; //Reset error register!
		ATA[channel].Drive[drive].STATUSREGISTER.data = 0; //Clear status!
		ATA[channel].Drive[drive].STATUSREGISTER.driveready = 1; //Ready!
		ATA[channel].Drive[drive].STATUSREGISTER.error = 1; //Ready!
		//Reset of the status register is 0!
		ATA[channel].commandstatus = 0xFF; //Move to error mode!
		ATA_IRQ(channel, ATA_activeDrive(channel));
		break;
	}
}

OPTINLINE void giveATAPISignature(byte channel)
{
	ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount = 0x01;
	ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh = 0xEB;
	ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow = 0x14;
	ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber = 0x01;
}

OPTINLINE void ATA_executeCommand(byte channel, byte command) //Execute a command!
{
#ifdef ATA_LOG
	dolog("ATA", "ExecuteCommand: %02X", command); //Execute this command!
#endif
	ATA[channel].longop = 0; //Default: no long operation!
	int drive;
	byte temp;
	uint_32 disk_size; //For checking against boundaries!
	switch (command) //What command?
	{
	case 0x90: //Execute drive diagnostic (Mandatory)?
#ifdef ATA_LOG
		dolog("ATA", "DIAGNOSTICS:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
#endif
		ATA[channel].Drive[0].ERRORREGISTER.data = 0x1; //OK!
		ATA[channel].Drive[1].ERRORREGISTER.data = 0x1; //OK!
		ATA[channel].Drive[0].STATUSREGISTER.error = 0; //Not an error!
		ATA[channel].Drive[1].STATUSREGISTER.error = 0; //Not an error!
		ATA[channel].commandstatus = 0; //Reset status!
		ATA_IRQ(channel, ATA_activeDrive(channel)); //IRQ!
		break;
	case 0xDB: //Acnowledge media change?
#ifdef ATA_LOG
		dolog("ATA", "ACNMEDIACHANGE:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
#endif
		if ((ATA_Drives[channel][ATA_activeDrive(channel)] >= CDROM0)) goto invalidcommand; //Special action for CD-ROM drives?
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
#ifdef ATA_LOG
		dolog("ATA", "RECALIBRATE:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
#endif
		if ((ATA_Drives[channel][ATA_activeDrive(channel)] >= CDROM0)) goto invalidcommand; //Special action for CD-ROM drives?
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //Default to no error!
		if (has_drive(ATA_Drives[channel][ATA_activeDrive(channel)]) && (ATA_Drives[channel][ATA_activeDrive(channel)]>=HDD0) && (ATA_Drives[channel][ATA_activeDrive(channel)]<=HDD1)) //Gotten drive and is a hard disk?
		{
#ifdef ATA_LOG
			dolog("ATA", "Recalibrated!");
#endif
			temp = (ATA[channel].command & 0xF); //???
			ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow = 0; //Clear cylinder #!
			ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveseekcomplete = 1; //We've completed seeking!
			ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 0; //Not an error!
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //No error!
			ATA[channel].commandstatus = 0; //Reset status!
			ATA_IRQ(channel, ATA_activeDrive(channel)); //Raise the IRQ!
		}
		else
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveseekcomplete = 0; //We've not completed seeking!
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //Track 0 couldn't be found!
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
#ifdef ATA_LOG
		dolog("ATA", "SEEK:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
#endif
		if ((ATA_Drives[channel][ATA_activeDrive(channel)] >= CDROM0)) goto invalidcommand; //Special action for CD-ROM drives?
		temp = (command & 0xF); //The head to select!
		if (((ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh << 8) | ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow) < ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[54]) //Cylinder correct?
		{
			if (temp < ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[55]) //Head within range?
			{
#ifdef ATA_LOG
				dolog("ATA", "Seeked!");
#endif
				ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.data = 0; //No error!
				ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveseekcomplete = 1; //We've completed seeking!
				ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 0; //Not an error!
				ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //No error!
				ATA[channel].commandstatus = 0; //Reset status!
				ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.head = (command & 0xF); //Select the following head!
				ATA_IRQ(channel, ATA_activeDrive(channel)); //Raise the IRQ!
			}
			else goto invalidcommand; //Error out!
		}
		else goto invalidcommand; //Error out!
		break;
	case 0x22: //Read long (w/retry, ATAPI Mandatory)?
	case 0x23: //Read long (w/o retry, ATAPI Mandatory)?
		ATA[channel].longop = 1; //Long operation!
	case 0x20: //Read sector(s) (w/retry, ATAPI Mandatory)?
	case 0x21: //Read sector(s) (w/o retry, ATAPI Mandatory)?
#ifdef ATA_LOG
		dolog("ATA", "READ(long:%i):%i,%i=%02X", ATA[channel].longop,channel, ATA_activeDrive(channel), command);
#endif
		if ((ATA_Drives[channel][ATA_activeDrive(channel)] >= CDROM0)) //Special action for CD-ROM drives?
		{
			//Enter reserved ATAPI result!
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 1; //Passed!
			giveATAPISignature(channel);
			goto invalidcommand_noerror; //Execute an invalid command result!
		}
		ATA[channel].datasize = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount; //Load sector count!
		if (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBAMode) //Are we in LBA mode?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address = read_LBA(channel); //The LBA address!
		}
		else //Normal CHS address?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address = ATA_CHS2LBA(channel,ATA_activeDrive(channel),
				((ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh << 8) | (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow)),
				ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.head,
				ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber); //The LBA address based on the CHS address!

		}
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 0; //Not an error!
		if (ATA_readsector(channel,command)) //OK?
		{
			ATA_IRQ(channel, ATA_activeDrive(channel)); //Give our requesting IRQ!
		}
		break;
	case 0x40: //Read verify sector(s) (w/retry)?
	case 0x41: //Read verify sector(s) (w/o retry)?
		if ((ATA_Drives[channel][ATA_activeDrive(channel)] >= CDROM0)) goto invalidcommand; //Special action for CD-ROM drives?
		disk_size = ((ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[61] << 16) | ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[60]); //The size of the disk in sectors!
		ATA[channel].datasize = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount; //Load sector count!
		if (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBAMode) //Are we in LBA mode?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address = read_LBA(channel); //The LBA address!
		}
		else //Normal CHS address?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address = ATA_CHS2LBA(channel, ATA_activeDrive(channel),
				((ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh << 8) | (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow)),
				ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.head,
				ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber); //The LBA address based on the CHS address!

		}
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 0; //Not an error!
		nextverification: //Verify the next sector!
		if (ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address<=disk_size) //OK?
		{
			++ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address; //Next sector!
			if (--ATA[channel].datasize) //Still left?
			{
				goto nextverification; //Verify the next sector!
			}
		}
		else //Out of range?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //Reset error register!
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.idmarknotfound = 1; //Not found!
			ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 1; //Error!
			ATA_updatesector(channel); //Update the current sector!
			ATA[channel].commandstatus = 0xFF; //Error!
		}
		if (!ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error) //Finished OK?
		{
			ATA_IRQ(channel, ATA_activeDrive(channel)); //Raise the OK IRQ!
		}
		break;
	case 0x32: //Write long (w/retry)?
	case 0x33: //Write long (w/o retry)?
		ATA[channel].longop = 1; //Long operation!
	case 0x30: //Write sector(s) (w/retry)?
	case 0x31: //Write sectors (w/o retry)?
		if ((ATA_Drives[channel][ATA_activeDrive(channel)] >= CDROM0)) goto invalidcommand; //Special action for CD-ROM drives?
#ifdef ATA_LOG
		dolog("ATA", "WRITE(LONG:%i):%i,%i=%02X; Length=%02X", ATA[channel].longop, channel, ATA_activeDrive(channel), command, ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount);
#endif
		ATA[channel].datasize = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount; //Load sector count!
		if (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.LBAMode) //Are we in LBA mode?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address = read_LBA(channel); //The LBA address!
		}
		else //Normal CHS address?
		{
			ATA[channel].Drive[ATA_activeDrive(channel)].current_LBA_address = ATA_CHS2LBA(channel, ATA_activeDrive(channel),
				((ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh << 8) | (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow)),
				ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.head,
				ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber); //The LBA address based on the CHS address!

		}
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 0; //Not an error!
		ATA_IRQ(channel, ATA_activeDrive(channel)); //Give our requesting IRQ!
		ATA[channel].commandstatus = 2; //Transferring data OUT!
		ATA[channel].datablock = 0x200; //We're writing 512 bytes to our output at a time!
		ATA[channel].datapos = 0; //Start at the beginning of the sector buffer!
		ATA[channel].command = command; //We're executing this command!
		break;
	case 0x91: //Initialise device parameters?
#ifdef ATA_LOG
		dolog("ATA", "INITDRVPARAMS:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
#endif
		if ((ATA_Drives[channel][ATA_activeDrive(channel)] >= CDROM0)) goto invalidcommand; //Special action for CD-ROM drives?
		ATA[channel].commandstatus = 0; //Requesting command again!
		ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[55] = (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.head + 1); //Set the current maximum head!
		ATA[channel].Drive[ATA_activeDrive(channel)].driveparams[56] = (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount); //Set the current sectors per track!
		ATA_updateCapacity(channel,ATA_activeDrive(channel)); //Update the capacity!
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //No errors!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 0; //Not an error!
		break;
	case 0xA1: //ATAPI: IDENTIFY PACKET DEVICE (ATAPI Mandatory)!
		if ((ATA_Drives[channel][ATA_activeDrive(channel)]>=CDROM0) && ATA_Drives[channel][ATA_activeDrive(channel)]) //CDROM drive?
		{
			ATA[channel].command = 0xA1; //We're running this command!
			goto CDROMIDENTIFY; //Execute CDROM identification!
		}
		goto invalidcommand; //We're an invalid command: we're not a CDROM drive!
	case 0xEC: //Identify device (Mandatory)?
#ifdef ATA_LOG
		dolog("ATA", "IDENTIFY:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
#endif
		if (!ATA_Drives[channel][ATA_activeDrive(channel)]) goto invalidcommand; //No drive errors out!
		if ((ATA_Drives[channel][ATA_activeDrive(channel)] >= CDROM0)) //Special action for CD-ROM drives?
		{
			//Enter reserved ATAPI result!
			ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 1; //Passed!
			giveATAPISignature(channel);
			goto invalidcommand_noerror; //Execute an invalid command result!
		}
		ATA[channel].command = 0xEC; //We're running this command!
		CDROMIDENTIFY:
		memcpy(&ATA[channel].data, &ATA[channel].Drive[ATA_activeDrive(channel)].driveparams, sizeof(ATA[channel].Drive[ATA_activeDrive(channel)].driveparams)); //Set drive parameters currently set!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.data = 0; //Clear any errors!
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //No errors!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow = 0; //Needs to be 0 to detect!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh = 0; //Needs to be 0 to detect!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 0; //Not an error!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveseekcomplete = 1; //We have data now!
		//Finish up!
		ATA[channel].datapos = 0; //Initialise data position for the result!
		ATA[channel].datablock = sizeof(ATA[channel].Drive[ATA_activeDrive(channel)].driveparams); //512 byte result!
		ATA[channel].commandstatus = 1; //We're requesting data to be read!
		ATA_IRQ(channel, ATA_activeDrive(channel)); //Execute an IRQ from us!
		break;
	case 0xA0: //ATAPI: PACKET (ATAPI mandatory)!
		if ((ATA_Drives[channel][ATA_activeDrive(channel)] < CDROM0) || !ATA_Drives[channel][ATA_activeDrive(channel)]) goto invalidcommand; //HDD/invalid disk errors out!
		ATA[channel].command = 0xA0; //We're sending a ATAPI packet!
		ATA[channel].datablock = 12; //We're receiving 12 bytes for the ATAPI packet!
		ATA[channel].datapos = 0; //Initialise data position for the packet!
		ATA[channel].commandstatus = 2; //We're requesting data to be written!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.data = 0; //Clear any errors!
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 0; //No errors!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow = 12; //We're requesting...
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh = 0; //12 bytes of data to be transferred!
		ATA[channel].Drive[ATA_activeDrive(channel)].ATAPI_processingPACKET = 1; //We're processing an ATAPI/SCSI packet!
		ATA_IRQ(channel,ATA_activeDrive(channel)); //Execute an IRQ: we're ready to receive the packet!
		break;
	case 0xDA: //Get media status?
#ifdef ATA_LOG
		dolog("ATA", "GETMEDIASTATUS:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
#endif
		if ((ATA_Drives[channel][ATA_activeDrive(channel)] >= CDROM0)) goto invalidcommand; //Special action for CD-ROM drives?
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
			ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 0; //Not an error!
			ATA_IRQ(channel, ATA_activeDrive(channel)); //Raise IRQ!
			ATA[channel].commandstatus = 0; //Reset status!
		}
		else goto invalidcommand;
		break;
	case 0xEF: //Set features (Mandatory)?
#ifdef ATA_LOG
		dolog("ATA", "Set features:%i,%i=%02X", channel, ATA_activeDrive(channel), ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.features); //Set these features!
#endif
		switch (ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.features) //What features to set?
		{
		case 0x01: //Enable 8-bit data transfers!
			ATA[channel].Drive[ATA_activeDrive(channel)].Enable8BitTransfers = 1; //Enable 8-bit transfers!
			break;
		case 0x81: //Disable 8-bit data transfers!
			ATA[channel].Drive[ATA_activeDrive(channel)].Enable8BitTransfers = 0; //Disable 8-bit transfers!
			break;
		case 0x02: //Enable write cache!
			//OK! Ignore!
			break;
		case 0x82: //Disable write cache!
			//OK! Ignore!
			break;
		default: //Invalid feature!
#ifdef ATA_LOG
			dolog("ATA", "Invalid feature set: %02X", ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.features);
#endif
			goto invalidcommand; //Error out!
			break;
		}
		ATA[channel].commandstatus = 0; //Reset command status!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.data = 0; //Reset data register!
		break;
	case 0x00: //NOP (ATAPI Mandatory)?
		break;
	case 0x08: //DEVICE RESET(ATAPI Mandatory)?
		ATA[channel].commandstatus = 0; //Reset command status!
		ATA[channel].command = 0; //Full reset!
		break;
	case 0xDC: //BIOS - post-boot?
	case 0xDD: //BIOS - pre-boot?
	case 0x50: //Format track?
	case 0x97:
	case 0xE3: //Idle?
	case 0x95:
	case 0xE1: //Idle immediate?
	case 0xE4: //Read buffer?
	case 0xC8: //Read DMA (w/retry)?
	case 0xC9: //Read DMA (w/o retry)?
	case 0xC4: //Read multiple?
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
#ifdef ATA_LOG
		dolog("ATA", "INVALIDCOMMAND:%i,%i=%02X", channel, ATA_activeDrive(channel), command);
#endif
		ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data = 4; //Reset error register!
		invalidcommand_noerror:
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.data = 0; //Clear status!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveready = 1; //Ready!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 1; //Ready!
		//Reset of the status register is 0!
		ATA[channel].commandstatus = 0xFF; //Move to error mode!
		ATA_IRQ(channel, ATA_activeDrive(channel));
		break;
	}
}

OPTINLINE void ATA_updateStatus(byte channel)
{
	switch (ATA[channel].commandstatus) //What command status?
	{
	case 0: //Ready for command?
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveready = 1; //We're ready to process a command!
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
	case 3: //Busy waiting?
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.busy = 1; //Busy! You can write to the CBRs!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.driveready = 0; //We're ready to process a command!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.datarequestready = 0; //We're requesting data to transfer!
		break;
	default: //Unknown?
	case 0xFF: //Error?
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.error = 1; //Error!
		ATA[channel].commandstatus = 0; //Reset command status: we've reset!
		break;
	}
}

OPTINLINE void ATA_writedata(byte channel, byte value)
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
	if (ATA[channel].Drive[ATA_activeDrive(channel)].Enable8BitTransfers) return 0; //We're only 8-bit data transfers!
	ATA_writedata(channel, (value&0xFF)); //Write the data low!
	ATA_writedata(channel, ((value >> 8) & 0xFF)); //Write the data high!
	return 1;
}

byte outATA8(word port, byte value)
{
	byte channel = 0; //What channel?
	if ((port<getPORTaddress(channel)) || (port>(getPORTaddress(channel) + 0x7))) //Primary channel?
	{
		if (port == (getControlPORTaddress(channel))) goto port3_write;
		channel = 1; //Try secondary channel!
		if ((port<getPORTaddress(channel)) || (port>(getPORTaddress(channel) + 0x7))) //Secondary channel?
		{
			if (port == (getControlPORTaddress(channel))) goto port3_write;
			return 0; //Not our port?
		}
	}
	port -= getPORTaddress(channel); //Get the port from the base!
	switch (port) //What port?
	{
	case 0: //DATA?
		if (ATA[channel].Drive[ATA_activeDrive(channel)].Enable8BitTransfers) //Enabled 8-bit transfers?
		{
			ATA_writedata(channel, value); //Write the data!
			return 1; //We're enabled!
		}
		return 0; //We're non-existant!
		break;
	case 1: //Features?
#ifdef ATA_LOG
		dolog("ATA", "Feature register write: %02X %i.%i", value,channel,ATA_activeDrive(channel));
#endif
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.features = value; //Use the set data! Ignore!
		return 1; //OK!
		break;
	case 2: //Sector count?
#ifdef ATA_LOG
		dolog("ATA", "Sector count write: %02X %i.%i", value,channel, ATA_activeDrive(channel));
#endif
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount = value; //Set sector count!
		return 1; //OK!
		break;
	case 3: //Sector number?
#ifdef ATA_LOG
		dolog("ATA", "Sector number write: %02X %i.%i", value, channel, ATA_activeDrive(channel));
#endif
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber = value; //Set sector number!
		return 1; //OK!
		break;
	case 4: //Cylinder low?
#ifdef ATA_LOG
		dolog("ATA", "Cylinder low write: %02X %i.%i", value, channel, ATA_activeDrive(channel));
#endif
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow = value; //Set cylinder low!
		return 1; //OK!
		break;
	case 5: //Cylinder high?
#ifdef ATA_LOG
		dolog("ATA", "Cylinder high write: %02X %i.%i", value, channel, ATA_activeDrive(channel));
#endif
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh = value; //Set cylinder high!
		return 1; //OK!
		break;
	case 6: //Drive/head?
#ifdef ATA_LOG
		dolog("ATA", "Drive/head write: %02X %i.%i", value, channel, ATA_activeDrive(channel));
#endif
		ATA[channel].activedrive = (value >> 4) & 1; //The active drive!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.drivehead = value; //Set drive head!
		ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow = 0; //We're an PATA device!
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
#ifdef ATA_LOG
		dolog("ATA", "Control register write: %02X %i.%i",value, channel, ATA_activeDrive(channel));
#endif
		ATA[channel].DriveControlRegister.data = value; //Set the data!
		return 1; //OK!
		break;
	default: //Unsupported!
		break;
	}
	return 0; //Unsupported!
}

OPTINLINE void ATA_readdata(byte channel, byte *result)
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
	if (ATA[channel].Drive[ATA_activeDrive(channel)].Enable8BitTransfers) return 0; //We're only 8-bit data transfers!
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
	if ((port<getPORTaddress(channel)) || (port>(getPORTaddress(channel) + 0x7))) //Primary channel?
	{
		if ((port >= (getControlPORTaddress(channel))) && (port <= (getControlPORTaddress(channel)+1))) goto port3_read;
		channel = 1; //Try secondary channel!
		if ((port<getPORTaddress(channel)) || (port>(getPORTaddress(channel) + 0x7))) //Secondary channel?
		{
			if ((port >= (getControlPORTaddress(channel))) && (port <= (getControlPORTaddress(channel)+1))) goto port3_read;
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
		if (ATA[channel].Drive[ATA_activeDrive(channel)].Enable8BitTransfers) //Enabled 8-bit transfers?
		{
			ATA_readdata(channel, result); //Read the data!
			return 1; //We're enabled!
		}
		return 0; //We're 16-bit only!
		break;
	case 1: //Error register?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].ERRORREGISTER.data; //Error register!
#ifdef ATA_LOG
		dolog("ATA", "Error register read: %02X %i.%i", *result, channel, ATA_activeDrive(channel));
#endif
		return 1;
		break;
	case 2: //Sector count?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectorcount; //Get sector count!
#ifdef ATA_LOG
		dolog("ATA", "Sector count register read: %02X %i.%i", *result, channel, ATA_activeDrive(channel));
#endif
		return 1;
		break;
	case 3: //Sector number?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.sectornumber; //Get sector number!
#ifdef ATA_LOG
		dolog("ATA", "Sector number register read: %02X %i.%i", *result, channel, ATA_activeDrive(channel));
#endif
		return 1; //OK!
		break;
	case 4: //Cylinder low?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderlow; //Get cylinder low!
#ifdef ATA_LOG
		dolog("ATA", "Cylinder low read: %02X %i.%i", *result, channel, ATA_activeDrive(channel));
#endif
		return 1; //OK!
		break;
	case 5: //Cylinder high?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.cylinderhigh; //Get cylinder high!
#ifdef ATA_LOG
		dolog("ATA", "Cylinder high read: %02X %i.%i", *result, channel, ATA_activeDrive(channel));
#endif
		return 1; //OK!
		break;
	case 6: //Drive/head?
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].PARAMETERS.drivehead; //Get drive/head!
#ifdef ATA_LOG
		dolog("ATA", "Drive/head register read: %02X %i.%i", *result, channel, ATA_activeDrive(channel));
#endif
		return 1; //OK!
		break;
	case 7: //Status?
		ATA_updateStatus(channel); //Update the status register if needed!
		ATA_removeIRQ(channel,ATA_activeDrive(channel)); //Acnowledge IRQ!
		*result = ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.data; //Get status!
		ATA[channel].Drive[ATA_activeDrive(channel)].STATUSREGISTER.drivewritefault = 0; //Reset write fault flag!
#ifdef ATA_LOG
		dolog("ATA", "Status register read: %02X %i.%i", *result, channel, ATA_activeDrive(channel));
#endif
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
#ifdef ATA_LOG
		dolog("ATA", "Alternate status register read: %02X %i.%i", *result, channel, ATA_activeDrive(channel));
#endif
		return 1; //OK!
		break;
	case 1: //Drive address register?
		*result = (ATA[channel].DriveAddressRegister.data&0x7F); //Give the data, make sure we don't apply the flag shared with the Floppy Disk Controller!
#ifdef ATA_LOG
		dolog("ATA", "Drive address register read: %02X %i.%i", *result, channel, ATA_activeDrive(channel));
#endif
		return 1; //OK!
		break;
	default: //Unsupported!
		break;
	}
	return 0; //Unsupported!
}

void ATA_ConfigurationSpaceChanged(uint_32 address, byte size)
{
	if (address == 0x3C) //IRQ changed?
	{
		PCI_IDE.InterruptLine = 0xFF; //We're unused, so let the software detect it, if required!
	}
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
			if ((disk ==HDD0) || (disk==HDD1)) ATA[disk_channel].Drive[disk_ATA].driveparams[0] = 0x40|(1<<10); //Hard sectored, Fixed drive! Disk transfer rate>10MBs.
			ATA[disk_channel].Drive[disk_ATA].driveparams[1] = ATA[disk_channel].Drive[disk_ATA].driveparams[54] = get_cylinders(disk_size); //1=Number of cylinders
			ATA[disk_channel].Drive[disk_ATA].driveparams[3] = ATA[disk_channel].Drive[disk_ATA].driveparams[55] = get_heads(disk_size); //3=Number of heads
			ATA[disk_channel].Drive[disk_ATA].driveparams[6] = ATA[disk_channel].Drive[disk_ATA].driveparams[56] = get_SPT(disk_size); //6=Sectors per track
			ATA[disk_channel].Drive[disk_ATA].driveparams[5] = 0x200; //512 bytes per sector!
			ATA[disk_channel].Drive[disk_ATA].driveparams[4] = 0x200*(ATA[disk_channel].Drive[disk_ATA].driveparams[6]); //512 bytes per sector per track!
			//ATA[disk_channel].Drive[disk_ATA].driveparams[20] = 1; //Only single port I/O (no simultaneous transfers)!
			ATA[disk_channel].Drive[disk_ATA].driveparams[21] = 0xFFFF; //Buffer size in sectors!
			ATA[disk_channel].Drive[disk_ATA].driveparams[49] = (1<<9); //LBA supported, DMA unsupported!
			ATA[disk_channel].Drive[disk_ATA].driveparams[51] = 0x200; //PIO data transfer timing node!
			--disk_size; //LBA is 0-based, not 1 based!
			ATA[disk_channel].Drive[disk_ATA].driveparams[60] = (word)(disk_size & 0xFFFF); //Number of addressable sectors, low word!
			ATA[disk_channel].Drive[disk_ATA].driveparams[61] = (word)(disk_size >> 16); //Number of addressable sectors, high word!
			ATA[disk_channel].Drive[disk_ATA].driveparams[80] = 0x02; //Supports ATA-1!
			ATA_updateCapacity(disk_channel,disk_ATA); //Update the drive capacity!
		}
		else //Drive not inserted?
		{
			memset(ATA[disk_channel].Drive[disk_ATA].driveparams, 0, sizeof(ATA[disk_channel].Drive[disk_ATA].driveparams)); //Clear the information on the drive: it's non-existant!
		}
		if ((disk == CDROM0) || (disk == CDROM1)) //CDROM?
		{
			ATA[disk_channel].Drive[disk_ATA].driveparams[0] = ((2 << 14) | (5 << 8) | (1 << 7) | (2 << 5) | (0 << 0)); //CDROM drive!
		}
		break;
	default: //Unknown?
		break;
	}
}

void initATA()
{
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
	ATA_Drives[CDROM_channel][1] = CDROM1; //CDROM1 always present as slave!
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
	register_PCI(&PCI_IDE, sizeof(PCI_IDE),&ATA_ConfigurationSpaceChanged); //Register the PCI data area!
	//Initialise our data area!
	PCI_IDE.DeviceID = 1;
	PCI_IDE.VendorID = 1; //DEVICEID::VENDORID: We're a ATA device!
	PCI_IDE.ProgIF = 0x80; //We use our own set interrupts and we're a parallel ATA controller!
}