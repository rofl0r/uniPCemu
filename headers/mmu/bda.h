#ifndef BDA_H
#define BDA_H

#include "headers/types.h" //We need types!

typedef struct
{
	byte x;
	byte y;
} VideoPageCursor; //Cursor position!

typedef struct
{
	byte StartRow; //Start row of cursor!
	byte EndRow; //End row of cursor!
} CursorShape; //Cursor shape!

typedef struct
{
	byte LastAccess : 3; //Last access:
	/*
	0=Trying 360k media in 360k drive.
	1=Trying 360k media in 1.2m drive.
	2=Trying 1.2m media in 1.2m drive.
	3=Known 360k media on 360k drive.
	4=Known 360k media in 1.2m drive.
	5=Known 1.2M media in 1.2M drive
	6=Not used.
	7=720K media in 720K drive or 1.44M media in 1.44M drive
	*/
	byte NotUsed : 1;
	byte MediaKnown : 1; //1=Known, 0=Unknown
	byte DoubleSteppingRequired : 1; //1=Required, 0=Not required
	byte TransferRate : 2; //0=500Kbit/s, 1=300Kb/s, 2=250Kb/s, 3=1Mb/s
} DISKETTEMEDIASTATE;

typedef struct
{
	byte ChangeLineDetection : 1; //1=Change line detection, 0=Not changed.
	byte DriveMultirateStatus : 1; //1=Drive is multirate, 0=Drive isn't multirate.
	byte DriveDetermined : 1; //1=Determined, 0=Not determined.
	byte NotUsed : 3;
	byte DataTransferRate : 2; //0=500Kbit/s, 1=300Kb/s, 2=250Kb/s, 3=1Mb/s
	/*
	*/
} DISKETTEOPERATIONALSTARTINGSTATUS;

//Info: bits are going from up-down=bit 0-7. (Reversed of BIOSCentral info, so read bottom-up!)

typedef union
{
	struct
	{
//Ports (0 for unset)

		word COM_BaseAdress[4]; //Base adresses COM1-4!
		word LPT_BaseAdress[4]; //Base adresses LPT1-4!

//Equipment

		struct
		{
			byte BootFloppyInstalled : 1; //Boot floppy installed (0=Not installed, 1=Installed)
			byte MathCOProcessorInstalled : 1; //Match Coprocessor installed (1=Installed, 0=Not installed)
			byte PS2MouseInstalled : 1; //PS/2 Mouse installed (1=Installed, 0=Not installed)
			byte NotUsedOnPS2 : 1; //Not used on PS/2
			byte VideoMode : 2; //00: EGA+; 1=color 40x25, 2=color 80x25, 3=Mono 80x25
			byte FloppyDrives : 2; //Number of floppy drives installed (plus 1 (0=1,1=2,2=3,3=4 drives)
			byte NoDMAInstalled : 1; //0 if DMA installed.
			byte SerialPorts : 3; //Number of serial ports installed (0+).
			byte GameAdapterInstalled : 1; //1 if installed
			byte InternalPS2Modem : 1; //1 if installed.
			byte ParallelPorts : 2; //Number of parallel ports installed (00=1;01=2;03=3)
		} Equipment;


		byte IF_ManufacturingTest; //Interrupt Flag - Manufacturing test

//Memory

		word MemorySize_KB; //Memory size in Kb
		word ErrorCodes_AdapterMemorySizePCXT; //Error codes for AT+; Adapter memory size for PC and XT

//Keyboard

		struct
		{
			union
			{
				struct
				{
					byte RightShiftDown : 1; //1=Down, 0=Up
					byte LeftShiftDown : 1; //1=Down, 0=Up
					byte ControlKeyDown : 1; //1=Down, 0=Up
					byte AltKeyDown : 1; //1=Down, 0=Up
					byte ScrollLockOn : 1; //1=On, 0=Off
					byte NumLockOn : 1; //1=On, 0=Off
					byte CapsLockOn : 1; //1=On, 0=Off
					byte InsertOn : 1; //1=On, 0=Off
				};
				byte data; //Value!
			};
		} KeyboardShiftFlags1;

		struct
		{
			union
			{
				struct
				{
					byte RightAltKeyDown : 1; //1=Down, 0=Up
					byte LeftAltKeyDown : 1; //1=Down, 0=Up
					byte SysRegKeyDown : 1; //1=Down, 0=Up
					byte PauseKeyActive : 1; //1=Active, 0=Inactive
					byte ScrollLockDown : 1; //1=Down, 0=Up
					byte NumLockDown : 1; //1=Down, 0=Up
					byte CapsLockDown : 1; //1=Down, 0=Up
					byte InsertKeyDown : 1; //1=Down, 0=Up
				};
				byte data; //Value!
			};
		} KeyboardShiftFlags2;

		byte AltNumpadWordArea;

		ptr32 NextCharacterInKeyboardBuffer;
		ptr32 LastCharacterInKeyboardBuffer;
		byte KeyboardBuffer[0x20]; //Keyboard buffer

//Floppy/HDD controller

		struct
		{
//Below values: 1=Calibrated, 0=Not calibrated.
			byte FloppyDrive0 : 1; //Floppy drive 0
			byte FloppyDrive1 : 1; //Floppy drive 1
			byte FloppyDrive2_PCXT : 1; //Floppy drive 2 (PC,XT)
			byte FloppyDrive3_PCXT : 1; //Floppy drive 3 (PC,XT)
			byte Reserved : 4; //Reserved
		} FloppyDiskDriveCalibrationStatus; //Floppy disk drive calibration status

		struct
		{
			byte Drive0Motor : 1; //1=On, 0=Off
			byte Drive1Motor : 1; //1=On, 0=Off
			byte Drive2Motor : 1; //1=On, 0=Off
			byte Drive3Motor : 1; //1=On, 0=Off
			byte DriveSelect : 2; //0=Drive 0, 1=Drive 1, 2=Drive 2 (PC,XT), 3=Drive 3 (PC,XT)
			byte NotUsed : 1; //Not used.
			byte CurrentOperation : 1; //0=Read/Verify; 1=Write/Format.
		} FloppyDiskDriveMotorStatus; //Floppy disk drive motor status

		byte FloppyDiskDriveMotorTimeout; //Floppy disk drive motor time-out

		struct
		{
			byte ErrorCode : 5; //Error codes:
			/*
			0=No errors
			1=Illegal function requested
			2=Address mark not found
			3=Write Protect error
			4=Sector not found
			5=Diskette change line active
			8=DMA overrun
			9=DMA boundary error
			10=Unknown media type
			16=CRC error during read
			*/
			byte FloppyDiskControllerTestFail : 1; //0=Passed, 1=Failed.
			byte SeekError : 1; //1=Seek error was detected, 0=No seek error detected.
			byte DriveNotReady : 1; //1=Drive not ready (time out), 0=Drive ready.
		} FloppyDiskDriveStatus; //Floppy disk drive status

		struct
		{
			byte DriveSelect : 2; //Drive select (0-3)
			byte HeadStateOnInterrupt : 1; //Which disk (0 or 1).
			byte DriveNotReady : 1; //1=Ready, 0=Not ready
			byte DriveFault : 1; //1=Drive fault, 0=No drive fault
			byte SeekCommandCompleted : 1; //1=Seek command completed, 0=Not completed.
			byte InterruptCode : 2; //Interrupt code:
			/*
			0=Command completed normally.
			1=Command terminated abnormally
			2=Abnormal termination, ready line on, or diskette changed
			3=Seek command not completed
			*/
		} HDD_and_FloppyControllerStatus0; //Hard disk and floppy controller status register 0

		struct
		{
			byte MissingAddressMark : 1; //1=indicated, 0=Not indicated.
			byte MediumWriteProtected : 1; //1=Write Protected, 0=Writable.
			byte SectorNotFound_or_ReadingDisketteIDFailed : 1; //1 on occur, 0 on not occurred
			byte NotUsed1 : 1;
			byte DMAOverrun : 1; //1=Occurred, 0=Not occurred
			byte CRCErrorDuringRead : 1; //1=CRC error, 0=No error
			byte NotUsed2 : 1;
			byte AttemptedAccessBeyondLastCylinder : 1; //1=Indicated, 0=Not indicated.
		} FloppyControllerStatus1; //Floppy drive controller status register 1

		struct
		{
			byte AddressMarkNotFoundDuringRead : 1; //1=Indicatd, 0=Not indicated
			byte BadCylinder : 1; //See above.
			byte SectorNotFoundDuringVerify : 1; //Idem.
			byte ConditionOfEqualDuringVerify : 1; //Idem.
			byte WrongCylinder : 1; //Idem.
			byte CRCErrorDetected : 1; //Idem.
			byte DeletedDataAddressMark : 1; //Idem.
			byte NotUsed : 1; //Not used: 0.
		} FloppyControllerStatus2; //Floppy drive controller status register 2

		byte FloppyDiskController_CylinderNumber; //Floppy disk controller: cylinder number
		byte FloppyDiskController_HeadNumber; //Floppy disk controller: head number
		byte FloppyDiskController_SectorNumber; //Floppy disk controller: sector number
		byte FloppyDiskController_BytesWrittenNumber; //Floppy disk controller: number of byte written

//Video Block

		byte ActiveVideoMode; //Active video mode setting
		word ActiveVideoMode_TextColumnsPerRow; //Number of textcolumns per row for the active video mode
		word ActiveVideoMode_Size; //Size of active video mode in Page bytes.
		word ActiveVideoMode_Offset; //Offset of the active video page relative to the start of VRAM.
		VideoPageCursor CursorPosition[8]; //Cursor positions!
		CursorShape CursorShape; //Cursor shape!
		byte ActiveVideoPage; //Active video page
		word VideoDisplayAdapter_IOPort; //I/O Port for the video display adapter CRT Controller!
		struct
		{
			byte Mode23TestOperation : 1; //0=Mode 4/5 Graphics Operation; 1=Mode 2/3 Test Operation.
			byte Mode45GraphicsOperation : 1; //1=Mode 4/5 Graphics Operation; 0=Disabled
			byte MonoOperation : 1; //0=Color operation; 1=Monochrome operation
			byte VideoOn : 1; //1=Video signal enabled, 0=Video signal disabled
			byte Mode6GraphicsOperation : 1; //1=Enabled, 0=Disabled
			byte Attribute : 1; //0=background intensity, 1=blinking
			byte Notused1 : 2; //Not used!
		} VideoDisplayAdapter_InternalModeRegister; //Video display adapter internal mode register
		struct
		{
			byte Blue : 1; //Indicats blue
			byte Green : 1; //Indicates green
			byte Red : 1; //Indicates red
			byte BorderOrBgColor : 1; //Intensified border color (mode 2) and bgcolor (mode 5)
			byte BackgroundIntensified : 1; //Background intensity: 0=Normal; 1=Intensified
			byte Mode5ForeGroundColors : 1; //Mode 5 foreground colors: 0=Green/red/yellow; 1=CVyan/magenta/white
			byte NotUsed : 2;
		} ColorPalette;
		word Video_AdapterROM_Offset; //Offset of Adapter ROM
		word Video_AdapterROM_Segment; //Segment of Adapter ROM

//Stuff for interrupts
		struct
		{
			byte IRQ0 : 1; //1=Occurred, 0=Not occurred.
			byte IRQ1 : 1; //Idem.
			byte IRQ2 : 1; //Idem.
			byte IRQ3 : 1; //...
			byte IRQ4 : 1; //...
			byte IRQ5 : 1; //...
			byte IRQ6 : 1; //...
			byte IRQ7 : 1; //...
		} LastInterrupt; //Last interrupt (not PC)
		uint_32 Int1ACounter; //Counter for Interupt 1Ah
		byte Timer24HourFlag; //Timer 24 hour flag
		byte KeyboardCtrlBreakFlag; //Keyboard Ctrl-Break flag
		word SoftResetFlag; //Soft reset flag

//HDD info

		byte LastHDDOperationStatus; //Status of last hard disk operation
		/*
		00h = no errors
		01h = invalid function requested
		02h = address mark not found
		04h = sector not found
		05h = reset failed
		06h = removable media changed
		07h = drive parameter activity failed
		08h = DMA overrun
		09h = DMA boundary overrun
		0Ah = bad sector flag detected
		0Bh = bad track detected
		0Dh = invalid number of sectors on format
		0Eh = control data address mark detected
		0Fh = DMA arbitration level out of range
		10h = uncorrectable ECC or CRC error
		11h = ECC corrected data error
		20h = general controller failure
		40h = seek operation failed
		80h = timeout
		AAh = drive not ready
		BBh = undefined error occurred
		CCh = write fault on selected drive
		E0h = status error or error register is zero
		FFh = sense operation failed
		*/
		byte NumHDDs; //Number of hard disk drives

		struct
		{
			byte NotUsed1 : 3;
			byte MoreThan8Heads : 1; //1=Drive has more than 8 heads, 0=Drive has less than 8 heads
			byte NotUsed2 : 2;
			byte EnableRetriesOnDiskError : 1; //1=Enabled, 0=Disabled
			byte DisalbeRetriesOnDiskError : 1; //0=Disabled, 1=Enabled.
		} HDDControlByte; //Hard disk control byte
		word HardDiskIOPort; //Offset address of hard disk I/O port (XT)

//Ports

		byte ParrallelPortTimeout[4]; //Parallel port 1-4 timeout (4(PC,XT) support for virtual DMA services (VDS))
		struct
		{
			byte NotUsed1 : 3;
			byte INT4BChainingRequired : 1; //Interrupt 4Bh chaining required?
			byte NotUsed2 : 1;
			byte VirtualDMAServicesSupported : 1; //1=Supported, 0=Not supported.
			byte NotUsed : 2;
		} SerialPortTimeout[4]; //Serial port 1-4 timeout

//Keyboard

		word KeyboardBufferStartAddress; //Starting address of keyboard buffer
		word KeyboardBufferEndAddress; //Ending address of keyboard buffer

//Video

		byte NumberOfVideoRows; //Number of video rows (minus 1)
		byte NumberOfScanlinesPerCharacter; //Number of scan lines per character
		struct
		{
			byte AlphaNumericCursorEmulation : 1; //1=Enabled, 0=Disabled.
			byte MonochomeMonitor : 1; //1=Monochome, 0=Color
			byte Reserved : 1;
			byte VideoSubsystemActive : 1; //1=Active, 0=Not active
			byte VideoDisplayAdapterMemorySize : 3; //0=64KB, 1=128KB, 2=256KB, 3=512KB, 4=1MB+
			byte LastVideoModeClearedDisplayBuffer : 1; //See bit 7 of last video mode.
		} VideoDisplayAdapterOptions; //Video display adapter options
		struct
		{
			byte AdapterTypeSwitchSettings : 4;
			/*
			0000b = MDA/color 40x25
			0001b = MDA/color 80x25
			0010b = MDA/high-resolution 80x25
			0011b = MDA/high-resolution enhanced
			0100b = CGA 40x25/monochrome
			0101b = CGA 80x25/monochrome
			0110b = color 40x25/MDA
			0111b = color 80x25/MDA
			1000b = high-resolution 80x25/MDA
			1001b = high-resolution enhanced/MDA
			1010b = monochrome/CGA 40x25
			1011b = monochrome/CGA 80x25
			*/
			byte NotUsed : 2;
			byte FeatureConnectorLine0State : 1;
			byte FeatureConnectorLine1State : 1;
		} VideoDisplayAdapterSwitches; //Video display adapter switches
		struct
		{
			byte VGAActive : 1; //1=Active, 0=Inactive
			byte GrayScaleSummingEnabled : 1; //1=Enabled, 0=Disabled.
			byte MonochomeMonitor : 1; //1=Mono, 0=Color
			byte ScanlineMode_Low : 1;
			byte DefaultPaletteLoadingEnabled : 1; //1=Enabled, 0=Disabled.
			byte Reserved : 1;
			byte DisplaySwitchEnabled : 1; //1=Enabled, 0=Disabled.
			byte ScanlineMode_High : 1;
			/*
			ScanlineMode_High<<1|ScanlineMode_Low: scanline mode:
			0=350-line mode
			1=400-line mode
			2=200-line mode
			*/
		} VGAVideoFlags1; //VGA video flags 1
		byte VGAVideoFlags2; //VGA video flags 2

//Floppy/HDD

//Hier gebleven!
		struct
		{
			byte notused : 2;
			byte DataRate : 2;
			byte LastDriveSteprateSend : 2; //8-VALUE=Ammount of ms
			byte LastDataSent : 2; //0=500kbit/s; 1=300kb/s; 2=250kb/s; 3=Not set or 1Mb/s
		} FloppyDiskConfigData; //Floppy disk configuration data

		struct
		{
			byte Error : 1; //1=Error in previous command, 0=No error.
			byte IndexPulseActive : 1; //1=Active, 0=Inactive
			byte DataCorrected : 1; //1=Corrected, 0=Not corrected.
			byte DataRequestActive : 1; //1=Active, 0=Inactive.
			byte SeekComplete : 1; //1=Seek complete, 0=Selected seeking.
			byte WriteFaultOccurred : 1; //1=Occurred, 0=Not occurred.
			byte DriveReady : 1; //1=Drive selected ready, 0=Drive selected not ready.
			byte ControllerBusy : 1; //1=Controller busy, 0=Controller not busy.
		} HDDControllerStatus; //Hard disk drive controller status

		struct
		{
			byte AddressMarkNotFound : 1; //1=Not found, 0=Not used
			byte DriveTrackError : 1; //1=Track 0 not found.
			byte CommandAborted : 1; //1=Command aborted.
			byte MediaChangeRequested : 1; //1=Media change requested.
			byte ID_or_TargetSectorNotFound : 1; //1=ID or Target Sector not found.
			byte MediaChanged : 1; //1=Media changed, 0=No change
			byte UncorrectableECCError : 1; //1=Uncorrectable ECC error occurred.
			byte BadSectorDetected : 1; //1=Bad sector detected.
		} HDDDriveError; //Hard disk drive error

		byte HDDDriveTaskCompleteFlag; //Hard disk drive task complete flag

		struct
		{
			byte Diskette0ChangeLineDetection : 1; //1=Yes, 0=No
			byte Drive0MultirateStatus : 1; //1=Yes, 0=No
			byte Drive0TypeDetermination : 1; //1=Determined, 0=Not determined
			byte NotUsed1 : 1; //Not used
			byte Diskette1ChangeLineDetection : 1; //1=Yes, 0=No
			byte Drive1MultirateStatus : 1; //1=Yes, 0=No
			byte Drive1TypeDetermination : 1; //1=Determined, 0=Not determined
			byte NotUsed2 : 1; //Not used
		} FloppyDiskDriveInfo; //Floppy disk drive information

		DISKETTEMEDIASTATE DisketteMediaState[2]; //Diskette 0&1 media state
		DISKETTEOPERATIONALSTARTINGSTATUS DisketteOperationalStartingState[2]; //Diskette 0&1 operational starting state
		byte Diskette0CurrentCylinder; //Diskette 0 current cylinder
		byte Diskette1CurrentCylinder; //Diskette 1 current cylinder

//Keyboard

		struct
		{
			byte LastScancodeWasE1 : 1; //Boolean, where 1=True
			byte LastScancodeWasE0 : 1; //Boolean, where 1=True
			byte RightControlKeyActive : 1; //1=Active, 0=Not active
			byte RightAltKeyActive : 1; //1=Active, 0=Not active
			byte KeyboardIsnt101_or_102Keys : 1; //1=Non-101/102 key keyboard, 0=101/102 key keyboard.
			byte ForcedNumLockOn : 1; //1=Forced NumLock On!
			byte LastCodeWasFirstIDCharacter : 1; //1=Last code was first ID character
			byte ReadingTwoByteKeyboardIDInProcess : 1; //1=Reading two byte keyboard ID in progress.
		} KeyboardStatusFlags3; //Keyboard status flags 3

		struct
		{
			byte ScrollLockLEDOn : 1; //1=ScrollLock LED on, 0=Off
			byte NumLockLEDOn : 1; //1=NumLock LED on, 0=Off
			byte CapsLockLEDOn : 1; //1=CapsLock LED on, 0=Off
			byte Reserved1 : 1; //1b=Reserved
			byte AcknowlegdeCodeReceived : 1; //1=Received
			byte ReSendCodeReceived : 1; //1=Re-send code received
			byte LEDUpdateInProgress : 1; //1=LED Update in progress
			byte KeyboardTransmitError : 1; //1=Keyboard Transmit error
		} KeyboardStatusFlags4; //Keyboard status flags 4

//System

		ptr32 UserWaitFlagPointer; //Segment:offset address of user wait flag pointer
		uint_32 UserWaitCount; //User wait count
		struct
		{
			byte WaitInProgress : 1; //1=Wait in progress
			byte NotUsed : 6;
			byte WaitTimeHasElapsed : 1; //1=Wait time has elapsed
		} UserWaitFlag; //User wait flag

//LAN

		byte LANBytes[7]; //Local area network (LAN) bytes

//Video

		ptr32 VideoParameterControlBlock; //Segment:offset address of video parameter control block

//Unused

		byte Reserved[68]; //Reserved

		byte IntraApplicationsCommunicationsArea[0x10]; //Intra-applications communications area
	}; //Contains all BDA items!
	byte data[0x100]; //The data representation of the 256-bytes!
} BDA_type; //BIOS Data Area contents!

#endif