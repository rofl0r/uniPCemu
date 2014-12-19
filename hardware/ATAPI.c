//TODO!


/* The default and seemingly universal sector size for CD-ROMs. */
  #define ATAPI_SECTOR_SIZE 2048
 
/*
 / The default ISA IRQ numbers of the ATA controllers. /
  #define ATA_IRQ_PRIMARY     0x0E
  #define ATA_IRQ_SECONDARY   0x0F
 
  / The necessary I/O ports, indexed by "bus". /
  #define ATA_DATA(x)         (x)
  #define ATA_FEATURES(x)     (x+1)
  #define ATA_SECTOR_COUNT(x) (x+2)
  #define ATA_ADDRESS1(x)     (x+3)
  #define ATA_ADDRESS2(x)     (x+4)
  #define ATA_ADDRESS3(x)     (x+5)
  #define ATA_DRIVE_SELECT(x) (x+6)
  #define ATA_COMMAND(x)      (x+7)
  #define ATA_DCR(x)          (x+0x206)   / device control register /
 
  / valid values for "bus" /
  #define ATA_BUS_PRIMARY     0x1F0
  #define ATA_BUS_SECONDARY   0x170
  / valid values for "drive" /
  #define ATA_DRIVE_MASTER    0xA0
  #define ATA_DRIVE_SLAVE     0xB0
 
  / ATA specifies a 400ns delay after drive switching -- often
   * implemented as 4 Alternative Status queries. /
  //#define ATA_SELECT_DELAY(bus) \
    {inb(ATA_DCR(bus));inb(ATA_DCR(bus));inb(ATA_DCR(bus));inb(ATA_DCR(bus));}

typedef struct
{
} ATA_INFO; //ATA data we need to use!
	
enum IDEDeviceType {
	IDE_TYPE_NONE,
	IDE_TYPE_HDD=1,
	IDE_TYPE_CDROM
};

enum IDEDeviceState {
	IDE_DEV_READY=0,
	IDE_DEV_SELECT_WAIT,
	IDE_DEV_CONFUSED,
	IDE_DEV_BUSY,
	IDE_DEV_DATA_READ,
	IDE_DEV_DATA_WRITE,
	IDE_DEV_ATAPI_PACKET_COMMAND,
	IDE_DEV_ATAPI_BUSY
};

enum {
	IDE_STATUS_BUSY=0x80,
	IDE_STATUS_DRIVE_READY=0x40,
	IDE_STATUS_DRIVE_SEEK_COMPLETE=0x10,
	IDE_STATUS_DRQ=0x08,
	IDE_STATUS_ERROR=0x01
};

class IDEController;

static bool drivehead_is_lba48(uint8_t val) {
	return (val&0xE0) == 0x40;
}

static bool drivehead_is_lba(uint8_t val) {
	return (val&0xE0) == 0xE0;
}

static bool drivehead_is_chs(uint8_t val) {
	return (val&0xE0) == 0xA0;
}
	
byte IO_is_Secondary = 0; //Secondary ATA (default is primary!)	
byte IO_drive = 0; //Drive to use: 1=Master, 2=Slave, 0=Unknown!
byte IO_determined_drive = 0; //Determined drive: 1=Primary master, 2=Primary slave, 3=Secondary master, 4=Secondary slave, 0=Unknown!
	
void determine_device(word port)
{
IO_is_Secondary = 0; //Default: primary!
if (port=>ATA_DATA(ATA_BUS_SECONDARY) && port<=ATA_COMMAND(ATA_BUS_SECONDARY)) || (port==ATA_DCR(ATA_BUS_SECONDARY)) //Secondary?
{
is_secondary = 1; //Using secondary, not primary drive!
}
IO_drive = 0; //Unknown if master or slave!
switch (ATA_DRIVE_SELECT(is_secondary?ATA_BUS_SECONDARY:ATA_BUS_PRIMARY)) //What drive has been selected?
{
case ATA_DRIVE_MASTER: //Master?
	IO_Drive = 1; //Master!
	break;
case ATA_DRIVE_SLAVE: //Slave?
	IO_Drive = 2; //Slave!
	break;
default: //Unknown?
	break; //Don't process!
}

if (IO_drive) //Drive detected and valid?
{
IO_determined_drive = ((IO_is_Secondary<<1)+IO_Drive)-1; //The determined drive (0-3)
}
}
	
void ATAPI_writeIO(word port, byte data)
{
	determine_device(port); //Determine the device we're using, if needed!
	switch (port)
	{
	case ATA_DATA(ATA_BUS_PRIMARY): //Primary ATA data?
	case ATA_DATA(ATA_BUS_SECONDARY): //Secondata ATA data?
		break;
	case ATA_FEATURES(ATA_BUS_PRIMARY): //Primary ATA features?
	case ATA_FEATURES(ATA_BUS_SECONDARY): //Secondary ATA features?
		break;
	case ATA_SECTOR_COUNT(ATA_BUS_PRIMARY): //Primary sector count?
	case ATA_SECTOR_COUNT(ATA_BUS_SECONDARY): //Secondary sector count?
		break;
	case ATA_ADDRESS1(ATA_BUS_PRIMARY): //Primary Address1?
	case ATA_ADDRESS1(ATA_BUS_SECONDARY): //Secondary address1?
		break;
	case ATA_ADDRESS2(ATA_BUS_PRIMARY): //Primary Address2?
	case ATA_ADDRESS2(ATA_BUS_SECONDARY): //Secondary address2?
		break;
	case ATA_ADDRESS3(ATA_BUS_PRIMARY): //Primary Address3?
	case ATA_ADDRESS3(ATA_BUS_SECONDARY): //Secondary address3?
		break;
	case ATA_DRIVE_SELECT(ATA_BUS_PRIMARY): //Primary Drive select?
	case ATA_DRIVE_SELECT(ATA_BUS_SECONDARY): //Secondary Drive select?
		break;
	case ATA_COMMAND(ATA_BUS_PRIMARY): //Primary Command?
	case ATA_COMMAND(ATA_BUS_SECONDARY): //Secondary Command?
		break;
	case ATA_DCR(ATA_BUS_PRIMARY): //Primary Device Control Register?
	case ATA_DCR(ATA_BUS_SECONDARY): //Secondary Device Control Register?
		break;
	default:
		break; //Nothing to do: we're not emulated!
	}
}

byte ATAPI_readIO(word port)
{
	determine_device(port); //Determine the device we're using, if needed!
	switch (port)
	{
	case ATA_DATA(ATA_BUS_PRIMARY): //Primary ATA data?
	case ATA_DATA(ATA_BUS_SECONDARY): //Secondata ATA data?
		break;
	case ATA_FEATURES(ATA_BUS_PRIMARY): //Primary ATA features?
	case ATA_FEATURES(ATA_BUS_SECONDARY): //Secondary ATA features?
		break;
	case ATA_SECTOR_COUNT(ATA_BUS_PRIMARY): //Primary sector count?
	case ATA_SECTOR_COUNT(ATA_BUS_SECONDARY): //Secondary sector count?
		break;
	case ATA_ADDRESS1(ATA_BUS_PRIMARY): //Primary Address1?
	case ATA_ADDRESS1(ATA_BUS_SECONDARY): //Secondary address1?
		break;
	case ATA_ADDRESS2(ATA_BUS_PRIMARY): //Primary Address2?
	case ATA_ADDRESS2(ATA_BUS_SECONDARY): //Secondary address2?
		break;
	case ATA_ADDRESS3(ATA_BUS_PRIMARY): //Primary Address3?
	case ATA_ADDRESS3(ATA_BUS_SECONDARY): //Secondary address3?
		break;
	case ATA_DRIVE_SELECT(ATA_BUS_PRIMARY): //Primary Drive select?
	case ATA_DRIVE_SELECT(ATA_BUS_SECONDARY): //Secondary Drive select?
		break;
	case ATA_COMMAND(ATA_BUS_PRIMARY): //Primary Command?
	case ATA_COMMAND(ATA_BUS_SECONDARY): //Secondary Command?
		break;
	case ATA_DCR(ATA_BUS_PRIMARY): //Primary Device Control Register?
	case ATA_DCR(ATA_BUS_SECONDARY): //Secondary Device Control Register?
		break;
	default:
		return PORT_UNDEFINED_RESULT;
	}
	return PORT_UNDEFINED_RESULT;
}

*/

