#include "headers/cpu/cpu.h" //Need CPU comp.
#include "headers/support/lba.h" //Need some support for LBA!
#include "headers/bios/io.h" //Basic I/O comp.
#include "headers/cpu/easyregs.h" //Easy register functionality last!
#include "headers/debugger/debugger.h" //For logging registers debugging!

//Are we disabled?
#define __HW_DISABLED 0

//Look at AX bit 0 of int 11h

//Extern data!
//EXT=part of the 13h Extensions which were written in the 1990s to support HDDs with more than 8GB.

/*


INT 41: Hard disk 0 Parameter table address
INT 46: Hard disk 0 copy

INT 41 points by default to memory F000:E401h

Drives 0x80@INT 41; 0x81-0x83 may follow.
		    Check by test INT 46 points tomewhere other than 16 bytes pas INT 41 and sixteen bytes starting at offset 10h are identical to int 46.
			-> or: INT 46<>INT 41+0x10 AND (INT 46 till INT 46+0x10) = offset 10h till offset 10h+10
			-> so:
				format of each table order:
				Offset:
				0x00: Primary Master
				0x10: Primary Slave
				0x20: Secondary Master
				0x30: Secondary Slave

				cylinders=0=Not set!



Format of fixed disk parameters:

Offset Size    Description     (Table 03196)
00h    WORD    number of cylinders
02h    BYTE    number of heads
03h    WORD    starting reduced write current cylinder (XT only, 0 for others)
05h    WORD    starting write precompensation cylinder number
07h    BYTE    maximum ECC burst length (XT only)
08h    BYTE    control byte (see #03197,#03198)
09h    BYTE    standard timeout (XT only, 0 for others)
0Ah    BYTE    formatting timeout (XT and WD1002 only, 0 for others)
0Bh    BYTE    timeout for checking drive (XT and WD1002 only, 0 for others)
0Ch    WORD    cylinder number of landing zone (AT and later only)
0Eh    BYTE    number of sectors per track (AT and later only)
0Fh    BYTE    reserved

Bitfields for XT fixed disk control byte:

Bit(s)  Description     (Table 03197)
2-0    drive step speed.
000  3ms.
100  200ms.
101  70ms (default).
110  3ms.
111  3ms
5-3    unused
6      disable ECC retries
7      disable access retries


Bitfields for AT fixed disk control byte:

Bit(s)  Description     (Table 03198)
0      unused
1      reserved (0)  (disable IRQ)
2      reserved (0)  (no reset)
3      set if more than 8 heads
4      always 0
5      set if manufacturer's defect map on max cylinder+1  (AT and later only)
6      disable ECC retries
7      disable access retries


*/

extern IODISK disks[6]; //All mounted disks!
byte mounteddrives[0x100]; //All mounted drives!


//Support for HDD CHS:

//Bytes per Sector
#define HDD_BPS 512
//Sectors per track
#define HDD_SPT 63
//Heads
#define HDD_HEADS 16

//# of Cylinders based on filesize
#define CYLINDERS(x) SAFEDIV(SAFEDIV(SAFEDIV(x,HEADS),SPT),BPS)

//# of Sectors based on filesize
#define SECTORS(x) (x/HDD_BPS)

#define KB(x) (x/1024)




/*
	{  KB,SPT,SIDES,TRACKS,  }
	{ 160,  8, 1   , 40   , 0},
	{ 180,  9, 1   , 40   , 0},
	{ 200, 10, 1   , 40   , 0},
	{ 320,  8, 2   , 40   , 1},
	{ 360,  9, 2   , 40   , 1},
	{ 400, 10, 2   , 40   , 1},
	{ 720,  9, 2   , 80   , 3},
	{1200, 15, 2   , 80   , 2},
	{1440, 18, 2   , 80   , 4},
	{2880, 36, 2   , 80   , 6},

*/

//BPS=512 always!

int floppy_spt(uint_64 floppy_size)
{
	switch (KB(floppy_size)) //Determine by size!
	{
	case 160:
	case 320:
		return 8;
	case 180:
	case 360:
	case 720:
		return 9;
	case 200:
	case 400:
		return 10;
	case 1200:
		return 15;
	case 1440:
		return 18;
	case 2880:
		return 36;
	default:
		return 18; //Default: 1.44MB floppy!
	}
}

int floppy_tracks(uint_64 floppy_size)
{
	switch (KB(floppy_size)) //Determine by size!
	{
	case 160:
	case 180:
	case 200:
	case 320:
	case 360:
	case 400:
		return 40;
	case 720:
	case 1200:
	case 1440:
	case 2880:
		return 80;
	default:
		return 80; //Default: 1.44MB floppy!
	}
}

int floppy_sides(uint_64 floppy_size)
{
	switch (KB(floppy_size)) //Determine by size!
	{
	case 160:
	case 180:
	case 200:
		return 1; //Only one side!
	case 320:
	case 360:
	case 400:
	case 720:
	case 1200:
	case 1440:
	case 2880:
		return 2; //Two sides!
	default:
		return 2; //Default: 1.44MB floppy!
	}
}

byte buffer[512]; //Our buffer!

int killRead; //For Pirates! game?

void int13_init(int floppy0, int floppy1, int hdd0, int hdd1, int cdrom0, int cdrom1)
{
	if (__HW_DISABLED) return; //Abort!	
	//We don't need to worry about file sizes: automatically done at Init!
	killRead = 0; //Init killRead!
	//Reset disk parameter table to 0's

	//Now set mounted drives according to set!
	memset(mounteddrives,0xFF,sizeof(mounteddrives)); //Reset all to unmounted!
	
	//Floppy=optional!
	if (floppy0) //Floppy0?
	{
		mounteddrives[0] = FLOPPY0; //Floppy0!
	}
	if (floppy0 && floppy1) //Floppy0&1?
	{
		mounteddrives[1] = FLOPPY1; //Floppy1!
	}
	else if (floppy1) //Floppy1 only?
	{
		mounteddrives[0] = FLOPPY1; //Floppy1 on floppy0!
	}

	//HDD=Dynamic!
	byte hdd;
	hdd = 0x80; //Init to first drive!
	if (hdd0) //Have?
	{
		mounteddrives[hdd++] = HDD0; //Set!
	}
	if (hdd1) //Have?
	{
		mounteddrives[hdd++] = HDD1; //Set!
	}
	//2 CD-ROM drives: solid and optional!
	if (cdrom0)
	{
		mounteddrives[hdd++] = CDROM0; //Set!
	}
	if (cdrom1)
	{
		mounteddrives[hdd++] = CDROM1; //Set!
	}
}

byte getdiskbymount(int drive) //Drive to disk converter (reverse of int13_init)!
{
	if (mounteddrives[0]==drive)
	{
		return 0; //Floppy 0 usually!
	}
	else if (mounteddrives[1]==drive)
	{
		return 1; //Floppy 1 usually!
	}
	else if (mounteddrives[0x80]==drive)
	{
		return 0x80; //HDD/CDROM
	}
	else if (mounteddrives[0x81]==drive)
	{
		return 0x81; //HDD/CDROM
	}
	else if (mounteddrives[0x82]==drive)
	{
		return 0x82; //HDD/CDROM
	}
	else if (mounteddrives[0x83]==drive)
	{
		return 0x83; //HDD/CDROM
	}
	return 0xFF; //Unknown disk!
}

uint_64 disksize(int disknumber)
{
	if (disknumber<0 || disknumber>6) return 0; //Not used!
	return disks[disknumber].size; //Get the size of the disk!
}

uint_64 floppy_LBA(int floppy,word head, word track, word sector)
{
	return ((((head*floppy_tracks(disksize(floppy))+track)*floppy_spt(disksize(floppy)))+sector-1)<<9); //Give LBA for floppy!
}


word gethddheads(uint_64 disksize)
{
	if (disksize<=(1000*63*16*512)) //1-504MB?
	{
		return 16; //16 heads!
	}
	else if (disksize<=(1000*63*32*512)) //504-1008MB?
	{
		return 32; //32 heads!
	}
	else if (disksize<=(1000*63*64*512)) //1008-2016MB?
	{
		return 64; //64 heads!
	}
	else if (disksize<=4128768000LL) //2016-4032MB?
	{
		return 128; //128 heads!
	}
	//4032-8032.5MB?
	return 255; //255 heads!
}

word gethddspt()
{
	return 63; //Always 63 SPT!
}

word gethddbps()
{
	return 512; //Always 512 bytes per sector!
}

word gethddcylinders(uint_64 disksize)
{
	return disksize/(gethddspt()*gethddheads(disksize)*gethddbps());
}









void getDiskGeometry(byte disk, word *heads, word *cylinders, uint_64 *sectors, uint_64 *bps)
{
	word head;
	word sector;
	if ((disk==FLOPPY0) || (disk==FLOPPY1)) //Floppy0 or Floppy1?
	{
		uint_64 oursize;
		oursize = disksize(disk); //Get size!
		*heads = floppy_sides(oursize);
		*cylinders = floppy_tracks(oursize);
		*sectors = floppy_spt(oursize);
		*bps = 512;
	}
	else if ((disk==HDD0) || (disk==HDD1) || (disk==CDROM0) || (disk==CDROM1)) //HDD0 or HDD1?
	{
		LBA2CHS(disksize(disk),cylinders,&head,&sector,gethddheads(disksize(disk)),SECTORS(disksize(disk))); //Convert to emulated value!
		*heads = (uint_64)head;
		*sectors = (uint_64)sector; //Transfer rest!
		*bps = 512; //Assume 512 Bytes Per Sector!
	}
	else //Unknown disk?
	{
		*heads = 0;
		*cylinders = 0;
		*sectors = 0;
		*bps = 0;
		CF = 1; //Set carry flag!
	}
}

byte GetBIOSType(byte disk)
{
//Not harddrive? Get from geometry list!
	return 0; //Else: only HDD type!
}

//Status flags for I/O!
byte last_status; //Status of last operation
byte last_drive; //Last drive something done to

byte readdiskdata(uint_32 startpos)
{
	byte buffer[512]; //A sector buffer to read!
	//Detect ammount of sectors to be able to read!
	word sectors;
	sectors = AL; //Number of sectors to be read!
	byte readdata_result;
	readdata_result = 1;
	while (sectors && !readdata_result)
	{
		readdata_result = disksize(mounteddrives[DL])>=(startpos+(sectors<<9)); //Have enough data to read this many sectors?
		if (!readdata_result) //Failed to read this many?
		{
			--sectors; //One sector less, etc.
		}
	}

	word sector;
	sector = 0; //Init sector!
	word position; //Current position in memory!
	if (!readdata_result)
	{
		last_status = 0x00;
		CF = 1; //Error!
		return 0; //Abort!
	}
	else //Ready to read?
	{
		position = BX; //Current position to write to!
		for (;sectors;) //Sectors left to read?
		{
			//Read from disk
			readdata_result = readdata(mounteddrives[DL],&buffer,startpos+(sector<<9),512); //Read the data to the buffer!
			if (!readdata_result) //Error?
			{
				last_status = 0x00;
				CF = 1; //Error!
				return sector; //Abort with ammount of sectors read!
			}
			//Sector is read, now write it to memory!
			word left;
			left = 512; //Data left!
			word current = 0; //Current byte in the buffer!
			for (;;)
			{
				MMU_wb(CPU_SEGMENT_ES,ES,position,buffer[current]); //Write the data to memory!
				if (!left--) goto nextsector; //Stop when nothing left!
				++current; //Next byte in the buffer!
				++position; //Next position in memory!
			}
			nextsector: //Process next sector!
			--sectors; //One sector processed!
			++sector; //Process to the next sector!
		}
	}
	
	dolog("int13","Read %i/%i sectors from drive %02X, start %i. Requested: Head: %i, Track: %i, Sector: %i. Disk size: %i bytes",sector,AL,DL,startpos,DH,CH,CL&0x3F,disksize(mounteddrives[DL]));
	return sector; //Give the ammount of sectors read!
}

byte writediskdata(uint_32 startpos)
{
	byte buffer[512]; //A sector buffer to read!
	//Detect ammount of sectors to be able to read!
	word sectors;
	sectors = AL; //Number of sectors to be read!
	byte writedata_result;
	writedata_result = 1;
	while (sectors && !writedata_result)
	{
		writedata_result = disksize(mounteddrives[DL])>=(startpos+(sectors<<9)); //Have enough data to read this many sectors?
		if (!writedata_result) //Failed to read this many?
		{
			--sectors; //One sector less, etc.
		}
	}

	word position; //Current position in memory!
	word sector;
	sector = 0; //Init sector!
	if (!writedata_result)
	{
		last_status = 0x00;
		CF = 1; //Error!
		return 0; //Abort!
	}
	else //Ready to read?
	{
		position = BX; //Current position to read from!
		for (;sectors;) //Sectors left to read?
		{
			//Fill the buffer!
			word left;
			left = 512; //Data left!
			word current = 0; //Current byte in the buffer!
			for (;;)
			{
				buffer[current] = MMU_rb(CPU_SEGMENT_ES,ES,position,0); //Read the data from memory (no opcode)!
				if (!left--) goto dosector; //Stop when nothing left!
				++current; //Next byte in the buffer!
				++position; //Next position in memory!
			}
			dosector: //Process next sector!
			//Write to disk!
			writedata_result = writedata(mounteddrives[DL],&buffer,startpos+(sector<<9),512); //Write the data to the disk!
			if (!writedata_result) //Error?
			{
				last_status = 0x00;
				CF = 1; //Error!
				return sector; //Abort!
			}
			--sectors; //One sector processed!
			++sector; //Process to the next sector!
		}
	}
	
	dolog("int13","Written %i/%i sectors from drive %02X, start %i. Requested: Head: %i, Track: %i, Sector: %i. Disk size: %i bytes",sector,AL,DL,startpos,DH,CH,CL&0x3F,disksize(mounteddrives[DL]));
	return sector; //Ammount of sectors read!
}

















//Now the functions!

/*Structure used in function description:*/
//Information
//CPU.registers
/*
Return values
*/

void int13_00() //OK!
{
//Reset Disk Drive
//DL=Drive

	/*
	CF=Set on Error
	*/
	last_status = 0x00; //Reset status!
	CF = 0; //No carry flag: OK!
}

void int13_01()
{
//Check Drive Status
//DL=Drive
//Bit 7=0:floppy;Else fixed.

	/*
	AL:
	00: Successful
	01: Invalid function in AH or invalid parameter
	02: Cannot Find Address Mark
	03: Attemted Write On Write Protected Disk
	04: Sector Not Found/read error
	05: Reset Failed (hdd)
	06: Disk change line 'active' (disk changed (floppy))
	07: Drive parameter activity failed. (hdd)
	08: DMA overrun
	09: Attemt to DMA over 64kb boundary
	0A: Bad sector detected (hdd)
	0B: Bad cylinder (track) detected (hdd)
	0C: Unsupported track or invalid media
	0D: Invalid number of sectors on format (PS/2 hdd)
	0E: Control data adress mark detected (hdd)
	0F: DMA arbitration level out of range (hdd)
	10: Uncorrectable CRC or ECC error on read
	11: Data ECC corrected (hdd)
	20: Controller failure
	31: No media in drive (IBM/MS int 13 extensions)
	32: Incorrect drive type stored in CMOS (Compaq)
	40: Seek failure
	80: Drive timed out, assumed not ready
	AA: Drive not ready
	B0: Volume not locked in drive (int13 EXT)
	B1: Volume locked in drive (int13 EXT)
	B2: Volume not removable (int13 EXT)
	B3: Volume in use (int13 EXT)
	B4: Lock count exceeded (int13 EXT)
	B5: Valid eject request failed (int13 EXT)
	B6: Volume present but read protected (int13 EXT)
	BB: Undefined error (hdd)
	CC: Write fault (hdd)
	E0: Status error (hdd)
	FF: Sense operation failed (hdd)

	CF: Set on error, no error=cleared.

	*/

	if (last_status!=0x00)
	{
		dolog("int13","Last status: %02X",last_status);
		AH = last_status;
		CF = 1;
	}
	else
	{
		dolog("int13","Last status: unknown");	
		AH = 0;
		CF = 0;
	}
}

void int13_02()
{
//Read Sectors From Drive
//AL=Sectors To Read Count
//CH=Track
//CL=Sector
//DH=Head
//DL=Drive
//ES:BX=Buffer Address Pointer

//HDD:
//cylinder := ( (CX and 0xFF00) shr 8 ) or ( (CX and 0xC0) shl 2)
//sector := CX and 63;
	
	uint_64 startpos; //Start position in image!
	word cylinder;
	word sector;
	if (!AL) //No sectors to read?
	{
		dolog("int13","Nothing to read specified!");
		last_status = 0x01;
		CF = 1;
		return; //Abort!
	}

	if (!has_drive(mounteddrives[DL])) //No drive image loaded?
	{
		dolog("int13","Media not mounted:%02X!",DL);
		last_status = 0x31; //No media in drive!
		CF = 1;
		return;
	}

	switch (mounteddrives[DL]) //Which drive?
	{
	case FLOPPY0: //Floppy 1
	case FLOPPY1: //Floppy 2
		debugger_logregisters(); //Log a register dump!
		startpos = floppy_LBA(mounteddrives[DL],DH,CH,CL&0x3F); //Floppy LBA!
		AL = readdiskdata(startpos); //Read the data to memory!
		break; //Done with floppy!
	case HDD0: //HDD1
	case HDD1: //HDD2
		cylinder = ((CX&0xFF00)>>8)|((CX&0xC0)<<2);
		sector = CX&63; //Starts at 1!
		if (!sector) //Sector 0 is invalid?
		{
			last_status = 0x00;
			CF = 1; //Error!
			return; //Break out!
		}
		startpos = CHS2LBA(cylinder,DH,sector,HDD_HEADS,SECTORS(disksize(mounteddrives[DL]))); //HDD LBA!

		AL = readdiskdata(startpos); //Read the data to memory!
		break; //Done with HDD!
	default:
		CF = 1;
		last_status = 0x40; //Seek failure!
		return; //Unknown disk!
	}
	AH = 0; //Reset AH!

	//Beyond 8GB: Use function 42h

	/*
	CF:Set on error, not error=cleared.
	AH=Return code
	AL=Actual Sectors Read Count
	*/
}

void int13_03()
{
//Write Sectors To Drive
//AL=Sectors To Write Count
//CH=Track
//CL=Sector
//DH=Head
//DL=Drive
//ES:BX=Buffer Address Pointer


	if (!has_drive(mounteddrives[DL])) //No drive image loaded?
	{
		last_status = 0x31; //No media in drive!
		CF = 1;
		return;
	}

	uint_64 startpos; //Start position in image!
	word cylinder;
	word sector;
	AL = 0; //Default: none written!
	switch (mounteddrives[DL]) //Which drive?
	{
	case 0x00: //Floppy 1
	case 0x01: //Floppy 2
		startpos = floppy_LBA(mounteddrives[DL],DH,CH,CL); //Floppy LBA!

		AL = writediskdata(startpos); //Write the data to memory!
		break; //Done with floppy!
	case 0x80: //HDD1
	case 0x81: //HDD2
		cylinder = ((CX&0xFF00)>>8)|((CX&0xC0)<<2);
		sector = CX&63;
		startpos = CHS2LBA(cylinder,DH,sector-1,HDD_HEADS,SECTORS(disksize(mounteddrives[DL]))); //HDD LBA!

		AL = writediskdata(startpos); //Write the data to memory!
		break; //Done with HDD!
	default:
		CF = 1;
		last_status = 0x40; //Seek failure!
		return; //Unknown disk!
	}
	AH = 0; //Reset AH!

	/*
	CF: Set On Error; Clear If No Error
	AH=Return code
	AL=Actual Sectors Written Count
	*/
}















void int13_04()
{
//Verify Sectors From Drive
//AL=Sectors To Verify Count
//CH=Track
//CL=Sector
//DH=Head
//DL=Drive
//ES:BX=Buffer Address Pointer

	/*
	CF: Set On Error, Clear On No Error
	AH=Return code
	AL=Actual Sectors Verified Count
	*/

	AH = 0; //Reset!
	CF = 0; //Default: OK!

	if (!AL) //NO sectors?
	{
		AH = 0x01;
		CF = 1;
		return;
	}

	if (!has_drive(mounteddrives[DL])) //No drive image loaded?
	{
		last_status = 0x31; //No media in drive!
		CF = 1;
		return;
	}


	int i;
	int sectorverified; //Sector verified?
	byte sectorsverified; //Sectors verified?
	sectorsverified = 0; //Default: none verified!
	for (i=0; i<AL; i++) //Process sectors!
	{

		uint_64 startpos; //Start position in image!
		int readdata_result = 0;
		word cylinder;
		word sector;
		int t;

		switch (mounteddrives[DL]) //Which drive?
		{
		case 0x00: //Floppy 1
		case 0x01: //Floppy 2
			startpos = floppy_LBA(mounteddrives[DL],DH,CH,CL); //Floppy LBA!

			//Detect ammount of sectors to be able to read!
			readdata_result = readdata(mounteddrives[DL],&buffer,startpos+(i<<9),512); //Read the data to memory!
			if (!readdata_result) //Read OK?
			{
				last_status = 0x05; //Error reading?
				CF = 1; //Error!
			}
			sectorverified = 1; //Default: verified!
			for (t=0; t<512; t++)
			{
				if (buffer[t]!=MMU_rb(CPU_SEGMENT_ES,ES,BX+(i<<9)+t,0)) //Error?
				{
					sectorverified = 0; //Not verified!
					break; //Stop checking!
				}
			}
			if (sectorverified) //Verified?
			{
				++sectorsverified; //Verified!
			}
			break; //Done with floppy!
		case 0x80: //HDD1
		case 0x81: //HDD2
			cylinder = ((CX&0xFF00)>>8)|((CX&0xC0)<<2);
			sector = CX&63;
			startpos = CHS2LBA(cylinder,DH,sector,HDD_HEADS,SECTORS(disksize(mounteddrives[DL]))); //HDD LBA!

			readdata_result = readdata(mounteddrives[DL],&buffer,startpos,512); //Write the data from memory!
			if (!readdata_result) //Read OK?
			{
				last_status = 0x05; //Error reading?
				CF = 1; //Error!
			}
			sectorverified = 1; //Default: verified!
			for (t=0; t<512; t++)
			{
				if (buffer[t]!=MMU_rb(CPU_SEGMENT_ES,ES,BX+(i<<9)+t,0)) //Error?
				{
					sectorverified = 0; //Not verified!
					break; //Stop checking!
				}
			}
			if (sectorverified) //Verified?
			{
				++sectorsverified; //Verified!
			}
			break; //Done with HDD!
		default:
			CF = 1;
			last_status = 0x40; //Seek failure!
			return; //Unknown disk!
		}
	}
	AL = sectorsverified; //Sectors verified!
	AH = 0; //No error code?
}

void int13_05()
{
//Format Track
//AL=Sectors To Format Count
//CH=Track
//CL=Sector
//DH=Head
//DL=Drive
//ES:BX=Buffer Address Pointer

	/*
	CF: Set On Error, Clear If No Error
	AH=Return code
	*/



}

void int13_06()
{
//Format Track Set Bad Sector Flags
//AL=Interleave
//CH=Track
//CL=Sector
//DH=Head
//DL=Drive
	/*
	CF: Set On Error, Clear If No Error
	AH=Return code
	*/
}

void int13_07()
{
	int13_06(); //Same!
}

void int13_08()
{
//Read Drive Parameters
//DL=Drive index (1st HDD =80h; 2nd=81h; else floppy)
//ES:DI=Set to 0000:0000 to workaround some buggy BIOS
	/*

	[bits A:B] = bits A to B of this value

	CF: Set on Error, CLear If No Error
	AH=Return code
	DL=Number of Hard Disk Drives
	DH=Logical last index of heads = number_of - 1 (index starts with 0)
	CX=[bits 7:6][bits 15:8] logical last index of cylinders = numbert_of - 1 (because starts with 0)
	   [bits 5:0] logical last index of sectors per track = number_of (because index starts with 1)
	BL=Drive type:
		01h: 360k
		02h: 1.2M
		03h: 720k
		04h: 1.44M
		05h: ??? (obscure drive type on IBM, 2.88M on at least AMI 486 BIOS)
		06h: 2.88M
		10h: ATAPI Removable Media Device
	*/
//status&ah=0x07&CF=1 on invalid drive!

	if (!has_drive(mounteddrives[DL])) //No drive image loaded?
	{
		last_status = 0x31; //No media in drive!
		CF = 1;
		return;
	}

	AX = 0x00;
	BL = GetBIOSType(mounteddrives[DL]);

	CF = 0; //Reset carry flag by default!

	word tmpheads, tmpcyl;
	uint_64 tmpsize, tmpsect;

	getDiskGeometry(mounteddrives[DL],&tmpheads,&tmpcyl,&tmpsect,&tmpsize); //Get geometry!

	if (CF) //Error within disk geometry (unknown disk)?
	{
		return; //STOP!
	}

	if (tmpcyl!=0) --tmpcyl; //Cylinder count -> max!
	
	if (mounteddrives[DL]==FLOPPY0 || mounteddrives[DL]==FLOPPY1) //Floppy?
	{
	}
	else //HDD, custom format?
	{
		CH = (byte)(tmpcyl&0xFF);
		CL = (byte)(((tmpcyl>>2)&0xC0) | (tmpsect * 0x3F));
		DH = (byte)tmpheads;
		last_status = 0x00;
	}
	if (DL&0x80) //Harddisks
	{
		DL = 0;
		if (has_drive(HDD0)) ++DL;
		if (has_drive(HDD1)) ++DL;
	}
	else //Floppy disks?
	{
		DL = 0;
		if (has_drive(FLOPPY0)) ++DL;
		if (has_drive(FLOPPY1)) ++DL;
	}
	CF = 0;
}

void int13_09()
{
//HDD: Init Drive Pair Characteristics
//DL=Drive
	/*
	CF: Set On Error, Clear If No Error
	AH=Return code
	*/
}

void int13_0A()
{
//HDD: Read Long Sectors From Drive
//See function 02, but with bonus of 4 bytes ECC (Error Correction Code: =Sector Data Checksum)
}

void int13_0B()
{
//HDD: Write Long Sectors To Drive
}

void int13_0C()
{
//HDD: Move Drive Head To Cylinder
}

void int13_0D()
{
//HDD: Reset Disk Drives
}

void int13_0E()
{
//For Hard Disk on PS/2 system only: Controller Read Test
}

void int13_0F()
{
//For Hard Disk on PS/2 system only: Controller Write Test
}

void int13_10()
{
//HDD: Test Whether Drive Is Ready
}

void int13_11()
{
//HDD: Recalibrate Drive
	AH = 0x00;
	CF = 0;
}

void int13_12()
{
//For Hard Disk on PS/2 system only: Controller RAM Test
}

void int13_13()
{
//For Hard Disk on PS/2 system only: Drive Test
}

void int13_14()
{
//HDD: Controller Diagnostic
}

void int13_15()
{
//Read Drive Type
}

void int13_16()
{
//Floppy disk: Detect Media Change
}

void int13_17()
{
//Floppy Disk: Set Media Type For Format (used by DOS versions <= 3.1)
	killRead = TRUE;
	AH = 0x00;
	CF = 0;
}

void int13_18()
{
//Floppy Disk: Set Media Type For Format (used by DOS versions <= 3.2)
}

void int13_19()
{
//Park Heads
}

void int13_41()
{
//EXT: Check Extensions Present
//DL=Drive index (1st HDD=80h etc.)
//BX=55AAh
	/*
	CF: Set On Not Present, Clear If Present
	AH=Error Code or Major Version Number
	BX=AA55h
	CX=Interfact support bitmask:
		1: Device Access using the packet structure
		2: Drive Locking and Ejecting
		4: Enhanced Disk Drive Support (EDD)
	*/
}

void int13_42()
{
//EXT: Extended Read Sectors From Drive
//DL=Drive index (1st HDD=80h etc.)
//DS;SI=Segment:offset pointer to the DAP:
	/*
		DAP:
		Offset range	Size	Description
		00h		1byte	size of DAP = 16 = 10h
		01h		1byte	unused, should be 0
		02h		2bytes	number of sectors to be read
		04h		4bytes	Segment:offset pointer to the memory buffer; note that x86 has first offset bytes (word), next segment!
		08h		8bytes	Absolute number of the start of the sectors to be read (first sector of drive has number 0)
	*/
	/*
	CF: Set on Error, Clear if No Error
	AH=Return code
	*/
}

void int13_43()
{
//EXT: Write Sectors To Drive
}

void int13_44()
{
//EXT: Verify Sectors
}

void int13_45()
{
//EXT: Lock/Unlock Drive
}

void int13_46()
{
//EXT: Eject Drive
}

void int13_47()
{
//EXT: Move Drive Head To Sector
}

void int13_48()
{
//EXT: Extended Read Drive Parameters
//DL=Drive index (1st HDD=80h etc.)
//DS:SI=Segment:offset pointer to Result Buffer, see below
	/*
	Result buffer:
		Offset:	Size:	Description:
		00h	2bytes	Size of Result Buffer = 30 = 1Eh
		02h	2bytes	information flags
		04h	4bytes	physical number of cylinders = last index + 1 (index starts with 0)
		08h	4bytes	physical number of heads = last index + 1 (index starts with 0)
		0Ch	4bytes	physical number of sectors per track = last index (index starts with 1)
		10h	8bytes	absolute number of sectors = last index + 1 (index starts with 0)
		18h	2bytes	bytes per sector
		1Ah	4bytes	Optional pointer to Enhanced Disk Drive (EDD) configuration parameters which may be used for subsequent interrupt 13h extension calls (if supported)
	*/
	/*
	CF: Set On Error, Clear If No Error
	AH=Return code
	*/
//Remark: Physical CHS values of function 48h may/should differ from logical values of function 08h
}

void int13_49()
{
//EXT: Detect Media Change
}

void int13_unhandled()
{
	last_status = 0x01; //Unknown command!
	CF = 1;
}

void int13_unimplemented()
{
	last_status = 0x01; //Unimplemented is unhandled in essence!
	CF = 1;
}

Handler int13Functions[0x50] =
{
//0x00
	int13_00,
	int13_01,
	int13_02,
	int13_03,
	int13_04,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_08,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
//0x10
	int13_11,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_17,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
//0x20
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
//0x30
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
//0x40
	int13_unhandled,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unimplemented,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled,
	int13_unhandled
}; //Interrupt 13h functions!

void BIOS_int13() //Interrupt #13h (Low Level Disk Services)!
{
	if (__HW_DISABLED) return; //Abort!
	if (AH<NUMITEMS(int13Functions)) //Within range of functions support?
	{
		dolog("int13","Function %02X called.",AH); //Log our function call!
		int13Functions[AH](); //Execute call!
	}
	else
	{
		dolog("int13","Unknown call: %02X",AH); //Unknown call!
//Unknown int13 call?
		last_status = 1; //Status: Invalid command!
		CF = 1; //Set carry flag to indicate error
	}
}