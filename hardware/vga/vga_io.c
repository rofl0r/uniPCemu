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
	NMIPrecursors = enabled?1:0; //Use precursor NMI?
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
	case 0x3B6: //Decodes to 3B4!
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
	case 0x3B5: //CRTC Controller Data Register		DATA
		if (getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishinput; //Block: we're a color mode addressing as mono!
		goto readcrtvalue;
	case 0x3D5: //CRTC Controller Data Register		DATA
		if (!getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishinput; //Block: we're a mono mode addressing as color!
		readcrtvalue:
		*result = PORT_readCRTC_3B5(); //Read port 3B5!
		ok = 1;
		break;
	case 0x3C0: //Attribute Address/Data register		ADDRESS/DATA
		//Do nothing: write only port! Undefined!
		*result = (VGA_3C0_PAL<<5)|VGA_3C0_INDEX; //Give the saved information!
		ok = 1;
		break;
	case 0x3C1: //Attribute Data Read Register		DATA
		if (VGA_3C0_INDEX>=sizeof(getActiveVGA()->registers->AttributeControllerRegisters.DATA)) break; //Out of range!
		*result = getActiveVGA()->registers->AttributeControllerRegisters.DATA[VGA_3C0_INDEX]; //Read from current index!
		ok = 1;
		break;
	case 0x3C2: //Read: Input Status #0 Register		DATA
		getActiveVGA()->registers->ExternalRegisters.INPUTSTATUS0REGISTER.SwitchSense = (((~getActiveVGA()->registers->switches)>>(getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.ClockSelect&3))&1); //Depends on the switches. This is the reverse of the actual switches used! Originally stuck to 1s, but reported as 0110!
		*result = getActiveVGA()->registers->ExternalRegisters.INPUTSTATUS0REGISTER.DATA; //Give the register!
		ok = 1;
		break;
	case 0x3C3: //Video subsystem enable?
		*result = getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.RAM_Enable; //RAM enabled?
		ok = 1;
		break;
	case 0x3C4: //Sequencer Address Register		ADDRESS
		*result = getActiveVGA()->registers->SequencerRegisters_Index; //Give the index!
		ok = 1;
		break;
	case 0x3C5: //Sequencer Data Register			DATA
		if (getActiveVGA()->registers->SequencerRegisters_Index>=sizeof(getActiveVGA()->registers->SequencerRegisters.DATA)) break; //Out of range!
		*result = getActiveVGA()->registers->SequencerRegisters.DATA[getActiveVGA()->registers->SequencerRegisters_Index]; //Give the data!
		ok = 1;
		break;
	case 0x3C6: //DAC Mask Register?
		*result = getActiveVGA()->registers->DACMaskRegister; //Give!
		ok = 1;
		break;
	case 0x3C7: //Read: DAC State Register			DATA
		*result = getActiveVGA()->registers->ColorRegisters.DAC_STATE_REGISTER.DATA; //Give!
		ok = 1;
		break;
	case 0x3C8: //DAC Address Write Mode Register		ADDRESS
		*result = getActiveVGA()->registers->ColorRegisters.DAC_ADDRESS_WRITE_MODE_REGISTER; //Give!
		ok = 1;
		break;
	case 0x3C9: //DAC Data Register				DATA
		*result = PORT_read_DAC_3C9(); //Read port 3C9!
		ok = 1;
		break;
	case 0x3CA: //Read: Feature Control Register		DATA
		*result = getActiveVGA()->registers->ExternalRegisters.FEATURECONTROLREGISTER.DATA; //Give!
		ok = 1;
		break;
	case 0x3CC: //Read: Miscellaneous Output Register	DATA
		*result = getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.DATA; //Give!
		ok = 1;
		break;
	case 0x3CE: //Graphics Controller Address Register	ADDRESS
		*result = getActiveVGA()->registers->GraphicsRegisters_Index; //Give!
		ok = 1;
		break;
	case 0x3CF: //Graphics Controller Data Register		DATA
		if (getActiveVGA()->registers->GraphicsRegisters_Index>=sizeof(getActiveVGA()->registers->GraphicsRegisters.DATA)) break; //Out of range!
		*result = getActiveVGA()->registers->GraphicsRegisters.DATA[getActiveVGA()->registers->GraphicsRegisters_Index]; //Give!
		ok = 1;
		break;
	case 0x3BA:	//Read: Input Status #1 Register (mono)	DATA
	case 0x3DA: //Input Status #1 Register (color)	DATA
		getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.ATTRIBUTECONTROLLERTOGGLEREGISTER.DataState = 0; //Reset flipflop for 3C0!
		*result = getActiveVGA()->registers->ExternalRegisters.INPUTSTATUS1REGISTER.DATA; //Give!
		ok = 1;
		break;
	
	//Precursors compatibility
	case 0x3B8: //MDA Mode Control Register
	case 0x3D8: //CGA mode control register
	case 0x3D9: //CGA palette register
		if (NMIPrecursors) ok = !execNMI(0); //Execute an NMI from Bus!
		break;
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
		if (getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishoutput; //Block: we're a color mode addressing as mono!
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
		if (getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishoutput; //Block: we're a color mode addressing as mono!
		goto accesscrtvalue;
	case 0x3D5: //CRTC Controller Data Register		DATA
		if (!getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishoutput; //Block: we're a mono mode addressing as color!
		accesscrtvalue:
		PORT_write_CRTC_3B5(value); //Write CRTC!
		ok = 1;
		break;
	case 0x3BA: //Write: Feature Control Register (mono)		DATA
		if (getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishoutput; //Block: we're a color mode addressing as mono!
		goto accessfc;
	case 0x3CA: //Same as above!
	case 0x3DA: //Same!
		if (!getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.IO_AS) goto finishoutput; //Block: we're a mono mode addressing as color!
		accessfc: //Allow!
		getActiveVGA()->registers->ExternalRegisters.FEATURECONTROLREGISTER.DATA = value; //Set!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_FEATURECONTROLREGISTER); //We have been updated!
		ok = 1;
		break;
	case 0x3C0: //Attribute Address/Data register		ADDRESS/DATA
		PORT_write_ATTR_3C0(value); //Write to 3C0!
		ok = 1;
		break;
	/*case 0x3C1: //Attribute Data Read Register		DATA
		//Undefined!
		break;*/
	case 0x3C2: //Write: Miscellaneous Output Register	DATA
	case 0x3CC: //Same as above!
		PORT_write_MISC_3C2(value); //Write to 3C2!
		ok = 1;
		break;
	case 0x3C3: //Video subsystem enable
		value &= 1; //Only 1 bit!
		getActiveVGA()->registers->ExternalRegisters.MISCOUTPUTREGISTER.RAM_Enable = value; //Enable RAM?
		ok = 1;
		break;
	case 0x3C4: //Sequencer Address Register		ADDRESS
		getActiveVGA()->registers->SequencerRegisters_Index = value; //Set!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_SEQUENCER); //Updated index!
		ok = 1;
		break;
	case 0x3C5: //Sequencer Data Register			DATA
		//lockVGA();
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
		//unlockVGA();
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_SEQUENCER|getActiveVGA()->registers->SequencerRegisters_Index); //We have been updated!		
		ok = 1;
		break;
	case 0x3C6: //DAC Mask Register?
		getActiveVGA()->registers->DACMaskRegister = value; //Set!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_DACMASKREGISTER); //We have been updated!				
		ok = 1;
		break;
	case 0x3C7: //Write: DAC Address Read Mode Register	ADDRESS
		getActiveVGA()->registers->ColorRegisters.DAC_ADDRESS_READ_MODE_REGISTER = value; //Set!
		getActiveVGA()->registers->ColorRegisters.DAC_STATE_REGISTER.DACState = 0; //Prepared for reads!
		getActiveVGA()->registers->current_3C9 = 0; //Reset!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_DACREAD); //Updated index!
		ok = 1;
		break;
	case 0x3C8: //DAC Address Write Mode Register		ADDRESS
		getActiveVGA()->registers->ColorRegisters.DAC_ADDRESS_WRITE_MODE_REGISTER = value; //Set index!
		getActiveVGA()->registers->ColorRegisters.DAC_STATE_REGISTER.DACState = 3; //Prepared for writes!
		getActiveVGA()->registers->current_3C9 = 0; //Reset!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_DACWRITE); //Updated index!
		ok = 1;
		break;
	case 0x3C9: //DAC Data Register				DATA
		PORT_write_DAC_3C9(value); //Write to 3C9!
		ok = 1;
		break;
	case 0x3CE: //Graphics Controller Address Register	ADDRESS
		getActiveVGA()->registers->GraphicsRegisters_Index = value; //Set index!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_INDEX|INDEX_GRAPHICSCONTROLLER); //Updated index!
		ok = 1;
		break;
	case 0x3CF: //Graphics Controller Data Register		DATA
		if (getActiveVGA()->registers->GraphicsRegisters_Index>=sizeof(getActiveVGA()->registers->GraphicsRegisters.DATA)) break; //Invalid index!
		getActiveVGA()->registers->GraphicsRegisters.DATA[getActiveVGA()->registers->GraphicsRegisters_Index] = value; //Set!
		VGA_calcprecalcs(getActiveVGA(),WHEREUPDATED_GRAPHICSCONTROLLER|getActiveVGA()->registers->GraphicsRegisters_Index); //We have been updated!				
		ok = 1;
		break;
	
	//Precursors compatibility
	case 0x3B8: //MDA Mode Control Register
	case 0x3D8: //CGA mode control register
	case 0x3D9: //CGA palette register
		if (NMIPrecursors) ok = !execNMI(0); //Execute an NMI from Bus!
		break;
	default: //Unknown?
		break; //Not used!
	}
	finishoutput: //Finisher?
	return ok; //Give if we're handled!
}
