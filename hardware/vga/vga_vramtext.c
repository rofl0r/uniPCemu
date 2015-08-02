#include "headers/hardware/vga.h" //Basic VGA stuff!
#include "headers/hardware/vga_screen/vga_vram.h" //VRAM!
#include "headers/hardware/vga_rest/textmodedata.h" //Text mode data for loading!
#include "headers/hardware/vga_screen/vga_crtcontroller.h" //For character sizes!
#include "headers/hardware/vga_screen/vga_vramtext.h" //Our VRAM text support!
#include "headers/support/log.h" //Logging support!
#include "headers/support/highrestimer.h" //High resolution timing!

extern byte is_loadchartable; //Loading character table?

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
			writeVRAMplane(getActiveVGA(),2,startaddr+counter,int10_font_14[counter],0); //Write font byte!
		}
		break;
	case 8:
		for (counter=0;counter<sizeof(int10_font_08);counter++) //Write font!
		{
			writeVRAMplane(getActiveVGA(),2,startaddr+counter,int10_font_08[counter],0); //Write font byte!
		}
		break;
	case 16:
		for (counter=0;counter<sizeof(int10_font_16);counter++) //Write font!
		{
			writeVRAMplane(getActiveVGA(),2,startaddr+counter,int10_font_16[counter],0); //Write font byte!
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
				uint_32 characterset_offset, add2000; //First, the character set, later translated to the real charset offset!
				if (attribute) //Charset A? (bit 2 (value 0x4) set?)
				{
					characterset_offset = VGA->registers->SequencerRegisters.REGISTERS.CHARACTERMAPSELECTREGISTER.CharacterSetASelect_low;
					add2000 = VGA->registers->SequencerRegisters.REGISTERS.CHARACTERMAPSELECTREGISTER.CharacterSetASelect_high; //Charset A!
				}
				else //Charset B?
				{
					characterset_offset = VGA->registers->SequencerRegisters.REGISTERS.CHARACTERMAPSELECTREGISTER.CharacterSetBSelect_low;
					add2000 = VGA->registers->SequencerRegisters.REGISTERS.CHARACTERMAPSELECTREGISTER.CharacterSetBSelect_high; //Charset B!
				}

				characterset_offset <<= 1; //Calculated 0,4,8,c! Add room for 0x2000!
				characterset_offset |= add2000; //Add the 2000 mark!
				characterset_offset <<= 13; //Shift to the start position: 0,4,8,c,2,6,a,e!

				word character2;
				character2 = character; //Load!
				character2 <<= 5; //Multiply by 32!
				characterset_offset += character2; //Start of the character!
				characterset_offset += y; //Add the row!
				
				byte row = readVRAMplane(VGA,2,characterset_offset,0); //Read the row from the character generator! Don't do anything special, just because we're from the renderer!
				getcharxy_values[(character<<6)|(y<<1)|attribute] = row; //Store the row for the character generator!
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
	fillgetcharxy_values(VGA,(address>>5)); //Update the character: character number is every 32 locations (5 bits), with 2-bit plane (2 bits), totalling 7 bits per character!
}

//This is heavy: it doubles (with about 25ms) the rendering time needed to render a line.
byte getcharxy(VGA_Type *VGA, byte attribute, byte character, byte x, byte y) //Retrieve a characters x,y pixel on/off from table!
{
	const static byte shift[8] = { 7, 6, 5, 4, 3, 2, 1, 0 }; //Shift for the pixel!
	static byte lastrow; //Last retrieved character row data!
	static word lastcharinfo = 0; //attribute|character|row|1, bit0=Set?
	register word lastlookup;
	register byte newx = x; //Default: use the 9th bit if needed!

	attribute >>= 2; //...
	attribute &= 1; //... Take bit 2 to get the actual attribute we need!
	if (newx&0xF8) //Extra ninth bit?
	{
		newx = 7; //Only 7 max!
		if (getcharacterwidth(VGA)!=8) //What width? 9 wide?
		{
			if (VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.LineGraphicsEnable || ((character & 0xE0) != 0xC0)) return 0; //9th bit is always background?
		}
	}
	
	lastlookup = character;
	lastlookup <<= 1;
	lastlookup |= attribute;
	lastlookup <<= 5;
	lastlookup |= y;
	lastlookup <<= 1;
	lastlookup |= 1; //A filled record!
	if (lastcharinfo!=lastlookup) //Last row not yet loaded?
	{
		register word charloc;
		charloc = character; //Character position!
		charloc <<= 5;
		charloc |= y;
		charloc <<= 1;
		charloc |= attribute;
		lastrow = VGA->getcharxy_values[charloc]; //Lookup the new row!
		lastcharinfo = lastlookup; //Save the loaded row as the current row!
	}
	
	return ((lastrow>>shift[newx])&1); //Give bit!
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