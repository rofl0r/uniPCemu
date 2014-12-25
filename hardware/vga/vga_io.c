#include "headers/hardware/vga.h" //VGA!
#include "headers/hardware/vga_screen/vga_precalcs.h" //Precalcs!
#include "headers/hardware/ports.h" //Ports!
extern VGA_Type *ActiveVGA; //Active VGA we need!

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

//Port 3C0 info holder:
#define VGA_3C0_HOLDER ActiveVGA->registers->CRTControllerRegisters.REGISTERS.ATTRIBUTECONTROLLERTOGGLEREGISTER

//The flipflop of the port 3C0 toggler and port itself!
#define VGA_3C0_FLIPFLOP VGA_3C0_HOLDER.DataState
#define VGA_3C0_PAL VGA_3C0_HOLDER.PAL
#define VGA_3C0_INDEX VGA_3C0_HOLDER.CurrentIndex

//CRTC

static byte PORT_readCRTC_3B5() //Read CRTC registers!
{
	if ((ActiveVGA->registers->CRTControllerRegisters_Index>0xF) && (ActiveVGA->registers->CRTControllerRegisters_Index<0x12) && (!ActiveVGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALBLANKINGREGISTER.EVRA)) //Reading from light pen location registers?
	{
		switch (ActiveVGA->registers->CRTControllerRegisters_Index) //What index?
		{
		case 0x10: //Light pen high?
			return ActiveVGA->registers->lightpen_high; //High lightpen!
		case 0x11: //Light pen low?
			return ActiveVGA->registers->lightpen_low; //Low lightpen!
		default: //Unknown?
			break; //Run normally.
		}
	}
	if (ActiveVGA->registers->CRTControllerRegisters_Index>=sizeof(ActiveVGA->registers->CRTControllerRegisters.DATA)) //Out of range?
	{
		return PORT_UNDEFINED_RESULT; //Undefined!
	}
	return ActiveVGA->registers->CRTControllerRegisters.DATA[ActiveVGA->registers->CRTControllerRegisters_Index]; //Give normal index!
}

static void PORT_write_CRTC_3B5(byte value)
{
	byte temp; //For index 7 write protected!
	byte index = ActiveVGA->registers->CRTControllerRegisters_Index; //What index?
	/*if ((index>0xF) && (index<0x12) && (!ActiveVGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALBLANKINGREGISTER.EVRA)) //Writing to light pen location registers?
	{
		return; //Write protected: light pen registers, they don't exist!
	}*/
	if ((index<8) && (ActiveVGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.Protect)) //Protected?
	{
		if (index==7) //Overflow register (allow changes in the bit 4 (line compare))
		{
			value &= 0x10; //Only line compare 8 can be changed!
			temp = ActiveVGA->registers->CRTControllerRegisters.DATA[0x07]; //Load the overflow register!
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
			ActiveVGA->registers->VerticalDisplayTotalReached = 1; //Force end-of-screen reached!
		}
		return; //Don't do anything on this register anymore!
	}
	if (index>0x18) //Not a VGA register OR is a READ-ONLY register (undocumented)?
	{
		return; //Write protected OR invalid register!
	}
	if (index<sizeof(ActiveVGA->registers->CRTControllerRegisters.DATA)) //Within range?
	{
		//Normal register update?
		ActiveVGA->registers->CRTControllerRegisters.DATA[index] = value; //Set!
		if (index==0x11) //Bit 4&5 of the Vertical Retrace End register have other effects!
		{
			//Bit 5: Input status Register 0, bit 7 needs to be updated with Bit 5?
			if (ActiveVGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalInterrupt_Enabled && !ActiveVGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.VerticalInterrupt_Disabled) //Vertical interrupt enabled?
			{
				ActiveVGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.CRTInterruptPending = 1; //Enable pending interrupt till VBlank occurrs!
			}
		}
		if (!ActiveVGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALBLANKINGREGISTER.EVRA) //Force to 1?
		{
			ActiveVGA->registers->CRTControllerRegisters.REGISTERS.ENDHORIZONTALBLANKINGREGISTER.EVRA = 1; //Force to 1!
			if (index!=3) //We've been updated too?
			{
				VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_CRTCONTROLLER|3); //We have been updated!
			}
		}
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_CRTCONTROLLER|index); //We have been updated!
	}
}

//ATTRIBUTE CONTROLLER

static void PORT_write_ATTR_3C0(byte value) //Attribute controller registers!
{
	if (!VGA_3C0_FLIPFLOP) //Index mode?
	{
		//Mirror to state register!
		VGA_3C0_PAL = ((value&0x20)>>5); //Palette Address Source!
		VGA_3C0_INDEX = (value&0x1F); //Which index?
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_INDEX|INDEX_ATTRIBUTECONTROLLER); //Updated index!
	}
	else //Data mode?
	{
		if (VGA_3C0_INDEX<sizeof(ActiveVGA->registers->AttributeControllerRegisters.DATA)) //Within range?
		{
			ActiveVGA->registers->AttributeControllerRegisters.DATA[VGA_3C0_INDEX] = value; //Set!
		}
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_ATTRIBUTECONTROLLER|VGA_3C0_INDEX); //We have been updated!
	}

	VGA_3C0_FLIPFLOP = !VGA_3C0_FLIPFLOP; //Flipflop!
}

//MISC

static void PORT_write_MISC_3C2(byte value) //Misc Output register!
{
	ActiveVGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.DATA = value; //Set!
	VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_MISCOUTPUTREGISTER); //We have been updated!
}

//DAC

static byte PORT_read_DAC_3C9() //DAC Data register!
{
	word index = ActiveVGA->registers->ColorRegisters.DAC_ADDRESS_READ_MODE_REGISTER; //Load current DAC index!
	index <<= 2; //Multiply for the index!
	index |= ActiveVGA->registers->current_3C9; //Current index!
	byte result; //The result!
	result = ActiveVGA->registers->DAC[index]; //Read the result!

	if (++ActiveVGA->registers->current_3C9>2) //Next entry?
	{
		++ActiveVGA->registers->ColorRegisters.DAC_ADDRESS_READ_MODE_REGISTER; //Next entry!
		if (ActiveVGA->registers->ColorRegisters.DAC_ADDRESS_READ_MODE_REGISTER>0xFF) //Overflow?
		{
			ActiveVGA->registers->ColorRegisters.DAC_ADDRESS_READ_MODE_REGISTER = 0; //Reset!
		}
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_INDEX|INDEX_DACREAD); //Updated index!
		ActiveVGA->registers->current_3C9 = 0; //Reset!
	}
	return result; //Give the result!
}

static void PORT_write_DAC_3C9(byte value) //DAC Data register!
{
	word entrynumber = ActiveVGA->registers->ColorRegisters.DAC_ADDRESS_WRITE_MODE_REGISTER; //Current entry number!
	word index = entrynumber; //Load current DAC index!
	index <<= 2; //Multiply for the index!
	index |= ActiveVGA->registers->current_3C9; //Current index!
	ActiveVGA->registers->DAC[index] = (value&0x3F); //Write the data!
	VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_DAC|entrynumber); //We've been updated!
	
	if (++ActiveVGA->registers->current_3C9>2) //Next entry?
	{
		if (++entrynumber>0xFF) //Overflow?
		{
			entrynumber = 0; //Reset!
		}
		ActiveVGA->registers->ColorRegisters.DAC_ADDRESS_WRITE_MODE_REGISTER = entrynumber; //Update the entry number!
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_INDEX|INDEX_DACWRITE); //Updated index!
		ActiveVGA->registers->current_3C9 = 0; //Reset!
	}
}

/*

Finally: the read/write handlers themselves!

*/

byte PORT_readVGA(word port) //Read from a port/register!
{
	if (!getActiveVGA()) //No active VGA?
	{
		raiseError("VGA","VGA Port Out, but no active VGA loaded!");
	}
	switch (port) //What port?
	{
	case 0x3B0:
	case 0x3B2:
	case 0x3B6: //Decodes to 3B4!
	case 0x3B4: //CRTC Controller Address Register		ADDRESS
	case 0x3D4: //CRTC Controller Address Register		ADDRESS
		return ActiveVGA->registers->CRTControllerRegisters_Index; //Give!
		break;
	case 0x3B1:
	case 0x3B3:
	case 0x3B7: //Decodes to 3B5!
	case 0x3B5: //CRTC Controller Data Register		DATA
	case 0x3D5: //CRTC Controller Data Register		DATA
		return PORT_readCRTC_3B5(); //Read port 3B5!
		break;
	case 0x3BA: //Read: Input Status #1 Register (mono)	DATA
		return ActiveVGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.DATA; //Give!
		break;
	case 0x3C0: //Attribute Address/Data register		ADDRESS/DATA
		//Do nothing: write only port! Undefined!
		return (VGA_3C0_PAL<<5)|VGA_3C0_INDEX; //Give the saved information!
		break;
	case 0x3C1: //Attribute Data Read Register		DATA
		if (VGA_3C0_INDEX>=sizeof(ActiveVGA->registers->AttributeControllerRegisters.DATA)) break; //Out of range!
		return ActiveVGA->registers->AttributeControllerRegisters.DATA[VGA_3C0_INDEX]; //Read from current index!
		break;
	case 0x3C2: //Read: Input Status #0 Register		DATA
		return ActiveVGA->registers->ExternalRegisters.INPUTSTATUS0REGISTER.DATA; //Give the register!
		break;
	case 0x3C3: //Video subsystem enable?
		return ActiveVGA->registers->VGA_3C3|ActiveVGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.RAM_Enable; //RAM enabled?
		break;
	case 0x3C4: //Sequencer Address Register		ADDRESS
		return ActiveVGA->registers->SequencerRegisters_Index; //Give the index!
		break;
	case 0x3C5: //Sequencer Data Register			DATA
		if (ActiveVGA->registers->SequencerRegisters_Index>=sizeof(ActiveVGA->registers->SequencerRegisters.DATA)) break; //Out of range!
		return ActiveVGA->registers->SequencerRegisters.DATA[ActiveVGA->registers->SequencerRegisters_Index]; //Give the data!
		break;
	case 0x3C6: //DAC Mask Register?
		return ActiveVGA->registers->DACMaskRegister; //Give!
		break;
	case 0x3C7: //Read: DAC State Register			DATA
		return ActiveVGA->registers->ColorRegisters.DAC_STATE_REGISTER.DATA; //Give!
		break;
	case 0x3C8: //DAC Address Write Mode Register		ADDRESS
		return ActiveVGA->registers->ColorRegisters.DAC_ADDRESS_WRITE_MODE_REGISTER; //Give!
		break;
	case 0x3C9: //DAC Data Register				DATA
		return PORT_read_DAC_3C9(); //Read port 3C9!
		break;
	case 0x3CA: //Read: Feature Control Register (mono Read)	DATA
		return ActiveVGA->registers->ExternalRegisters.FEATURECONTROLREGISTER.DATA; //Give!
		break;
	case 0x3CC: //Read: Miscellaneous Output Register	DATA
		return ActiveVGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.DATA; //Give!
		break;
	case 0x3CE: //Graphics Controller Address Register	ADDRESS
		return ActiveVGA->registers->GraphicsRegisters_Index; //Give!
		break;
	case 0x3CF: //Graphics Controller Data Register		DATA
		if (ActiveVGA->registers->GraphicsRegisters_Index>=sizeof(ActiveVGA->registers->GraphicsRegisters.DATA)) break; //Out of range!
		return ActiveVGA->registers->GraphicsRegisters.DATA[ActiveVGA->registers->GraphicsRegisters_Index]; //Give!
		break;

	case 0x3DA: //Input Status #1 Register (color)	DATA
		ActiveVGA->registers->CRTControllerRegisters.REGISTERS.ATTRIBUTECONTROLLERTOGGLEREGISTER.DataState = 0; //Reset flipflop for 3C0!
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_CRTCONTROLLER|0x24); //We have been updated!		
		return ActiveVGA->registers->ExternalRegisters.INPUTSTATUS1REGISTER.DATA; //Give!
		break;
	default: //Unknown?
		break; //Not used!
	}
	return PORT_UNDEFINED_RESULT; //Disabled for now or unknown port!
}

extern byte LOG_VRAM_WRITES; //Log it?
void PORT_writeVGA(word port, byte value) //Write to a port/register!
{
	if (!getActiveVGA()) //No active VGA?
	{
		raiseError("VGA","VGA Port Out, but no active VGA loaded!");
	}
	switch (port) //What port?
	{
	case 0x3B0:
	case 0x3B2:
	case 0x3B6: //Decodes to 3B4!
	case 0x3B4: //CRTC Controller Address Register		ADDRESS
	case 0x3D4: //CRTC Controller Address Register		ADDRESS
		ActiveVGA->registers->CRTControllerRegisters_Index = value; //Set!
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_INDEX|INDEX_CRTCONTROLLER); //Updated index!
		break;
	case 0x3B1:
	case 0x3B3:
	case 0x3B7: //Decodes to 3B5!
	case 0x3B5: //CRTC Controller Data Register		DATA
	case 0x3D5: //CRTC Controller Data Register		DATA
		PORT_write_CRTC_3B5(value); //Write CRTC!
		break;
	case 0x3BA: //Write: Feature Control Register (mono)		DATA
	case 0x3CA: //Same as above!
	case 0x3DA: //Same!
		ActiveVGA->registers->ExternalRegisters.FEATURECONTROLREGISTER.DATA = value; //Set!
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_FEATURECONTROLREGISTER); //We have been updated!
		break;
	case 0x3C0: //Attribute Address/Data register		ADDRESS/DATA
		PORT_write_ATTR_3C0(value); //Write to 3C0!
		break;
	case 0x3C1: //Attribute Data Read Register		DATA
		//Undefined!
		break;
	case 0x3C2: //Write: Miscellaneous Output Register	DATA
	case 0x3CC: //Same as above!
		PORT_write_MISC_3C2(value); //Write to 3C2!
		break;
	case 0x3C3: //Video subsystem enable
		ActiveVGA->registers->ExternalRegisters.MISCOUTPUTREGISTER.RAM_Enable = (value&1); //Enable RAM?
		ActiveVGA->registers->VGA_3C3 = (value&0xFE); //Write port 3C3, clear our unused bit for easier retrieval!
		break;
	case 0x3C4: //Sequencer Address Register		ADDRESS
		ActiveVGA->registers->SequencerRegisters_Index = value; //Set!
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_INDEX|INDEX_SEQUENCER); //Updated index!
		break;
	case 0x3C5: //Sequencer Data Register			DATA
		if (ActiveVGA->registers->SequencerRegisters_Index>7) return; //Invalid data!
		if (ActiveVGA->registers->SequencerRegisters_Index==7) //Disable display till write to sequencer registers 0-6?
		{
			ActiveVGA->registers->CRTControllerDontRender = 0xFF; //Force to 0xFF indicating display disabled!
		}
		else if ((ActiveVGA->registers->SequencerRegisters_Index<7) && (ActiveVGA->registers->CRTControllerDontRender)) //Disabled and enabled again?
		{
			ActiveVGA->registers->CRTControllerDontRender = 0x00; //Reset, effectively enabling VGA rendering!
		}
		if (ActiveVGA->registers->SequencerRegisters_Index>=sizeof(ActiveVGA->registers->SequencerRegisters.DATA)) break; //Out of range!
		ActiveVGA->registers->SequencerRegisters.DATA[ActiveVGA->registers->SequencerRegisters_Index] = value; //Set!
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_SEQUENCER|ActiveVGA->registers->SequencerRegisters_Index); //We have been updated!		
		break;
	case 0x3C6: //DAC Mask Register?
		ActiveVGA->registers->DACMaskRegister = value; //Set!
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_DACMASKREGISTER); //We have been updated!				
		break;
	case 0x3C7: //Write: DAC Address Read Mode Register	ADDRESS
		ActiveVGA->registers->ColorRegisters.DAC_ADDRESS_READ_MODE_REGISTER = value; //Set!
		ActiveVGA->registers->ColorRegisters.DAC_STATE_REGISTER.DACState = 0; //Prepared for reads!
		ActiveVGA->registers->current_3C9 = 0; //Reset!
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_INDEX|INDEX_DACREAD); //Updated index!
		break;
	case 0x3C8: //DAC Address Write Mode Register		ADDRESS
		ActiveVGA->registers->ColorRegisters.DAC_ADDRESS_WRITE_MODE_REGISTER = value; //Set index!
		ActiveVGA->registers->ColorRegisters.DAC_STATE_REGISTER.DACState = 3; //Prepared for writes!
		ActiveVGA->registers->current_3C9 = 0; //Reset!
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_INDEX|INDEX_DACWRITE); //Updated index!
		break;
	case 0x3C9: //DAC Data Register				DATA
		PORT_write_DAC_3C9(value); //Write to 3C9!
		break;
	case 0x3CE: //Graphics Controller Address Register	ADDRESS
		ActiveVGA->registers->GraphicsRegisters_Index = value; //Set index!
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_INDEX|INDEX_GRAPHICSCONTROLLER); //Updated index!
		break;
	case 0x3CF: //Graphics Controller Data Register		DATA
		if (ActiveVGA->registers->GraphicsRegisters_Index>=sizeof(ActiveVGA->registers->GraphicsRegisters.DATA)) return; //Invalid index!
		ActiveVGA->registers->GraphicsRegisters.DATA[ActiveVGA->registers->GraphicsRegisters_Index] = value; //Set!
		VGA_calcprecalcs(ActiveVGA,WHEREUPDATED_GRAPHICSCONTROLLER|ActiveVGA->registers->GraphicsRegisters_Index); //We have been updated!				
		break;
	default: //Unknown?
		break; //Not used!
	}
}