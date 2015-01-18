#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //Real ouput module!
//#include "headers/cpu/cpu.h" //CPU module!
//#include "headers/cpu/easyregs.h" //Easy register access!
//#include "headers/cpu/cpu_OP8086.h" //8086 comp.
//#include "headers/hardware/vga_rest/colorconversion.h" //Color conversion compatibility for text output!
#include "headers/cpu/interrupts.h" //Interrupt support for GRAPHIC modes!
#include "headers/hardware/vga.h" //Basic typedefs!
//#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Attribute controller for debug screen output!
#include "headers/header_dosbox.h" //Screen modes from DOSBox!
#include "headers/hardware/ports.h" //Port support!
#include "headers/mmu/mmu.h" //MMU access!
#include "headers/support/log.h" //Logging support for debugging!

#include "headers/interrupts/interrupt10.h" //Our typedefs!
#include "headers/cpu/callback.h" //Callback support!
#include "headers/cpu/80286/protection.h" //Protection support!

//Log options!
#define LOG_SWITCHMODE 0
#define LOG_FILE "int10"

//Are we disabled?
#define __HW_DISABLED 0

//Helper functions:

extern VideoModeBlock ModeList_VGA[63]; //VGA Modelist!
VideoModeBlock *CurMode = &ModeList_VGA[0]; //Current video mode information block!

//Patches for dosbox!

//Now for real:

uint_32 machine = M_VGA; //Active machine!
//EGA/VGA?

void INT10_SetSingleDACRegister(Bit8u index,Bit8u red,Bit8u green,Bit8u blue) {
	IO_Write(VGAREG_DAC_WRITE_ADDRESS,(Bit8u)index);
	if ((real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL)&0x06)==0) {
		IO_Write(VGAREG_DAC_DATA,red);
		IO_Write(VGAREG_DAC_DATA,green);
		IO_Write(VGAREG_DAC_DATA,blue);
	} else {
		/* calculate clamped intensity, taken from VGABIOS */
		Bit32u i=(( 77*red + 151*green + 28*blue ) + 0x80) >> 8;
		Bit8u ic=(i>0x3f) ? 0x3f : ((Bit8u)(i & 0xff));
		IO_Write(VGAREG_DAC_DATA,ic);
		IO_Write(VGAREG_DAC_DATA,ic);
		IO_Write(VGAREG_DAC_DATA,ic);
	}
}

void VGA_DAC_SetEntry(byte index, byte r, byte g, byte b)
{
	INT10_SetSingleDACRegister(index,r,g,b);
}

void INT10_PerformGrayScaleSumming(Bit16u start_reg,Bit16u count) { //Creates a grayscale palette!
    Bitu ct;
	Bit8u red, green, blue, ic;
	Bit32u i;
	if (count>0x100) count=0x100;
	for (ct=0; ct<count; ct++) {
		IO_Write(VGAREG_DAC_READ_ADDRESS,start_reg+ct);
		red=IO_Read(VGAREG_DAC_DATA);
		green=IO_Read(VGAREG_DAC_DATA);
		blue=IO_Read(VGAREG_DAC_DATA);

		/* calculate clamped intensity, taken from VGABIOS */
		i=(( 77*red + 151*green + 28*blue ) + 0x80) >> 8;
		ic=(i>0x3f) ? 0x3f : ((Bit8u)(i & 0xff));
		INT10_SetSingleDACRegister(start_reg+ct,ic,ic,ic);
	}
}

int setCurMode(uint_32 type, word mode)
{
	return (mode<=0x13); //Accepted?
}

//All palettes:
extern byte text_palette[64][3];
extern byte mtext_palette[64][3];
extern byte mtext_s3_palette[64][3];
extern byte ega_palette[64][3];
extern byte cga_palette[16][3];
extern byte cga_palette_2[64][3];
extern byte vga_palette[256][3];

extern VideoModeBlock ModeList_VGA_Text_200lines[4];
extern VideoModeBlock ModeList_VGA_Text_350lines[4];
extern VideoModeBlock ModeList_VGA_Tseng[34];
extern VideoModeBlock ModeList_VGA_Paradise[24];

extern VideoModeBlock ModeList_EGA[13];
extern VideoModeBlock ModeList_OTHER[11];
extern VideoModeBlock Hercules_Mode;



//Now the function itself (a big one)!
/* Setup the BIOS */

static bool SetCurMode(VideoModeBlock modeblock[],word mode)
{
	byte i=0;
	while (modeblock[i].mode!=0xffff)
	{
		if (modeblock[i].mode!=mode) i++;
		else
		{
			if ((ModeList_VGA[i].mode<0x120))
			{
				CurMode=&modeblock[i];
				return true;
			}
			return false;
		}
	}
	return false;
}

void FinishSetMode(int clearmem)
{
	VGA_Type *currentVGA;
	byte ct;
	uint_32 ct2;
	word seg;
	word CRTCAddr;
	/* Clear video memory if needs be */
	if (clearmem)
	{
		switch (CurMode->type)
		{
		case M_CGA4:
		case M_CGA2:
		case M_TANDY16:
			for (ct2=0;ct2<16*1024;ct2++) {
				memreal_writew( 0xb800,ct2<<1,0x0000);
			}
			break;
		case M_TEXT: //{
			seg = (CurMode->mode==7)?0xb000:0xb800;
			for (ct2=0;ct2<16*1024;ct2++) memreal_writew(seg,ct2<<1,0x0720);
			//}
			break;
		case M_EGA:
		case M_VGA:
		case M_LIN8:
		case M_LIN4:
		case M_LIN15:
		case M_LIN16:
		case M_LIN32:
			/* Hack we just acess the memory directly */
			if ((currentVGA = getActiveVGA())) //Gotten active VGA?
			{
				memset(currentVGA->VRAM,0,currentVGA->VRAM_size);
			}
			break;
		default: //Unknown?
			break;
		}
	}


	if (CurMode->mode<128) MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,(byte)CurMode->mode);
	else MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,(byte)(CurMode->mode-0x98)); //Looks like the s3 bios
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,CurMode->twidth&0xFF); //Low!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS+1,((CurMode->twidth&0xFF00)>>8)); //High!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,(CurMode->plength&0xFF)); //Low!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_PAGE_SIZE+1,((CurMode->plength&0xFF00)>>8)); //High!
	CRTCAddr = ((CurMode->mode==7 )|| (CurMode->mode==0x0f)) ? 0x3b4 : 0x3d4; //Address!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS,(CRTCAddr&0xFF)); //Low!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS+1,((CRTCAddr&0xFF00)>>8)); //High!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_ROWS,(byte)(CurMode->theight-1)); //Height!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,(CurMode->cheight&0xFF)); //Low!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT+1,((CurMode->cheight&0xFF00)>>8)); //High!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,(0x60|(((MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,0)&0x80) && EMU_VGA)?0:0x80)));
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_SWITCHES,0x09);


	// this is an index into the dcc table:
	if (IS_VGA_ARCH) MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_DCC_INDEX,0x0b);
	//MMU_wd(BIOSMEM_SEG,BIOSMEM_VS_POINTER,int10.rom.video_save_pointers); //Unknown/unimplemented yet!

	// Set cursor shape
	if (CurMode->type==M_TEXT)
	{
		EMU_CPU_setCursorScanlines(CurMode->theight-2,CurMode->theight-1);
	}

	// Set cursor pos for page 0..7
	for (ct=0; ct<8; ct++) cursorXY(ct,0,0);
	// Set active page 0
	emu_setactivedisplaypage(0);
}

bool INT10_SetVideoMode_OTHER(word mode,bool clearmem)
{
	byte ct;
	byte scanline,crtpage;
	word crtc_base;
	byte mode_control_list[0xa+1]=
	{
		0x2c,0x28,0x2d,0x29,	//0-3
		0x2a,0x2e,0x1e,0x29,	//4-7
		0x2a,0x2b,0x3b			//8-a
	};
	byte mode_control_list_pcjr[0xa+1]=
	{
		0x0c,0x08,0x0d,0x09,	//0-3
		0x0a,0x0e,0x0e,0x09,	//4-7
		0x1a,0x1b,0x0b			//8-a
	};
	byte mode_control,color_select;
	switch (machine)
	{
	case MCH_CGA:
		if (mode>6) return false;
		/*case TANDY_ARCH_CASE:
			if (mode>0xa) return false;
			if (mode==7) mode=0; // PCJR defaults to 0 on illegal mode 7
			if (!SetCurMode(ModeList_OTHER,mode)) {
				//LOG(LOG_INT10,LOG_ERROR)("Trying to set illegal mode %X",mode);
				return false;
			}
			break;*/
	case MCH_HERC:
		// Only init the adapter if the equipment word is set to monochrome (Testdrive)
		if ((real_readw(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE)&0x30)!=0x30) return false;
		CurMode=&Hercules_Mode;
		mode=7; // in case the video parameter table is modified
		break;
	}
	//LOG(LOG_INT10,LOG_NORMAL)("Set Video Mode %X",mode);

	/* Setup the VGA to the correct mode */
//	VGA_SetMode(CurMode->type);
	/* Setup the CRTC */
	crtc_base=(machine==MCH_HERC) ? 0x3b4 : 0x3d4;
	//Horizontal total
	IO_WriteW(crtc_base,0x00 | (CurMode->htotal) << 8);
	//Horizontal displayed
	IO_WriteW(crtc_base,0x01 | (CurMode->hdispend) << 8);
	//Horizontal sync position
	IO_WriteW(crtc_base,0x02 | (CurMode->hdispend+1) << 8);
	//Horizontal sync width, seems to be fixed to 0xa, for cga at least, hercules has 0xf
	IO_WriteW(crtc_base,0x03 | (0xa) << 8);
	////Vertical total
	IO_WriteW(crtc_base,0x04 | (CurMode->vtotal) << 8);
	//Vertical total adjust, 6 for cga,hercules,tandy
	IO_WriteW(crtc_base,0x05 | (6) << 8);
	//Vertical displayed
	IO_WriteW(crtc_base,0x06 | (CurMode->vdispend) << 8);
	//Vertical sync position
	IO_WriteW(crtc_base,0x07 | (CurMode->vdispend + ((CurMode->vtotal - CurMode->vdispend)/2)-1) << 8);
	//Maximum scanline
	scanline=8;
	switch(CurMode->type)
	{
	case M_TEXT:
		if (machine==MCH_HERC) scanline=14;
		else scanline=8;
		break;
	case M_CGA2:
		scanline=2;
		break;
	case M_CGA4:
		if (CurMode->mode!=0xa) scanline=2;
		else scanline=4;
		break;
	case M_TANDY16:
		if (CurMode->mode!=0x9) scanline=2;
		else scanline=4;
		break;
	default: //Default!
		break;
	}
	IO_WriteW(crtc_base,0x09 | (scanline-1) << 8);
	//Setup the CGA palette using VGA DAC palette
	for (ct=0; ct<16; ct++) VGA_DAC_SetEntry(ct,cga_palette[ct][0],cga_palette[ct][1],cga_palette[ct][2]);
	//Setup the tandy palette
	//for (ct=0;ct<16;ct++) VGA_DAC_CombineColor(ct,ct);
	//Setup the special registers for each machine type

	switch (machine)
	{
	case MCH_HERC:
		IO_WriteB(0x3b8,0x28);	// TEXT mode and blinking characters

		//Herc_Palette();
		//VGA_DAC_CombineColor(0,0);
		//VGA_DAC_CombineColor(1,7);

		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x29); // attribute controls blinking
		break;
	case MCH_CGA:
		mode_control=mode_control_list[CurMode->mode];
		if (CurMode->mode == 0x6) color_select=0x3f;
		else color_select=0x30;
		IO_WriteB(0x3d8,mode_control);
		IO_WriteB(0x3d9,color_select);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,mode_control);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,color_select);
		break;
	case MCH_TANDY:
		/* Init some registers */
		IO_WriteB(0x3da,0x1);
		IO_WriteB(0x3de,0xf);		//Palette mask always 0xf
		IO_WriteB(0x3da,0x2);
		IO_WriteB(0x3de,0x0);		//black border
		IO_WriteB(0x3da,0x3);							//Tandy color overrides?
		switch (CurMode->mode)
		{
		case 0x8:
			IO_WriteB(0x3de,0x14);
			break;
		case 0x9:
			IO_WriteB(0x3de,0x14);
			break;
		case 0xa:
			IO_WriteB(0x3de,0x0c);
			break;
		default:
			IO_WriteB(0x3de,0x0);
			break;
		}
		//Clear extended mapping
		IO_WriteB(0x3da,0x5);
		IO_WriteB(0x3de,0x0);
		//Clear monitor mode
		IO_WriteB(0x3da,0x8);
		IO_WriteB(0x3de,0x0);
		crtpage=(CurMode->mode>=0x9) ? 0xf6 : 0x3f;
		IO_WriteB(0x3df,crtpage);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CRTCPU_PAGE,crtpage);
		mode_control=mode_control_list[CurMode->mode];
		if (CurMode->mode == 0x6 || CurMode->mode==0xa) color_select=0x3f;
		else color_select=0x30;
		IO_WriteB(0x3d8,mode_control);
		IO_WriteB(0x3d9,color_select);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,mode_control);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,color_select);
		break;
	case MCH_PCJR:
		/* Init some registers */
		IO_ReadB(0x3da);
		IO_WriteB(0x3da,0x1);
		IO_WriteB(0x3da,0xf);		//Palette mask always 0xf
		IO_WriteB(0x3da,0x2);
		IO_WriteB(0x3da,0x0);		//black border
		IO_WriteB(0x3da,0x3);
		if (CurMode->mode<=0x04) IO_WriteB(0x3da,0x02);
		else if (CurMode->mode==0x06) IO_WriteB(0x3da,0x08);
		else IO_WriteB(0x3da,0x00);

		/* set CRT/Processor page register */
		if (CurMode->mode<0x04) crtpage=0x3f;
		else if (CurMode->mode>=0x09) crtpage=0xf6;
		else crtpage=0x7f;
		IO_WriteB(0x3df,crtpage);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CRTCPU_PAGE,crtpage);

		mode_control=mode_control_list_pcjr[CurMode->mode];
		IO_WriteB(0x3da,0x0);
		IO_WriteB(0x3da,mode_control);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,mode_control);

		if (CurMode->mode == 0x6 || CurMode->mode==0xa) color_select=0x3f;
		else color_select=0x30;
		IO_WriteB(0x3d9,color_select);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,color_select);
		break;
	}

	/*RealPt vparams = RealGetVec(0x1d);
	if ((vparams != RealMake(0xf000,0xf0a4)) && (mode < 8)) {
		// load crtc parameters from video params table
		word crtc_block_index = 0;
		if (mode < 2) crtc_block_index = 0;
		else if (mode < 4) crtc_block_index = 1;
		else if (mode < 7) crtc_block_index = 2;
		else if (mode == 7) crtc_block_index = 3; // MDA mono mode; invalid for others
		else if (mode < 9) crtc_block_index = 2;
		else crtc_block_index = 3; // Tandy/PCjr modes

		// init CRTC registers
		for (word i = 0; i < 16; i++)
			IO_WriteW(crtc_base, i | (real_readb(RealSeg(vparams),
				RealOff(vparams) + i + crtc_block_index*16) << 8));
	}*/
	FinishSetMode(clearmem);
	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: Set_OTHER: RET!");
	return true;
}

int INT10_Internal_SetVideoMode(word mode)
{
	byte ct;
	bool clearmem=true;
	uint_32 i;
	byte modeset_ctl;
	word crtc_base;
	bool mono_mode=(mode == 7) || (mode==0xf);
	byte misc_output;
	byte seq_data[SEQ_REGS];
	byte overflow=0;
	byte max_scanline=0;
	byte ver_overflow=0;
	byte hor_overflow=0;
	byte ret_start;
	byte ret_end;
	word vretrace;
	byte vblank_trim;
	byte underline=0;
	byte offset;
	byte mode_control=0;
	byte gfx_data[GFX_REGS];
	byte att_data[ATT_REGS];
	byte feature;

	if (__HW_DISABLED) return true; //Abort!
	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode (%04X)...",mode);

	if (mode>=0x100)
	{
		//if ((mode & 0x4000) && int10.vesa_nolfb) return false;
		if (mode & 0x8000) clearmem=false;
		mode&=0xfff;
	}
	if ((mode<0x100) && (mode & 0x80))
	{
		clearmem=false;
		mode-=0x80;
	}

	//raiseError("Debug_INT10_SetVideoMode","Checkpoint 1");

	//int10.vesa_setmode=0xffff;

	//LOG(LOG_INT10,LOG_NORMAL)("Set Video Mode %X",mode);
	if (!IS_EGAVGA_ARCH) return INT10_SetVideoMode_OTHER(mode,clearmem);

	//raiseError("Debug_INT10_SetVideoMode","Checkpoint 1A");

	/* First read mode setup settings from bios area */
//	byte video_ctl=real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL);
//	byte vga_switches=real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES);
	modeset_ctl=real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL);

	//raiseError("Debug_INT10_SetVideoMode","Checkpoint 1B");

	if (IS_VGA_ARCH)
	{
		/*if (svga.accepts_mode) {
			if (!svga.accepts_mode(mode)) return false;
		}*/

		/*switch(svgaCard) {
		case SVGA_TsengET4K:
		case SVGA_TsengET3K:
			if (!SetCurMode(ModeList_VGA_Tseng,mode)){
				//LOG(LOG_INT10,LOG_ERROR)("VGA:Trying to set illegal mode %X",mode);
				return false;
			}
			break;
		case SVGA_ParadisePVGA1A:
			if (!SetCurMode(ModeList_VGA_Paradise,mode)){
				//LOG(LOG_INT10,LOG_ERROR)("VGA:Trying to set illegal mode %X",mode);
				return false;
			}
			break;
		default:*/
		if (!SetCurMode(ModeList_VGA,mode))
		{
			//LOG(LOG_INT10,LOG_ERROR)("VGA:Trying to set illegal mode %X",mode);
			return false;
		}
		/*}*/
		// check for scanline backwards compatibility (VESA text modes??)
		if (CurMode->type==M_TEXT)
		{
			//raiseError("Debug_INT10_SetVideoMode_TEXT200","Checkpoint 1A");
			if ((modeset_ctl&0x90)==0x80)   // 200 lines emulation
			{
				if (CurMode->mode <= 3)
				{
					CurMode = &ModeList_VGA_Text_200lines[CurMode->mode];
				}
			}
			else if ((modeset_ctl&0x90)==0x00)     // 350 lines emulation
			{
				if (CurMode->mode <= 3)
				{
					CurMode = &ModeList_VGA_Text_350lines[CurMode->mode];
				}
			}
		}
	}
	else
	{
		//raiseError("Debug_INT10_SetVideoMode_TEXTEGA","Checkpoint 1G");
		if (!SetCurMode(ModeList_EGA,mode))
		{
			//LOG(LOG_INT10,LOG_ERROR)("EGA:Trying to set illegal mode %X",mode);
			return false;
		}
	}

	//raiseError("Debug_INT10_SetVideoMode","Checkpoint 2");

	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - Setup the CRTC...");

	/* Setup the VGA to the correct mode */

	if (mono_mode) crtc_base=0x3b4;
	else crtc_base=0x3d4;

	/*if (IS_VGA_ARCH && (svgaCard == SVGA_S3Trio)) {
		// Disable MMIO here so we can read / write memory
		IO_Write(crtc_base,0x53);
		IO_Write(crtc_base+1,0x0);
	}*/

	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - Setup the Misc output register...");
	/* Setup MISC Output Register */
	misc_output=0x2 | (mono_mode ? 0x0 : 0x1);

	if ((CurMode->type==M_TEXT) && (CurMode->cwidth==9))
	{
		// 28MHz (16MHz EGA) clock for 9-pixel wide chars
		misc_output|=0x4;
	}

	switch (CurMode->vdispend)
	{
	case 400:
		misc_output|=0x60;
		break;
	case 480:
		misc_output|=0xe0;
		break;
	case 350:
		misc_output|=0xa0;
		break;
	default:
		misc_output|=0x60;
	}
	
	misc_output &= 0xDF; //Added by superfury: odd/even mode always uses plane 0/1!

	IO_Write(0x3c2,misc_output);		//Setup for 3b4 or 3d4

	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - Setup the Sequencer...");
	/* Program Sequencer */
	memset(seq_data,0,SEQ_REGS);
	seq_data[1]|=0x01;	//8 dot fonts by default
	if (CurMode->special & _EGA_HALF_CLOCK) seq_data[1]|=0x08; //Check for half clock
	if ((machine==MCH_EGA) && (CurMode->special & _EGA_HALF_CLOCK)) seq_data[1]|=0x02;
	seq_data[4]|=0x02;	//More than 64kb
	switch (CurMode->type)
	{
	case M_TEXT:
		if (CurMode->cwidth==9) seq_data[1] &= ~1;
		seq_data[2]|=0x3;				//Enable plane 0 and 1
		seq_data[4]|=0x01;				//Alpanumeric
		if (IS_VGA_ARCH) seq_data[4]|=0x04;				//odd/even enabled
		break;
	case M_CGA2:
		seq_data[2]|=0xf;				//Enable plane 0
		if (machine==MCH_EGA) seq_data[4]|=0x04;		//odd/even enabled
		break;
	case M_CGA4:
		seq_data[2]|=0x03;		//Enable plane 0 and 1
		break;
	case M_LIN4:
	case M_EGA:
		seq_data[2]|=0xf;				//Enable all planes for writing
		//if (machine==MCH_EGA) seq_data[4]|=0x04;		//odd/even enabled
		break;
	case M_LIN8:						//Seems to have the same reg layout from testing
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
	case M_VGA:
		seq_data[2]|=0xf;				//Enable all planes for writing
		seq_data[4]|=0xc;				//Graphics - odd/even - Chained
			break;
	default:
		break;
	}
	for (ct=0; ct<SEQ_REGS; ct++)
	{
		IO_Write(0x3c4,ct);
		IO_Write(0x3c5,seq_data[ct]);
	}
	//vga.config.compatible_chain4 = true; // this may be changed by SVGA chipset emulation

	//raiseError("Debug_INT10_SetVideoMode","Checkpoint 3");


	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - Setup the CRTC...");
	/* Program CRTC */
	/* First disable write protection */
	IO_Write(crtc_base,0x11);
	IO_Write(crtc_base+1,IO_Read(crtc_base+1)&0x7f);
	/* Clear all the regs */
	for (ct=0x0; ct<=0x18; ct++)
	{
		IO_Write(crtc_base,ct);
		IO_Write(crtc_base+1,0);
	}
	/* Horizontal Total */
	IO_Write(crtc_base,0x00);
	IO_Write(crtc_base+1,(byte)(CurMode->htotal-5));
	hor_overflow|=((CurMode->htotal-5) & 0x100) >> 8;
	/* Horizontal Display End */
	IO_Write(crtc_base,0x01);
	IO_Write(crtc_base+1,(byte)(CurMode->hdispend-1));
	hor_overflow|=((CurMode->hdispend-1) & 0x100) >> 7;
	/* Start horizontal Blanking */
	IO_Write(crtc_base,0x02);
	IO_Write(crtc_base+1,(byte)CurMode->hdispend);
	hor_overflow|=((CurMode->hdispend) & 0x100) >> 6;
	/* End horizontal Blanking */
	byte blank_end=(CurMode->htotal-2) & 0x7f;
	IO_Write(crtc_base,0x03);
	IO_Write(crtc_base+1,0x80|(blank_end & 0x1f));

	/* Start Horizontal Retrace */
	if ((CurMode->special & _EGA_HALF_CLOCK) && (CurMode->type!=M_CGA2)) ret_start = (CurMode->hdispend+3);
	else if (CurMode->type==M_TEXT) ret_start = (CurMode->hdispend+5);
	else ret_start = (CurMode->hdispend+4);
	IO_Write(crtc_base,0x04);
	IO_Write(crtc_base+1,(byte)ret_start);
	hor_overflow|=(ret_start & 0x100) >> 4;

	/* End Horizontal Retrace */
	if (CurMode->special & _EGA_HALF_CLOCK)
	{
		if (CurMode->type==M_CGA2) ret_end=0;	// mode 6
		else if (CurMode->special & _EGA_LINE_DOUBLE) ret_end = (CurMode->htotal-18) & 0x1f;
		else ret_end = ((CurMode->htotal-18) & 0x1f) | 0x20; // mode 0&1 have 1 char sync delay
	}
	else if (CurMode->type==M_TEXT) ret_end = (CurMode->htotal-3) & 0x1f;
	else ret_end = (CurMode->htotal-4) & 0x1f;

	IO_Write(crtc_base,0x05);
	IO_Write(crtc_base+1,(byte)(ret_end | (blank_end & 0x20) << 2));

	/* Vertical Total */
	IO_Write(crtc_base,0x06);
	IO_Write(crtc_base+1,(byte)(CurMode->vtotal-2));
	overflow|=((CurMode->vtotal-2) & 0x100) >> 8;
	overflow|=((CurMode->vtotal-2) & 0x200) >> 4;
	ver_overflow|=((CurMode->vtotal-2) & 0x400) >> 10;

	if (IS_VGA_ARCH)
	{
		switch (CurMode->vdispend)
		{
		case 400:
			vretrace=CurMode->vdispend+12;
			break;
		case 480:
			vretrace=CurMode->vdispend+10;
			break;
		case 350:
			vretrace=CurMode->vdispend+37;
			break;
		default:
			vretrace=CurMode->vdispend+12;
		}
	}
	else
	{
		switch (CurMode->vdispend)
		{
		case 350:
			vretrace=CurMode->vdispend;
			break;
		default:
			vretrace=CurMode->vdispend+24;
		}
	}

	/* Vertical Retrace Start */
	IO_Write(crtc_base,0x10);
	IO_Write(crtc_base+1,(byte)vretrace);
	overflow|=(vretrace & 0x100) >> 6;
	overflow|=(vretrace & 0x200) >> 2;
	ver_overflow|=(vretrace & 0x400) >> 6;

	/* Vertical Retrace End */
	IO_Write(crtc_base,0x11);
	IO_Write(crtc_base+1,(vretrace+2) & 0xF);

	/* Vertical Display End */
	IO_Write(crtc_base,0x12);
	IO_Write(crtc_base+1,(byte)(CurMode->vdispend-1));
	overflow|=((CurMode->vdispend-1) & 0x100) >> 7;
	overflow|=((CurMode->vdispend-1) & 0x200) >> 3;
	ver_overflow|=((CurMode->vdispend-1) & 0x400) >> 9;

	if (IS_VGA_ARCH)
	{
		switch (CurMode->vdispend)
		{
		case 400:
			vblank_trim=6;
			break;
		case 480:
			vblank_trim=7;
			break;
		case 350:
			vblank_trim=5;
			break;
		default:
			vblank_trim=8;
		}
	}
	else
	{
		switch (CurMode->vdispend)
		{
		case 350:
			vblank_trim=0;
			break;
		default:
			vblank_trim=23;
		}
	}

	/* Vertical Blank Start */
	IO_Write(crtc_base,0x15);
	IO_Write(crtc_base+1,(byte)(CurMode->vdispend+vblank_trim));
	overflow|=((CurMode->vdispend+vblank_trim) & 0x100) >> 5;
	max_scanline|=((CurMode->vdispend+vblank_trim) & 0x200) >> 4;
	ver_overflow|=((CurMode->vdispend+vblank_trim) & 0x400) >> 8;

	/* Vertical Blank End */
	IO_Write(crtc_base,0x16);
	IO_Write(crtc_base+1,(byte)(CurMode->vtotal-vblank_trim-2));

	/* Line Compare */
	word line_compare=(CurMode->vtotal < 1024) ? 1023 : 2047;
	IO_Write(crtc_base,0x18);
	IO_Write(crtc_base+1,line_compare&0xff);
	overflow|=(line_compare & 0x100) >> 4;
	max_scanline|=(line_compare & 0x200) >> 3;
	ver_overflow|=(line_compare & 0x400) >> 4;
	/* Maximum scanline / Underline Location */
	if (CurMode->special & _EGA_LINE_DOUBLE)
	{
		if (machine!=MCH_EGA) max_scanline|=0x80;
	}
	switch (CurMode->type)
	{
	case M_TEXT:
		max_scanline|=CurMode->cheight-1;
		underline=mono_mode ? 0x0f : 0x1f; // mode 7 uses a diff underline position
		break;
	case M_VGA:
		underline=0x40;
		max_scanline|=1;		//Vga doesn't use double line but this
		break;
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		underline=0x60;			//Seems to enable the every 4th clock on my s3
		break;
	case M_CGA2:
	case M_CGA4:
		max_scanline|=1;
		break;
	default:
		break;
	}
	if (CurMode->vdispend==350) underline=0x0f;

	IO_Write(crtc_base,0x09);
	IO_Write(crtc_base+1,max_scanline);
	IO_Write(crtc_base,0x14);
	IO_Write(crtc_base+1,underline);

	/* OverFlow */
	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: Setting overflow register: %02X",overflow);
	IO_Write(crtc_base,0x07);
	IO_Write(crtc_base+1,overflow);

	/*if (svgaCard == SVGA_S3Trio) {
		/ Extended Horizontal Overflow /
		IO_Write(crtc_base,0x5d);IO_Write(crtc_base+1,hor_overflow);
		/ Extended Vertical Overflow /
		IO_Write(crtc_base,0x5e);IO_Write(crtc_base+1,ver_overflow);
	}*/

	/* Offset Register */
	switch (CurMode->type)
	{
	case M_LIN8:
		offset = CurMode->swidth/8;
		break;
	case M_LIN15:
	case M_LIN16:
		offset = 2 * CurMode->swidth/8;
		break;
	case M_LIN32:
		offset = 4 * CurMode->swidth/8;
		break;
	default:
		offset = CurMode->hdispend/2;
	}
	IO_Write(crtc_base,0x13);
	IO_Write(crtc_base + 1,offset & 0xff);

	/*if (svgaCard == SVGA_S3Trio) {
		/ Extended System Control 2 Register  /
		/ This register actually has more bits but only use the extended offset ones /
		IO_Write(crtc_base,0x51);
		IO_Write(crtc_base + 1,(byte)((offset & 0x300) >> 4));
		/ Clear remaining bits of the display start /
		IO_Write(crtc_base,0x69);
		IO_Write(crtc_base + 1,0);
		/ Extended Vertical Overflow /
		IO_Write(crtc_base,0x5e);IO_Write(crtc_base+1,ver_overflow);
	}*/

	/* Mode Control */

	switch (CurMode->type)
	{
	case M_CGA2:
		mode_control=0xc2; // 0x06 sets address wrap.
		break;
	case M_CGA4:
		mode_control=0xa2;
		break;
	case M_LIN4:
	case M_EGA:
		if (CurMode->mode==0x11) // 0x11 also sets address wrap.  thought maybe all 2 color modes did but 0x0f doesn't.
			mode_control=0xc3; // so.. 0x11 or 0x0f a one off?
		else
		{
			if (machine==MCH_EGA)
			{
				if (CurMode->special & _EGA_LINE_DOUBLE) mode_control=0xc3;
				else mode_control=0x8b;
			}
			else
			{
				mode_control=0xe3;
			}
		}
		break;
	case M_TEXT:
	case M_VGA:
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		mode_control=0xa3;
		if (CurMode->special & _VGA_PIXEL_DOUBLE)
			mode_control |= 0x08;
		break;
	default:
		break;
	}

	IO_Write(crtc_base,0x17);
	IO_Write(crtc_base+1,mode_control);
	/* Renable write protection */
	IO_Write(crtc_base,0x11);
	IO_Write(crtc_base+1,IO_Read(crtc_base+1)|0x80);

	/*if (svgaCard == SVGA_S3Trio) {
		/ Setup the correct clock /
		if (CurMode->mode>=0x100) {
			misc_output|=0xef;		//Select clock 3
			byte clock=CurMode->vtotal*8*CurMode->htotal*70;
			VGA_SetClock(3,clock/1000);
		}
		byte misc_control_2;
		/ Setup Pixel format /
		switch (CurMode->type) {
		case M_LIN8:
			misc_control_2=0x00;
			break;
		case M_LIN15:
			misc_control_2=0x30;
			break;
		case M_LIN16:
			misc_control_2=0x50;
			break;
		case M_LIN32:
			misc_control_2=0xd0;
			break;
		default:
			misc_control_2=0x0;
			break;
		}
		IO_WriteB(crtc_base,0x67);IO_WriteB(crtc_base+1,misc_control_2);
	}

	/ Write Misc Output /
	IO_Write(0x3c2,misc_output);
	*/


	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - Setup Graphics controller...");

	/* Program Graphics controller */
	memset(gfx_data,0,GFX_REGS);
	gfx_data[0x7]=0xf;				/* Color don't care */
	gfx_data[0x8]=0xff;				/* BitMask */
	switch (CurMode->type)
	{
	case M_TEXT:
		gfx_data[0x5]|=0x10;		//Odd-Even Mode
		gfx_data[0x6]|=mono_mode ? 0x0a : 0x0e;		//Either b800 or b000
		break;
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
	case M_VGA:
		gfx_data[0x5]|=0x40;		//256 color mode
		gfx_data[0x6]|=0x05;		//graphics mode at 0xa000-affff
		break;
	case M_LIN4:
	case M_EGA:
		gfx_data[0x6]|=0x05;		//graphics mode at 0xa000-affff
		break;
	case M_CGA4:
		gfx_data[0x5]|=0x20;		//CGA mode
		gfx_data[0x6]|=0x0f;		//graphics mode at at 0xb800=0xbfff
		if (machine==MCH_EGA) gfx_data[0x5]|=0x10;
		break;
	case M_CGA2:
		if (machine==MCH_EGA)
		{
			gfx_data[0x6]|=0x0d;		//graphics mode at at 0xb800=0xbfff
		}
		else
		{
			gfx_data[0x6]|=0x0f;		//graphics mode at at 0xb800=0xbfff
		}
		break;
	default:
		break;
	}
	for (ct=0; ct<GFX_REGS; ct++)
	{
		IO_Write(0x3ce,ct);
		IO_Write(0x3cf,gfx_data[ct]);
	}
	memset(att_data,0,ATT_REGS);
	att_data[0x12]=0xf;				//Always have all color planes enabled

	//raiseError("Debug_INT10_SetVideoMode","Checkpoint 4");


	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - Setup Attribute controller...");
	/* Program Attribute Controller */
	switch (CurMode->type)
	{
	case M_EGA:
	case M_LIN4:
		att_data[0x10]=0x01;		//Color Graphics
		switch (CurMode->mode)
		{
		case 0x0f:
			att_data[0x10]|=0x0a;	//Monochrome
			att_data[0x01]=0x08;
			att_data[0x04]=0x18;
			att_data[0x05]=0x18;
			att_data[0x09]=0x08;
			att_data[0x0d]=0x18;
			break;
		case 0x11:
			for (i=1; i<16; i++) att_data[i]=0x3f;
			break;
		case 0x10:
		case 0x12:
			goto att_text16;
		default:
			if ( CurMode->type == M_LIN4 )
				goto att_text16;
			for (ct=0; ct<8; ct++)
			{
				att_data[ct]=ct;
				att_data[ct+8]=ct+0x10;
			}
			break;
		}
		break;
	case M_TANDY16:
		att_data[0x10]=0x01;		//Color Graphics
		for (ct=0; ct<16; ct++) att_data[ct]=ct;
		break;
	case M_TEXT:
		if (CurMode->cwidth==9)
		{
			att_data[0x13]=0x08;	//Pel panning on 8, although we don't have 9 dot text mode
			att_data[0x10]=0x0C;	//Color Text with blinking, 9 Bit characters
		}
		else
		{
			att_data[0x13]=0x00;
			att_data[0x10]=0x08;	//Color Text with blinking, 8 Bit characters
		}
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,0x30);
att_text16:
		if (CurMode->mode==7)   //Monochrome mode?
		{
			att_data[0]=0x00; //Black!
			att_data[8]=0x10; //Green!
			for (i=1; i<8; i++)   //Everything in between?
			{
				att_data[i]=0x08;
				att_data[i+8]=0x18;
			}
		}
		else
		{
			for (ct=0; ct<8; ct++) //Used to be up to 8!
			{
				att_data[ct]=ct; //Color all, dark!
				att_data[ct+8]=ct+0x38; //Color all, lighter!
			}
			if (IS_VGA_ARCH) att_data[0x06]=0x14; //Odd Color 6 yellow/brown.
		}
		break;
	case M_CGA2:
		att_data[0x10]=0x01;		//Color Graphics
		att_data[0]=0x0;
		for (i=1; i<0x10; i++) att_data[i]=0x17;
		att_data[0x12]=0x1;			//Only enable 1 plane
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,0x3f);
		break;
	case M_CGA4:
		att_data[0x10]=0x01;		//Color Graphics
		att_data[0]=0x0;
		att_data[1]=0x13;
		att_data[2]=0x15;
		att_data[3]=0x17;
		att_data[4]=0x02;
		att_data[5]=0x04;
		att_data[6]=0x06;
		att_data[7]=0x07;
		for (ct=0x8; ct<0x10; ct++)
			att_data[ct] = ct + 0x8;
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,0x30);
		break;
	case M_VGA:
	case M_LIN8:
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		for (ct=0; ct<16; ct++) att_data[ct]=ct;
		att_data[0x10]=0x41; //Color Graphics 8-bit
		break;
	default:
		break;
	}
	IO_Read(mono_mode ? 0x3ba : 0x3da);
	if ((modeset_ctl & 8)==0)
	{
		for (ct=0; ct<ATT_REGS; ct++)
		{
			IO_Write(0x3c0,ct);
			IO_Write(0x3c0,att_data[ct]);
		}

		if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - Setup DAC...");

		//vga.config.pel_panning = 0;
		IO_Write(0x3c0,0x20);
		IO_Write(0x3c0,0x00); //Enable palette access for HW!
		//dolog("VGA_DAC","Writing DAC Mask 0xFF...");
		IO_Write(0x3c6,0xff); //Reset Pelmask
		/* Setup the DAC */
		IO_Write(0x3c8,0);
		switch (CurMode->type)
		{
		case M_EGA:
			if (CurMode->mode>0xf)
			{
				goto dac_text16;
			}
			else if (CurMode->mode==0xf)
			{
				for (i=0; i<64; i++)
				{
					IO_Write(0x3c9,mtext_s3_palette[i][0]);
					IO_Write(0x3c9,mtext_s3_palette[i][1]);
					IO_Write(0x3c9,mtext_s3_palette[i][2]);
				}
			}
			else
			{
				for (i=0; i<64; i++)
				{
					IO_Write(0x3c9,ega_palette[i][0]);
					IO_Write(0x3c9,ega_palette[i][1]);
					IO_Write(0x3c9,ega_palette[i][2]);
				}
			}
			break;
		case M_CGA2:
		case M_CGA4:
		case M_TANDY16:
			for (i=0; i<64; i++)
			{
				IO_Write(0x3c9,cga_palette_2[i][0]);
				IO_Write(0x3c9,cga_palette_2[i][1]);
				IO_Write(0x3c9,cga_palette_2[i][2]);
			}
			break;
		case M_TEXT:
			if (CurMode->mode==7)
			{
				/*if ((IS_VGA_ARCH) && (svgaCard == SVGA_S3Trio)) {
					for (i=0;i<64;i++) {
						IO_Write(0x3c9,mtext_s3_palette[i][0]);
						IO_Write(0x3c9,mtext_s3_palette[i][1]);
						IO_Write(0x3c9,mtext_s3_palette[i][2]);
					}
				} else */
				{
					for (i=0; i<64; i++)
					{
						IO_Write(0x3c9,mtext_palette[i][0]);
						IO_Write(0x3c9,mtext_palette[i][1]);
						IO_Write(0x3c9,mtext_palette[i][2]);
					}
				}
				break;
			} //FALLTHROUGH!!!!
		case M_LIN4: //Added for CAD Software
dac_text16:
			for (i=0; i<64; i++)
			{
				IO_Write(0x3c9,text_palette[i][0]);
				IO_Write(0x3c9,text_palette[i][1]);
				IO_Write(0x3c9,text_palette[i][2]);
			}
			break;
		case M_VGA:
		case M_LIN8:
		case M_LIN15:
		case M_LIN16:
		case M_LIN32:
			if (LOG_SWITCHMODE) dolog(LOG_FILE,"Setting full VGA palette...");
			for (i=0; i<256; i++)
			{
				if (LOG_SWITCHMODE) dolog(LOG_FILE,"Setting palette index %i...",i);
				IO_Write(0x3c9,vga_palette[i][0]);
				IO_Write(0x3c9,vga_palette[i][1]);
				IO_Write(0x3c9,vga_palette[i][2]);
			}
			if (LOG_SWITCHMODE) dolog(LOG_FILE,"Full VGA palette set...");
			break;
		default:
			break;
		}
		if (IS_VGA_ARCH)
		{
			/* check if gray scale summing is enabled */
			if (real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL) & 2)
			{
				INT10_PerformGrayScaleSumming(0,256);
			}
		}
	}
	else
	{
		for (ct=0x10; ct<ATT_REGS; ct++)
		{
			if (ct==0x11) continue;	// skip overscan register
			IO_Write(0x3c0,ct);
			IO_Write(0x3c0,att_data[ct]);
		}
		//vga.config.pel_panning = 0;
		IO_Write(0x3c0,0x20); //Enable palette access
	}


	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - Setup Special stuff for different modes...");
	/* Setup some special stuff for different modes */
	feature=real_readb(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE);
	switch (CurMode->type)
	{
	case M_CGA2:
		feature=(feature&~0x30)|0x20;
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x1e);
		break;
	case M_CGA4:
		feature=(feature&~0x30)|0x20;
		if (CurMode->mode==4) real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2a);
		else if (CurMode->mode==5) real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2e);
		else real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2);
		break;
	case M_TANDY16:
		feature=(feature&~0x30)|0x20;
		break;
	case M_TEXT:
		feature=(feature&~0x30)|0x20;
		switch (CurMode->mode)
		{
		case 0:
			real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2c);
			break;
		case 1:
			real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x28);
			break;
		case 2:
			real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x2d);
			break;
		case 3:
		case 7:
			real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,0x29);
			break;
		}
		break;
	case M_LIN4:
	case M_EGA:
	case M_VGA:
		feature=(feature&~0x30);
		break;
	default:
		break;
	}
	// disabled, has to be set in bios.cpp exclusively
//	real_writeb(BIOSMEM_SEG,BIOSMEM_INITIAL_MODE,feature);

	/*if (svgaCard == SVGA_S3Trio) {
		/ Setup the CPU Window /
		IO_Write(crtc_base,0x6a);
		IO_Write(crtc_base+1,0);
		/ Setup the linear frame buffer /
		IO_Write(crtc_base,0x59);
		IO_Write(crtc_base+1,(byte)((S3_LFB_BASE >> 24)&0xff));
		IO_Write(crtc_base,0x5a);
		IO_Write(crtc_base+1,(byte)((S3_LFB_BASE >> 16)&0xff));
		IO_Write(crtc_base,0x6b); // BIOS scratchpad
		IO_Write(crtc_base+1,(byte)((S3_LFB_BASE >> 24)&0xff));

		/ Setup some remaining S3 registers /
		IO_Write(crtc_base,0x41); // BIOS scratchpad
		IO_Write(crtc_base+1,0x88);
		IO_Write(crtc_base,0x52); // extended BIOS scratchpad
		IO_Write(crtc_base+1,0x80);

		IO_Write(0x3c4,0x15);
		IO_Write(0x3c5,0x03);

		// Accellerator setup
		byte reg_50=S3_XGA_8BPP;
		switch (CurMode->type) {
			case M_LIN15:
			case M_LIN16: reg_50|=S3_XGA_16BPP; break;
			case M_LIN32: reg_50|=S3_XGA_32BPP; break;
			default: break;
		}
		switch(CurMode->swidth) {
			case 640:  reg_50|=S3_XGA_640; break;
			case 800:  reg_50|=S3_XGA_800; break;
			case 1024: reg_50|=S3_XGA_1024; break;
			case 1152: reg_50|=S3_XGA_1152; break;
			case 1280: reg_50|=S3_XGA_1280; break;
			default: break;
		}
		IO_WriteB(crtc_base,0x50); IO_WriteB(crtc_base+1,reg_50);

		byte reg_31, reg_3a;
		switch (CurMode->type) {
			case M_LIN15:
			case M_LIN16:
			case M_LIN32:
				reg_3a=0x15;
				break;
			case M_LIN8:
				// S3VBE20 does it this way. The other double pixel bit does not
				// seem to have an effect on the Trio64.
				if(CurMode->special&_VGA_PIXEL_DOUBLE) reg_3a=0x5;
				else reg_3a=0x15;
				break;
			default:
				reg_3a=5;
				break;
		};

		switch (CurMode->type) {
		case M_LIN4: // <- Theres a discrepance with real hardware on this
		case M_LIN8:
		case M_LIN15:
		case M_LIN16:
		case M_LIN32:
			reg_31 = 9;
			break;
		default:
			reg_31 = 5;
			break;
		}
		IO_Write(crtc_base,0x3a);IO_Write(crtc_base+1,reg_3a);
		IO_Write(crtc_base,0x31);IO_Write(crtc_base+1,reg_31);	//Enable banked memory and 256k+ access
		IO_Write(crtc_base,0x58);IO_Write(crtc_base+1,0x3);		//Enable 8 mb of linear addressing

		IO_Write(crtc_base,0x38);IO_Write(crtc_base+1,0x48);	//Register lock 1
		IO_Write(crtc_base,0x39);IO_Write(crtc_base+1,0xa5);	//Register lock 2
	} else if (svga.set_video_mode) {
		VGA_ModeExtraData modeData;
		modeData.ver_overflow = ver_overflow;
		modeData.hor_overflow = hor_overflow;
		modeData.offset = offset;
		modeData.modeNo = CurMode->mode;
		modeData.htotal = CurMode->htotal;
		modeData.vtotal = CurMode->vtotal;
		svga.set_video_mode(crtc_base, &modeData);
	}*/

	//raiseError("Debug_INT10_SetVideoMode","Checkpoint 5");

	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - Finish setmode...");
	FinishSetMode(clearmem);

	//raiseError("Debug_INT10_SetVideoMode","Checkpoint 6");

	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - Reset VGA attrib register into defined state...");
	/* Set vga attrib register into defined state */
	IO_Read(mono_mode ? 0x3ba : 0x3da);
	IO_Write(0x3c0,0x20);

	//raiseError("Debug_INT10_SetVideoMode","Checkpoint 7");


	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - Autoloading text mode font...");
	/* Load text mode font */
	if (CurMode->type==M_TEXT)
	{
		INT10_ReloadFont(); //Reload the font!
	}

	//raiseError("Debug_INT10_SetVideoMode","Checkpoint RET");

	if (LOG_SWITCHMODE) dolog(LOG_FILE,"switchvideomode: INT10_Internal_SetVideoMode - RET...");
	return true;
}