#include "headers/hardware/vga.h" //Basic VGA stuff!
#include "headers/hardware/vga_screen/vga_vram.h" //VRAM!
#include "headers/hardware/vga_rest/textmodedata.h" //Text mode data for loading!
#include "headers/hardware/vga_screen/vga_crtcontroller.h" //For character sizes!
#include "headers/hardware/vga_screen/vga_vramtext.h" //Our VRAM text support!
#include "headers/support/log.h" //Logging support!
#include "headers/support/highrestimer.h" //High resolution timing!

extern byte is_loadchartable; //Loading character table?
extern VGA_Type *ActiveVGA;

void VGALoadCharTable(VGA_Type *VGA, int rows, word startaddr) //Load a character table from ROM to VRAM!
{
	if (!VGA) //No active VGA?
	{
		raiseError("VGA","VGA Load Character Table, but no VGA loaded!");
	}
	is_loadchartable = 1; //Loading character table!
	uint_32 counter;
	switch (rows)
	{
	case 14:
		for (counter=0;counter<sizeof(int10_font_14);counter++) //Write font!
		{
			writeVRAMplane(ActiveVGA,2,startaddr+counter,int10_font_14[counter]); //Write font byte!
		}
		break;
	case 8:
		for (counter=0;counter<sizeof(int10_font_08);counter++) //Write font!
		{
			writeVRAMplane(ActiveVGA,2,startaddr+counter,int10_font_08[counter]); //Write font byte!
		}
		break;
	case 16:
		for (counter=0;counter<sizeof(int10_font_16);counter++) //Write font!
		{
			writeVRAMplane(ActiveVGA,2,startaddr+counter,int10_font_16[counter]); //Write font byte!
		}
		break;
	default: //Unknown ammount of rows!
		//Do nothing: we don't know what font to use!
		break;
	}
	is_loadchartable = 0; //Not loading character table!
}

OPTINLINE void fillgetcharxy_values(VGA_Type *VGA, int singlecharacter)
{
	byte *getcharxy_values;
	getcharxy_values = &VGA->getcharxy_values[0]; //The values!
	word character = 0; //From 0-255!
	if (singlecharacter!=-1) //Single character only?
	{
		character = (word)singlecharacter; //Only single character to edit!
	}
	for (;character<0x100;) //256 characters (8 bits)!
	{
		byte attribute = 0; //0 or 1 (bit value 0x4 of the attribute, 1 bit)!
		for (;attribute<2;) //2 attributes!
		{
			byte y = 0; //From 0-32 (5 bits)!
			for (;y<0x20;) //33 rows!
			{
				uint_32 characterset_offset; //First, the character set, later translated to the real charset offset!
				if (attribute) //Charset A? (bit 2 (value 0x4) set?)
				{
					characterset_offset = VGA->registers->SequencerRegisters.REGISTERS.CHARACTERMAPSELECTREGISTER.CharacterSetASelect_low+
										  (VGA->registers->SequencerRegisters.REGISTERS.CHARACTERMAPSELECTREGISTER.CharacterSetASelect_high<<2); //Charset A!
				}
				else //Charset B?
				{
					characterset_offset = VGA->registers->SequencerRegisters.REGISTERS.CHARACTERMAPSELECTREGISTER.CharacterSetBSelect_low+
										  (VGA->registers->SequencerRegisters.REGISTERS.CHARACTERMAPSELECTREGISTER.CharacterSetBSelect_high<<2); //Charset B!
				}

				characterset_offset <<= 14; //The calculated offset from the base!
				if (characterset_offset>=3) //Base, normally 0, or 0x2000!
				{
					characterset_offset += 0x2000; //Other base!
				}
				//characterset_offset += OPTMUL32(character,getcharacterheight(VGA)); //Start adress of character!
				uint_32 character2;
				character2 = character; //Load!
				character2 <<= 5; //Multiply by 32!
				characterset_offset += character2; //Start of the character!
				//characterset_offset += SAFEMODUINT32(y,getcharacterheight(VGA)); //1 byte per row!
				characterset_offset += y; //Add the row!
				
				byte row = readVRAMplane(VGA,2,characterset_offset,0); //Read the row from the character generator, use second buffer! Don't do anything special, just because we're from the renderer!
				getcharxy_values[character|(attribute<<8)|(y<<9)] = row; //Store the row for the character generator!
				++y; //Next row!
			}
			++attribute; //Next attribute!
		}
		++character; //Next character!
		if (singlecharacter!=-1) return; //Stop on single character update!
	}
}

void VGA_plane2updated(VGA_Type *VGA, uint_32 address) //Plane 2 has been updated?
{
	fillgetcharxy_values(VGA,(address>>5)); //Update the character!
}

//This is heavy: it doubles (with about 25ms) the rendering time needed to render a line.
OPTINLINE byte getcharxy(VGA_Type *VGA, byte attribute, byte character, byte x, byte y) //Retrieve a characters x,y pixel on/off from table!
{
	if (!VGA) //No active VGA?
	{
		return 0; //Nothing!
	}

	byte newx = x; //Default: use the 9th bit if needed!
	byte newy = y; //Default: use the standard y!
	attribute >>= 2; //...
	attribute &= 1; //... Take bit 2 to get the actual attribute we need!
	if (newx&0xFFF8) //Extra ninth bit?
	{
		newx = 7; //Only 7 max!
		if (getcharacterwidth(VGA)!=8) //What width? 9 wide?
		{
			if (!((!VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.LineGraphicsEnable) && ((character&0xE0)==0xC0))) //Replicate the 8th one (C0-FLAG_DF)? (Highest 3 bits are 0x6)
			{ //9th bit becomes background color!
				return 0; //Background color for 9th pixel!
			}
		}
	}
	
	static uint_32 lastcharinfo = 0; //attribute|character|0x80|row, bit8=Set?
	uint_32 lastlookup;
	lastlookup = character;
	lastlookup <<= 1;
	lastlookup |= 1;
	lastlookup <<= 1;
	lastlookup |= attribute;
	lastlookup <<= 5;
	lastlookup |= newy;
	if ((lastcharinfo&0xFFFFFF)!=lastlookup) //Last row not yet loaded?
	{
		uint_32 charloc;
		charloc = newy;
		charloc <<= 1;
		charloc |= attribute;
		charloc <<= 8;
		charloc |= character; //Character position!
		lastcharinfo = ((VGA->getcharxy_values[charloc]<<16)|(character<<8)|0x80|(attribute<<5)|newy); //Last character info loaded!
		lastcharinfo = VGA->getcharxy_values[charloc]; //Lookup!
		lastcharinfo <<= 8; //Create space for the character!
		lastcharinfo |= character;
		lastcharinfo <<= 1;
		lastcharinfo |= 1; //Used!
		lastcharinfo <<= 1;
		lastcharinfo |= attribute;
		lastcharinfo <<= 5;
		lastcharinfo |= newy;
	}
	
	byte result = ((lastcharinfo>>(23-newx))&1); //Give bit!
	return result; //Give bit!
}

OPTINLINE byte getcharxy_8(byte character, int x, int y) //Retrieve a characters x,y pixel on/off from the unmodified 8x8 table!
{
	static uint_32 lastcharinfo; //attribute|character|0x80|row, bit8=Set?

	if ((lastcharinfo&0xFFFF)!=((character<<8)|0x80|y)) //Last row not yet loaded?
	{
		uint_32 addr = 0; //Address for old method!
		addr += character<<3; //Start adress of character!
		addr += (y&7); //1 byte per row!
		
		byte lastrow = int10_font_08[addr]; //Read the row from the character generator!
		lastcharinfo = ((lastrow<<16)|(character<<8)|0x80|y); //Last character info loaded!
	}

	byte bitpos = 23-(x%8); //x or 7-x for reverse?
	return ((lastcharinfo&(1<<bitpos))>>bitpos); //Give result!
}

void VGA_dumpchar(VGA_Type *VGA, byte c)
{
	byte y=0;
	byte maxx=0;
	maxx = VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.DotMode8?8:9; //8/9 dot mode!
	byte maxy=0;
	maxy = VGA->registers->CRTControllerRegisters.REGISTERS.MAXIMUMSCANLINEREGISTER.MaximumScanLine+1; //Ammount of scanlines!
	for (;;)
	{
		char row[10]; //9-character row for the letter!
		char buf[2] = " "; //Zero terminated string!
		memset(row,0,sizeof(row));
		byte x = 0;
		for (;;)
		{
			buf[0] = getcharxy(VGA,0xF,c,x,y)?'X':' '; //Load character pixel!
			strcat(row,buf); //The character to use!
			if (++x>=maxx) goto nexty;
		}
		nexty:
		dolog("VRAM_CHARS","%s",row); //Log the row!
		if (++y>=maxy) break; //Done!
	}
	dolog("VRAM_CHARS",""); //Empty row!
}

void VGA_dumpstr(VGA_Type *VGA, char *s)
{
	if (!s) return;
	char *s2 = s; //Load string!
	while (*s2!='\0') //Not EOS?
	{
		VGA_dumpchar(VGA,*s2++); //Dump the next character!
	}
}

void VGA_dumpFonts()
{
	VGA_Type *VGA = getActiveVGA(); //Get the active VGA!
	if (VGA) //Gotten?
	{
		VGA_dumpstr(VGA,"Testing ABC!");
		return;
		//Dump of all available characters!
		int c=0;
		for (;;)
		{
			VGA_dumpchar(VGA,c);
			if (++c>0xFF) return; //Abort: done!
		}
	}
}