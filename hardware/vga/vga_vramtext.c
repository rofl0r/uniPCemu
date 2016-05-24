#include "headers/hardware/vga/vga.h" //Basic VGA stuff!
#include "headers/hardware/vga/vga_vram.h" //VRAM!
#include "headers/hardware/vga/vga_crtcontroller.h" //For character sizes!
#include "headers/hardware/vga/vga_vramtext.h" //Our VRAM text support!
#include "headers/support/log.h" //Logging support!
#include "headers/support/highrestimer.h" //High resolution timing!
#include "headers/support/bmp.h" //BMP dumping support!

OPTINLINE byte reverse8_VGA(byte b) { //Reverses byte value bits!
	INLINEREGISTER byte temp=b, temp2=b; //Load our initial values!
	temp = ((temp & 0xF0) >> 4);
	temp2 = ((temp2 & 0x0F) << 4); //Swap 4 high and low bits!
	temp |= temp2; //Combine into 1!
	temp2 = temp; //Make both equal!
	temp = ((temp & 0xCC) >> 2);
	temp2 = ((temp2 & 0x33) << 2); //Swap 2 high and low bits of both nibbles!
	temp |= temp2; //Combine into 1!
	temp2 = temp; //Make both equal!
	temp = ((temp & 0xAA) >> 1);
	temp2 = ((temp2 & 0x55) << 1); //Swap odd and even bits!
	temp |= temp2; //Combine into 1!
	return temp; //Give the result!
}

OPTINLINE void fillgetcharxy_values(VGA_Type *VGA, int_32 address)
{
	byte *getcharxy_values;
	getcharxy_values = &VGA->getcharxy_values[0]; //The values!
	word character = 0; //From 0-255!
	sbyte singlerow = -1; //Single row only?
	byte y=0; //From 0-32 (5 bits)!
	if (address!=-1) //Single character row only?
	{
		character = (word)((address >> 5) & 0xFF); //Only single character to edit!
		singlerow = (sbyte)(address&0x1F); //The single row to edit!
		y = singlerow; //Only process this row!
	}
	for (;character<0x100;) //256 characters (8 bits)!
	{
		byte attribute = 0; //0 or 1 (bit value 0x4 of the attribute, 1 bit)!
		for (;attribute<2;) //2 attributes!
		{
			if (singlerow==-1) y = 0; //Ignore the selected row if single isn't set!
			for (;y<0x20;) //33 rows!
			{
				uint_32 characterset_offset, add2000; //First, the character set, later translated to the real charset offset!
				if (VGA->registers->SequencerRegisters.REGISTERS.SEQUENCERMEMORYMODEREGISTER.ExtendedMemory) //Memory maps are enabled?
				{
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
				}
				else //Force character set #0?
				{
					characterset_offset = 0; //We're at the start of VRAM plane 2 always!
				}

				word character2;
				character2 = character; //Load!
				character2 <<= 5; //Multiply by 32!
				characterset_offset += character2; //Start of the character!
				characterset_offset += y; //Add the row!
				
				getcharxy_values[(character<<6)|(y<<1)|attribute] = reverse8_VGA(readVRAMplane(VGA,2,characterset_offset)); //Read the row from the character generator! Don't do anything special, just because we're from the renderer! Also reverse the data in the byte for a little speedup! Store the row for the character generator!
				if (singlerow!=-1) goto nextattr; //Don't change the row if a single line is updated!
				++y; //Next row!
			}
			nextattr:
			++attribute; //Next attribute!
		}
		++character; //Next character!
		if (singlerow!=-1) return; //Stop on single character update!
	}
}

uint_32 textdisplay[32 * 8 * 256 * 2]; //All possible output!
void dumpVGATextFonts()
{
	uint_32 displayindex;
	byte *getcharxy_values;

	byte currentattribute;
	byte currentcharacter;
	byte currentrow;
	byte currentpixel;

	getcharxy_values = &getActiveVGA()->getcharxy_values[0]; //The values!
	for (displayindex=0;displayindex<NUMITEMS(textdisplay);displayindex++)
	{
		currentpixel = (displayindex&7); //Every pixel we change the font/back pixel!
		currentcharacter = ((displayindex>>3)&0xFF); //The character changes every 8 pixels!
		currentrow = ((displayindex>>11)&0x1F); //The row changes every 256 characters!
		currentattribute = ((displayindex>>16)&1); //The attribute changes every 32 rows!
		if (getActiveVGA())
		{
			if (getActiveVGA()->registers->specialCGAflags&1) //CGA font is used instead?
			{
				textdisplay[displayindex] = getcharxy_CGA(currentcharacter,currentpixel,currentrow&7)?RGB(0xFF,0xFF,0xFF):RGB(0x00,0x00,0x00);
			}
			else //VGA mode?
			{
				textdisplay[displayindex] = ((getcharxy_values[(currentcharacter<<6)|(currentrow<<1)|currentattribute]>>currentpixel)&1)?RGB(0xFF,0xFF,0xFF):RGB(0x00,0x00,0x00);
			}
		}
		else
		{
			textdisplay[displayindex] = ((getcharxy_values[(currentcharacter<<6)|(currentrow<<1)|currentattribute]>>currentpixel)&1)?RGB(0xFF,0xFF,0xFF):RGB(0x00,0x00,0x00);
		}
	}
	domkdir("captures"); //Make sure we can log!
	writeBMP("captures/VRAMText",&textdisplay[0],256*8,32*2,0,0,256*8); //Dump our font to the BMP file! We're two characters high (one for every font table) and 256 characters wide(total characters in the font).
}

void VGA_plane2updated(VGA_Type *VGA, uint_32 address) //Plane 2 has been updated?
{
	fillgetcharxy_values(VGA,address); //Update the character: character number is increased every 32 locations (5 bits row index), but we include the character set too(bits 13-15), so ignore that for correct character and character set handling!
}

void VGA_charsetupdated(VGA_Type *VGA)
{
	fillgetcharxy_values(VGA,-1); //Update all characters: the character sets are updated!	
}

byte getcharxy(VGA_Type *VGA, byte attribute, byte character, byte x, byte y) //Retrieve a characters x,y pixel on/off from table!
{
	static byte lastrow; //Last retrieved character row data!
	static word lastcharinfo = 0; //attribute|character|row|1, bit0=Set?
	INLINEREGISTER word lastlookup;
	INLINEREGISTER word charloc;
	INLINEREGISTER byte newx;
	newx = x; //Default: use the 9th bit if needed! Otherwise use the horizontal coordinate within the character!

	attribute >>= 3; //...
	attribute &= 1; //... Take bit 3 to get the actual attribute we need!
	if (newx>7) //Extra ninth bit?
	{
		newx = 7; //Only 7 max!
		if (getcharacterwidth(VGA)!=8) //What width? 9 wide?
		{
			if (VGA->registers->AttributeControllerRegisters.REGISTERS.ATTRIBUTEMODECONTROLREGISTER.LineGraphicsEnable || ((character & 0xE0) != 0xC0)) return 0; //9th bit is always background?
		}
	}
	
	lastlookup = (((((character << 1) | attribute) << 5) | y) | 0x8000); //The last lookup!
	if (lastcharinfo!=lastlookup) //Row not yet loaded?
	{
		charloc = character; //Character position!
		charloc <<= 5;
		charloc |= y;
		charloc <<= 1;
		charloc |= attribute;
		lastrow = VGA->getcharxy_values[charloc]; //Lookup the new row!
		lastcharinfo = lastlookup; //Save the loaded row as the current row!
	}
	
	return (lastrow>>newx)&1; //Give bit!
}

OPTINLINE void VGA_dumpchar(VGA_Type *VGA, byte c)
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
