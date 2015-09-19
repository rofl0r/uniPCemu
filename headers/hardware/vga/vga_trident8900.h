#ifndef VGA_TRIDENT8900_H
#define VGA_TRIDENT8900_H

//? = B or D, depending on Monochrome or Color operations.

struct
{
byte HardwareVersionRegister; //New definition (read at 3c5h when 3c4h=0Bh)
byte VersionSelectorRegister; //Version Selector Register (old definition) (write @3c5h when 3c4h=0Bh)
byte ConfigurationPortRegister1; //Configuration Port Register 1 (R/W @3c5h when 3c4h=0Ch)
byte ModeControlRegister2; //Mode Control Register 2 (R/W @3c5h when 3c4h=0Dh)
byte ModeControlRegister1; //Mode Control Register 1 (R/W @3c5h when 3c4h=0Eh)
byte PowerUpModeRegister2; //Power-up Mode Register 2 (R/W @3c5h when 3c4h=0Fh)
byte CRTCModuleTestingRegister; //CRTC Module Testing Register (R/W @3?5h when 3?4h=1Eh)
byte SoftwareProgrammingRegister; //Software Programming Register (R/W @3?5h when 3?4h=1fh)
byte AttributeIndexReadBackRegister; //Attribute Index Read Back Register (Readonly @3?5h when 3?4h=26h)
byte SourceAddressRegister; //Source Address Register (R/W @3CFh when 3CEh=0Eh)
byte SourceAddressEnableRegister; //Source Address Enable Register (R/W @3CFh when 3CEh=0Fh)
byte
} SVGA_TRIDENT8900; //Extension of VGA, downwards compatible with standard VGA!


/*

HardwareVersionRegister:

<3..0>   Chip version identification   {Trident}
         ---------------------------

         <=0000>  Unassigned.
         <=0001>  Unassigned.
         <=0010>  Trident 8800CS
         <=0011>  Trident 8900B
         <=0100>  Trident 8900C
         <=0101>  Unassigned.
         <=0110>  Unassigned.
         <=0111>  Unassigned.
         <=1000>  Unassigned.
         <=1001>  Unassigned.
         <=1010>  Unassigned.
         <=1011>  Unassigned.
         <=1100>  Unassigned.
         <=1101>  Unassigned.
         <=1110>  Unassigned.
         <=1111>  Unassigned.
		 
VersionSelectorRegister:
Writing causes the two mode control registers to transform to their old definitions. Data written is irrelevant, as it will be ignored.

ConfigurationPortRegister1:
Definition (When NMI disabled):
Bit0: Address Decode Source:
	0=Employ latched system address for A16 Decode.
	1=Employ unlatched A19..A17 system address for A16 decode.
Bit1: ROM Chip Count Specify
	0=Two BIOS ROM Chips present.
	1=One BIOS ROM Chip present.
Bit2: Reserved
Bit3: ROM Address Span Specify
	0=Decode 24K ROM BIOS address space.
	1=Decode 32K ROM BIOS address space.
Bit4: VGA Control Port specify
	0=Select hex '46e8' as VGA Control Port. (This is a standard plug-in board control port address)
	1=Select hex '3c3' as VGA Control Port. (This is a standard motherboard control port address)
Bit5-6: DRAM Configuration
	00=64K by 4 bits (either 8 or 16 chips)
	01= 256K by 4 bits {2 chips}
	10=256K by 4 bits {4 chips} (Superfury: should be 512K?)
	11=256K by 4 bits {8 chips} (Superfury: should be 1024K?)
Bit 7: DRAM Address Width
	0=8-bit memory access
	1=16-bit memory access

Definition (Only when NMI Enabled):
Bit0: Software Status (Read/Write to this bit by software to store a binary status flag)
Bit1: NMI State (NMI State)
	0=NMI Disable: Remove this interpretation of the register.
	1=NMI Enable: Consistent with this register interpretation.
Bit2-3: Testing bits
	00=Manufacturing test
	01=Manufacturing test
	10=Manufacturing test
	11=Normal operation
Bit7-4: NMI Vector Enable
         <=0000>  No NMI vector specified.
         <=0001>  NMI vector is specifed as hex '3x8'.
         <=0010>  NMI vector is specifed as hex '3d9'.
         <=0011>  Illegal value.
         <=0100>  NMI vector is specifed as hex '3dd'.
         <=0101>  Illegal value.
         <=0110>  Illegal value.
         <=0111>  Illegal value.
         <=1000>  NMI vector is specifed as hex '3bf'.
         <=1001>  Illegal value.
         <=1010>  Illegal value.
         <=1011>  Illegal value.
         <=1100>  Illegal value.
         <=1101>  Illegal value.
         <=1110>  Illegal value.
         <=1111>  Illegal value.

ModeControlRegister2:

		 
*/
#endif