#include "headers/hardware/vga/vga.h" //VGA!
#include "headers/hardware/vga/vga_precalcs.h" //Precalcs!
#include "headers/hardware/ports.h" //Ports!
#include "headers/cpu/cpu.h" //NMI support!

/*

Read and write ports:

Info:						Type:
3B4h: CRTC Controller Address Register		ADDRESS
3B5h: CRTC Controller Data Register		DATA

3BAh Read: Input Status #1 Register (mono)	DATA
3BAh Write: Feature Control Register		DATA

3C0h: Attribute Address/Data register		ADDRESS/DATA
3C1h: Attribute Data Read Register		DATA

3C2h Read: Input Status #0 Register		DATA
3C2h Write: Miscellaneous Output Register	DATA

3C4h: Sequencer Address Register		ADDRESS
3C5h: Sequencer Data Register			DATA

3C7h Read: DAC State Register			DATA
3C7h Write: DAC Address Read Mode Register	ADDRESS

3C8h: DAC Address Write Mode Register		ADDRESS
3C9h: DAC Data Register				DATA

3CAh Read: Feature Control Register (mono Read)	DATA

3CCh Read: Miscellaneous Output Register	DATA

3CEh: Graphics Controller Address Register	ADDRESS
3CFh: Graphics Controller Data Register		DATA

3D4h: CRTC Controller Address Register		ADDRESS
3D5h: CRTC Controller Data Register		DATA

3DAh Read: Input Status #1 Register (color)	DATA
3DAh Write: Feature Control Register (color)	DATA

*/

//Now the CPU ports!

/*

Port 3DA/3BA info:
read: Reset 3C0 flipflop to 0!

Port 3C0 info:
read:
	@flipflop 0: 0
	@flipflop 1: 0
write:
	@flipflop 0: write to address register. flipflop.
	@flipflop 1: write to data register using address. flipflop.
Port 3C1 info:
read:
	read from data register using address. DO NOT FLIPFLOP!
write:
	do nothing!

*/

byte NMIPrecursors = 0; //Execute a NMI for our precursors?

void setVGA_NMIonPrecursors(byte enabled)
{
	NMIPrecursors = enabled?1:0; //Use precursor NMI as set with protection?
}

//Port 3C0 info holder:
#define VGA_3C0_HOLDER getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.ATTRIBUTECONTROLLERTOGGLEREGISTER

//The flipflop of the port 3C0 toggler and port itself!
#define VGA_3C0_FLIPFLOP VGA_3C0_HOLDER.DataState
#define VGA_3C0_PAL VGA_3C0_HOLDER.PAL
#define VGA_3C0_INDEX VGA_3C0_HOLDER.CurrentIndex

//CRTC

OPTINLINE byte PORT_readCRTC_3B5() //Read CRTC registers!
{
	if ((getActiveVGA()->registers->CRTControllerRegisters_Index>0xF) && (getActiveVGA()->registers->CRTControllerRegisters_Index<0x12) && (!getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALBLANKINGREGISTER.EVRA)) //Reading from light pen location registers?
	{
		switch (getActiveVGA()->registers->CRTControllerRegisters_Index) //What index?
		{
		case 0x10: //Light pen high?
			return getActiveVGA()->registers->lightpen_high; //High lightpen!
		case 0x11: //Light pen low?
			return getActiveVGA()->registers->lightpen_low; //Low lightpen!
		default: //Unknown?
			break; //Run normally.
		}
	}
	if (getActiveVGA()->registers->CRTControllerRegisters_Index>=sizeof(getActiveVGA()->registers->CRTControllerRegisters.DATA)) //Out of range?
	{
		return PORT_UNDEFINED_RESULT; //Undefined!
	}
	return getActiveVGA()->registers->CRTControllerRegisters.DATA[getActiveVGA()->registers->CRTControllerRegisters_Index]; //Give normal index!
}

OPTINLINE void PORT_write_CRTC_3B5(byte value)
{
	byte temp; //For index 7 write protected!
	byte index;
	index = getActiveVGA()->registers->CRTControllerRegisters_Index; //What index?
	if ((index<8) && (getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.Protect)) //Protected?
	{
		if (index==7) //Overflow register (allow changes in the bit 4 (line compare))
		{
			value &= 0x10; //Only line compare 8 can be changed!
			temp = getActiveVGA()->registers->CRTControllerRegisters.DATA[0x07]; //Load the overflow register!
			temp &= 0xEF; //Clear our value!
			value |= temp; //Add the line compare 8 value!
		}
		else
		{
			return; //Write protected, so don't process!
		}
	}
	if ((index>0x2F) && (index<0x40)) //30-3F=Odd->Clear screen early!
	{
		if (value&1) //Odd=Set flag!
		{
			getActiveVGA()->registers->VerticalDisplayTotalReached = 1; //Force end-of-screen reached!
		}
		return; //Don't do anything on this register anymore!
	}
	if (index>0x18) //Not a VGA register OR is a READ-ONLY register (undocumented)?
	{
		return; //Write protected OR invalid register!
	}
	if (index<sizeof(getActiveVGA()->registers->CRTControllerRegisters.DATA)) //Within range?
	{
		//Normal register update?
		getActiveVGA()->registers->CRTControllerRegisters.DATA[index] = value; //Set!
		if (index==0x11) //Bit 4&5 of the Vertical Retrace End register have other effects!
		{
			//Bit 5: Input status Register 0, bit 7 needs to be updated with Bit 5?
			if (!getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalInterrupt_NotCleared) //Vertical interrupt cleared?
			{
				getActiveVGA()->registers->ExternalRegisters.INPUTSTATUS1REGISTER.CRTInterruptPending = 0; //Clear the vertical interrupt pending flag!
			}
		}
		if (!getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALBLANKINGREGISTER.EVRA) //Force to 1?
		{
			getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALBLANKINGREGISTER.EVRA = 1; //Force to 1!
			if (index!=3) //We've been updated too?
			{
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CRTCONTROLLER|3); //We have been updated!
			}
		}
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CRTCONTROLLER|index); //We have been updated!
	}
}

//ATTRIBUTE CONTROLLER

OPTINLINE void PORT_write_ATTR_3C0(byte value) //Attribute controller registers!
{
	if (!VGA_3C0_FLIPFLOP) //Index mode?
	{
		//Mirror to state register!
		VGA_3C0_PAL = ((value&0x20)>>5); //Palette Address Source!
		VGA_3C0_INDEX = (value&0x1F); //Which index?
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_ATTRIBUTECONTROLLER); //Updated index!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CRTCONTROLLER|VGA_CRTC_ATTRIBUTECONTROLLERTOGGLEREGISTER); //Our actual location!
	}
	else //Data mode?
	{
		if (VGA_3C0_INDEX<sizeof(getActiveVGA()->registers->AttributeControllerRegisters.DATA)) //Within range?
		{
			getActiveVGA()->registers->AttributeControllerRegisters.DATA[VGA_3C0_INDEX] = value; //Set!
		}
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_ATTRIBUTECONTROLLER|VGA_3C0_INDEX); //We have been updated!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CRTCONTROLLER|VGA_CRTC_ATTRIBUTECONTROLLERTOGGLEREGISTER); //Our actual location!
	}

	VGA_3C0_FLIPFLOP = !VGA_3C0_FLIPFLOP; //Flipflop!
}

//MISC

OPTINLINE void PORT_write_MISC_3C2(byte value) //Misc Output register!
{
	getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.DATA = value; //Set!
	VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_MISCOUTPUTREGISTER); //We have been updated!
}

//DAC

OPTINLINE byte PORT_read_DAC_3C9() //DAC Data register!
{
	word index = getActiveVGA()->registers->ColorRegisters.DAC_ADDRESS_READ_MODE_REGISTER; //Load current DAC index!
	index <<= 2; //Multiply for the index!
	index |= getActiveVGA()->registers->current_3C9; //Current index!
	byte result; //The result!
	result = getActiveVGA()->registers->DAC[index]; //Read the result!

	if (++getActiveVGA()->registers->current_3C9>2) //Next entry?
	{
		++getActiveVGA()->registers->ColorRegisters.DAC_ADDRESS_READ_MODE_REGISTER; //Next entry!
		getActiveVGA()->registers->ColorRegisters.DAC_ADDRESS_READ_MODE_REGISTER &= 0xFF; //Reset when needed!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_DACREAD); //Updated index!
		getActiveVGA()->registers->current_3C9 = 0; //Reset!
	}
	return result; //Give the result!
}

OPTINLINE void PORT_write_DAC_3C9(byte value) //DAC Data register!
{
	byte entrynumber = getActiveVGA()->registers->ColorRegisters.DAC_ADDRESS_WRITE_MODE_REGISTER; //Current entry number!
	word index = entrynumber; //Load current DAC index!
	index <<= 2; //Multiply for the index!
	index |= getActiveVGA()->registers->current_3C9; //Current index!
	getActiveVGA()->registers->DAC[index] = (value&0x3F); //Write the data!
	VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_DAC|entrynumber); //We've been updated!
	
	if (++getActiveVGA()->registers->current_3C9>2) //Next entry?
	{
		++entrynumber; //Overflow when needed!
		getActiveVGA()->registers->ColorRegisters.DAC_ADDRESS_WRITE_MODE_REGISTER = (byte)entrynumber; //Update the entry number!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_DACWRITE); //Updated index!
		getActiveVGA()->registers->current_3C9 = 0; //Reset!
	}
}

void setVGA_CGA(byte enabled)
{
	if (enabled)
	{
		if (enabled==1) //Pure CGA Mode?
		{
			getActiveVGA()->registers->specialCGAflags |= 3; //Enable CGA!
			getActiveVGA()->registers->specialCGAflags &= ~0x80; //Disable VGA!
		}
		else
		{
			getActiveVGA()->registers->specialCGAflags |= 0x83; //Enable VGA and CGA!
		}
	}
	else
	{
		getActiveVGA()->registers->specialCGAflags = 0; //Disable CGA!
	}
}

void setVGA_MDA(byte enabled)
{
	if (enabled)
	{
		if (enabled==1) //Pure MDA Mode?
		{
			getActiveVGA()->registers->specialMDAflags |= 1; //Enable MDA!
			getActiveVGA()->registers->specialMDAflags &= ~0x80; //Disable VGA!
		}
		else
		{
			getActiveVGA()->registers->specialMDAflags |= 0x83; //Enable VGA and MDA!
		}
	}
	else
	{
		getActiveVGA()->registers->specialMDAflags = 0; //Disable MDA!
	}
}

//Foreground colors: Red green yellow(not set), Magenta cyan white(set), Black red cyan white on a color monitor(RGB)!
byte CGA_lowcolors[3][4] = {{0,0x4,0x2,0xE},{0,0x5,0x3,0xF},{0,0x4,0x3,0xF}};
byte CGA_RGB = 0; //Are we a RGB monitor(1) or Composite monitor(0)?

//Compatibility handling on both writes and reads to compatibility registers!
void applyCGAPaletteRegisters()
{
	byte i,color;
	if (getActiveVGA()->registers->specialMDAflags&1) //MDA enabled?
	{
		//Apply the MDA palette registers!
		for (i=0;i<0x10;i++) //Process all colours!
		{
			color = 0; //Default to black!
			if (i&0x8) //Bright(8-F)?
			{
				color |= 2; //Set bright attribute!
			}
			if (i&0x7) //On (1-7 and 9-15)?
			{
				color |= 1; //Set on attribute!
			}
			switch (color) //What color to map?
			{
				default:
				case 0: //Black=Black!
				case 3: //Bright foreground=Bright foreground!
					break;
				case 1: //Normal on?
					color = 2; //Bright!
					break;
				case 2: //Bright background!
					color = 1; //Lighter!
					break;
			}
			getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.PALETTEREGISTERS[i].DATA = color; //Make us equal!
		}
		getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.OVERSCANCOLORREGISTER = 0; //This forces black overscan! We don't have overscan!		
	}
	//Apply the new CGA palette register?
	else if (!(getActiveVGA()->registers->Compatibility_CGAModeControl&0x2)) //Text mode?
	{
		for (i=0;i<0x10;i++) //Process all colours!
		{
			getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.PALETTEREGISTERS[i].DATA = i; //Make us equal!
		}
		if (getActiveVGA()->registers->Compatibility_CGAModeControl&0x10) //High resolution graphics mode(640 pixels)?
		{
			getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.OVERSCANCOLORREGISTER = 0; //This forces black overscan!
		}
		else //Use overscan!
		{
			getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.OVERSCANCOLORREGISTER = (getActiveVGA()->registers->Compatibility_CGAPaletteRegister&0x1F);
		}
	}
	else //Graphics mode?
	{
		if ((getActiveVGA()->registers->Compatibility_CGAModeControl&0x12)==0x10) //High resolution graphics mode(640 pixels)?
		{
			getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.OVERSCANCOLORREGISTER = 0; //Black overscan!
		}
		else if ((getActiveVGA()->registers->Compatibility_CGAModeControl&0x12)==2) //Low resolution graphics mode (320 pixels)?
		{
			getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.OVERSCANCOLORREGISTER = (getActiveVGA()->registers->Compatibility_CGAPaletteRegister&0x1F); //Use the specified color for border!
		}
		else //Disabled border?
		{
			getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.OVERSCANCOLORREGISTER = 0; //Black overscan!
		}
		for (i=0;i<0x10;i++) //Process all colours!
		{
			color = i; //Default to the normal color!
			if ((getActiveVGA()->registers->Compatibility_CGAModeControl&0x4)) //Monochrome mode?
			{
				if (i) //We're on?
				{
					color = (getActiveVGA()->registers->Compatibility_CGAPaletteRegister&0x1F); //Use the specified ON color!
				}
			}
			else //Color mode?
			{
				if (!i) //Background color?
				{
					if ((getActiveVGA()->registers->Compatibility_CGAModeControl&0x12)) //320x200 graphics mode?
					{
						color = (getActiveVGA()->registers->Compatibility_CGAPaletteRegister&0x1F); //Use the specified background color!
					}
				}
				else //Three foreground colors?
				{
					if (i&3) //Foreground color?
					{
						if (getActiveVGA()->registers->Compatibility_CGAModeControl&0x4) //B/W set applies 3rd palette?
						{
							color = CGA_lowcolors[2][color&3]; //Use the RGB-specific 3rd palette!
						}
						else //Normal palettes?
						{
							color = CGA_lowcolors[(getActiveVGA()->registers->Compatibility_CGAModeControl&0x20)>>5][color&3]; //Don't use the RGB palette!
						}
					}
					else //Background?
					{
						color = 0; //Background color!
					}
					if (getActiveVGA()->registers->Compatibility_CGAModeControl&0x10) //Display in low intensity?
					{
						color &= 7; //Apply low intensity!
					}
				}
			}
			getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.PALETTEREGISTERS[i].DATA = color; //Make us the specified value!
		}
	}
	VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_ALL_SECTION|WHEREUPDATED_ATTRIBUTECONTROLLER); //We have been updated(whole attribute controller mode)!
}

void applyCGAPaletteRegister() //Update the CGA colors!
{
	applyCGAPaletteRegisters(); //Apply the palette registers!
}

//useGraphics: 0 for text mode, 1 for graphics mode! GraphicsMode: 0=Text mode, 1=4 color graphics, 2=B/W graphics
void setCGAMDAMode(byte useGraphics, byte GraphicsMode)
{ 
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.DATAROTATEREGISTER.RotateCount = 0; //No special operation!
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.DATAROTATEREGISTER.LogicalOperation = 0; //No special operation!
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.ENABLESETRESETREGISTER.EnableSetReset = 0; //No set/reset used!
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.WriteMode = 0;
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ReadMode = 0;
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.OddEvenMode = 1;
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.ShiftRegisterInterleaveMode = (GraphicsMode==2)?1:0;
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.GRAPHICSMODEREGISTER.Color256ShiftMode = 0;
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.AlphaNumericModeDisable = useGraphics;
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.EnableOddEvenMode = 1;
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.MISCGRAPHICSREGISTER.MemoryMapSelect = ((useGraphics&&(GraphicsMode==2)) || (!useGraphics && (GraphicsMode==1)))?2:3; //Use map B000 or B800, depending on the graphics mode!
	getActiveVGA()->registers->GraphicsRegisters.REGISTERS.BITMASKREGISTER = 0xFF; //Use all bits supplied by the CPU!
	getActiveVGA()->registers->SequencerRegisters.REGISTERS.MAPMASKREGISTER.MemoryPlaneWriteEnable = 3; //Write to planes 0/1 only, since we're emulating CGA!
	getActiveVGA()->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.S4 = 0; //CGA display!
	getActiveVGA()->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.DCR = 0; //CGA display! Single pixels only!
	getActiveVGA()->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.SLR = 0; //CGA display! Single load rate!
	getActiveVGA()->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.DotMode8 = 1; //CGA display! 8 dots/character!
	getActiveVGA()->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.OEDisabled = 0; //Write to planes 0/1 only, since we're emulating CGA!
	getActiveVGA()->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.Chain4Enable = 0; //Write to planes 0/1 only, since we're emulating CGA!
	getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.AttributeControllerGraphicsEnable = useGraphics; //Text mode!
	getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.MonochromeEmulation = ((!useGraphics) && (GraphicsMode==1)); //CGA attributes!
	getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.LineGraphicsEnable = 1; //CGA line graphics!
	getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.PixelPanningMode = 0; //CGA pixel panning mode!
	getActiveVGA()->registers->AttributeControllerRegisters.REGISTERS.COLORPLANEENABLEREGISTER.DATA = 0xF; //CGA: enable all color planes!
	getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.UNDERLINELOCATIONREGISTER.DIV4 = 0; //CGA normal mode!
	getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.UNDERLINELOCATIONREGISTER.DW = 0; //CGA normal mode!
	getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.SLDIV = 0; //CGA no scanline division!
	getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.DIV2 = 0; //CGA no scanline division!
	getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.UseByteMode = 0; //CGA word mode!
	getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.SE = 1; //CGA enable CRT rendering HSYNC/VSYNC!
	//Memory mapping special: always map like a CGA!
	getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.MAP13 = 1; //CGA mapping!
	getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.MAP14 = 1; //CGA mapping!
	getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.AW = 1; //CGA mapping!
	getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.RAM_Enable = 1; //CGA!
	getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.OE_HighPage = 0; //CGA!
	getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.HSyncP = 0; //CGA has positive polarity!
	getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.VSyncP = 0; //CGA has positive polarity!
	getActiveVGA()->registers->ExternalRegisters.FEATURECONTROLREGISTER.FC0 = 0; //CGA!
	getActiveVGA()->registers->ExternalRegisters.FEATURECONTROLREGISTER.FC1 = 1; //CGA!
}

void applyCGAModeControl()
{
	//Apply the new CGA mode control register?
	if (getActiveVGA()->registers->Compatibility_CGAModeControl&8) //Video enabled on the CGA?
	{
		if (getActiveVGA()->registers->Compatibility_MDAModeControl&8) //MDA also enabled?
		{
			getActiveVGA()->registers->Compatibility_MDAModeControl &= ~8; //Disable the MDA!
		}
	}
	if (!(getActiveVGA()->registers->Compatibility_CGAModeControl&0x2)) //Text mode?
	{
		if (getActiveVGA()->registers->Compatibility_CGAModeControl&0x1) //80 column text mode?
		{
			getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.OFFSETREGISTER = 40; //We're 80 column text!
		}
		else //40 column text mode?
		{
			getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.OFFSETREGISTER = 20; //We're 40 column text!
		}
		setCGAMDAMode(0,0); //Text mode!
	}
	else //Graphics mode?
	{
		if (getActiveVGA()->registers->Compatibility_CGAModeControl&0x4) //2 colour?
		{
			setCGAMDAMode(1,2); //Set up basic 2-color graphics!
		}
		else //4 colour?
		{
			setCGAMDAMode(1,1); //Set up basic 4-color graphics!
		}
		getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.OFFSETREGISTER = 40; //We're 80 bytes per row(divide by 2)!
	}
	applyCGAPaletteRegisters(); //Apply the palette registers according to our settings!
	VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_ALL); //We have been updated!	
}

void applyMDAModeControl()
{
	//Apply the new MDA mode control register?
	if (getActiveVGA()->registers->Compatibility_MDAModeControl&8) //Video enabled on the MDA?
	{
		if (getActiveVGA()->registers->Compatibility_CGAModeControl&8) //CGA also enabled?
		{
			getActiveVGA()->registers->Compatibility_CGAModeControl &= ~8; //Disable the CGA!
		}
		getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.OFFSETREGISTER = 40; //We're 80(x2) column text!
		setCGAMDAMode(0,1); //Set special CGA/VGA MDA compatible text mode!
	}
	applyCGAPaletteRegisters(); //Apply the palette registers according to our settings!
	VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_ALL); //We have been updated!
}

/*

Finally: the read/write handlers themselves!

*/

byte PORT_readVGA(word port, byte *result) //Read from a port/register!
{
	byte ok = 0;
	if (!getActiveVGA()) //No active VGA?
	{
		return 0;
	}
	switch (port) //What port?
	{
	case 0x3B0:
	case 0x3B2:
	case 0x3B6:
	case 0x3B4: //Decodes to 3B4!
		if (getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishinput; //Block: we're a color mode addressing as mono!
		goto readcrtaddress;
	case 0x3D4: //CRTC Controller Address Register		ADDRESS
		if (!getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishinput; //Block: we're a mono mode addressing as color!
		readcrtaddress:
		*result = getActiveVGA()->registers->CRTControllerRegisters_Index; //Give!
		ok = 1;
		break;
	case 0x3B1:
	case 0x3B3:
	case 0x3B7: //Decodes to 3B5!
	case 0x3B5: //CRTC Controller Data Register		5DATA
		if (getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishinput; //Block: we're a color mode addressing as mono!
		goto readcrtvalue;
	case 0x3D5: //CRTC Controller Data Register		DATA
		if (!getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishinput; //Block: we're a mono mode addressing as color!
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) /*|| ((getActiveVGA()->registers->specialMDAflags&0x81)==1)*/ || (getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.Protect)) //Special CGA flag set?
		{
			if (getActiveVGA()->registers->CRTControllerRegisters_Index>=18) goto finishinput; //Invalid register, just handle normally!
			*result = getActiveVGA()->registers->CGARegisters[getActiveVGA()->registers->CRTControllerRegisters_Index]; //Give the CGA register!
			ok = 1;
			goto finishinput; //Finish us! Don't use the VGA registers!
		}
		readcrtvalue:
		*result = PORT_readCRTC_3B5(); //Read port 3B5!
		ok = 1;
		break;
	case 0x3C0: //Attribute Address/Data register		ADDRESS/DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		//Do nothing: write only port! Undefined!
		*result = (VGA_3C0_PAL<<5)|VGA_3C0_INDEX; //Give the saved information!
		ok = 1;
		break;
	case 0x3C1: //Attribute Data Read Register		DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		if (VGA_3C0_INDEX>=sizeof(getActiveVGA()->registers->AttributeControllerRegisters.DATA)) break; //Out of range!
		*result = getActiveVGA()->registers->AttributeControllerRegisters.DATA[VGA_3C0_INDEX]; //Read from current index!
		ok = 1;
		break;
	case 0x3C2: //Read: Input Status #0 Register		DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		getActiveVGA()->registers->ExternalRegisters.INPUTSTATUS0REGISTER.SwitchSense = (((~getActiveVGA()->registers->switches)>>(getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.ClockSelect&3))&1); //Depends on the switches. This is the reverse of the actual switches used! Originally stuck to 1s, but reported as 0110!
		*result = getActiveVGA()->registers->ExternalRegisters.INPUTSTATUS0REGISTER.DATA; //Give the register!
		ok = 1;
		break;
	case 0x3C3: //Video subsystem enable?
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		*result = getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.RAM_Enable; //RAM enabled?
		ok = 1;
		break;
	case 0x3C4: //Sequencer Address Register		ADDRESS
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		*result = getActiveVGA()->registers->SequencerRegisters_Index; //Give the index!
		ok = 1;
		break;
	case 0x3C5: //Sequencer Data Register			DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		if (getActiveVGA()->registers->SequencerRegisters_Index>=sizeof(getActiveVGA()->registers->SequencerRegisters.DATA)) break; //Out of range!
		*result = getActiveVGA()->registers->SequencerRegisters.DATA[getActiveVGA()->registers->SequencerRegisters_Index]; //Give the data!
		ok = 1;
		break;
	case 0x3C6: //DAC Mask Register?
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		*result = getActiveVGA()->registers->DACMaskRegister; //Give!
		ok = 1;
		break;
	case 0x3C7: //Read: DAC State Register			DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		*result = getActiveVGA()->registers->ColorRegisters.DAC_STATE_REGISTER.DATA; //Give!
		ok = 1;
		break;
	case 0x3C8: //DAC Address Write Mode Register		ADDRESS
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		*result = getActiveVGA()->registers->ColorRegisters.DAC_ADDRESS_WRITE_MODE_REGISTER; //Give!
		ok = 1;
		break;
	case 0x3C9: //DAC Data Register				DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		*result = PORT_read_DAC_3C9(); //Read port 3C9!
		ok = 1;
		break;
	case 0x3CA: //Read: Feature Control Register		DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		*result = getActiveVGA()->registers->ExternalRegisters.FEATURECONTROLREGISTER.DATA; //Give!
		ok = 1;
		break;
	case 0x3CC: //Read: Miscellaneous Output Register	DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		*result = getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.DATA; //Give!
		ok = 1;
		break;
	case 0x3CE: //Graphics Controller Address Register	ADDRESS
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		*result = getActiveVGA()->registers->GraphicsRegisters_Index; //Give!
		ok = 1;
		break;
	case 0x3CF: //Graphics Controller Data Register		DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishinput; //CGA doesn't have VGA registers!
		if (getActiveVGA()->registers->GraphicsRegisters_Index>=sizeof(getActiveVGA()->registers->GraphicsRegisters.DATA)) break; //Out of range!
		*result = getActiveVGA()->registers->GraphicsRegisters.DATA[getActiveVGA()->registers->GraphicsRegisters_Index]; //Give!
		ok = 1;
		break;
	case 0x3BA:	//Read: Input Status #1 Register (mono)	DATA
		if (getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishinput; //Block: we're a color mode addressing as mono!
		goto readInputStatus1;
	case 0x3DA: //Input Status #1 Register (color)	DATA
		if (!getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishinput; //Block: we're a mono mode addressing as color!
		readInputStatus1:
		getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.ATTRIBUTECONTROLLERTOGGLEREGISTER.DataState = 0; //Reset flipflop for 3C0!
		*result = getActiveVGA()->registers->ExternalRegisters.INPUTSTATUS1REGISTER.DATA; //Give!
		if (getActiveVGA()->registers->specialMDAflags&1) //3BA doesn't have bit 4? Bit4=Mono operation!
		{
			*result &= ~8; //Clear the VRetrace bit: we're mono operation, as we're the monochrome port used!
			*result &= ~0x1; //Bit 0 is different in the MDA? Clear it to be set if needed!
			if (getActiveVGA()->CRTC.DisplayDriven) *result |= 1; //Are we driving display(Display Enable bit) on the output?
			if ((getActiveVGA()->registers->specialMDAflags&0x81)==1) //Pure MDA mode?
			{
				*result &= ~0x06; //Clear bits 2-1 on real IBM MDA!
				*result |= 0xF0; //Set bit 7-4 on real IBM MDA!
			}
		}
		else if (getActiveVGA()->registers->specialCGAflags&1) //CGA status port?
		{
			*result &= ~1; //Clear bit 0!
			if (getActiveVGA()->CRTC.DisplayDriven) *result |= 1; //Are we driving display(Display Enable bit) on the output?
			*result ^= 1; //We're 0 when display isn't driven!
		} //Else: normal VGA documented result!
		ok = 1;
		break;
	
	//Precursors compatibility
	case 0x3D8: //CGA mode control register
		if (getActiveVGA()->registers->specialCGAflags&0x1) //Not NMI used on CGA-specific registers being called?
		{
			*result = getActiveVGA()->registers->Compatibility_CGAModeControl; //Set the MDA Mode Control Register!
			ok = 1; //OK!
		}
		else if (NMIPrecursors) ok = !execNMI(0); //Execute an NMI from Bus!
		break;
	case 0x3D9: //CGA palette register
		if (getActiveVGA()->registers->specialCGAflags&0x1) //Not NMI used on CGA-specific registers being called?
		{
			*result = getActiveVGA()->registers->Compatibility_CGAPaletteRegister; //Set the MDA Mode Control Register!
			ok = 1; //OK!
		}
		else if (NMIPrecursors) ok = !execNMI(0); //Execute an NMI from Bus!
		break;
	case 0x3B8: //MDA Mode Control Register
		if (getActiveVGA()->registers->specialMDAflags&0x1) //Not NMI used on MDA-specific registers being called?
		{
			*result = getActiveVGA()->registers->Compatibility_MDAModeControl; //Set the MDA Mode Control Register!
			ok = 1; //OK!
		}
		else if (NMIPrecursors) ok = !execNMI(0); //Execute an NMI from Bus!
	default: //Unknown?
		break; //Not used address!
	}
	finishinput:
	return ok; //Disabled for now or unknown port!
}

byte PORT_writeVGA(word port, byte value) //Write to a port/register!
{
	if (!getActiveVGA()) //No active VGA?
	{
		return 0;
	}
	byte ok = 0;
	switch (port) //What port?
	{
	case 0x3B0:
	case 0x3B2:
	case 0x3B6: //Decodes to 3B4!
	case 0x3B4: //CRTC Controller Address Register		ADDRESS
		//if ((getActiveVGA()->registers->specialCGAflags&0x81)==1) goto accesscrtaddress; //Little hack: allow the CGA only to access the CGA/MDA CRT registers!
		if (getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishoutput; //Block: we're a color mode addressing as mono!
		//if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		goto accesscrtaddress;
	case 0x3D4: //CRTC Controller Address Register		ADDRESS
		if (!getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishoutput; //Block: we're a mono mode addressing as color!
		accesscrtaddress:
		getActiveVGA()->registers->CRTControllerRegisters_Index = value; //Set!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_CRTCONTROLLER); //Updated index!
		ok = 1;
		break;
	case 0x3B1:
	case 0x3B3:
	case 0x3B7: //Decodes to 3B5!
	case 0x3B5: //CRTC Controller Data Register		DATA
		//if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto accessCGACRT; //CGA&MDA registers are always mapped with CGA/MDA only mode!
		if (getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishoutput; //Block: we're a color mode addressing as mono!
		goto accesscrtvalue;
	case 0x3D5: //CRTC Controller Data Register		DATA
		if (!getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishoutput; //Block: we're a mono mode addressing as color!
		accesscrtvalue:
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1) || (getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.Protect && ((getActiveVGA()->registers->specialCGAflags&0x81)==0x81))) //Special CGA flag set? Protect bit also enables when used with the CGA mode!
		{
			if (getActiveVGA()->registers->CRTControllerRegisters_Index>=18) goto skipVGACRTwrite; //Invalid register, just handle normally(skip it)!
			getActiveVGA()->registers->CGARegisters[getActiveVGA()->registers->CRTControllerRegisters_Index] = value; //Set the CGA register!
			getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value; //Set the CGA register(unmasked)!
			switch (getActiveVGA()->registers->CRTControllerRegisters_Index) //Check address registers to translate from the CGA!
			{
			case 0x0: //HTotal?
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER_HORIZONTAL|0x00); //This CRT Register has been updated!
				break;
			case 0x1: //H Displayed?
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER_HORIZONTAL|0x01); //This CRT Register has been updated!
				break;
			case 0x2: //H Sync Position?
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER_HORIZONTAL|0x02); //This CRT Register has been updated!
				break;
			case 0x3: //H Sync Width?
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0xF; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER_HORIZONTAL|0x03); //This CRT Register has been updated!
				break;
			case 0x4:  //Special CGA compatibilty action? Vertical total register?
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0x7F; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER_VERTICAL|0x04); //This CRT Register has been updated!
				break;
			case 0x5: //V Total Adjust?
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0x1F; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER_VERTICAL|0x05); //This CRT Register has been updated!
				break;
			case 0x6: //V Displayed?
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0x7F; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER_VERTICAL|0x06); //This CRT Register has been updated!
				break;
			case 0x7: //V Sync Position?
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0x7F; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER_VERTICAL|0x07); //This CRT Register has been updated!
				break;
			case 0x8: //Interlace mode register?
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0x3; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER|0x8); //CRT Mode Control Register has been updated!
				break;
			case 0x9: //Max scan line address?
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0x1F; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER|0x9); //This CRT Register has been updated!
				break;
			case 0xA: //Cursor Start?
				//Bit 6&5: 00=Non-blink(ON), 01=Non-Display(OFF), 10=Blink 1/16 field rate, 11=Blink 1/32 field rate!
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0x7F; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER|0xA); //This CRT Register has been updated!
				break;
			case 0xB: //Cursor End?
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0x1F; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER|0xB); //This CRT Register has been updated!
				break;
			case 0xC: //Start address(H)?
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0x3F; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER|getActiveVGA()->registers->CRTControllerRegisters_Index); //This CRT Register has been updated!
				break;
			case 0xD: //Start address(L)?
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER|getActiveVGA()->registers->CRTControllerRegisters_Index); //This CRT Register has been updated!
				break;
			case 0xE: //Cursor(H)?
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0x3F; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER|getActiveVGA()->registers->CRTControllerRegisters_Index); //This CRT Register has been updated!
				break;
			case 0xF: //Cursor(L)?
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER|getActiveVGA()->registers->CRTControllerRegisters_Index); //This CRT Register has been updated!
				break;
			case 0x10: //Light Pen(H)?
				getActiveVGA()->registers->CGARegistersMasked[getActiveVGA()->registers->CRTControllerRegisters_Index] = value&0x3F; //Set the CGA register(masked)!
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER|getActiveVGA()->registers->CRTControllerRegisters_Index); //This CRT Register has been updated!
			case 0x11: //Light Pen(L)?
				VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_CGACRTCONTROLLER|getActiveVGA()->registers->CRTControllerRegisters_Index); //This CRT Register has been updated!
				break; //Not handled yet!
			default:
				break;
			}
			goto skipVGACRTwrite; //Don't apply the VGA CRT normally!
		}
		PORT_write_CRTC_3B5(value); //Write CRTC!
		skipVGACRTwrite: //Skip the CRT handling?
		ok = 1;
		break;
	case 0x3BA: //Write: Feature Control Register (mono)		DATA
		if (getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishoutput; //Block: we're a color mode addressing as mono!
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		goto accessfc;
	case 0x3CA: //Same as above!
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
	case 0x3DA: //Same!
		if (!getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishoutput; //Block: we're a mono mode addressing as color!
		accessfc: //Allow!
		getActiveVGA()->registers->ExternalRegisters.FEATURECONTROLREGISTER.DATA = value; //Set!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_FEATURECONTROLREGISTER); //We have been updated!
		ok = 1;
		break;
	case 0x3C0: //Attribute Address/Data register		ADDRESS/DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		PORT_write_ATTR_3C0(value); //Write to 3C0!
		ok = 1;
		break;
	case 0x3C1: //Attribute Data Read Register		DATA
		//Undefined!
		goto finishoutput; //Unknown port! Ignore our call!
		break;
	case 0x3C2: //Write: Miscellaneous Output Register	DATA
	case 0x3CC: //Same as above!
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		PORT_write_MISC_3C2(value); //Write to 3C2!
		ok = 1;
		break;
	case 0x3C3: //Video subsystem enable
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		value &= 1; //Only 1 bit!
		getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.RAM_Enable = value; //Enable RAM?
		ok = 1;
		break;
	case 0x3C4: //Sequencer Address Register		ADDRESS
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		getActiveVGA()->registers->SequencerRegisters_Index = value; //Set!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_SEQUENCER); //Updated index!
		ok = 1;
		break;
	case 0x3C5: //Sequencer Data Register			DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		if (getActiveVGA()->registers->SequencerRegisters_Index>7) break; //Invalid data!
		if (getActiveVGA()->registers->SequencerRegisters_Index==7) //Disable display till write to sequencer registers 0-6?
		{
			getActiveVGA()->registers->CRTControllerDontRender = 0xFF; //Force to 0xFF indicating display disabled!
		}
		else if ((getActiveVGA()->registers->SequencerRegisters_Index<7) && (getActiveVGA()->registers->CRTControllerDontRender)) //Disabled and enabled again?
		{
			getActiveVGA()->registers->CRTControllerDontRender = 0x00; //Reset, effectively enabling VGA rendering!
		}
		if (getActiveVGA()->registers->SequencerRegisters_Index>=sizeof(getActiveVGA()->registers->SequencerRegisters.DATA))
		{
			//unlockVGA(); //Finished with the VGA!
			break; //Out of range!
		}
		getActiveVGA()->registers->SequencerRegisters.DATA[getActiveVGA()->registers->SequencerRegisters_Index] = value; //Set!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_SEQUENCER|getActiveVGA()->registers->SequencerRegisters_Index); //We have been updated!		
		ok = 1;
		break;
	case 0x3C6: //DAC Mask Register?
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		getActiveVGA()->registers->DACMaskRegister = value; //Set!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_DACMASKREGISTER); //We have been updated!				
		ok = 1;
		break;
	case 0x3C7: //Write: DAC Address Read Mode Register	ADDRESS
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		getActiveVGA()->registers->ColorRegisters.DAC_ADDRESS_READ_MODE_REGISTER = value; //Set!
		getActiveVGA()->registers->ColorRegisters.DAC_STATE_REGISTER.DACState = 0; //Prepared for reads!
		getActiveVGA()->registers->current_3C9 = 0; //Reset!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_DACREAD); //Updated index!
		ok = 1;
		break;
	case 0x3C8: //DAC Address Write Mode Register		ADDRESS
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		getActiveVGA()->registers->ColorRegisters.DAC_ADDRESS_WRITE_MODE_REGISTER = value; //Set index!
		getActiveVGA()->registers->ColorRegisters.DAC_STATE_REGISTER.DACState = 3; //Prepared for writes!
		getActiveVGA()->registers->current_3C9 = 0; //Reset!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_DACWRITE); //Updated index!
		ok = 1;
		break;
	case 0x3C9: //DAC Data Register				DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		PORT_write_DAC_3C9(value); //Write to 3C9!
		ok = 1;
		break;
	case 0x3CE: //Graphics Controller Address Register	ADDRESS
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		getActiveVGA()->registers->GraphicsRegisters_Index = value; //Set index!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_GRAPHICSCONTROLLER); //Updated index!
		ok = 1;
		break;
	case 0x3CF: //Graphics Controller Data Register		DATA
		if (((getActiveVGA()->registers->specialCGAflags&0x81)==1) || ((getActiveVGA()->registers->specialMDAflags&0x81)==1)) goto finishoutput; //CGA doesn't have VGA registers!
		if (getActiveVGA()->registers->GraphicsRegisters_Index>=sizeof(getActiveVGA()->registers->GraphicsRegisters.DATA)) break; //Invalid index!
		getActiveVGA()->registers->GraphicsRegisters.DATA[getActiveVGA()->registers->GraphicsRegisters_Index] = value; //Set!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_GRAPHICSCONTROLLER|getActiveVGA()->registers->GraphicsRegisters_Index); //We have been updated!				
		ok = 1;
		break;
	
	//Precursors compatibility
	case 0x3D8: //CGA mode control register
		if (getActiveVGA()->registers->specialCGAflags&0x1) //Not NMI used on CGA-specific registers being called?
		{
			getActiveVGA()->registers->Compatibility_CGAModeControl = value; //Set the MDA Mode Control Register!
			ok = 1; //OK!
			applyCGAModeControl();
		}
		else if (NMIPrecursors) ok = !execNMI(0); //Execute an NMI from Bus!
		break;
	case 0x3D9: //CGA palette register
		if (getActiveVGA()->registers->specialCGAflags&0x1) //Not NMI used on CGA-specific registers being called?
		{
			getActiveVGA()->registers->Compatibility_CGAPaletteRegister = value; //Set the MDA Mode Control Register!
			ok = 1; //OK!
			applyCGAPaletteRegister();
		}
		else if (NMIPrecursors) ok = !execNMI(0); //Execute an NMI from Bus!
		break;
	case 0x3B8: //MDA Mode Control Register
		if (getActiveVGA()->registers->specialMDAflags&0x1) //Not NMI used on MDA-specific registers being called? Also emulate with CGA enabled!
		{
			getActiveVGA()->registers->Compatibility_MDAModeControl = value; //Set the MDA Mode Control Register!
			ok = 1; //OK!
			applyMDAModeControl();
		}
		else if (NMIPrecursors) ok = !execNMI(0); //Execute an NMI from Bus!
		break;
	default: //Unknown?
		goto finishoutput; //Unknown port! Ignore our call!
		break; //Not used!
	}
	//Extra universal handling!
	if ((getActiveVGA()->registers->specialCGAflags!=getActiveVGA()->precalcs.LastCGAFlags) || (getActiveVGA()->registers->specialMDAflags!=getActiveVGA()->precalcs.LastMDAFlags)) //CGA/MDA flags updated?
	{
		getActiveVGA()->precalcs.LastCGAFlags = getActiveVGA()->registers->specialCGAflags; //Update the last value used!
		getActiveVGA()->precalcs.LastMDAFlags = getActiveVGA()->registers->specialMDAflags; //Update the last value used!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_ALL_SECTION|WHEREUPDATED_CRTCONTROLLER); //We have been updated! Update the whole section, as we don't know anything about the exact registers affected by the special action!
	}
	finishoutput: //Finishing up our call?
	return ok; //Give if we're handled!
}

void VGA_initIO()
{
	//Our own settings we use:
	register_PORTIN(&PORT_readVGA);
	register_PORTOUT(&PORT_writeVGA);
	if (getActiveVGA()) //Gotten active VGA? Initialise the full hardware if needed!
	{
		if ((getActiveVGA()->registers->specialMDAflags&0x81)==1) //Pure MDA mode?
		{
			getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS = 0; //Mono(MDA) mode!
			applyMDAModeControl();
			getActiveVGA()->registers->SequencerRegisters.REGISTERS.RESETREGISTER.AR = 1;
			getActiveVGA()->registers->SequencerRegisters.REGISTERS.RESETREGISTER.SR = 1;
		}
		if ((getActiveVGA()->registers->specialCGAflags&0x81)==1) //Pure CGA mode?
		{
			getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS = 1; //Color(CGA) mode!
			applyCGAModeControl();
			applyCGAPaletteRegister();
			getActiveVGA()->registers->SequencerRegisters.REGISTERS.RESETREGISTER.AR = 1;
			getActiveVGA()->registers->SequencerRegisters.REGISTERS.RESETREGISTER.SR = 1;
		}
	}
}