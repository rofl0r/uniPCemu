#include "headers/types.h" //Basic typedefs!
#include "headers/hardware/8237A.h" //DMA controller support!
#include "headers/hardware/ports.h" //Port support!
#include "headers/basicio/io.h" //Basic I/O functionality!
#include "headers/hardware/pic.h" //PIC support!
#include "headers/basicio/dskimage.h" //DSK image support!
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/floppy.h" //Our type definitions!
#include "headers/bios/biosrom.h" //ROM support for Turbo XT BIOS detection!
#include "headers/emu/debugger/debugger.h" //For logging extra information when debugging!

//Configuration of the FDC...

//Enable density errors or gap length errors?
#define EMULATE_DENSITY 0
#define EMULATE_GAPLENGTH 0

//Double logging if FLOPPY_LOGFILE2 is defined!
#define FLOPPY_LOGFILE "debugger"
//#define FLOPPY_LOGFILE2 "floppy"
//#define FLOPPY_FORCELOG

//What IRQ is expected of floppy disk I/O
#define FLOPPY_IRQ 6
//What DMA channel is expected of floppy disk I/O
#define FLOPPY_DMA 2

//Floppy DMA transfer pulse time, in nanoseconds! How long to take to transfer one byte! Use the sector byte speed for now!
#define FLOPPY_DMA_TIMEOUT FLOPPY.DMArate

//Automatic setup.
#ifdef FLOPPY_LOGFILE
#ifdef FLOPPY_LOGFILE2
#define FLOPPY_LOG(...) { dolog(FLOPPY_LOGFILE,__VA_ARGS__); dolog(FLOPPY_LOGFILE2,__VA_ARGS__); }
#else
#define FLOPPY_LOG(...) { dolog(FLOPPY_LOGFILE,__VA_ARGS__); }
#endif
#else
#define FLOPPY_LOG(...)
#endif

//Logging with debugger only!
#ifdef FLOPPY_LOGFILE
#ifdef FLOPPY_FORCELOG
#define FLOPPY_LOGD(...) {FLOPPY_LOG(__VA_ARGS__)}
#else
#define FLOPPY_LOGD(...) if (debugger_logging()) {FLOPPY_LOG(__VA_ARGS__)}
#endif
#else
#define FLOPPY_LOGD(...)
#endif

//Redirect to direct log always if below is uncommented?
/*
#undef FLOPPY_LOGD
#define FLOPPY_LOGD FLOPPY_LOG
*/

struct
{
	byte DOR; //DOR
	byte MSR; //MSR
	byte CCR; //CCR
	byte DIR; //DIR
	byte DSR;
	byte ST0;
	byte ST1;
	byte ST2;
	byte ST3;
	struct
	{
		byte data[2]; //Both data bytes!
		double headloadtime, headunloadtime, steprate; //Current head load time, unload time and step rate for this drive!
	} DriveData[4]; //Specify for each of the 4 floppy drives!
	union
	{
		byte data[3]; //All data bytes!
		struct
		{
			byte FirstParameter0; //Set to 0
			byte SecondParameterByte;
			byte PreComp; //Precompensation value!
		};
	} Configuration; //The data from the Configure command!
	byte Locked; //Are we locked?
	byte commandstep; //Current command step! 0=Command, 1=Parameter, 2=Data, 3=Result, 0xFD: Give result then lockup, 0xFE: Locked up, 0xFF: Give error code and reset.
	byte commandbuffer[0x10000]; //Outgoing command buffer!
	word commandposition; //What position in the command (starts with commandstep=commandposition=0).
	byte databuffer[0x10000]; //Incoming data buffer!
	word databufferposition; //How much data is buffered!
	word databuffersize; //How much data are we to buffer!
	byte resultbuffer[0x10]; //Incoming result buffer!
	byte resultposition; //The position in the result!
	uint_64 disk_startpos; //The start position of the buffered data in the floppy disk!
	byte IRQPending; //Are we waiting for an IRQ?
	byte DMAPending; //Pending DMA transfer?
	byte diskchanged[4]; //Disk changed?
	FLOPPY_GEOMETRY *geometries[4]; //Disk geometries!
	FLOPPY_GEOMETRY customgeometry[4]; //Custom disk geometries!
	byte reset_pending,reset_pended; //Reset pending?
	byte reset_pending_size; //Size of the pending reset max value! A maximum set of 3 with 4 drives reset!
	byte currentcylinder[4], currenthead[4], currentsector[4]; //Current head for all 4 drives(current cylinder = the idea the FDC has of the current cylinder)!
	byte physicalcylinder[4]; //Actual physical drive cyclinder that's been selected on the drive(the physical cylinder on the drive)!
	byte activecommand[4]; //What command is running to time?
	byte TC; //Terminal count triggered?
	uint_32 sectorstransferred; //Ammount of sectors transferred!
	byte MT,DoubleDensity,Skip; //MT bit, Double Density bit and Skip bit  as set by the command, if any!
	byte floppy_resetted; //Are we resetted?
	byte ignorecommands; //Locked up by an invalid Sense Interrupt?
	byte recalibratestepsleft[4]; //Starts out at 79. Counts down with each step! Error if 0 and not track 0 reached yet!
	byte seekdestination[4]; //Where to seek to?
	byte seekrel[4]; //Seek relatively?
	byte seekrelup[4]; //Seek relatively upwards(towards larger cylinders)?
	byte MTMask; //Allow MT to be used in sector increase operations?
	double DMArate, DMAratePending; //Current DMA transfer rate!
	byte RWRequestedCylinder; //Read/Write requested cylinder!
} FLOPPY; //Our floppy drive data!

//DOR

//What drive to address?
#define FLOPPY_DOR_DRIVENUMBERR (FLOPPY.DOR&3)
//Enable controller when set!
#define FLOPPY_DOR_RESTR ((FLOPPY.DOR>>2)&1)
//0=IRQ channel(Disable IRQ), 1=DMA mode(Enable IRQ)
#define FLOPPY_DOR_DMA_IRQR ((FLOPPY.DOR>>3)&1)
//All drive motor statuses!
#define FLOPPY_DOR_MOTORCONTROLR ((FLOPPY.DOR>>4)&0xF)

//Configuration byte 1

//Threadhold!
#define FLOPPY_CONFIGURATION_THRESHOLDW(val) FLOPPY.Configuration.SecondParameterByte=((FLOPPY.Configuration.SecondParameterByte&~0xF)|((val)&0xF))
#define FLOPPY_CONFIGURATION_THRESHOLDR (FLOPPY.Configuration.SecondParameterByte&0xF)
//Disable drive polling mode if set!
#define FLOPPY_CONFIGURATION_DRIVEPOLLINGMODEDISABLER ((FLOPPY.Configuration.SecondParameterByte>>4)&1)
//Disable FIFO if set!
#define FLOPPY_CONFIGURATION_FIFODISABLEW(val) FLOPPY.Configuration.SecondParameterByte=((FLOPPY.Configuration.SecondParameterByte&~0x20)&1)|(((val)&1)<<5)
#define FLOPPY_CONFIGURATION_FIFODISABLER ((FLOPPY.Configuration.SecondParameterByte>>5)&1)
//Enable Implied Seek if set!
#define FLOPPY_IMPLIEDSEEKENABLER ((FLOPPY.Configuration.SecondParameterByte>>6)&1)

//Drive data

#define FLOPPY_DRIVEDATA_HEADUNLOADTIMER(drive) (FLOPPY.DriveData[drive].data[0]&0xF)
#define FLOPPY_DRIVEDATA_STEPRATER(drive) ((FLOPPY.DriveData[drive].data[0]>>4)&0xF)
#define FLOPPY_DRIVEDATA_NDMR(drive) (FLOPPY.DriveData[drive].data[1]&1)
#define FLOPPY_DRIVEDATA_HEADLOADTIMER(drive) ((FLOPPY.DriveData[drive].data[1]>>1)&0x7F)

//MSR

//1 if busy in seek mode.
#define FLOPPY_MSR_BUSYINPOSITIONINGMODEW(drive,val) FLOPPY.MSR=((FLOPPY.MSR&~(1<<drive))|((val&1)<<drive))
//Busy: read/write command of FDC in progress. Set when received command byte, cleared at end of result phase
#define FLOPPY_MSR_COMMANDBUSYW(val) FLOPPY.MSR=((FLOPPY.MSR&~0x10)|(((val)&1)<<4))
//1 when not in DMA mode, else DMA mode, during execution phase.
#define FLOPPY_MSR_NONDMAW(val) FLOPPY.MSR=((FLOPPY.MSR&~0x20)|(((val)&1)<<5))
//1 when has data for CPU, 0 when expecting data.
#define FLOPPY_MSR_HAVEDATAFORCPUW(val) FLOPPY.MSR=((FLOPPY.MSR&~0x40)|(((val)&1)<<6))
//1 when ready for data transfer, 0 when not ready.
#define FLOPPY_MSR_RQMW(val) FLOPPY.MSR=((FLOPPY.MSR&~0x80)|(((val)&1)<<7))

//CCR
//0=500kbits/s, 1=300kbits/s, 2=250kbits/s, 3=1Mbits/s
#define FLOPPY_CCR_RATER (FLOPPY.CCR&3)
#define FLOPPY_CCR_RATEW(val) FLOPPY.CCR=((FLOPPY.CCR&~3)|((val)&3))

//DIR
//1 if high density, 0 otherwise.
#define FLOPPY_DIR_HIGHDENSITYW(val) FLOPPY.DIR=((FLOPPY.DIR&~1)|((val)&1))
//0=500, 1=300, 2=250, 3=1MBit/s
#define FLOPPY_DIR_DATARATEW(val) FLOPPY.DIR=((FLOPPY.DIR&~6)|(((val)&3)<<1))
//Always 0xF
#define FLOPPY_DIR_ALWAYSFW(val) FLOPPY.DIR=((FLOPPY.DIR&~0x78)|(((val)&0xF)<<3))
//1 when disk changed. Executing a command clears this.
#define FLOPPY_DIR_DISKCHANGE(val) FLOPPY.DIR==((FLOPPY.DIR&0x7F)|(((val)&1)<<7))

//DSR
#define FLOPPY_DSR_DRATESELR (FLOPPY.DSR&3)
#define FLOPPY_DSR_DRATESELW(val) FLOPPY.DSR=((FLOPPY.DSR&~3)|((val)&3))
#define FLOPPY_DSR_PRECOMPR ((FLOPPY.DSR>>2)&7)
#define FLOPPY_DSR_DSR_0R ((FLOPPY.DSR>>5)&1)
#define FLOPPY_DSR_POWERDOWNR ((FLOPPY.DSR>>6)&1)
#define FLOPPY_DSR_SWRESETR ((FLOPPY.DSR>>7)&1)
#define FLOPPY_DSR_SWRESETW(val) FLOPPY.DSR=((FLOPPY.DSR&~0x80)|(((val)&1)<<7))

//Status registers:

//ST0
#define FLOPPY_ST0_UNITSELECTW(val) FLOPPY.ST0=((FLOPPY.ST0&~3)|((val)&3))
#define FLOPPY_ST0_CURRENTHEADW(val) FLOPPY.ST0=((FLOPPY.ST0&~4)|(((val)&1)<<2))
#define FLOPPY_ST0_NOTREADYW(val) FLOPPY.ST0=((FLOPPY.ST0&~8)|(((val)&1)<<3))
//Set with drive fault or cannot find track 0 after 79 pulses!
#define FLOPPY_ST0_UNITCHECKW(val) FLOPPY.ST0=((FLOPPY.ST0&~0x10)|(((val)&1)<<4))
#define FLOPPY_ST0_SEEKENDW(val) FLOPPY.ST0=((FLOPPY.ST0&~0x20)|(((val)&1)<<5))
#define FLOPPY_ST0_INTERRUPTCODEW(val) FLOPPY.ST0=((FLOPPY.ST0&~0xC0)|(((val)&3)<<6))

//ST1
#define FLOPPY_ST1_NOADDRESSMARKW(val) FLOPPY.ST1=((FLOPPY.ST1&~1)|((val)&1))
#define FLOPPY_ST1_NOTWRITABLEDURINGWRITECOMMANDW(val) FLOPPY.ST1=((FLOPPY.ST1&~2)|(((val)&1)<<1))
#define FLOPPY_ST1_NODATAW(val) FLOPPY.ST1=((FLOPPY.ST1&~4)|(((val)&1)<<2))
#define FLOPPY_ST1_ALWAYS0_1(val) FLOPPY.ST1=((FLOPPY.ST1&~8)|(((val)&1)<<3))
#define FLOPPY_ST1_TIMEOUTW(val) FLOPPY.ST1=((FLOPPY.ST1&~0x10)|(((val)&1)<<4))
#define FLOPPY_ST1_DATAERRORW(val) FLOPPY.ST1=((FLOPPY.ST1&~0x20)|(((val)&1)<<5))
#define FLOPPY_ST1_ALWAYS0_2(val) FLOPPY.ST1=((FLOPPY.ST1&~0x40)|(((val)&1)<<6))
#define FLOPPY_ST1_ENDOFCYCLINDER(val) FLOPPY.ST1=((FLOPPY.ST1&~0x80)|(((val)&1)<<7))

//ST2
#define FLOPPY_ST2_NODATAADDRESSMASKDAMW(val) FLOPPY.ST2=((FLOPPY.ST2&~1)|((val)&1))
#define FLOPPY_ST2_BADCYCLINDERW(val) FLOPPY.ST2=((FLOPPY.ST2&~2)|(((val)&1)<<1))
#define FLOPPY_ST2_SEEKERRORW(val) FLOPPY.ST2=((FLOPPY.ST2&~4)|(((val)&1)<<2))
#define FLOPPY_ST2_SEEKEQUALW(val) FLOPPY.ST2=((FLOPPY.ST2&~8)|(((val)&1)<<3))
#define FLOPPY_ST2_WRONGCYCLINDERW(val) FLOPPY.ST2=((FLOPPY.ST2&~0x10)|(((val)&1)<<4))
#define FLOPPY_ST2_CRCERRORW(val) FLOPPY.ST2=((FLOPPY.ST2&~0x20)|(((val)&1)<<5))
#define FLOPPY_ST2_DELETEDADDRESSMARKW(val) FLOPPY.ST2=((FLOPPY.ST2&~0x40)|(((val)&1)<<6))
#define FLOPPY_ST2_UNUSEDW(val) FLOPPY.ST2=((FLOPPY.ST2&~0x80)|(((val)&1)<<7))

//ST3
#define FLOPPY_ST3_DRIVESELECTW(val) FLOPPY.ST3=((FLOPPY.ST3&~3)|((val)&3))
#define FLOPPY_ST3_HEAD1ACTIVEW(val) FLOPPY.ST3=((FLOPPY.ST3&~4)|(((val)&1)<<2))
#define FLOPPY_ST3_DOUBLESIDEDW(val) FLOPPY.ST3=((FLOPPY.ST3&~8)|(((val)&1)<<3))
#define FLOPPY_ST3_TRACK0W(val) FLOPPY.ST3=((FLOPPY.ST3&~0x10)|(((val)&1)<<4))
#define FLOPPY_ST3_DRIVEREADYW(val) FLOPPY.ST3=((FLOPPY.ST3&~0x20)|(((val)&1)<<5))
#define FLOPPY_ST3_WRITEPROTECTIONW(val) FLOPPY.ST3=((FLOPPY.ST3&~0x40)|(((val)&1)<<6))
#define FLOPPY_ST3_ERRORSIGNATUREW(val) FLOPPY.ST3=((FLOPPY.ST3&~0x80)|(((val)&1)<<7))

//Start normal data!

byte density_forced = 0; //Default: don't ignore the density with the CPU!

double floppytimer[4] = {0.0,0.0,0.0,0.0}; //The timer for ticking floppy disk actions!
double floppytime[4] = {0.0,0.0,0.0,0.0}; //Buffered floppy disk time!
byte floppytiming = 0; //Are we timing?
byte currentfloppytimerstep[4] = {0,0,0,0}; //Current step to execute within the floppy disk timer process!

extern byte is_XT; //Are we emulating a XT architecture?

//Formulas for the different rates:

/*

Step rate(ms):
1M = 8-(val*0.5)
500k = 16-val
300k = (26+(2/3))-(val*(1+(2/3)))
250k = 32-(val*2)

Head Unload Time:
val 0h becomes 10h.
1M = 8*val
500k = 16*val
300k = (26+(2/3))*val
250k = 32*val

Head Load Time:
val 0h becomes 80h.
1M = val
500k = 2*val
300k = (3+(1/3))*val
250k = 4*val

*/

double floppy_steprate[4][0x10]; //All possible step rates!
double floppy_headunloadtimerate[4][0x10]; //All possible head (un)load times!
double floppy_headloadtimerate[4][0x80]; //All possible head load times!

void initFloppyRates()
{
	double steprate_base[4] = {0,0,0,0}; //The base to take, in ms!
	double steprate_addition[4] = {0,0,0,0}; //The multiplier to add, in ms
	double headunloadtime_addition[4] = {0,0,0,0}; //The multiplier to add, in ms
	double headloadtime_addition[4] = {0,0,0,0}; //The multiplier to add, in ms
	//We initialize all floppy disk rates, in milliseconds!
	//The order is of the rates is: 500k, 300k, 250k, 1M
	//Step rate!
	steprate_base[1] = 16.0;
	steprate_base[2] = 26.0+(2.0/3.0);
	steprate_base[3] = 32.0;
	steprate_base[0] = 8.0;
	steprate_addition[0] = 1.0;
	steprate_addition[1] = 1.0+(2.0/3.0);
	steprate_addition[2] = 2.0;
	steprate_addition[3] = 0.5;

	//Head Unload Time
	headunloadtime_addition[0] = 16.0;
	headunloadtime_addition[1] = 26.0+(2.0/3.0);
	headunloadtime_addition[2] = 32.0;
	headunloadtime_addition[3] = 8.0;

	//Head Load Time
	headloadtime_addition[0] = 2.0;
	headloadtime_addition[1] = 3.0+(1.0/3.0);
	headloadtime_addition[2] = 4.0;
	headloadtime_addition[3] = 1.0;

	//Now, to be based on the used data, calculate all used lookup tables!
	byte rate,ratesel,usedrate;
	for (ratesel=0;ratesel<4;++ratesel) //All rate selections!
	{
		for (rate=0;rate<0x10;++rate) //Process all rate timings for step rate&head unload time!
		{
			usedrate = rate?rate:0x10; //0 sets bit 4!
			floppy_steprate[ratesel][rate] = steprate_base[ratesel]+(steprate_addition[ratesel]*(double)usedrate)*1000000.0; //Time, in nanoseconds!
			floppy_headunloadtimerate[ratesel][rate] = (headunloadtime_addition[ratesel]*(double)usedrate)*1000000.0; //Time, in nanoseconds!
		}
		for (rate=0;rate<0x80;++rate) //Process all rate timings for head load time!
		{
			usedrate = rate?rate:0x80; //0 sets bit 8!
			floppy_headloadtimerate[ratesel][rate] = (headloadtime_addition[ratesel]*(double)usedrate)*1000000.0; //Time, in nanoseconds!
		}
	}
}

//Step rate is the duration between pulses of a Seek/Recalibrate command.
OPTINLINE double FLOPPY_steprate(byte drivenumber)
{
	return floppy_steprate[FLOPPY_DSR_DRATESELR][FLOPPY_DRIVEDATA_STEPRATER(drivenumber)]; //Look up the step rate for this disk!
}

//Head Load Time is applied when the head is unloaded and an operation doing anything with floppy media is executed(before data transfer).
OPTINLINE double FLOPPY_headloadtimerate(byte drivenumber)
{
	return floppy_headloadtimerate[FLOPPY_DSR_DRATESELR][FLOPPY_DRIVEDATA_HEADLOADTIMER(drivenumber)]; //Look up the head load time rate for this disk!
}

//Head Unload Time is the time after the read/write data operation, at which the head is unloaded.
OPTINLINE double FLOPPY_headunloadtimerate(byte drivenumber)
{
	return floppy_headunloadtimerate[FLOPPY_DSR_DRATESELR][FLOPPY_DRIVEDATA_HEADUNLOADTIMER(drivenumber)]; //Look up the head load time rate for this disk!
}

//Floppy sector reading rate, depending on RPM and Sectors per Track! Each round reads/writes a full track always! Gives the amount of nanoseconds per sector!
OPTINLINE double FLOPPY_sectorrate(byte drivenumber)
{
	if (FLOPPY.geometries[drivenumber]) //Valid geometry?
	{
		return (60000000000.0/(double)FLOPPY.geometries[drivenumber]->RPM)/(double)FLOPPY.geometries[drivenumber]->SPT; //We're at a constant speed, which is RPM divided up by Sectors per Track(Each track takes one round to read always)!
	}
	else //Default rate for unknown disk geometries!
	{
		return (60000000000.0/(double)300)/(double)80; //We're at a constant speed, which is RPM divided up by Sectors per Track(Each track takes one round to read always)!
	}
}

//Normal floppy specific stuff

#define KB(x) (x/1024)

//Floppy commands from OSDev.
enum FloppyCommands
{
   READ_TRACK =                 2,	// generates IRQ6
   SPECIFY =                    3,      // * set drive parameters
   SENSE_DRIVE_STATUS =         4,
   WRITE_DATA =                 5,      // * write to the disk
   READ_DATA =                  6,      // * read from the disk
   RECALIBRATE =                7,      // * seek to cylinder 0
   SENSE_INTERRUPT =            8,      // * ack IRQ6, get status of last command
   WRITE_DELETED_DATA =         9,
   READ_ID =                    10,	// generates IRQ6
   READ_DELETED_DATA =          12,
   FORMAT_TRACK =               13,     // *
   DUMPREG =                    14, //extended controller only!
   SEEK =                       15,     // * seek both heads to cylinder X
   VERSION =                    16,	// * used during initialization, once
   SCAN_EQUAL =                 17,
   PERPENDICULAR_MODE =         18,	// * used during initialization, once, maybe
   CONFIGURE =                  19,     // * set controller parameters
   LOCK =                       20,     // * protect controller params from a reset
   VERIFY =                     22,
   SCAN_LOW_OR_EQUAL =          25,
   SCAN_HIGH_OR_EQUAL =         29
};

/*
{  KB,SPT,SIDES,TRACKS,  }
{ 160,  8, 1   , 40   , 0 },
{ 180,  9, 1   , 40   , 0 },
{ 200, 10, 1   , 40   , 0 },
{ 320,  8, 2   , 40   , 1 },
{ 360,  9, 2   , 40   , 1 },
{ 400, 10, 2   , 40   , 1 },
{ 720,  9, 2   , 80   , 3 },
{1200, 15, 2   , 80   , 2 },
{1440, 18, 2   , 80   , 4 },
{2880, 36, 2   , 80   , 6 },

*/

//Allowed transfer rates!
#define TRANSFERRATE_500k 0
#define TRANSFERRATE_300k 1
#define TRANSFERRATE_250k 2
#define TRANSFERRATE_1M 3

//Allowed gap length!
#define GAPLENGTH_IGNORE 0
#define GAPLENGTH_STD 42
#define GAPLENGTH_5_14 32
#define GAPLENGTH_3_5 27

//Flags when executing commands!
#define CMD_EXT_SKIPDELETEDADDRESSMARKS 0x20
#define CMD_EXT_DOUBLEDENSITYMODE 0x40
#define CMD_EXT_MULTITRACKOPERATION 0x80

//Density limits and specification!
#define DENSITY_SINGLE 0
#define DENSITY_DOUBLE 1
#define DENSITY_HD 2
#define DENSITY_ED 4
//Ignore density on specific target BIOS/CPU?
#define DENSITY_IGNORE 8

//82072AA diskette EHD controller board jumper settings
#define FLOPPYTYPE_12MB 0
#define FLOPPYTYPE_720K 1
#define FLOPPYTYPE_28MB 2
#define FLOPPYTYPE_14MB 3

//Simple defines for optimizing floppy disk speed in the lookup table!
#define FLOPPYDISK_LOWSPEED TRANSFERRATE_250k|(TRANSFERRATE_300k<<2)|(TRANSFERRATE_500k<<4)|(TRANSFERRATE_500k<<6)
#define FLOPPYDISK_MIDSPEED TRANSFERRATE_250k|(TRANSFERRATE_300k<<2)|(TRANSFERRATE_500k<<4)|(TRANSFERRATE_500k<<6)
#define FLOPPYDISK_HIGHSPEED (TRANSFERRATE_1M|(TRANSFERRATE_1M<<2)|(TRANSFERRATE_1M<<4)|(TRANSFERRATE_1M<<6))

FLOPPY_GEOMETRY floppygeometries[NUMFLOPPYGEOMETRIES] = { //Differently formatted disks, and their corresponding geometries
	//First, 5"
	{ 160,  8,  1, 40, FLOPPYTYPE_12MB, 0, FLOPPYDISK_LOWSPEED, 0xFE,512 , 1, 64 ,DENSITY_SINGLE           ,GAPLENGTH_5_14,    0x00, 300}, //160K 5.25" supports 250kbits, 300kbits SD!
	{ 180,  9,  1, 40, FLOPPYTYPE_12MB, 0, FLOPPYDISK_LOWSPEED, 0xFC,512 , 2, 64 ,DENSITY_SINGLE           ,GAPLENGTH_5_14,    0x00, 300}, //180K 5.25" supports 250kbits, 300kbits SD!
	{ 200, 10,  1, 40, FLOPPYTYPE_12MB, 0, FLOPPYDISK_LOWSPEED, 0xFC,512 , 2, 64 ,DENSITY_SINGLE           ,GAPLENGTH_5_14,    0x00, 300}, //200K 5.25" supports 250kbits, 300kbits SD!
	{ 320,  8,  2, 40, FLOPPYTYPE_12MB, 0, FLOPPYDISK_LOWSPEED, 0xFF,512 , 1, 112,DENSITY_SINGLE           ,GAPLENGTH_5_14,    0x00, 300}, //320K 5.25" supports 250kbits, 300kbits SD!
	{ 360,  9,  2, 40, FLOPPYTYPE_12MB, 0, FLOPPYDISK_LOWSPEED, 0xFD,1024, 2, 112,DENSITY_DOUBLE           ,GAPLENGTH_5_14,    0x00, 300}, //360K 5.25" supports 250kbits, 300kbits DD!
	{ 400, 10,  2, 40, FLOPPYTYPE_12MB, 0, FLOPPYDISK_LOWSPEED, 0xFD,1024, 2, 112,DENSITY_SINGLE           ,GAPLENGTH_5_14,    0x00, 300}, //400K 5.25" supports 250kbits, 300kbits SD!
	{1200, 15,  2, 80, FLOPPYTYPE_12MB, 0, FLOPPYDISK_MIDSPEED, 0xF9,512 , 7, 224,DENSITY_SINGLE           ,GAPLENGTH_5_14,    0x00, 360}, //1200K 5.25" supports 300kbits, 500kbits SD!
	//Now 3.5"
	{ 720,  9,  2, 80, FLOPPYTYPE_720K, 1, FLOPPYDISK_LOWSPEED, 0xF9,1024, 3, 112,DENSITY_DOUBLE           ,GAPLENGTH_3_5,     0xC0, 300 }, //720K 3.5" supports 250kbits, 300kbits DD! Disable gap length checking here because we need to work without it on a XT?
	{1440, 18,  2, 80, FLOPPYTYPE_14MB, 1, FLOPPYDISK_MIDSPEED, 0xF0,512 , 9, 224,DENSITY_IGNORE|DENSITY_HD,GAPLENGTH_3_5,     0x80, 300 }, //1.44M 3.5" supports 250kbits, 500kbits HD! Disable gap length checking here because we need to work without it on a XT?
	{1680, 21,  2, 80, FLOPPYTYPE_14MB, 1, FLOPPYDISK_MIDSPEED, 0xF0,512 , 9, 224,DENSITY_IGNORE|DENSITY_HD,GAPLENGTH_3_5,     0x80, 300 }, //1.68M 3.5" supports 250kbits, 500kbits HD! Supporting BIOS only!
	{1722, 21,  2, 82, FLOPPYTYPE_14MB, 1, FLOPPYDISK_MIDSPEED, 0xF0,512 , 9, 224,DENSITY_IGNORE|DENSITY_HD,GAPLENGTH_3_5,     0x80, 300 }, //1.722M 3.5" supports 250kbits, 500kbits HD! Supporting BIOS only!
	{1840, 23,  2, 80, FLOPPYTYPE_14MB, 1, FLOPPYDISK_MIDSPEED, 0xF0,512 , 9, 224,DENSITY_IGNORE|DENSITY_HD,GAPLENGTH_3_5,     0x80, 300 }, //1.84M 3.5" supports 250kbits, 500kbits HD! Supporting BIOS only!
	{2880, 36,  2, 80, FLOPPYTYPE_28MB, 1, FLOPPYDISK_HIGHSPEED,0xF0,1024, 9, 240,DENSITY_IGNORE|DENSITY_ED,GAPLENGTH_IGNORE,  0x40, 300 } //2.88M 3.5" supports 1Mbits ED!
};

//BPS=512 always(except differently programmed)!

//Floppy geometries

byte floppy_spt(uint_64 floppy_size)
{
	int i;
	for (i = 0; i<(int)NUMITEMS(floppygeometries); i++)
	{
		if (floppygeometries[i].KB == KB(floppy_size)) return (byte)floppygeometries[i].SPT; //Found?
	}
	return 0; //Unknown!
}

byte floppy_tracks(uint_64 floppy_size)
{
	int i;
	for (i = 0; i<(int)NUMITEMS(floppygeometries); i++)
	{
		if (floppygeometries[i].KB == KB(floppy_size)) return floppygeometries[i].tracks; //Found?
	}
	return 0; //Unknown!
}

byte floppy_sides(uint_64 floppy_size)
{
	int i;
	for (i = 0; i<(int)NUMITEMS(floppygeometries); i++)
	{
		if (floppygeometries[i].KB == KB(floppy_size)) return (byte)floppygeometries[i].sides; //Found?
	}
	return 0; //Unknown!
}

//Simple floppy recalibrate/seek action complete handlers!
void FLOPPY_finishrecalibrate(byte drive);
void FLOPPY_finishseek(byte drive);
void FLOPPY_checkfinishtiming(byte drive);


OPTINLINE void updateFloppyGeometries(byte floppy, byte side, byte track)
{
	uint_64 floppysize = disksize(floppy); //Retrieve disk size for reference!
	byte i;
	char *DSKImageFile = NULL; //DSK image file to use?
	DISKINFORMATIONBLOCK DSKInformation;
	TRACKINFORMATIONBLOCK DSKTrackInformation;
	FLOPPY.geometries[floppy] = NULL; //Init geometry to unknown!
	for (i = 0; i < NUMITEMS(floppygeometries); i++) //Update the geometry!
	{
		if (floppygeometries[i].KB == KB(floppysize)) //Found?
		{
			FLOPPY.geometries[floppy] = &floppygeometries[i]; //The geometry we use!
			return; //Stop searching!
		}
	}

	//Unknown geometry!
	if ((DSKImageFile = getDSKimage((floppy) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
	{
		if (readDSKInfo(DSKImageFile, &DSKInformation)) //Gotten information about the DSK image?
		{
			if (readDSKTrackInfo(DSKImageFile, side, track, &DSKTrackInformation))
			{
				FLOPPY.geometries[floppy] = &FLOPPY.customgeometry[floppy]; //Apply custom geometry!
				FLOPPY.customgeometry[floppy].sides = DSKInformation.NumberOfSides; //Number of sides!
				FLOPPY.customgeometry[floppy].tracks = DSKInformation.NumberOfTracks; //Number of tracks!
				FLOPPY.customgeometry[floppy].SPT = DSKTrackInformation.numberofsectors; //Number of sectors in this track!
				//Fill in the remaining information with defaults!
				FLOPPY.customgeometry[floppy].RPM = 300; //Default to 300 RPM!
				FLOPPY.customgeometry[floppy].boardjumpersetting  = 0; //Unknown, leave at 0!
				FLOPPY.customgeometry[floppy].ClusterSize = 0; //Unknown!
				FLOPPY.customgeometry[floppy].DirectorySize = 0; //Unknown!
				FLOPPY.customgeometry[floppy].DoubleDensity = (DSKTrackInformation.numberofsectors>40); //Probably double density?
				FLOPPY.customgeometry[floppy].FATSize = 0; //Unknown!
				FLOPPY.customgeometry[floppy].GAPLength = DSKTrackInformation.GAP3Length; //Our GAP3 length used!
				FLOPPY.customgeometry[floppy].KB = (word)((uint_32)(DSKInformation.NumberOfTracks*DSKInformation.NumberOfSides*DSKInformation.TrackSize)>>10); //Raw size!
				FLOPPY.customgeometry[floppy].measurement = DSKTrackInformation.numberofsectors>40?1:0; //Unknown, take 3,5" when >40 tracks!
				FLOPPY.customgeometry[floppy].MediaDescriptorByte = 0x00; //Unknown!
				FLOPPY.customgeometry[floppy].supportedrates = 0x1B; //Support all rates!
				FLOPPY.customgeometry[floppy].TapeDriveRegister = 0x00; //Unknown!
			}
		}
	}
}

uint_32 floppy_LBA(byte floppy, word side, word track, word sector)
{
	updateFloppyGeometries(floppy,(byte)side,(byte)track); //Update the floppy geometries!
	if (!FLOPPY.geometries[floppy]) return 0; //Unknown floppy geometry!
	return (uint_32)(((track*FLOPPY.geometries[floppy]->sides) + side) * FLOPPY.geometries[floppy]->SPT) + sector - 1; //Give LBA for floppy!
}

//Sector size

OPTINLINE word translateSectorSize(byte size)
{
	return 128<<size; //Give the translated sector size!
}

void FLOPPY_notifyDiskChanged(int disk)
{
	switch (disk)
	{
	case FLOPPY0:
		FLOPPY.diskchanged[0] = 1; //Changed!
		break;
	case FLOPPY1:
		FLOPPY.diskchanged[1] = 1; //Changed!
		break;
	}
}

OPTINLINE void FLOPPY_raiseIRQ() //Execute an IRQ!
{
	FLOPPY.IRQPending = 1; //We're waiting for an IRQ!
	if (FLOPPY_DOR_DMA_IRQR) raiseirq(FLOPPY_IRQ); //Execute the IRQ when enabled!
}

OPTINLINE void FLOPPY_lowerIRQ()
{
	FLOPPY.IRQPending = 0; //We're not pending anymore!
	lowerirq(FLOPPY_IRQ); //Lower the IRQ!
	acnowledgeIRQrequest(FLOPPY_IRQ); //Acnowledge!
}

OPTINLINE byte FLOPPY_useDMA()
{
	return (FLOPPY_DOR_DMA_IRQR && (FLOPPY_DRIVEDATA_NDMR(FLOPPY_DOR_DRIVENUMBERR)==0)); //Are we using DMA?
}

OPTINLINE byte FLOPPY_supportsrate(byte disk)
{
	if (!FLOPPY.geometries[disk]) //Unknown geometry?
	{
		FLOPPY.DMAratePending = (FLOPPY_sectorrate(FLOPPY_DOR_DRIVENUMBERR)/512.0); //Set the rate used as active to transfer data one byte at a time, simply taken the sector rate!
		return 1; //No disk geometry, so supported by default(unknown drive)!
	}
	byte supported = 0, current=0, currentrate;
	supported = FLOPPY.geometries[disk]->supportedrates; //Load the supported rates!
	currentrate = FLOPPY_CCR_RATER; //Current rate we use (both CCR and DSR can be used, since they're both updated when either changes)!
	for (;current<4;) //Check all available rates!
	{
		if (currentrate==(supported&3))
		{
			FLOPPY.DMAratePending = (FLOPPY_sectorrate(FLOPPY_DOR_DRIVENUMBERR)/512.0); //Set the rate used as active to transfer data one byte at a time, simply taken the sector rate!
			return 1; //We're a supported rate!
		}
		supported  >>= 2; //Check next rate!
		++current; //Next supported!
	}
	return 0; //Unsupported rate!
}

OPTINLINE void updateST3(byte drivenumber)
{
	FLOPPY.ST3 |= 0x28; //Always set according to Bochs!
	FLOPPY_ST3_TRACK0W((FLOPPY.physicalcylinder[drivenumber] == 0)?1:0); //Are we at track 0?

	if (FLOPPY.geometries[drivenumber]) //Valid drive?
	{
		FLOPPY_ST3_DOUBLESIDEDW((FLOPPY.geometries[drivenumber]->sides==2)?1:0); //Are we double sided?
	}
	else //Apply default disk!
	{
		FLOPPY_ST3_DOUBLESIDEDW(1); //Are we double sided?
	}
	FLOPPY_ST3_HEAD1ACTIVEW(FLOPPY.currenthead[drivenumber]); //Is head 1 active?
	FLOPPY_ST3_DRIVESELECTW(drivenumber); //Our selected drive!
	FLOPPY_ST3_DRIVEREADYW(1); //We're always ready on PC!
	if (drivenumber<2) //Valid drive number?
	{
		FLOPPY_ST3_WRITEPROTECTIONW(drivereadonly(drivenumber ? FLOPPY1 : FLOPPY0)?1:0); //Read-only drive and tried to write?
	}
	else
	{
		FLOPPY_ST3_WRITEPROTECTIONW(0); //Drive unsupported? No write protection!
	}
	FLOPPY_ST3_ERRORSIGNATUREW(0); //No errors here!
}

byte FLOPPY_hadIRQ = 0; //Did we have an IRQ raised?

OPTINLINE void FLOPPY_handlereset(byte source) //Resets the floppy disk command when needed!
{
	byte pending_size; //Our currently pending size to use!
	if ((!FLOPPY_DOR_RESTR) || FLOPPY_DSR_SWRESETR) //We're to reset by either one enabled?
	{
		if (!FLOPPY.floppy_resetted) //Not resetting yet?
		{
			if (source==1)
			{
				FLOPPY_LOGD("FLOPPY: Reset requested by DSR!")
			}
			else
			{
				FLOPPY_LOGD("FLOPPY: Reset requested by DOR!")
			}
			FLOPPY.DOR = 0; //Reset motors! Reset drives! IRQ channel!
			FLOPPY.MSR = 0; //Default to no data!
			FLOPPY.commandposition = 0; //No command!
			FLOPPY.commandstep = 0; //Reset step to indicate we're to read the result in ST0!
			FLOPPY.ST0 = 0xC0; //Reset ST0 to the correct value: drive became not ready!
			FLOPPY.ST1 = FLOPPY.ST2 = 0; //Reset the ST data!
			pending_size = 4; //Pending full size with polling mode enabled!
			if (FLOPPY_CONFIGURATION_DRIVEPOLLINGMODEDISABLER) pending_size = 0; //Don't pend when polling mode is off!
			FLOPPY.reset_pending_size = FLOPPY.reset_pending = pending_size; //We have a reset pending for all 4 drives, unless interrupted by an other command!
			FLOPPY.reset_pended = 1; //We're pending a reset! Clear status once we're becoming active!
			memset(FLOPPY.currenthead, 0, sizeof(FLOPPY.currenthead)); //Clear the current heads!
			memset(FLOPPY.currentsector, 1, sizeof(FLOPPY.currentsector)); //Clear the current sectors!
			updateST3(0); //Update ST3 only!
			FLOPPY.TC = 0; //Disable TC identifier!
			if (FLOPPY.Locked==0) //Are we not locked? Perform stuff that's not locked during reset!
			{
				FLOPPY_CONFIGURATION_THRESHOLDW(0); //Reset threshold!
				FLOPPY_CONFIGURATION_FIFODISABLEW(1); //Disable the FIFO!
			}
			//Make sure the IRQ works when resetting always!
			FLOPPY.floppy_resetted = 1; //We're resetted!
			FLOPPY.ignorecommands = 0; //We're enabling commands again!
			//Make sure
			FLOPPY_hadIRQ = 0; //Was an IRQ Pending? Nope, we're resetting!
			FLOPPY_lowerIRQ(); //Lower the IRQ!
		}
	}
	else if (FLOPPY.floppy_resetted) //We were resetted and are activated?
	{
		FLOPPY_raiseIRQ(); //Raise the IRQ: We're reset and have been activated!
		FLOPPY.floppy_resetted = 0; //Not resetted anymore!
		if (source==1)
		{
			FLOPPY_LOGD("FLOPPY: Activation requested by DSR!")
		}
		else
		{
			FLOPPY_LOGD("FLOPPY: Activation requested by DOR!")
		}
	}
}

//Execution after command and data phrases!
byte oldMSR = 0; //Old MSR!

OPTINLINE void updateFloppyMSR() //Update the floppy MSR!
{
	switch (FLOPPY.commandstep) //What command step?
	{
	case 0: //Command?
		FLOPPY.sectorstransferred = 0; //There's nothing transferred yet!
		FLOPPY_MSR_COMMANDBUSYW(0); //Not busy: we're waiting for a command!
		FLOPPY_MSR_RQMW(!FLOPPY.floppy_resetted); //Ready for data transfer when not being reset!
		FLOPPY_MSR_HAVEDATAFORCPUW(0); //We don't have data for the CPU!
		FLOPPY_MSR_NONDMAW(0); //No DMA transfer busy!
		break;
	case 1: //Parameters?
		FLOPPY_MSR_COMMANDBUSYW(1); //Default: busy!
		FLOPPY_MSR_RQMW(1); //Ready for data transfer!
		FLOPPY_MSR_HAVEDATAFORCPUW(0); //We don't have data for the CPU!
		FLOPPY_MSR_NONDMAW(0); //No DMA transfer busy!
		break;
	case 2: //Data?
		FLOPPY_MSR_COMMANDBUSYW(1); //Default: busy!
		//Check DMA, RQM and Busy flag!
		switch (FLOPPY.commandbuffer[0]) //What command are we processing?
		{
		case WRITE_DATA: //Write sector?
		case WRITE_DELETED_DATA: //Write deleted sector?
		case FORMAT_TRACK: //Format sector?
		case READ_DATA: //Read sector?
		case READ_DELETED_DATA: //Read deleted sector?
		case SCAN_EQUAL:
		case SCAN_LOW_OR_EQUAL:
		case SCAN_HIGH_OR_EQUAL:
			FLOPPY_MSR_RQMW(!FLOPPY_useDMA()); //Use no DMA? Then transfer data and set NonDMA! Else, clear non DMA and don't transfer!
			FLOPPY_MSR_NONDMAW(!FLOPPY_useDMA()); //Use no DMA? Then transfer data and set NonDMA! Else, clear non DMA and don't transfer!
			break;
		case VERIFY: //Verify doesn't transfer data directly!
			FLOPPY_MSR_RQMW(0); //Use no DMA? Then transfer data and set NonDMA! Else, clear non DMA and don't transfer!
			FLOPPY_MSR_NONDMAW(0); //Use no DMA? Then transfer data and set NonDMA! Else, clear non DMA and don't transfer!
			break;
		default: //Unknown command?
			FLOPPY_MSR_RQMW(1); //Use no DMA by default, for safety!
			FLOPPY_MSR_NONDMAW(0); //Use no DMA by default, for safety!
			break; //Don't process!
		}

		//Check data direction!
		switch (FLOPPY.commandbuffer[0]) //Process input/output to/from controller!
		{
		case WRITE_DATA: //Write sector?
		case WRITE_DELETED_DATA: //Write deleted sector?
		case FORMAT_TRACK: //Format sector?
		case SCAN_EQUAL:
		case SCAN_LOW_OR_EQUAL:
		case SCAN_HIGH_OR_EQUAL:
			FLOPPY_MSR_HAVEDATAFORCPUW(0); //We request data from the CPU!
			break;
		case READ_DATA: //Read sector?
		case READ_DELETED_DATA: //Read deleted sector?
		case VERIFY: //Verify doesn't transfer data directly!
			FLOPPY_MSR_HAVEDATAFORCPUW(1); //We have data for the CPU!
			break;
		default: //Unknown direction?
			FLOPPY_MSR_HAVEDATAFORCPUW(0); //Nothing, say output by default!
			break;
		}
		break;
	case 0xFD: //Give result and lockup?
	case 3: //Result?
		FLOPPY_MSR_COMMANDBUSYW(1); //Default: busy!
		FLOPPY_MSR_RQMW(1); //Data transfer!
		FLOPPY_MSR_HAVEDATAFORCPUW(1); //We have data for the CPU!
		FLOPPY_MSR_NONDMAW(0); //No DMA transfer busy!
		break;
	case 0xFE: //Locked up?
	case 0xFF: //Error?
		FLOPPY_MSR_COMMANDBUSYW(1); //Default: busy!
		FLOPPY_MSR_RQMW(1); //Data transfer!
		FLOPPY_MSR_HAVEDATAFORCPUW(1); //We have data for the CPU!
		FLOPPY_MSR_NONDMAW(0); //No DMA transfer busy!
		break;
	default: //Unknown status?
		break; //Unknown?
	}
	if (FLOPPY.MSR != oldMSR) //MSR changed?
	{
		oldMSR = FLOPPY.MSR; //Update old!
		FLOPPY_LOGD("FLOPPY: MSR changed: %02x", FLOPPY.MSR) //The updated MSR!
	}
}

OPTINLINE void updateFloppyDIR() //Update the floppy DIR!
{
	FLOPPY.DIR = 0; //Init to not changed!
	if (FLOPPY.diskchanged[0] && (FLOPPY_DOR_MOTORCONTROLR&1))
	{
		FLOPPY.DIR = 0x80; //Set our bit!
	}
	if (FLOPPY.diskchanged[1] && (FLOPPY_DOR_MOTORCONTROLR&2))
	{
		FLOPPY.DIR = 0x80; //Set our bit!
	}
	if (FLOPPY.diskchanged[2] && (FLOPPY_DOR_MOTORCONTROLR&4))
	{
		FLOPPY.DIR = 0x80; //Set our bit!
	}
	if (FLOPPY.diskchanged[3] && (FLOPPY_DOR_MOTORCONTROLR&8))
	{
		FLOPPY.DIR = 0x80; //Set our bit!
	}
	//Rest of the bits are reserved on an AT!
}

OPTINLINE void clearDiskChanged()
{
	//Reset state for all drives!
	FLOPPY.diskchanged[0] = 0; //Reset!
	FLOPPY.diskchanged[1] = 0; //Reset!
	FLOPPY.diskchanged[2] = 0; //Reset!
	FLOPPY.diskchanged[3] = 0; //Reset!
}

OPTINLINE void updateFloppyWriteProtected(byte iswrite, byte drivenumber)
{
	FLOPPY.ST1 = (FLOPPY.ST1&~2); //Default: not write protected!
	if (drivereadonly(drivenumber ? FLOPPY1 : FLOPPY0) && iswrite) //Read-only drive and tried to write?
	{
		FLOPPY.ST1 |= 2; //Write protected!
	}
}

OPTINLINE byte floppy_increasesector(byte floppy) //Increase the sector number automatically!
{
	byte result = 1; //Default: read/write more
	//byte headoverflow = 0; //Head overflown?
	if (FLOPPY.geometries[floppy]) //Do we have a valid geometry?
	{
		if (++FLOPPY.currentsector[floppy] > FLOPPY.geometries[floppy]->SPT) //Overflow next sector by parameter?
		{
			if (!FLOPPY_useDMA()) //Non-DMA mode?
			{
				if (((FLOPPY.MT&FLOPPY.MTMask) && FLOPPY.currenthead[floppy]) || !(FLOPPY.MT&FLOPPY.MTMask)) //Multi-track and side 1, or not Multi-track?
				{
					result = 0; //SPT finished!
				}
			}

			FLOPPY.currentsector[floppy] = 1; //Reset sector number!

			//Apply Multi Track accordingly!
			if (FLOPPY.MT&FLOPPY.MTMask) //Multi Track used?
			{
				++FLOPPY.currenthead[floppy]; //Increase the head!
				if (FLOPPY.currenthead[floppy] >= FLOPPY.geometries[floppy]->sides) //Side overflow?
				{
					FLOPPY.currenthead[floppy] = 0; //Reset side number!
					//headoverflow = 1; //We've overflown the head!
				}
			}
			/*else //Single head?
			{
				headoverflow = 1; //We always overflow the head!
			}*/

			/*if (headoverflow) //Head overflown?
			{
				Overflow doesn't tick the cylinder to another track?
			}*/
			updateST3(floppy); //Update ST3 only!
		}
	}
	
	FLOPPY_ST0_CURRENTHEADW(FLOPPY.currenthead[floppy]); //Our idea of the current head!

	if (FLOPPY_useDMA()) //DMA mode determines our triggering?
	{
		if (result) //OK to transfer more?
		{
			result = !FLOPPY.TC; //No terminal count triggered? Then we read the next sector!
		}
		else //Error occurred during DMA transfer?
		{
			result = 2; //Abort!
			FLOPPY_ST0_INTERRUPTCODEW(1); //Couldn't finish correctly!
			FLOPPY_ST0_SEEKENDW(0); //Failed!
		}
	}

	++FLOPPY.sectorstransferred; //Increase the amount of sectors transferred.

	return result; //Give the result: we've overflown the max sector number!
}

OPTINLINE void FLOPPY_dataReady() //Data transfer ready to transfer!
{
	if (FLOPPY_DRIVEDATA_NDMR(FLOPPY_DOR_DRIVENUMBERR)) //Interrupt for each byte transferred?
	{
		FLOPPY_raiseIRQ(); //Raise the floppy IRQ: We have data to transfer!
	}
}

OPTINLINE void FLOPPY_startData() //Start a Data transfer if needed!
{
	FLOPPY.databufferposition = 0; //Start with the new buffer!
	if (FLOPPY.commandstep != 2) //Entering data phase?
	{
		FLOPPY_LOGD("FLOPPY: Start transfer of data (DMA: %u)...",FLOPPY_useDMA())
	}
	switch (FLOPPY.commandbuffer[0]) //What kind of transfer?
	{
	case SCAN_EQUAL: //Equal mismatch?
	case SCAN_LOW_OR_EQUAL: //Low or equal mismatch?
	case SCAN_HIGH_OR_EQUAL: //High or equal mismatch?
		FLOPPY_ST2_SEEKERRORW(0); //No seek error yet!
		FLOPPY_ST2_SEEKEQUALW(1); //Equal by default: we're starting to match until we don't anymore!
		break;
	}
	FLOPPY.commandstep = 2; //Move to data phrase!
	if (FLOPPY.commandbuffer[0]==VERIFY) //Verify doesn't transfer data directly?
	{
		FLOPPY_supportsrate(FLOPPY_DOR_DRIVENUMBERR); //Make sure we have a rate set!
		FLOPPY.DMArate = FLOPPY.DMAratePending; //Start running at the specified speed!		
		floppytimer[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY_DMA_TIMEOUT; //Time the timeout for floppy!
		floppytiming |= (1<<FLOPPY_DOR_DRIVENUMBERR); //Make sure we're timing on the specified disk channel!
	}
	else //Normal data transfer?
	{
		if (FLOPPY_useDMA()) //DMA mode?
		{
			FLOPPY.DMAPending = 1; //Pending DMA! Start when available!
			FLOPPY_supportsrate(FLOPPY_DOR_DRIVENUMBERR); //Make sure we have a rate set!
			FLOPPY.DMArate = FLOPPY.DMAratePending; //Start running at the specified speed!
		}
		FLOPPY_dataReady(); //We have data to transfer!
	}
}

//Physical floppy CHS emulation!
/*
double floppyCHStiming = 0.0; //CHS timing!
void updateFloppy(double timepassed)
{
	byte timed = 0; //Are we timed?
	//Use FLOPPY_steprate, FLOPPY_head(un)loadtimerate and FLOPPY_sectorrate(/bytespersector) to time all output!
	if (unlikely(floppyCHStiming)>0.0) //Time left?
	{
		floppyCHStiming -= timepassed; //Tick some time!
		if (likely(floppyCHStiming>0.0)) return; //Abort when time is left?
	}
	nexttiming: //Next timing is looped?
	//We're to process the next requested step!
	if (FLOPPY.currentaction&1) //What action to take? We're seeking the track?
	{
		if (FLOPPY.currentcylinder[FLOPPPY_DOR_DRIVENUMBERR]<FLOPPY.requestedcylinder[FLOPPY_DOR_DRIVENUMBERR]) //To increase cylinder?
		{
			++FLOPPY.currentcylinder[FLOPPY_DOR_DRIVENUMBER]; //Increase cylinder!
			floppyCHStiming += FLOPPY_steprate(FLOPPY_DOR_DRIVENUMBERR); //Delay until done!
			timed = 1; //Timed!
		}
		else if (FLOPPY.currentcylinder[FLOPPPY_DOR_DRIVENUMBERR]>FLOPPY.requestedcylinder[FLOPPY_DOR_DRIVENUMBERR]) //To decrease cylinder?
		{
			--FLOPPY.currentcylinder[FLOPPY_DOR_DRIVENUMBER]; //Increase cylinder!
			floppyCHStiming += FLOPPY_steprate(FLOPPY_DOR_DRIVENUMBERR); //Delay until done!
			timed = 1; //Timed!
		}
		else //Track found?
		{
			FLOPPY.currentaction &= ~1; //Finished this action!
		}
	}
	else if (FLOPPY.currentaction&2) //Are we seeking a sector?
	{
		if (FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR]==FLOPPY.requestedsector[FLOPPY_DOR_DRIVENUMBERR]) //Requested sector found?
		{
			FLOPPY.currentaction &= ~1; //Finished: sector found!
		}
		else //Sector not yet found? Increase sector, check and delay!
		{
			//Detect index hole, when at sector #0!
			if (FLOPPY.indexhole[FLOPPY_DOR_DRIVENUMBERR]) //Index hole?
			{
				if (++FLOPPY.indexholes[FLOPPY_DOR_DRIVENUMBERR]==2) //Passed for the second time?
				{
					FLOPPY.currentaction &= ~2; //Abort: sector not found!
					return; //Abort!
				}
			}
			++FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR]; //Goto next sector!
			floppyCHStiming += FLOPPY_sectorrate(FLOPPY_DOR_DRIVENUMBERR); //Delay until done!
			timed = 1; //Timed!
		}
	}
	else //At the sector that's requested(or not when erroring out)!
	{
		if (FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR]==FLOPPY.requestedsector[FLOPPY_DOR_DRIVENUMBER]) //Found sector at the track?
		{
			//Do something with the sector, read it or write it?
		}
	}
	if ((floppyCHStiming<0) && timed) //Still something left to time?
	{
		timed = 0; //Timed!
		goto nexttiming; //Apply next timing, if needed, right away!
	}
}
*/

//Normal floppy disk emulation again!

OPTINLINE void floppy_readsector() //Request a read sector command!
{
	char *DSKImageFile = NULL; //DSK image file to use?
	SECTORINFORMATIONBLOCK sectorinformation; //Information about the sector!

	if (!FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]) //Not inserted or valid?
	{
		FLOPPY_LOGD("FLOPPY: Error: Invalid drive!")
		FLOPPY.ST0 = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}
	if ((FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->DoubleDensity!=(FLOPPY.DoubleDensity&~DENSITY_IGNORE)) && (!(FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->DoubleDensity&DENSITY_IGNORE) || density_forced) && EMULATE_DENSITY) //Wrong density?
	{
		FLOPPY_LOGD("FLOPPY: Error: Invalid density!")
		FLOPPY.ST0 = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}

	FLOPPY.databuffersize = translateSectorSize(FLOPPY.commandbuffer[5]); //Sector size into data buffer!
	if (!FLOPPY.commandbuffer[5]) //Special case? Use given info!
	{
		FLOPPY.databuffersize = FLOPPY.commandbuffer[8]; //Use data length!
	}
	FLOPPY.disk_startpos = floppy_LBA(FLOPPY_DOR_DRIVENUMBERR, FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR]); //The start position, in sectors!
	FLOPPY_LOGD("FLOPPY: Read sector #%u", FLOPPY.disk_startpos) //We're reading this sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Sector size: %u bytes", FLOPPY.databuffersize) }
	FLOPPY.disk_startpos *= FLOPPY.databuffersize; //Calculate the start sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Requesting transfer for %u bytes.", FLOPPY.databuffersize) } //Transfer this many sectors!

	if (!(FLOPPY_DOR_MOTORCONTROLR&(1 << FLOPPY_DOR_DRIVENUMBERR))) //Not motor ON?
	{
		FLOPPY_LOGD("FLOPPY: Error: drive motor not ON!")
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		FLOPPY.ST0 = 0x40; //Abnormal termination!
		return;
	}

	FLOPPY_ST0_UNITSELECTW(FLOPPY_DOR_DRIVENUMBERR); //Current unit!
	FLOPPY_ST0_CURRENTHEADW(FLOPPY.commandbuffer[2] & 1); //Current head!
	FLOPPY_ST0_NOTREADYW(0); //We're not ready yet!
	FLOPPY_ST0_UNITCHECKW(0); //Clear unit check and Interrupt code: we're OK. Also clear SE flag: we're still busy!
	FLOPPY_ST0_SEEKENDW(0); //Clear unit check and Interrupt code: we're OK. Also clear SE flag: we're still busy!
	FLOPPY_ST0_INTERRUPTCODEW(0); //Clear unit check and Interrupt code: we're OK. Also clear SE flag: we're still busy!

	if (!FLOPPY_supportsrate(FLOPPY_DOR_DRIVENUMBERR) || !FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]) //We don't support the rate or geometry?
	{
		goto floppy_errorread; //Error out!
	}

	if (FLOPPY_IMPLIEDSEEKENABLER) //Implied seek?
	{
		if (FLOPPY.RWRequestedCylinder<FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->tracks) //Valid track?
		{
			FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.currentcylinder[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.RWRequestedCylinder; //Implied seek!
			FLOPPY_finishseek(FLOPPY_DOR_DRIVENUMBERR); //Simulate seek complete!
			FLOPPY_checkfinishtiming(FLOPPY_DOR_DRIVENUMBERR); //Seek is completed!
		}
	}

	if (FLOPPY.RWRequestedCylinder!=FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR]) //Wrong cylinder to access?
	{
		goto floppy_errorread; //Error out!
	}

	if (readdata(FLOPPY_DOR_DRIVENUMBERR ? FLOPPY1 : FLOPPY0, &FLOPPY.databuffer, FLOPPY.disk_startpos, FLOPPY.databuffersize)) //Read the data into memory?
	{
		if ((FLOPPY.commandbuffer[7]!=FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->GAPLength) && (FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->GAPLength!=GAPLENGTH_IGNORE) && EMULATE_GAPLENGTH) //Wrong GAP length?
		{
			FLOPPY.ST0 = 0x40; //Abnormal termination!
			FLOPPY.commandstep = 0xFF; //Move to error phase!
			return;					
		}
		FLOPPY_ST0_SEEKENDW(1); //Successfull read with implicit seek!
		FLOPPY_startData();
	}
	else //DSK or error?
	{
		if ((DSKImageFile = getDSKimage((FLOPPY_DOR_DRIVENUMBERR) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
		{
			if (readDSKSectorData(DSKImageFile,FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.commandbuffer[5], &FLOPPY.databuffersize)) //Read the data into memory?
			{
				if (readDSKSectorInfo(DSKImageFile, FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[4], &sectorinformation)) //Read the sector information too!
				{
					FLOPPY.ST1 = sectorinformation.ST1; //Load ST1!
					FLOPPY.ST2 = sectorinformation.ST2; //Load ST2!
				}
				FLOPPY_startData();
				return; //Just execute it!
			}
		}

		floppy_errorread: //Error reading data?
		//Plain error reading the sector!
		FLOPPY.ST0 = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Error!
	}
}

OPTINLINE void FLOPPY_formatsector() //Request a read sector command!
{
	char *DSKImageFile;
	SECTORINFORMATIONBLOCK sectorinfo;
	++FLOPPY.sectorstransferred; //A sector has been transferred!
	if (!FLOPPY_supportsrate(FLOPPY_DOR_DRIVENUMBERR) || !FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]) //We don't support the rate or geometry?
	{
		goto floppy_errorformat; //Error out!
	}

	if (!FLOPPY_supportsrate(FLOPPY_DOR_DRIVENUMBERR) || !FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]) //We don't support the rate or geometry?
	{
		FLOPPY.ST0 = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}

	if ((FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->DoubleDensity!=(FLOPPY.DoubleDensity&~DENSITY_IGNORE)) && (!(FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->DoubleDensity&DENSITY_IGNORE) || density_forced) && EMULATE_DENSITY) //Wrong density?
	{
		FLOPPY_LOGD("FLOPPY: Error: Invalid density!")
		FLOPPY.ST0 = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;					
	}

	if ((FLOPPY.commandbuffer[5]!=FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->GAPLength) && (FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->GAPLength!=GAPLENGTH_IGNORE) && EMULATE_GAPLENGTH) //Wrong GAP length?
	{
		FLOPPY.ST0 = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;					
	}


	if (drivereadonly(FLOPPY_DOR_DRIVENUMBERR ? FLOPPY1 : FLOPPY0)) //Read only drive?
	{
		FLOPPY_LOGD("FLOPPY: Finished transfer of data (%u sector(s)).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
		FLOPPY.resultposition = 0;
		FLOPPY.resultbuffer[0] = FLOPPY.ST0;
		FLOPPY.resultbuffer[1] = FLOPPY.ST1;
		FLOPPY.resultbuffer[2] = FLOPPY.ST2;
		FLOPPY.resultbuffer[3] = FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR];
		FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR];
		FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR];
		FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[2]; //Sector size from the command buffer!
		FLOPPY.commandstep = 3; //Move to result phrase and give the result!
		FLOPPY_raiseIRQ(); //Entering result phase!
		return; //Abort!
	}
	else //Writeable disk?
	{
		//Check normal error conditions that applies to all disk images!
		if (FLOPPY_IMPLIEDSEEKENABLER) //Implied seek?
		{
			if (FLOPPY.RWRequestedCylinder<FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->tracks) //Valid track?
			{
				FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.currentcylinder[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.RWRequestedCylinder; //Implied seek!
				FLOPPY_finishseek(FLOPPY_DOR_DRIVENUMBERR); //Simulate seek complete!
				FLOPPY_checkfinishtiming(FLOPPY_DOR_DRIVENUMBERR); //Seek is completed!
			}
		}
		if (FLOPPY.RWRequestedCylinder!=FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR]) //Wrong cylinder to access?
		{
			goto floppy_errorformat; //Error out!
		}

		if (FLOPPY.databuffer[0] != FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR]) //Not current track?
		{
			floppy_errorformat:
			FLOPPY.ST0 = 0x40; //Invalid command!
			FLOPPY.commandstep = 0xFF; //Error!
			return; //Error!
		}
		if (FLOPPY.databuffer[1] != FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR]) //Not current head?
		{
			goto floppy_errorformat;
			return; //Error!
		}
		if (FLOPPY.databuffer[2] != FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR]) //Not current sector?
		{
			goto floppy_errorformat;
			return; //Error!
		}

		//Check disk specific information!
		if ((DSKImageFile = getDSKimage((FLOPPY_DOR_DRIVENUMBERR) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
		{
			if (!readDSKSectorInfo(DSKImageFile, FLOPPY.databuffer[1], FLOPPY.databuffer[0], FLOPPY.databuffer[2], &sectorinfo)) //Failed to read sector information block?
			{
				goto floppy_errorformat;
				return; //Error!
			}

			//Verify sector size!
			if (sectorinfo.SectorSize != FLOPPY.databuffer[3]) //Invalid sector size?
			{
				goto floppy_errorformat;
				return; //Error!
			}

			//Fill the sector buffer and write it!
			memset(FLOPPY.databuffer, FLOPPY.commandbuffer[5], ((size_t)1 << sectorinfo.SectorSize)); //Clear our buffer with the fill byte!
			if (!writeDSKSectorData(DSKImageFile, FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR], sectorinfo.SectorSize, &FLOPPY.databuffer)) //Failed writing the formatted sector?
			{
				goto floppy_errorformat;
				return; //Error!
			}
		}
		else //Are we a normal image file?
		{
			if (FLOPPY.databuffer[3] != 0x2) //Not 512 bytes/sector?
			{
				goto floppy_errorformat;
				return; //Error!
			}
			memset(FLOPPY.databuffer, FLOPPY.commandbuffer[5], 512); //Clear our buffer with the fill byte!
			if (!writedata(FLOPPY_DOR_DRIVENUMBERR ? FLOPPY1 : FLOPPY0, &FLOPPY.databuffer, floppy_LBA(FLOPPY_DOR_DRIVENUMBERR, FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR]),512)) //Failed writing the formatted sector?
			{
				goto floppy_errorformat;
				return; //Error!
			}
		}
	}

	FLOPPY_ST0_CURRENTHEADW(FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR]); //Our idea of the current head!

	if (++FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR] > FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->SPT) //SPT passed? We're finished!
	{
		FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR] = 1; //Reset sector number!
		//Enter result phase!
		FLOPPY.resultposition = 0; //Reset result position!
		FLOPPY.commandstep = 3; //Enter the result phase!
		FLOPPY.resultbuffer[0] = FLOPPY.ST0;
		FLOPPY.resultbuffer[1] = FLOPPY.ST1;
		FLOPPY.resultbuffer[2] = FLOPPY.ST2;
		FLOPPY.resultbuffer[3] = FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR];
		FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR];
		FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR];
		FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[2]; //Sector size from the command buffer!
		return; //Finished!
	}

	//Start transfer of the next sector!
	FLOPPY_startData();
}

OPTINLINE void floppy_writesector() //Request a write sector command!
{
	FLOPPY.databuffersize = translateSectorSize(FLOPPY.commandbuffer[5]); //Sector size into data buffer!
	if (!FLOPPY.commandbuffer[5]) //Special case? Use given info!
	{
		FLOPPY.databuffersize = FLOPPY.commandbuffer[8]; //Use data length!
	}
	FLOPPY.disk_startpos = floppy_LBA(FLOPPY_DOR_DRIVENUMBERR, FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[4]); //The start position, in sectors!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Write sector #%u", FLOPPY.disk_startpos) } //We're reading this sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Sector size: %u bytes", FLOPPY.databuffersize) }
	FLOPPY.disk_startpos *= FLOPPY.databuffersize; //Calculate the start sector!
	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Requesting transfer for %u bytes.", FLOPPY.databuffersize) } //Transfer this many sectors!

	if (FLOPPY.commandstep != 2) { FLOPPY_LOGD("FLOPPY: Write sector: CHS=%u,%u,%u; Params: %02X%02X%02x%02x%02x%02x%02x%02x", FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[4], FLOPPY.commandbuffer[1], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[4], FLOPPY.commandbuffer[5], FLOPPY.commandbuffer[6], FLOPPY.commandbuffer[7], FLOPPY.commandbuffer[8]) } //Log our request!

	if (!(FLOPPY_DOR_MOTORCONTROLR&(1 << FLOPPY_DOR_DRIVENUMBERR))) //Not motor ON?
	{
		FLOPPY_LOGD("FLOPPY: Error: drive motor not ON!")
		FLOPPY.ST0 = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}

	if (!FLOPPY_supportsrate(FLOPPY_DOR_DRIVENUMBERR) || !FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]) //We don't support the rate or geometry?
	{
		FLOPPY.ST0 = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}

	FLOPPY_ST0_UNITSELECTW(FLOPPY_DOR_DRIVENUMBERR); //Current unit!
	FLOPPY_ST0_CURRENTHEADW(FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR] & 1); //Current head!
	FLOPPY_ST0_NOTREADYW(0); //We're not ready yet!
	FLOPPY_ST0_UNITCHECKW(0); //Clear unit check and Interrupt code: we're OK. Also clear SE flag: we're still busy!
	FLOPPY_ST0_SEEKENDW(0); //Clear unit check and Interrupt code: we're OK. Also clear SE flag: we're still busy!
	FLOPPY_ST0_INTERRUPTCODEW(0); //Clear unit check and Interrupt code: we're OK. Also clear SE flag: we're still busy!

	if (!(FLOPPY_DOR_MOTORCONTROLR&(1 << FLOPPY_DOR_DRIVENUMBERR))) //Not motor ON?
	{
		FLOPPY.ST0 = ((FLOPPY.ST0 & 0x3B) | 1) | ((FLOPPY.commandbuffer[3] & 1) << 2); //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}

	FLOPPY_startData(); //Start the DMA transfer if needed!
}

OPTINLINE void floppy_executeWriteData()
{
	char *DSKImageFile = NULL; //DSK image file to use?
	if (!FLOPPY_supportsrate(FLOPPY_DOR_DRIVENUMBERR) || !FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]) //We don't support the rate or geometry?
	{
		FLOPPY_LOGD("FLOPPY: Error: Invalid disk rate/geometry!")
		FLOPPY.ST0 = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;
	}
	if ((FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->DoubleDensity!=(FLOPPY.DoubleDensity&~DENSITY_IGNORE)) && (!(FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->DoubleDensity&DENSITY_IGNORE) || density_forced) && EMULATE_DENSITY) //Wrong density?
	{
		FLOPPY_LOGD("FLOPPY: Error: Invalid density!")
		FLOPPY.ST0 = 0x40; //Abnormal termination!
		FLOPPY.commandstep = 0xFF; //Move to error phase!
		return;					
	}

	if (FLOPPY_IMPLIEDSEEKENABLER) //Implied seek?
	{
		if (FLOPPY.RWRequestedCylinder<FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->tracks) //Valid track?
		{
			FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.currentcylinder[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.RWRequestedCylinder; //Implied seek!
			FLOPPY_finishseek(FLOPPY_DOR_DRIVENUMBERR); //Simulate seek complete!
			FLOPPY_checkfinishtiming(FLOPPY_DOR_DRIVENUMBERR); //Seek is completed!
		}
	}

	if (FLOPPY.RWRequestedCylinder!=FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR]) //Wrong cylinder to access?
	{
		goto floppy_errorwrite; //Error out!
	}

	if (writedata(FLOPPY_DOR_DRIVENUMBERR ? FLOPPY1 : FLOPPY0, &FLOPPY.databuffer, FLOPPY.disk_startpos, FLOPPY.databuffersize)) //Written the data to disk?
	{
		switch (floppy_increasesector(FLOPPY_DOR_DRIVENUMBERR)) //Goto next sector!
		{
		case 1: //OK?
			//More to be written?
			floppy_writesector(); //Write another sector!
			return; //Finished!
		case 2: //Error during transfer?
			//Let the floppy_increasesector determine the error!
			break;
		case 0: //OK?
		default: //Unknown?
			floppy_errorwrite:
			FLOPPY_ST0_SEEKENDW(1); //Successfull write with implicit seek!
			FLOPPY_ST0_INTERRUPTCODEW(0); //Normal termination!
			FLOPPY_ST0_NOTREADYW(0); //We're ready!
			break;
		}
		FLOPPY_LOGD("FLOPPY: Finished transfer of data (%u sector(s)).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
		FLOPPY.resultposition = 0;
		FLOPPY.resultbuffer[0] = FLOPPY.ST0; //ST0!
		FLOPPY.resultbuffer[1] = FLOPPY.ST1; //ST1!
		FLOPPY.resultbuffer[2] = FLOPPY.ST2; //ST2!
		FLOPPY.resultbuffer[3] = FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR];
		FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR];
		FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR];
		FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size from the command buffer!
		FLOPPY.commandstep = 3; //Move to result phrase and give the result!
		FLOPPY_raiseIRQ(); //Entering result phase!
	}
	else
	{
		if (drivereadonly(FLOPPY_DOR_DRIVENUMBERR ? FLOPPY1 : FLOPPY0)) //Read-only drive?
		{
			FLOPPY_LOGD("FLOPPY: Finished transfer of data (readonly).") //Log the completion of the sectors written!
			FLOPPY.resultposition = 0;
			FLOPPY.resultbuffer[0] = FLOPPY.ST0 = ((FLOPPY.ST0 & 0x3B) | 1) | ((FLOPPY.commandbuffer[3] & 1) << 2); //Abnormal termination! ST0!
			FLOPPY.resultbuffer[1] = FLOPPY.ST1; //Drive write-protected! ST1!
			FLOPPY.resultbuffer[2] = FLOPPY.ST2 = 0x00; //ST2!
			FLOPPY.resultbuffer[3] = FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR];
			FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR];
			FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR];
			FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size!
			FLOPPY.commandstep = 3; //Move to result phase!
			FLOPPY_raiseIRQ(); //Entering result phase!
		}
		else //DSK or error?
		{
			if ((DSKImageFile = getDSKimage((FLOPPY_DOR_DRIVENUMBERR) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
			{
				if (writeDSKSectorData(DSKImageFile, FLOPPY.commandbuffer[3], FLOPPY.commandbuffer[2], FLOPPY.commandbuffer[4], FLOPPY.commandbuffer[5], &FLOPPY.databuffersize)) //Read the data into memory?
				{
					switch (floppy_increasesector(FLOPPY_DOR_DRIVENUMBERR)) //Goto next sector!
					{
					case 1: //OK?
						//More to be written?
						floppy_writesector(); //Write another sector!
						return; //Finished!
					case 2: //Error during transfer?
						//Let the floppy_increasesector determine the error!
						break;
					case 0: //OK?
					default: //Unknown?
						FLOPPY_ST0_SEEKENDW(1); //Successfull write with implicit seek!
						FLOPPY_ST0_INTERRUPTCODEW(0); //Normal termination!
						FLOPPY_ST0_NOTREADYW(0); //We're ready!
						break;
					}
					FLOPPY_LOGD("FLOPPY: Finished transfer of data (%u sector(s)).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
					FLOPPY_ST0_SEEKENDW(1); //Successfull write with implicit seek!
					FLOPPY.resultposition = 0;
					FLOPPY.resultbuffer[0] = FLOPPY.ST0 = ((FLOPPY.ST0 & 0x3B) | 1) | ((FLOPPY.commandbuffer[3] & 1) << 2); //Abnormal termination! ST0!
					FLOPPY.resultbuffer[1] = FLOPPY.ST1; //Drive write-protected! ST1!
					FLOPPY.resultbuffer[2] = FLOPPY.ST2 = 0x00; //ST2!
					FLOPPY.resultbuffer[3] = FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR];
					FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR];
					FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR];
					FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size!
					FLOPPY.commandstep = 3; //Move to result phase!
					FLOPPY_raiseIRQ(); //Entering result phase!
					return;
				}
			}
			//Plain error!
			FLOPPY.ST0 = 0x40; //Invalid command!
			FLOPPY.commandstep = 0xFF; //Error!
		}
	}
}

OPTINLINE void floppy_executeReadData()
{
	switch (floppy_increasesector(FLOPPY_DOR_DRIVENUMBERR)) //Goto next sector!
	{
	case 1: //Read more?
		//More to be written?
		floppy_readsector(); //Read another sector!
		return; //Finished!
	case 2: //Error during transfer?
		//Let the floppy_increasesector determine the error!
		break;
	case 0: //OK?
	default: //Unknown?
		FLOPPY_ST0_SEEKENDW(1); //Successfull write with implicit seek!
		FLOPPY_ST0_INTERRUPTCODEW(0); //Normal termination!
		FLOPPY_ST0_NOTREADYW(0); //We're ready!
		break;
	}
	FLOPPY.resultposition = 0;
	FLOPPY.resultbuffer[0] = FLOPPY.ST0;
	FLOPPY.resultbuffer[1] = FLOPPY.ST1;
	FLOPPY.resultbuffer[2] = FLOPPY.ST2;
	FLOPPY.resultbuffer[3] = FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR];
	FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR];
	FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR];
	FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size from the command buffer!
	FLOPPY.commandstep = 3; //Move to result phrase and give the result!
	FLOPPY_LOGD("FLOPPY: Finished transfer of data (%u sectors).", FLOPPY.sectorstransferred) //Log the completion of the sectors written!
	FLOPPY_raiseIRQ(); //Entering result phase!
}

OPTINLINE void floppy_executeData() //Execute a floppy command. Data is fully filled!
{
	switch (FLOPPY.commandbuffer[0]) //What command!
	{
		case WRITE_DATA: //Write sector
		case WRITE_DELETED_DATA: //Write deleted sector
			//Write sector to disk!
			updateFloppyWriteProtected(1,FLOPPY_DOR_DRIVENUMBERR); //Try to write with(out) protection!
			if (FLOPPY.databufferposition == FLOPPY.databuffersize) //Fully buffered?
			{
				floppy_executeWriteData(); //Execute us for now!
			}
			else //Unfinished buffer? Terminate!
			{
				FLOPPY.resultposition = 0;
				FLOPPY.resultbuffer[0] = FLOPPY.ST0 = ((FLOPPY.ST0 & 0x3B) | 1) | ((FLOPPY.commandbuffer[3] & 1) << 2); //Abnormal termination! ST0!
				FLOPPY.resultbuffer[1] = FLOPPY.ST1; //Drive write-protected! ST1!
				FLOPPY.resultbuffer[2] = FLOPPY.ST2; //ST2!
				FLOPPY.resultbuffer[3] = FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR];
				FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR];
				FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR];
				FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size from the command buffer!
				FLOPPY.commandstep = 3; //Move to result phase!
				FLOPPY_raiseIRQ(); //Entering result phase!
			}
			break;
		case READ_TRACK: //Read complete track
		case READ_DATA: //Read sector
		case READ_DELETED_DATA: //Read deleted sector
		case SCAN_EQUAL:
		case SCAN_LOW_OR_EQUAL:
		case SCAN_HIGH_OR_EQUAL:
		case VERIFY: //Verify doesn't transfer data directly!
			//We've finished reading the read data!
			//updateFloppyWriteProtected(0); //Try to read with(out) protection!
			if (FLOPPY.databufferposition == FLOPPY.databuffersize) //Fully processed?
			{
				floppy_executeReadData(); //Execute us for now!
			}
			else //Unfinished buffer? Terminate!
			{
				FLOPPY.resultposition = 0;
				FLOPPY.resultbuffer[0] = FLOPPY.ST0 = ((FLOPPY.ST0 & 0x3B) | 1) | ((FLOPPY.commandbuffer[3] & 1) << 2); //Abnormal termination! ST0!
				FLOPPY.resultbuffer[1] = FLOPPY.ST1; //Drive write-protected! ST1!
				FLOPPY.resultbuffer[2] = FLOPPY.ST2; //ST2!
				FLOPPY.resultbuffer[3] = FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR];
				FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR];
				FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR];
				FLOPPY.resultbuffer[6] = FLOPPY.commandbuffer[5]; //Sector size!
				FLOPPY.commandstep = 3; //Move to result phase!
				FLOPPY_raiseIRQ(); //Entering result phase!
			}
			break;
		case FORMAT_TRACK: //Format sector
			updateFloppyWriteProtected(1,FLOPPY_DOR_DRIVENUMBERR); //Try to write with(out) protection!
			FLOPPY_formatsector(); //Execute a format sector command!
			break;
		default: //Unknown command?
			FLOPPY.commandstep = 0xFF; //Move to error phrase!
			FLOPPY.ST0 = 0x80; //Invalid command!
			break;
	}
}

OPTINLINE void floppy_executeCommand() //Execute a floppy command. Buffers are fully filled!
{
	char *DSKImageFile = NULL; //DSK image file to use?
	SECTORINFORMATIONBLOCK sectorinformation; //Information about the sector!
	FLOPPY.TC = 0; //Reset TC flag!
	FLOPPY.resultposition = 0; //Default: start of the result!
	FLOPPY.databuffersize = 0; //Default: nothing to write/read!
	FLOPPY_LOGD("FLOPPY: executing command: %02X", FLOPPY.commandbuffer[0]) //Executing this command!
	updateFloppyGeometries(FLOPPY_DOR_DRIVENUMBERR, FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR]); //Update the floppy geometries!
	switch (FLOPPY.commandbuffer[0]) //What command!
	{
		case WRITE_DATA: //Write sector
		case WRITE_DELETED_DATA: //Write deleted sector
			FLOPPY.activecommand[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.commandbuffer[0]; //Our command to execute!
			FLOPPY.RWRequestedCylinder = FLOPPY.commandbuffer[2]; //Requested cylinder!
			FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.commandbuffer[3]; //Current head!
			FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.commandbuffer[4]; //Current sector!
			updateST3(FLOPPY_DOR_DRIVENUMBERR); //Update ST3 only!
			floppy_writesector(); //Start writing a sector!
			break;
		case READ_DATA: //Read sector
		case READ_DELETED_DATA: //Read deleted sector
		case SCAN_EQUAL:
		case SCAN_LOW_OR_EQUAL:
		case SCAN_HIGH_OR_EQUAL:
		case VERIFY: //Verify doesn't transfer data directly!
			FLOPPY.activecommand[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.commandbuffer[0]; //Our command to execute!
			FLOPPY.RWRequestedCylinder = FLOPPY.commandbuffer[2]; //Requested cylinder!
			FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.commandbuffer[3]; //Current head!
			FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.commandbuffer[4]; //Current sector!
			updateST3(FLOPPY_DOR_DRIVENUMBERR); //Update ST3 only!
			floppy_readsector(); //Start reading a sector!
			break;
		case SPECIFY: //Fix drive data/specify command
			FLOPPY.DriveData[FLOPPY_DOR_DRIVENUMBERR].data[0] = FLOPPY.commandbuffer[1]; //Set setting byte 1/2!
			FLOPPY.DriveData[FLOPPY_DOR_DRIVENUMBERR].data[1] = FLOPPY.commandbuffer[2]; //Set setting byte 2/2!
			FLOPPY.commandstep = 0; //Reset controller command status!
			updateFloppyWriteProtected(0,FLOPPY_DOR_DRIVENUMBERR); //Try to read with(out) protection!
			//No interrupt, according to http://wiki.osdev.org/Floppy_Disk_Controller
			break;
		case RECALIBRATE: //Calibrate drive
			FLOPPY.commandstep = 0; //Start our timed execution!
			FLOPPY.activecommand[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.commandbuffer[0]; //Our command to execute timing!
			floppytime[FLOPPY_DOR_DRIVENUMBERR] = 0.0;
			floppytimer[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY_steprate(FLOPPY.commandbuffer[1]); //Step rate!
			floppytiming |= (1<<FLOPPY_DOR_DRIVENUMBERR); //Timing!
			FLOPPY.recalibratestepsleft[FLOPPY_DOR_DRIVENUMBERR] = 79; //Up to 79 pulses!
			FLOPPY_MSR_BUSYINPOSITIONINGMODEW(FLOPPY_DOR_DRIVENUMBERR,1); //Seeking!
			if (!FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR]) //Already there?
			{
				FLOPPY_finishrecalibrate(FLOPPY_DOR_DRIVENUMBERR); //Finish the recalibration automatically(we're eating up the command)!
				FLOPPY_checkfinishtiming(FLOPPY_DOR_DRIVENUMBERR); //Finish if required!
			}
			else
			{
				clearDiskChanged(); //Clear the disk changed flag for the new command!
			}
			break;
		case SENSE_INTERRUPT: //Check interrupt status
			//Set result
			updateFloppyWriteProtected(0,FLOPPY_DOR_DRIVENUMBERR); //Try to read with(out) protection!
			FLOPPY.commandstep = 3; //Move to result phrase!
			byte datatemp;
			datatemp = FLOPPY.ST0; //Save default!
			//Reset IRQ line!
			if (FLOPPY.reset_pending) //Reset is pending?
			{
				byte reset_drive;
				reset_drive = FLOPPY.reset_pending_size - (FLOPPY.reset_pending--); //We're pending this drive!
				FLOPPY_LOGD("FLOPPY: Reset Sense Interrupt, pending drive %u/%u...",reset_drive,FLOPPY.reset_pending_size)
				FLOPPY.ST0 &= 0xF8; //Clear low 3 bits!
				FLOPPY_ST0_UNITSELECTW(reset_drive); //What drive are we giving!
				FLOPPY_ST0_CURRENTHEADW(FLOPPY.currenthead[reset_drive] & 1); //Set the current head of the drive!
				FLOPPY_ST0_UNITCHECKW(0); //We're valid, because polling more is valid by default!
				datatemp = FLOPPY.ST0; //Use the current data, not the cleared data!
			}
			else if (!FLOPPY_hadIRQ) //Not an pending IRQ?
			{
				FLOPPY_LOGD("FLOPPY: Warning: Checking interrupt status without IRQ pending! Locking up controller!")
				FLOPPY.ignorecommands = 1; //Ignore commands until a reset!
				FLOPPY.ST0 = 0x80; //Error!
				FLOPPY.commandstep = 0xFD; //Error out! Lock up after reading result!
				return; //Error out now!
			}
			
			FLOPPY_LOGD("FLOPPY: Sense interrupt: ST0=%02X, Currentcylinder=%02X", datatemp, FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR])
			FLOPPY_ST0_INTERRUPTCODEW(3); //Polling more is invalid!
			FLOPPY_ST0_SEEKENDW(0); //Not seeking anymore if we were!
			FLOPPY_ST0_UNITCHECKW(1); //We're invalid, because polling more is invalid!
			FLOPPY_ST0_NOTREADYW(0); //We're ready again!
			FLOPPY.resultbuffer[0] = datatemp; //Give old ST0 if changed this call!
			FLOPPY.resultbuffer[1] = FLOPPY.currentcylinder[FLOPPY_DOR_DRIVENUMBERR]; //Our idea of the current cylinder!
			FLOPPY.resultposition = 0; //Start result!
			FLOPPY.commandstep = 3; //Result phase!
			break;
		case SEEK: //Seek/park head
			FLOPPY.commandstep = 0; //Start our timed execution!
			FLOPPY.activecommand[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.commandbuffer[0]; //Our command to execute!
			FLOPPY.seekdestination[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.commandbuffer[2]; //Our destination!
			FLOPPY.seekrel[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.MT; //Seek relative?
			FLOPPY.seekrelup[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.DoubleDensity; //Seek relative up(when seeking relatively)
			floppytime[FLOPPY_DOR_DRIVENUMBERR] = 0.0;
			floppytiming |= (1<<FLOPPY_DOR_DRIVENUMBERR); //Timing!
			floppytimer[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY_steprate(FLOPPY_DOR_DRIVENUMBERR); //Step rate!
			FLOPPY_MSR_BUSYINPOSITIONINGMODEW(FLOPPY_DOR_DRIVENUMBERR,1); //Seeking!
			if ((FLOPPY_DOR_DRIVENUMBERR<2) && (((FLOPPY.currentcylinder[FLOPPY_DOR_DRIVENUMBERR]==FLOPPY.seekdestination[FLOPPY_DOR_DRIVENUMBERR]) && (FLOPPY.currentcylinder[FLOPPY_DOR_DRIVENUMBERR] < floppy_tracks(disksize(FLOPPY_DOR_DRIVENUMBERR ? FLOPPY1 : FLOPPY0))) && (FLOPPY.seekrel[FLOPPY_DOR_DRIVENUMBERR]==0)) || (FLOPPY.seekrel[FLOPPY_DOR_DRIVENUMBERR] && (FLOPPY.seekdestination[FLOPPY_DOR_DRIVENUMBERR]==0)))) //Found and existant?
			{
				FLOPPY_finishseek(FLOPPY_DOR_DRIVENUMBERR); //Finish the recalibration automatically(we're eating up the command)!
				FLOPPY_checkfinishtiming(FLOPPY_DOR_DRIVENUMBERR); //Finish if required!
			}
			else
			{
				clearDiskChanged(); //Clear the disk changed flag for the new command!
			}
			break;
		case SENSE_DRIVE_STATUS: //Check drive status
			FLOPPY.currenthead[FLOPPY.commandbuffer[1]&3] = (FLOPPY.commandbuffer[1]&4)>>2; //Set the new head from the parameters!
			updateST3(FLOPPY.commandbuffer[1]&3); //Update ST3 only!
			FLOPPY.resultbuffer[0] = FLOPPY.ST3; //Give ST3!
			FLOPPY.resultposition = 0; //Start the result!
			FLOPPY.commandstep = 3; //Result phase!
			break;
		case READ_ID: //Read sector ID
			FLOPPY.activecommand[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.commandbuffer[0]; //Our command to execute!
			if (!FLOPPY_supportsrate(FLOPPY_DOR_DRIVENUMBERR)) //We don't support the rate?
			{
				goto floppy_errorReadID; //Error out!
			}
			FLOPPY.RWRequestedCylinder = FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR]; //Cylinder to access?
			FLOPPY.ST0 = 0x00; //Clear ST0 by default!
			FLOPPY_ST0_UNITCHECKW(0); //Not faulted!
			FLOPPY_ST0_NOTREADYW(0); //Ready!
			FLOPPY_ST0_INTERRUPTCODEW(0); //OK! Correctly executed!
			FLOPPY_ST0_CURRENTHEADW(FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR]&1); //Head!
			FLOPPY_ST0_UNITSELECTW(FLOPPY_DOR_DRIVENUMBERR); //Unit selected!
			if ((DSKImageFile = getDSKimage((FLOPPY_DOR_DRIVENUMBERR) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
			{
				if (readDSKSectorInfo(DSKImageFile, FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR], &sectorinformation)) //Read the sector information too!
				{
					FLOPPY.ST1 = sectorinformation.ST1; //Load ST1!
					FLOPPY.ST2 = sectorinformation.ST2; //Load ST2!
					FLOPPY.resultbuffer[6] = sectorinformation.SectorSize; //Sector size!
				}
				else
				{
					FLOPPY.ST1 = 0x00; //Not found!
					FLOPPY.ST2 = 0x00; //Not found!
					FLOPPY.resultbuffer[6] = 0; //Unknown sector size!
					goto floppy_errorReadID; //Error out!
				}
			}
			else //Normal disk? Generate valid data!
			{
				FLOPPY.ST1 = 0x00; //Clear ST1!
				FLOPPY.ST2 = 0x00; //Clear ST2!
				updateST3(FLOPPY_DOR_DRIVENUMBERR); //Update track 0!
				updateFloppyWriteProtected(0,FLOPPY_DOR_DRIVENUMBERR); //Update write protected related flags!
				if (FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]) //Valid geometry?
				{
					if ((int_32)floppy_LBA(FLOPPY_DOR_DRIVENUMBERR, FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR], FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR]) >= (int_32)(FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->KB * 1024)) //Invalid address within our image!
					{
						goto floppy_errorReadID; //Error out!
					}
				}
				else //No geometry? Always error out!
				{
					goto floppy_errorReadID; //Error out!
				}
				FLOPPY.resultbuffer[6] = 2; //Always 512 byte sectors!
			}
			FLOPPY.resultposition = 0; //Start the result!
			FLOPPY.resultbuffer[0] = FLOPPY.ST0; //ST0!
			FLOPPY.resultbuffer[1] = FLOPPY.ST1; //ST1!
			FLOPPY.resultbuffer[2] = FLOPPY.ST2; //ST2!
			FLOPPY.resultbuffer[3] = FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR]; //Cylinder!
			FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR]; //Head!
			FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR]; //Sector!
			FLOPPY.commandstep = 3; //Result phase!
			FLOPPY_raiseIRQ(); //Entering result phase!
			return; //Correct read!
			floppy_errorReadID:
			FLOPPY.ST0 = 0x40; //Error!
			FLOPPY_ST1_NOADDRESSMARKW(1);
			FLOPPY_ST1_NODATAW(1); //Invalid sector!
			FLOPPY.resultposition = 0; //Start the result!
			FLOPPY.resultbuffer[0] = FLOPPY.ST0; //ST0!
			FLOPPY.resultbuffer[1] = FLOPPY.ST1; //ST1!
			FLOPPY.resultbuffer[2] = FLOPPY.ST2; //ST2!
			FLOPPY.resultbuffer[3] = FLOPPY.physicalcylinder[FLOPPY_DOR_DRIVENUMBERR]; //Cylinder!
			FLOPPY.resultbuffer[4] = FLOPPY.currenthead[FLOPPY_DOR_DRIVENUMBERR]; //Head!
			FLOPPY.resultbuffer[5] = FLOPPY.currentsector[FLOPPY_DOR_DRIVENUMBERR]; //Sector!
			FLOPPY.commandstep = 3; //Result phase!
			FLOPPY_raiseIRQ(); //Entering result phase!
			return; //Incorrect read!
			break;
		case FORMAT_TRACK: //Format sector
			FLOPPY.activecommand[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY.commandbuffer[0]; //Our command to execute!
			if (!(FLOPPY_DOR_MOTORCONTROLR&(1 << FLOPPY_DOR_DRIVENUMBERR))) //Not motor ON?
			{
				FLOPPY_LOGD("FLOPPY: Error: drive motor not ON!")
				FLOPPY.ST0 = 0x40; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Move to error phase!
				return;
			}

			if (!FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]) //No geometry?
			{
				FLOPPY.ST0 = 0x40; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Error!
			}

			if (FLOPPY.commandbuffer[3] != FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->SPT) //Invalid SPT?
			{
				FLOPPY.ST0 = 0x40; //Invalid command!
				FLOPPY.commandstep = 0xFF; //Error!
			}

			if ((DSKImageFile = getDSKimage((FLOPPY_DOR_DRIVENUMBERR) ? FLOPPY1 : FLOPPY0))) //Are we a DSK image file?
			{
				FLOPPY.databuffersize = 4; //We're 4 bytes per sector!
				FLOPPY_startData(); //Start the data transfer!
			}
			else //Normal standard emulated sector?
			{
				if (FLOPPY.commandbuffer[2] != 0x2) //Not 512 bytes/sector?
				{
					FLOPPY.ST0 = 0x40; //Invalid command!
					FLOPPY.commandstep = 0xFF; //Error!
				}
				else
				{
					FLOPPY.databuffersize = 4; //We're 4 bytes per sector!
					FLOPPY_startData(); //Start the data transfer!
				}
			}
			break;
		case VERSION: //Version command?
			FLOPPY.resultposition = 0; //Start our result phase!
			FLOPPY.resultbuffer[0] = 0x90; //We're a 82077AA!
			FLOPPY.commandstep = 3; //We're starting the result phase!
			break;
		case CONFIGURE: //Configuration command?
			//Load our 3 parameters for usage!
			FLOPPY.Configuration.data[0] = FLOPPY.commandbuffer[1];
			FLOPPY.Configuration.data[1] = FLOPPY.commandbuffer[2];
			FLOPPY.Configuration.data[2] = FLOPPY.commandbuffer[3];
			FLOPPY.commandstep = 0; //Finish silently! No result bytes or interrupt!
			break;
		case LOCK: //Lock command?
			FLOPPY.Locked = FLOPPY.MT; //Set/unset the lock depending on the MT bit!
			FLOPPY.resultposition = 0; //Start our result phase!
			FLOPPY.resultbuffer[0] = (FLOPPY.Locked<<4); //Give the lock bit as a result!
			FLOPPY.commandstep = 3; //We're starting the result phase!
			break;
		case DUMPREG: //Dumpreg command
			FLOPPY.resultposition = 0; //Start our result phase!
			FLOPPY.resultbuffer[0] = FLOPPY.currentcylinder[0]; //Give the cylinder as a result!
			FLOPPY.resultbuffer[1] = FLOPPY.currentcylinder[1]; //Give the cylinder as a result!
			FLOPPY.resultbuffer[2] = FLOPPY.currentcylinder[2]; //Give the cylinder as a result!
			FLOPPY.resultbuffer[3] = FLOPPY.currentcylinder[3]; //Give the cylinder as a result!
			FLOPPY.resultbuffer[4] = FLOPPY.DriveData[FLOPPY_DOR_DRIVENUMBERR].data[0]; //Give the cylinder as a result!
			FLOPPY.resultbuffer[5] = FLOPPY.DriveData[FLOPPY_DOR_DRIVENUMBERR].data[1]; //Give the cylinder as a result!
			if (FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR])
			{
				FLOPPY.resultbuffer[6] = (byte)((FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->SPT)&0xFF); //Give the cylinder size in sectors as a result!
			}
			else
			{
				FLOPPY.resultbuffer[6] = 0; //Give the sectors/track!
			}
			FLOPPY.commandstep = 3; //We're starting the result phase!
			FLOPPY_raiseIRQ(); //Give the result!
			break;
		case PERPENDICULAR_MODE:	// * used during initialization, once, maybe
			FLOPPY.ST0 = 0x00; //OK!
			FLOPPY.commandstep = 0; //Ready for a new command!
			FLOPPY_raiseIRQ(); //Give the OK signal!
			break;
		case READ_TRACK: //Read complete track!
			FLOPPY.commandstep = 0xFF; //Move to error phrase!
			FLOPPY.ST0 = 0x40; //Invalid command!
			break;
	}
}

OPTINLINE void floppy_abnormalpolling()
{
	FLOPPY_LOGD("FLOPPY: Abnormal termination because of abnormal polling!")
	FLOPPY_ST0_INTERRUPTCODEW(3); //Abnormal termination by polling!
	FLOPPY_ST0_NOTREADYW(0); //We became not ready!
	FLOPPY.commandstep = 0xFF; //Error!
}

OPTINLINE void floppy_scanbyte(byte fdcbyte, byte cpubyte)
{
	//Bit 2=Seek error(ST3)
	//Bit 3=Seek equal(ST3)
	if ((FLOPPY.ST2&0xC)!=8) return; //Don't do anything on mismatch!
	if ((fdcbyte==cpubyte) || (fdcbyte==0xFF) || (cpubyte==0xFF)) return; //Bytes match?
	//Bytes do differ!
	switch (FLOPPY.commandbuffer[0]) //What kind of mismatch?
	{
	case SCAN_EQUAL: //Equal mismatch?
		FLOPPY_ST2_SEEKERRORW(1); //Seek error!
		FLOPPY_ST2_SEEKEQUALW(0); //Not equal!
		break;
	case SCAN_LOW_OR_EQUAL: //Low or equal mismatch?
		if (fdcbyte<cpubyte)
		{
			FLOPPY_ST2_SEEKERRORW(0);
			FLOPPY_ST2_SEEKEQUALW(0);
		}
		if (fdcbyte>cpubyte)
		{
			FLOPPY_ST2_SEEKERRORW(1);
			FLOPPY_ST2_SEEKEQUALW(0);
		}
		break;
	case SCAN_HIGH_OR_EQUAL: //High or equal mismatch?
		if (fdcbyte<cpubyte)
		{
			FLOPPY_ST2_SEEKERRORW(1);
			FLOPPY_ST2_SEEKEQUALW(0);
		}
		if (fdcbyte>cpubyte)
		{
			FLOPPY_ST2_SEEKERRORW(0);
			FLOPPY_ST2_SEEKEQUALW(0);
		}
		break;
	}
}

OPTINLINE void floppy_writeData(byte value)
{
	byte isscan = 0; //We're not scanning something by default!
	byte commandlength[0x20] = {
		0, //0
		0, //1
		8, //2
		2, //3
		1, //4
		8, //5
		8, //6
		1, //7
		0, //8
		8, //9
		1, //A
		0, //B
		8, //C
		5, //D
		0, //E
		2 //F
		,0 //10
		,8 //11
		,0 //12
		,0 //13
		,0 //14
		,0 //15
		,0 //16
		,0 //17
		,0 //18
		,8 //19
		,0 //1A
		,0 //1B
		,0 //1C
		,8 //1D
		,0 //1E
		,0 //1F
		};
	//TODO: handle floppy writes!
	isscan = 0; //Init scan type!
	switch (FLOPPY.commandstep) //What step are we at?
	{
		case 0: //Command
			if (FLOPPY.ignorecommands) return; //Ignore commands: we're locked up!
			FLOPPY.commandstep = 1; //Start inserting parameters!
			FLOPPY.commandposition = 1; //Start at position 1 with out parameters/data!
			FLOPPY_LOGD("FLOPPY: Command byte sent: %02X", value) //Log our information about the command byte!
			FLOPPY.MT = (value & CMD_EXT_MULTITRACKOPERATION)?1:0; //Multiple track mode?
			FLOPPY.DoubleDensity = (value & CMD_EXT_DOUBLEDENSITYMODE)?1:0; //Multiple track mode?
			FLOPPY.Skip = (value & CMD_EXT_SKIPDELETEDADDRESSMARKS)?1:0; //Multiple track mode?
			FLOPPY.MTMask = 1; //Default: allow the MT bit to be applied during sector calculations!
			value &= 0x1F; //Make sure that the high data is filtered out!
			switch (value) //What command?
			{
				case SENSE_INTERRUPT: //Check interrupt status
				case DUMPREG: //Dumpreg command
				case PERPENDICULAR_MODE:	// * used during initialization, once, maybe
					FLOPPY.commandbuffer[0] = value; //Set the command to use!
					floppy_executeCommand(); //Execute the command!
					break;
				case READ_TRACK: //Read complete track
					FLOPPY.MTMask = 0; //Don't allow the MT bit to be applied during sector calculations!
				case WRITE_DATA: //Write sector
				case WRITE_DELETED_DATA: //Write deleted sector
				case READ_DATA: //Read sector
				case VERIFY: //Verify
				case READ_DELETED_DATA: //Read deleted sector
				case SPECIFY: //Fix drive data
				case SENSE_DRIVE_STATUS: //Check drive status
				case RECALIBRATE: //Calibrate drive
				case SEEK: //Seek/park head
				case READ_ID: //Read sector ID
				case FORMAT_TRACK: //Format sector
				case VERSION: //Version
				case CONFIGURE: //Configure
				case LOCK: //Lock
				case SCAN_EQUAL:
				case SCAN_LOW_OR_EQUAL:
				case SCAN_HIGH_OR_EQUAL:
					FLOPPY.reset_pending = 0; //Stop pending reset if we're pending it: we become active!
					if (FLOPPY.reset_pended) //Finished reset?
					{
						FLOPPY_LOGD("FLOPPY: Reset for all drives has been finished!");
	 					FLOPPY.ST0 = 0x00; //Reset the ST0 register after we've all been read!
						FLOPPY.reset_pended = 0; //Not pending anymore, so don't check for it!
					}
					FLOPPY.commandbuffer[0] = value; //Set the command to use!
					break;
				default: //Invalid command
					FLOPPY_LOGD("FLOPPY: Invalid or unsupported command: %02X",value); //Detection of invalid/unsupported command!
					FLOPPY.ST0 = 0x80; //Invalid command!
					FLOPPY.commandstep = 0xFF; //Error: lockup!
					break;
			}
			break;
		case 1: //Parameters
			FLOPPY_LOGD("FLOPPY: Parameter sent: %02X(#%u/%u)", value, FLOPPY.commandposition, commandlength[FLOPPY.commandbuffer[0]]) //Log the parameter!
			FLOPPY.commandbuffer[FLOPPY.commandposition++] = value; //Set the command to use!
			if (FLOPPY.commandposition > (commandlength[FLOPPY.commandbuffer[0]])) //All parameters have been processed?
			{
				floppy_executeCommand(); //Execute!
				break;
			}
			break;
		case 2: //Data
			switch (FLOPPY.commandbuffer[0]) //What command?
			{
				case SCAN_EQUAL:
				case SCAN_LOW_OR_EQUAL:
				case SCAN_HIGH_OR_EQUAL:
					isscan = 1; //We're scanning instead!
					floppy_scanbyte(FLOPPY.databuffer[FLOPPY.databufferposition++],value); //Execute the scanning!
				case WRITE_DATA: //Write sector
				case WRITE_DELETED_DATA: //Write deleted sector
				case FORMAT_TRACK: //Format track
					if (likely(isscan==0)) //Not Scanning? We're writing to the buffer!
					{
						FLOPPY.databuffer[FLOPPY.databufferposition++] = value; //Set the command to use!
					}
					if (FLOPPY.databufferposition==FLOPPY.databuffersize) //Finished?
					{
						floppy_executeData(); //Execute the command with the given data!
					}
					else //Not completed?
					{
						FLOPPY_dataReady(); //We have data ready to transfer!
						if (FLOPPY_useDMA() && FLOPPY.TC) //DMA mode, Terminal count and not completed? We're ending too soon!
						{
							FLOPPY_LOGD("FLOPPY: Terminal count reached in the middle of a data transfer! Position: %u/%u bytes",FLOPPY.databufferposition,FLOPPY.databuffersize)
							floppy_executeData(); //Execute the command with the given data!
						}
					}
					break;
				default: //Invalid command
					floppy_abnormalpolling(); //Abnormal polling!
					break;
			}
			break;
		case 3: //Result
			floppy_abnormalpolling();
			break; //We don't write during the result phrase!
		case 0xFF: //Error
			floppy_abnormalpolling();
			//We can't do anything! Ignore any writes now!
			break;
		default:
			break; //Unknown status, hang the controller or do nothing!
	}
}

OPTINLINE byte floppy_readData()
{
	byte resultlength[0x20] = {
		0, //0
		0, //1
		7, //2
		0, //3
		1, //4
		7, //5
		7, //6
		0, //7
		2, //8
		7, //9
		7, //a
		0, //b
		7, //c
		7, //d
		7, //e
		7, //f
		1, //10
		7, //11
		0, //12
		0, //13
		0, //14
		0, //15
		7, //16
		0, //17
		0, //18
		7, //19
		0, //1a
		0, //1b
		0, //1c
		7, //1d
		0, //1e
		0 //1f
	};
	byte temp;
	switch (FLOPPY.commandstep) //What step are we at?
	{
		case 0: //Command
			if (FLOPPY.ignorecommands) return 0; //Ignore commands: we're locked up!
			floppy_abnormalpolling(); //Abnormal polling!
			break; //Nothing to read during command phrase!
		case 1: //Parameters
			floppy_abnormalpolling(); //Abnormal polling!
			break; //Nothing to read during parameter phrase!
		case 2: //Data
			switch (FLOPPY.commandbuffer[0]) //What command?
			{
				case READ_TRACK: //Read complete track
				case READ_DATA: //Read sector
				case READ_DELETED_DATA: //Read deleted sector
					temp = FLOPPY.databuffer[FLOPPY.databufferposition++]; //Read data!
					if (FLOPPY.databufferposition==FLOPPY.databuffersize) //Finished?
					{
						floppy_executeData(); //Execute the data finished phrase!
					}
					else //Not completed?
					{
						FLOPPY_dataReady(); //We have data ready to transfer!
						if (FLOPPY_useDMA() && FLOPPY.TC) //DMA mode, Terminal count and not completed? We're ending too soon!
						{
							FLOPPY_LOGD("FLOPPY: Terminal count reached in the middle of a data transfer! Position: %u/%u bytes",FLOPPY.databufferposition,FLOPPY.databuffersize)
							floppy_executeData(); //Execute the command with the given data!
						}
					}
					return temp; //Give the result!
					break;
				default: //Invalid command: we have no data to be READ!
					floppy_abnormalpolling(); //Abnormal polling!
					break;
			}
			break;
		case 3: //Result
			temp = FLOPPY.resultbuffer[FLOPPY.resultposition++]; //Read a result byte!
			switch (FLOPPY.commandbuffer[0]) //What command?
			{
				case READ_TRACK: //Read complete track
				case WRITE_DATA: //Write sector
				case READ_DATA: //Read sector
				case WRITE_DELETED_DATA: //Write deleted sector
				case READ_DELETED_DATA: //Read deleted sector
				case FORMAT_TRACK: //Format sector
				case SENSE_DRIVE_STATUS: //Check drive status
				case RECALIBRATE: //Calibrate drive
				case SENSE_INTERRUPT: //Check interrupt status
				case READ_ID: //Read sector ID
				case VERSION: //Version information!
				case SCAN_EQUAL:
				case SCAN_LOW_OR_EQUAL:
				case SCAN_HIGH_OR_EQUAL:
				case VERIFY:
					FLOPPY_LOGD("FLOPPY: Reading result byte %u/%u=%02X",FLOPPY.resultposition,resultlength[FLOPPY.commandbuffer[0]&0x1F],temp)
					if (FLOPPY.resultposition>=resultlength[FLOPPY.commandbuffer[0]]) //Result finished?
					{
						FLOPPY.commandstep = 0; //Reset step!
					}
					return temp; //Give result value!
					break;
				default: //Invalid command to read!
					floppy_abnormalpolling(); //Abnormal polling!
					break;
			}
			break;
		case 0xFD: //Give result and lockup?
		case 0xFF: //Error or reset result
			if (FLOPPY.commandstep==0xFD) //Lock up now?
			{
				FLOPPY.commandstep = 0xFE; //Lockup!
			}
			else //Reset?
			{
				FLOPPY.commandstep = 0; //Reset step!
			}
			return FLOPPY.ST0; //Give ST0, containing an error!
			break;
		default:
			break; //Unknown status, hang the controller!
	}
	return ~0; //Not used yet!
}

void FLOPPY_finishrecalibrate(byte drive)
{
	//Execute interrupt!
	FLOPPY.currentcylinder[drive] = 0; //Goto cylinder #0 according to the FDC!
	FLOPPY.ST0 = 0x20|drive; //Completed command!
	updateST3(drive); //Update ST3 only!
	if (((FLOPPY_DOR_MOTORCONTROLR&(1<<(drive&3)))==0) || ((drive&3)>1) || (FLOPPY.physicalcylinder[drive]!=0)) //Motor not on or invalid drive?
	{
		FLOPPY.ST0 |= 0x50; //Completed command! 0x10: Unit Check, cannot find track 0 after 79 pulses.
	}
	updateFloppyWriteProtected(0,drive); //Try to read with(out) protection!
	FLOPPY_raiseIRQ(); //We're finished!
	FLOPPY_MSR_BUSYINPOSITIONINGMODEW(drive,0); //Not seeking anymore!
	floppytimer[drive] = 0.0; //Don't time anymore!
}

void FLOPPY_finishseek(byte drive)
{
	FLOPPY.ST0 = 0x20 | (FLOPPY.currenthead[drive]<<2) | drive; //Valid command!
	if (((FLOPPY_DOR_MOTORCONTROLR&(1<<(drive&3)))==0) || ((drive&3)>1)) //Motor not on or invalid drive(which can't finish the seek correctly and provide the signal for completion)?
	{
		FLOPPY.ST0 |= 0x50; //Completed command! 0x10: Unit Check, cannot find track 0 after 79 pulses.
	}
	updateST3(drive); //Update ST3 only!
	FLOPPY_raiseIRQ(); //Finished executing phase!
	floppytimer[drive] = 0.0; //Don't time anymore!
	FLOPPY_MSR_BUSYINPOSITIONINGMODEW(drive,0); //Not seeking anymore!
}

void FLOPPY_checkfinishtiming(byte drive)
{
	if (!floppytimer[drive]) //Finished timing?
	{
		floppytime[drive] = (double)0; //Clear the remaining time!
		floppytiming &= ~(1<<drive); //We're not timing anymore on this drive!
	}
}

//Timed floppy disk operations!
void updateFloppy(double timepassed)
{
	byte drive=0; //Drive loop!
	byte movedcylinder;
	if (unlikely(floppytiming)) //Are we timing?
	{
		do
		{
			if (floppytimer[drive]) //Are we timing?
			{
				floppytime[drive] += timepassed; //We're measuring time!
				for (;(floppytime[drive]>=floppytimer[drive]) && floppytimer[drive];) //Timeout and still timing?
				{
					floppytime[drive] -= floppytimer[drive]; //Time some!
					switch (FLOPPY.activecommand[drive]) //What command is processing?
					{
						case SEEK: //Seek/park head
							updateFloppyWriteProtected(0,drive); //Try to read with(out) protection!
							if ((drive >= 2) /*|| (drive!=(FLOPPY.commandbuffer[1]&3))*/) //Invalid drive specified?
							{
								goto invalidtrackseek; //Error out!
							}
							if (!is_mounted(drive ? FLOPPY1 : FLOPPY0)) //Floppy not inserted?
							{
								FLOPPY.ST0 = 0x20 | (FLOPPY.currenthead[drive]<<2) | drive; //Error: drive not ready!
								clearDiskChanged(); //Clear the disk changed flag for the new command!
								FLOPPY_raiseIRQ(); //Finished executing phase!
								floppytimer[drive] = 0.0; //Don't time anymore!
								FLOPPY_MSR_BUSYINPOSITIONINGMODEW(drive,0); //Not seeking anymore!
								goto finishdrive; //Abort!
							}
						
							if ((FLOPPY.currentcylinder[drive]>FLOPPY.seekdestination[drive] && (FLOPPY.seekrel[drive]==0)) || (FLOPPY.seekrel[drive] && (FLOPPY.seekrelup[drive]==0) && FLOPPY.seekdestination[drive])) //Step out towards smaller cylinder numbers?
							{
								--FLOPPY.currentcylinder[drive]; //Step up!
								if (FLOPPY.physicalcylinder[drive]) --FLOPPY.physicalcylinder[drive]; //Decrease when available!
								movedcylinder = 1;
							}
							else if ((FLOPPY.currentcylinder[drive]<FLOPPY.seekdestination[drive] && (FLOPPY.seekrel[drive]==0)) || (FLOPPY.seekrel[drive] && FLOPPY.seekrelup[drive] && FLOPPY.seekdestination[drive])) //Step in towards bigger cylinder numbers?
							{
								++FLOPPY.currentcylinder[drive]; //Step down!
								if (FLOPPY.physicalcylinder[drive]<FLOPPY.geometries[drive]->tracks) ++FLOPPY.physicalcylinder[drive]; //Increase when available!
								movedcylinder = 1;
							}
							else movedcylinder = 0; //We didn't move?

							updateST3(drive); //Update ST3 only!

							//Check if we're there!
							if ((drive<2) && (((FLOPPY.currentcylinder[drive]==FLOPPY.seekdestination[drive]) && (FLOPPY.currentcylinder[drive] < floppy_tracks(disksize(drive ? FLOPPY1 : FLOPPY0))) && (FLOPPY.seekrel[drive]==0)) || (FLOPPY.seekrel[drive] && (FLOPPY.seekdestination[drive]==0)))) //Found and existant?
							{
								FLOPPY_finishseek(drive); //Finish!
								goto finishdrive; //Give an error!
							}
							else if (movedcylinder==0) //Reached no destination?
							{
								invalidtrackseek:
								//Invalid track?
								FLOPPY.ST0 = (FLOPPY.ST0 & 0x30) | 0x00 | drive; //Valid command! Just don't report completion(invalid track to seek to)!
								FLOPPY.ST2 = 0x00; //Nothing to report! We're not completed!
								FLOPPY_raiseIRQ(); //Finished executing phase!
								floppytimer[drive] = 0.0; //Don't time anymore!
								FLOPPY_MSR_BUSYINPOSITIONINGMODEW(drive,0); //Not seeking anymore!
								goto finishdrive;
							}
							break;
						case RECALIBRATE: //Calibrate drive
							if (FLOPPY.physicalcylinder[drive] && (drive<2)) //Not there yet?
							{
								--FLOPPY.physicalcylinder[drive]; //Step down!
							}
							if (((FLOPPY.physicalcylinder[drive]) || (drive>=2)) && FLOPPY.recalibratestepsleft[drive]) //Not there yet?
							{
								--FLOPPY.recalibratestepsleft[drive];
							}
							else //Finished? Track 0 might be found!
							{
								FLOPPY_finishrecalibrate(drive); //Finish us!
								goto finishdrive;
							}
							break;
						case VERIFY: //Executing verify validation of data?
							++FLOPPY.databufferposition; //Read data!
							if (FLOPPY.databufferposition==FLOPPY.databuffersize) //Finished?
							{
								floppytimer[drive] = 0.0; //Don't time anymore!
								floppy_executeData(); //Execute the data finished phrase!
								goto finishdrive; //Finish!
							}
							//Continue while busy!
							break;
						default: //Unsupported command?
							if ((FLOPPY.commandstep==2) && FLOPPY_useDMA() && (FLOPPY.DMAPending&2) && (drive==FLOPPY_DOR_DRIVENUMBERR)) //DMA transfer busy on this channel?
							{
								FLOPPY.DMAPending &= ~2; //Start up DMA again!
								floppytimer[drive] = FLOPPY_DMA_TIMEOUT; //How long for a DMA transfer to take?
							}
							else //Unsupported?
							{
								floppytimer[drive] = 0.0; //Don't time anymore!
								goto finishdrive;
							}
							break; //Don't handle us yet!
					}
				}
				finishdrive:
				FLOPPY_checkfinishtiming(drive); //Check for finished timing!
			}
		} while (++drive<4); //Process all drives!
	}
}

byte getfloppydisktype(byte floppy)
{
	if (FLOPPY.geometries[floppy]) //Gotten a known geometry?
	{
		return FLOPPY.geometries[floppy]->boardjumpersetting; //Our board jumper settings for this drive!
	}
	return FLOPPYTYPE_12MB; //Default to the default AT controller to fit all!
}

byte PORT_IN_floppy(word port, byte *result)
{
	if ((port&~7) != 0x3F0) return 0; //Not our port range!
	byte temp;
	switch (port & 0x7) //What port?
	{
	case 0: //diskette EHD controller board jumper settings (82072AA)!
		//Officially only on AT systems, but use on XT as well for proper detection!
		//Create floppy flags!
		temp = getfloppydisktype(3); //Floppy #3!
		temp <<= 2;
		temp = getfloppydisktype(2); //Floppy #2!
		temp <<= 2;
		temp = getfloppydisktype(1); //Floppy #1!
		temp <<= 2;
		temp = getfloppydisktype(0); //Floppy #0!
		FLOPPY_LOGD("FLOPPY: Read port Diskette EHD controller board jumper settings=%02X",temp);
		*result = temp; //Give the result!
		return 1; //Used!
		break;
	//IBM PC XT supports DOR, MSR and Data ports. AT also supports DIR and CCR registers.
	case 2: //DOR?
		*result = FLOPPY.DOR; //Give the DOR!
		return 1; //Used!
		break;
	case 3: //Tape Drive register (82077AA)?
		temp = 0x20; //No drive present here by default!
		if (FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]) //Nothing there?
		{
			temp = FLOPPY.geometries[FLOPPY_DOR_DRIVENUMBERR]->TapeDriveRegister; //What format are we?
		}
		FLOPPY_LOGD("FLOPPY: Read port Tape Drive Register=%02X",temp);
		*result = temp; //Give the result!
		return 1; //Used!
		break;
	case 4: //MSR?
		updateFloppyMSR(); //Update the MSR with current values!
		FLOPPY_LOGD("FLOPPY: Read MSR=%02X",FLOPPY.MSR)
		*result = FLOPPY.MSR; //Give MSR!
		return 1;
	case 5: //Data?
		//Process data!
		*result = floppy_readData(); //Read data!
		return 1;
	case 7: //DIR?
		if (is_XT==0) //AT?
		{
			updateFloppyDIR(); //Update the DIR register!
			FLOPPY_LOGD("FLOPPY: Read DIR=%02X", FLOPPY.DIR)
			*result = FLOPPY.DIR; //Give DIR!
			return 1;
		}
		break;
	default: //Unknown port?
		break;
	}
	//Not one of our ports?
	return 0; //Unknown port!
}

OPTINLINE void updateMotorControl()
{
	EMU_setDiskBusy(FLOPPY0, FLOPPY_DOR_MOTORCONTROLR & 1); //Are we busy?
	EMU_setDiskBusy(FLOPPY1, (FLOPPY_DOR_MOTORCONTROLR & 2) >> 1); //Are we busy?
}

byte PORT_OUT_floppy(word port, byte value)
{
	if ((port&~7) != 0x3F0) return 0; //Not our address range!
	switch (port & 0x7) //What port?
	{
	case 2: //DOR?
		FLOPPY_LOGD("FLOPPY: Write DOR=%02X", value)
		FLOPPY.DOR = value; //Write to register!
		updateMotorControl(); //Update the motor control!
		FLOPPY_handlereset(0); //Execute a reset by DOR!
		return 1; //Finished!
	case 4: //DSR?
		if (is_XT==0) //AT?
		{
			FLOPPY_LOGD("FLOPPY: Write DSR=%02X", value)
			FLOPPY.DSR = value; //Write to register to check for reset first!
			FLOPPY_handlereset(1); //Execute a reset by DSR!
			if (FLOPPY_DSR_SWRESETR) FLOPPY_DSR_SWRESETW(0); //Reset requested? Clear the reset bit automatically!
			FLOPPY_handlereset(1); //Execute a reset by DSR if needed!
			FLOPPY_CCR_RATEW(FLOPPY_DSR_DRATESELR); //Setting one sets the other!
			return 1; //Finished!
		}
	case 5: //Data?
		FLOPPY_hadIRQ = FLOPPY.IRQPending; //Was an IRQ Pending?
		FLOPPY_lowerIRQ(); //Lower the IRQ!
		floppy_writeData(value); //Write data!
		return 1; //Default handler!
	case 7: //CCR?
		if (is_XT==0) //AT?
		{
			FLOPPY_LOGD("FLOPPY: Write CCR=%02X", value)
			FLOPPY.CCR = value; //Set CCR!
			FLOPPY_DSR_DRATESELW(FLOPPY_CCR_RATER); //Setting one sets the other!
			return 1;
		}
		break;
	default: //Unknown port?
		break; //Unknown port!
	}
	//Not one of our ports!
	return 0; //Unknown port!
}

//DMA logic

void DMA_floppywrite(byte data)
{
	floppy_writeData(data); //Send the data to the FDC!
}

byte DMA_floppyread()
{
	return floppy_readData(); //Read data!
}

void FLOPPY_DMADREQ() //For checking any new DREQ signals!
{
	DMA_SetDREQ(FLOPPY_DMA, (FLOPPY.commandstep == 2) && FLOPPY_useDMA() && (FLOPPY.DMAPending==1)); //Set DREQ from hardware when in the data phase and using DMA transfers and not busy yet(pending)!
}

void FLOPPY_DMADACK() //For processing DACK signal!
{
	DMA_SetDREQ(FLOPPY_DMA,0); //Stop the current transfer!
	FLOPPY.DMAPending |= 2; //We're not pending anymore, until timed out!
	floppytimer[FLOPPY_DOR_DRIVENUMBERR] = FLOPPY_DMA_TIMEOUT; //Time the timeout for floppy!
	floppytiming |= (1<<FLOPPY_DOR_DRIVENUMBERR); //Make sure we're timing on the specified disk channel!
}

void FLOPPY_DMATC() //Terminal count triggered?
{
	FLOPPY.TC = 1; //Terminal count triggered!
}

void initFDC()
{
	density_forced = (is_XT==0); //Allow force density check if 286+ (non XT)!
	memset(&FLOPPY, 0, sizeof(FLOPPY)); //Initialise floppy!
	FLOPPY.Configuration.data[0] = 0; //Default!
	FLOPPY.Configuration.data[1] = 0x60; //Implied seek enable, FIFO disable, Drive polling mode enable, no treshold(0)
	FLOPPY.Configuration.data[2] = 0; //No write precompensation!

	//Initialise DMA controller settings for the FDC!
	DMA_SetDREQ(FLOPPY_DMA,0); //No DREQ!
	registerDMA8(FLOPPY_DMA, &DMA_floppyread, &DMA_floppywrite); //Register our DMA channels!
	registerDMATick(FLOPPY_DMA, &FLOPPY_DMADREQ, &FLOPPY_DMADACK, &FLOPPY_DMATC); //Our handlers for DREQ, DACK and TC!

	//Set basic I/O ports
	register_PORTIN(&PORT_IN_floppy);
	register_PORTOUT(&PORT_OUT_floppy);
	register_DISKCHANGE(FLOPPY0, &FLOPPY_notifyDiskChanged);
	register_DISKCHANGE(FLOPPY1, &FLOPPY_notifyDiskChanged);

	memset(&floppytime,0,sizeof(floppytime));
	memset(&floppytimer,0,sizeof(floppytimer)); //No time spent or in-use by the floppy disk!
	floppytiming = 0; //We're not timing!
	initFloppyRates(); //Initialize the floppy disk rate tables to use!
}
