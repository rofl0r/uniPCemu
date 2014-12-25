#include "headers/hardware/vga.h" //Our typedefs etc!
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Attribute controller for typedef of attribute info!
#include "headers/hardware/vga_screen/vga_sequencer_textmode_cursor.h" //Cursor!
#include "headers/hardware/vga_screen/vga_crtcontroller.h" //CRT controller!
#include "headers/hardware/vga_screen/vga_vram.h" //Our VRAM support!
#include "headers/hardware/vga_screen/vga_precalcs.h" //Precalculation typedefs etc.
#include "headers/support/log.h" //Logging support!
#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer!

//Character sizes in pixels!
OPTINLINE byte getcharacterwidth(VGA_Type *VGA)
{
	return VGA->precalcs.characterwidth; //8 or 9 dots per line?
}

OPTINLINE byte getcharacterheight(VGA_Type *VGA)
{
	return VGA->precalcs.characterheight; //The character height!
}

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

static OPTINLINE byte is_cursorscanline(VGA_Type *VGA,word Sequencer_x,byte Rendery,uint_32 Sequencer_textmode_charindex,VGA_AttributeInfo *Sequencer_attributeinfo) //Cursor scanline within character is cursor? Used to be: VGA_Type *VGA, byte ScanLine,uint_32 characterlocation
{
	if (CURSORENABLED1) //Cursor enabled?
	{
		if (CHARISCURSOR) //Character is cursor?
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
	return 0; //Done!
}

/*void VGA_Sequencer_TextMode(VGA_Type *VGA,VGA_AttributeInfo *Sequencer_Attributeinfo, word tempx,word tempy,word x,word Scanline,uint_32 bytepanning) //Process text-data line! Returns attribute info!
{
	/
	//For detecting newlines!
	static word last_x;
	static byte gotlast_x = 0;

	//Last row info!
	static uint_32 last_charystart; //Last start of a row!
	//Last row savestate info!
	static word last_tempy; //Last Y position!
	static byte last_charheight; //Last character height!
	static byte gotlast_y = 0; //Got last to process?
	byte y_updated = 0; //Y updated (combine with checking charx)?

	//Last column&character info!
	static uint_32 last_charx; //Last character X position!
	static byte last_charwidth; //Last character width!
	static byte last_bytepanning; //Last byte panning!
	static byte last_character; //Last character on that position in the VRAM plane!
	static byte last_attribute; //Last attribute on that position in the VRAM plane!
	static byte gotlast_character = 0; //Got last to process?
	static uint_32 Sequencer_textmode_charindex; //Charindex within VRAM plane 0&1!
	static byte x_updated = 0;

	SEQ_DATA *Sequencer = GETSEQUENCER(VGA);
	Sequencer_Attributeinfo->attribute_graphics = 0; //We're a text-mode attribute, needed for the attribute controller!

//First: character info!
	byte charheight = getcharacterheight(VGA);
	Sequencer_Attributeinfo->chary = Scanline; //Y of current character!

	if (gotlast_x && (x<last_x)) //Newline detected with only a single line?
	{
		gotlast_y = 0; //Force refresh our row (fixes bugging at single row display (1 row and/or 1 column))!
	}
	
	if (!gotlast_y || !(gotlast_y && last_charheight==charheight && last_tempy==tempy)) //Different character row? Update savestate!
	{
		uint_32 last_tempysaved = last_tempy; //For checking if we're a different character line!
		last_tempy = tempy; //Last temp Y saving for next need!
		y_updated = (last_tempysaved!=last_tempy); //Row changed?
		last_charheight = charheight; //Last charheight!
		last_charystart = getVRAMScanlineStart(VGA,Sequencer_Attributeinfo->chary); //Calculate row start!
		last_charystart += Sequencer->currentPrecalcs->startmap; //What start address?
		gotlast_y = 1; //We've got the last Y and have been updated!
	} //This works!

	//tempx is always different, we can safely assume!
	byte charwidth = getcharacterwidth(VGA); //Character width!
	byte charx = Sequencer_Attributeinfo->charx = OPTDIV(tempx,charwidth); //X of current character!
	byte charinnerx = Sequencer_Attributeinfo->charinner_x = OPTMOD(tempx,charwidth); //Current pixel within the ScanLine!

	if (y_updated || !(gotlast_character && last_charwidth==charwidth && last_charx==charx && last_bytepanning==bytepanning)) //Not the same character or Y updated?
	{
		last_charx = Sequencer_Attributeinfo->charx = charx; //Last charx update!
		last_bytepanning = bytepanning; //Last bytepanning update!

		Sequencer_textmode_charindex = last_charystart; //Get the start of the row!
		Sequencer_textmode_charindex += last_charx; //Add the character column for the base character index!
		Sequencer_textmode_charindex += last_bytepanning; //Apply byte panning to the index!

		last_character = readVRAMplane(VGA,0,Sequencer_textmode_charindex,3); //The character itself! From plane 0!
		last_attribute = Sequencer_Attributeinfo->attribute = readVRAMplane(VGA,1,Sequencer_textmode_charindex,3); //The attribute itself! From plane 1!
		
		last_charwidth = charwidth; //Update last characterwidth!
		gotlast_character = 1; //We've got the last character data!
		//x_updated = 1; //We're updated!
	}
	
	Sequencer_Attributeinfo->attributesource = 0x22222222; //Set our source data location we're using!
	
	//Set last x coordinates for checking re-reading the same row/column!
	last_x = x;
	gotlast_x = 1;
	
	byte y2=VGA->LinesToRender; //The ammount to render!
	if (!y2) return; //Abort when nothing to render!
	nextpixel: //Process all lines to render!
	{
		--y2; //Next pixel!
		byte pixel = getcharxy(VGA,last_attribute,last_character,charinnerx,y2); //Check for the character, the simple way!
		if (!pixel) //Not already on?
		{
			pixel = is_cursorscanline(VGA,x,y2,Sequencer_textmode_charindex,Sequencer_Attributeinfo); //Get if we're to plot font, include cursor? (Else back) Used to be: VGA,attributeinfo->charinner_y,charindex
		}
		VGA->CurrentScanLine[y2] = pixel; //Set the pixel to use!
		if (!y2) return; //Stop searching when done!
		goto nextpixel; //Next pixel!
	}/
}*/

void VGA_Sequencer_TextMode(VGA_Type *VGA, SEQ_DATA *Sequencer, VGA_AttributeInfo *attributeinfo) //Render a text mode pixel!
{
	attributeinfo->attributesource = 0x33333333; //Our plane sources!
	
	//First, full value to lookup!
	register uint_32 character;
	register uint_32 charinner;
	register uint_32 charsize;
	//X!
	charsize = getcharacterwidth(VGA); //Current character width!
	character = charinner = Sequencer->tempx; //Current position to render into both values!
	character /= charsize; //Current character!
	charsize *= character; //Calculate total pixels for the character to start!
	charinner -= charsize; //Calculate inner pixel!
	attributeinfo->charx = character; //Load the character!
	attributeinfo->charinner_x = charinner; //Load the inner position!

	//Y!
	charsize = getcharacterheight(VGA); //Current character width!
	character = charinner = Sequencer->Scanline; //Current position to render into both values!
	character /= charsize; //Current character!
	charsize *= character; //Calculate total pixels for the character to start!
	charinner -= charsize; //Calculate inner pixel!
	attributeinfo->chary = character; //Load the character!
	attributeinfo->charinner_y = charinner; //Load the inner position!
	
	register uint_32 charystart;
	charystart = getVRAMScanlineStart(VGA,character); //Calculate row start!
	charystart += Sequencer->startmap; //What start address?
	
	register uint_32 Sequencer_textmode_charindex; //Where do we find our info!
	Sequencer_textmode_charindex = charystart; //Get the start of the row!
	Sequencer_textmode_charindex += attributeinfo->charx; //Add the character column for the base character index!
	Sequencer_textmode_charindex += Sequencer->bytepanning; //Apply byte panning to the index!

	register uint_32 currentchar;
	currentchar = readVRAMplane(VGA,0,Sequencer_textmode_charindex,3); //The character itself! From plane 0!
	attributeinfo->attribute = readVRAMplane(VGA,1,Sequencer_textmode_charindex,3); //The attribute itself! From plane 1!
	
	register byte pixel = getcharxy(VGA,attributeinfo->attribute,currentchar,attributeinfo->charinner_x,charinner); //Check for the character, the simple way!
	pixel |= is_cursorscanline(VGA,attributeinfo->charx,charinner,Sequencer_textmode_charindex,attributeinfo); //Get if we're to plot font, include cursor? (Else back) Used to be: VGA,attributeinfo->charinner_y,charindex
	attributeinfo->fontpixel = pixel; //We're the font pixel?
}