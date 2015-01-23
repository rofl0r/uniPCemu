#include "headers/hardware/vga.h" //Our typedefs etc!
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Attribute controller for typedef of attribute info!
#include "headers/hardware/vga_screen/vga_crtcontroller.h" //CRT Controller!
#include "headers/hardware/vga_screen/vga_vram.h" //Our VRAM support!
#include "headers/hardware/vga_screen/vga_vramtext.h" //Our VRAM text support!
#include "headers/hardware/vga_screen/vga_precalcs.h" //Precalculation typedefs etc.
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer!

OPTINLINE uint_32 getcursorlocation(VGA_Type *VGA) //Location of the cursor!
{
	return VGA->precalcs.cursorlocation; //Cursor location!
}

//Character is cursor position?
#define CHARISCURSOR (Sequencer_textmode_charindex==getcursorlocation(VGA))
//Scanline is cursor position?
#define SCANLINEISCURSOR1 (Rendery>=VGA->registers->CRTControllerRegisters.REGISTERS.CURSORSTARTREGISTER.CursorScanLineStart)
#define SCANLINEISCURSOR2 (Rendery<=VGA->registers->CRTControllerRegisters.REGISTERS.CURSORENDREGISTER.CursorScanLineEnd)
//Cursor is enabled atm?
#define CURSORENABLED1 (!VGA->registers->CRTControllerRegisters.REGISTERS.CURSORSTARTREGISTER.CursorDisable)
#define CURSORENABLED2 (VGA->CursorOn)

OPTINLINE byte is_cursorscanline(VGA_Type *VGA,byte Rendery,uint_32 Sequencer_textmode_charindex) //Cursor scanline within character is cursor? Used to be: VGA_Type *VGA, byte ScanLine,uint_32 characterlocation
{
	if (CHARISCURSOR) //Character is cursor?
	{
		if (CURSORENABLED1) //Cursor enabled?
		{
			if (CURSORENABLED2) //Cursor on?
			{
				if (SCANLINEISCURSOR1 && SCANLINEISCURSOR2) //Scanline is cursor top&bottom?
				{
					return 1; //To show the cursor on this scanline?
				}
			}
		}
	}
	return 0; //No cursor!
}

void VGA_Sequencer_TextMode_updateRow(VGA_Type *VGA, SEQ_DATA *Sequencer)
{
	register word character;
	word effectivescanline;
	effectivescanline = Sequencer->Scanline; //Default: our normal scanline!
	effectivescanline >>= VGA_ScanDoubling(VGA); //Apply Scan Doubling here!
	word *currowstatus = &VGA->CRTC.charrowstatus[effectivescanline<<1]; //Current row status!
	Sequencer->chary = character = *currowstatus++; //First is chary!
	Sequencer->charinner_y = *currowstatus; //Second is charinner_y!
	
	Sequencer->charystart = getVRAMScanlineStart(VGA,character); //Calculate row start!

	Sequencer->doublepixels = 0; //Reset double pixels for odd sized screens.
}

void VGA_Sequencer_TextMode(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo) //Render a text mode pixel!
{
	//First, full value to lookup!
	register word character;
	register word charinner;
	//X!
	Sequencer->activex >>= VGA->registers->CRTControllerRegisters.REGISTERS.CRTCMODECONTROLREGISTER.DIV2; //Apply DIVIDE by 2 when needed!

	word *curcolstatus;
	curcolstatus = &VGA->CRTC.charcolstatus[Sequencer->activex<<1]; //Current col status!
	character = *curcolstatus++; //First is charx!
	attributeinfo->charx = character; //Load the character we use!
	attributeinfo->charinner_x = charinner = *curcolstatus; //Second is charinner_y!
	
	register word Sequencer_textmode_charindex; //Where do we find our info!
	Sequencer_textmode_charindex = Sequencer->charystart; //Get the start of the row!
	Sequencer_textmode_charindex += character; //Add the character column for the base character index!
	Sequencer_textmode_charindex += Sequencer->bytepanning; //Apply byte panning to the index!
	Sequencer_textmode_charindex += Sequencer->startmap; //Add the start of the map for us to look at!

	byte currentchar, attribute;
	currentchar = readVRAMplane(VGA,0,Sequencer_textmode_charindex,3); //The character itself! From plane 0!
	attribute = readVRAMplane(VGA,1,Sequencer_textmode_charindex,3); //The attribute itself! From plane 1!
	
	register byte pixel;
	pixel = getcharxy(VGA,attribute,currentchar,(byte)charinner,(byte)Sequencer->charinner_y); //Check for the character, the simple way!
	pixel |= is_cursorscanline(VGA,(byte)Sequencer->charinner_y,Sequencer_textmode_charindex); //Get if we're to plot font, include cursor? (Else back) Used to be: VGA,attributeinfo->charinner_y,charindex
	attributeinfo->fontpixel = pixel; //We're the font pixel?
	attributeinfo->attribute = attribute; //The attribute for this pixel!
}