#ifndef HEADER_DOSBOX_H
#define HEADER_DOSBOX_H
//Dosbox Takeover stuff!

#include "headers/types.h" //Basic types!

/*

Constants for memory adressing:

*/

#define S3_LFB_BASE		0xC0000000

#define BIOSMEM_SEG		0x40

#define BIOSMEM_INITIAL_MODE  0x10
#define BIOSMEM_CURRENT_MODE  0x49
#define BIOSMEM_NB_COLS       0x4A
#define BIOSMEM_PAGE_SIZE     0x4C
#define BIOSMEM_CURRENT_START 0x4E
#define BIOSMEM_CURSOR_POS    0x50
#define BIOSMEM_CURSOR_TYPE   0x60
#define BIOSMEM_CURRENT_PAGE  0x62
#define BIOSMEM_CRTC_ADDRESS  0x63
#define BIOSMEM_CURRENT_MSR   0x65
#define BIOSMEM_CURRENT_PAL   0x66
#define BIOSMEM_NB_ROWS       0x84
#define BIOSMEM_CHAR_HEIGHT   0x85
#define BIOSMEM_VIDEO_CTL     0x87
#define BIOSMEM_SWITCHES      0x88
#define BIOSMEM_MODESET_CTL   0x89
#define BIOSMEM_DCC_INDEX     0x8A
#define BIOSMEM_CRTCPU_PAGE   0x8A
#define BIOSMEM_VS_POINTER    0xA8


/*
 *
 * VGA registers
 *
 */
#define VGAREG_ACTL_ADDRESS            0x3c0
#define VGAREG_ACTL_WRITE_DATA         0x3c0
#define VGAREG_ACTL_READ_DATA          0x3c1

#define VGAREG_INPUT_STATUS            0x3c2
#define VGAREG_WRITE_MISC_OUTPUT       0x3c2
#define VGAREG_VIDEO_ENABLE            0x3c3
#define VGAREG_SEQU_ADDRESS            0x3c4
#define VGAREG_SEQU_DATA               0x3c5

#define VGAREG_PEL_MASK                0x3c6
#define VGAREG_DAC_STATE               0x3c7
#define VGAREG_DAC_READ_ADDRESS        0x3c7
#define VGAREG_DAC_WRITE_ADDRESS       0x3c8
#define VGAREG_DAC_DATA                0x3c9

#define VGAREG_READ_FEATURE_CTL        0x3ca
#define VGAREG_READ_MISC_OUTPUT        0x3cc

#define VGAREG_GRDC_ADDRESS            0x3ce
#define VGAREG_GRDC_DATA               0x3cf

#define VGAREG_MDA_CRTC_ADDRESS        0x3b4
#define VGAREG_MDA_CRTC_DATA           0x3b5
#define VGAREG_VGA_CRTC_ADDRESS        0x3d4
#define VGAREG_VGA_CRTC_DATA           0x3d5

#define VGAREG_MDA_WRITE_FEATURE_CTL   0x3ba
#define VGAREG_VGA_WRITE_FEATURE_CTL   0x3da
#define VGAREG_ACTL_RESET              0x3da
#define VGAREG_TDY_RESET               0x3da
#define VGAREG_TDY_ADDRESS             0x3da
#define VGAREG_TDY_DATA                0x3de
#define VGAREG_PCJR_DATA               0x3da

#define VGAREG_MDA_MODECTL             0x3b8
#define VGAREG_CGA_MODECTL             0x3d8
#define VGAREG_CGA_PALETTE             0x3d9

/* Video memory */
#define VGAMEM_GRAPH 0xA000
#define VGAMEM_CTEXT 0xB800
#define VGAMEM_MTEXT 0xB000

#define BIOS_NCOLS word ncols=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
#define BIOS_NROWS word nrows=(word)real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1;

extern byte int10_font_08[256 * 8];
extern byte int10_font_14[256 * 14];
extern byte int10_font_16[256 * 16];

//Set to byte for now!


#define _EGA_HALF_CLOCK		0x0001
#define _EGA_LINE_DOUBLE	0x0002
#define _VGA_PIXEL_DOUBLE	0x0004

#define SEQ_REGS 0x05
#define GFX_REGS 0x09
#define ATT_REGS 0x15

typedef enum
{
    M_CGA2, M_CGA4,
    M_EGA, M_VGA,
    M_LIN4, M_LIN8, M_LIN15, M_LIN16, M_LIN32,
    M_TEXT,
    M_HERC_GFX, M_HERC_TEXT,
    M_CGA16, M_TANDY2, M_TANDY4, M_TANDY16, M_TANDY_TEXT,
    M_ERROR
} VGAModes;

typedef struct
{
	word	mode;
	VGAModes	type;
	uint_32	swidth, sheight;
	uint_32	twidth, theight;
	uint_32	cwidth, cheight;
	uint_32	ptotal,pstart,plength;

	uint_32	htotal,vtotal;
	uint_32	hdispend,vdispend;
	uint_32	special;

} VideoModeBlock;
extern VideoModeBlock ModeList_VGA[];
extern VideoModeBlock * CurMode;

typedef uint_32 RealPt; //16-bit segment:offset value! (segment high, offset low)

typedef struct
{
	struct
	{
		RealPt font_8_first;
		RealPt font_8_second;
		RealPt font_14;
		RealPt font_16;
		RealPt font_14_alternate;
		RealPt font_16_alternate;
		RealPt static_state;
		RealPt video_save_pointers;
		RealPt video_parameter_table;
		RealPt video_save_pointer_table;
		RealPt video_dcc_table;
		RealPt oemstring;
		RealPt vesa_modes;
		RealPt pmode_interface;
		word pmode_interface_size;
		word pmode_interface_start;
		word pmode_interface_window;
		word pmode_interface_palette;
		word used;
	} rom;
	word vesa_setmode;
	bool vesa_nolfb;
	bool vesa_oldvbe;
} Int10Data;

extern Int10Data int10;

#define CLK_25 25175
#define CLK_28 28322

#define MIN_VCO	180000
#define MAX_VCO 360000

#define S3_CLOCK_REF	14318	/* KHz */
#define S3_CLOCK(_M,_N,_R)	((S3_CLOCK_REF * ((_M) + 2)) / (((_N) + 2) * (1 << (_R))))
#define S3_MAX_CLOCK	150000	/* KHz */

#define S3_XGA_1024		0x00
#define S3_XGA_1152		0x01
#define S3_XGA_640		0x40
#define S3_XGA_800		0x80
#define S3_XGA_1280		0xc0
#define S3_XGA_WMASK	(S3_XGA_640|S3_XGA_800|S3_XGA_1024|S3_XGA_1152|S3_XGA_1280)

#define S3_XGA_8BPP  0x00
#define S3_XGA_16BPP 0x10
#define S3_XGA_32BPP 0x30
#define S3_XGA_CMASK (S3_XGA_8BPP|S3_XGA_16BPP|S3_XGA_32BPP)

/*

end of dosbox data!

*/

/*

Patches for dosbox!

*/

enum MachineType
{
    MCH_HERC,
    MCH_CGA,
    MCH_TANDY,
    MCH_PCJR,
    MCH_EGA,
    MCH_VGA
};

//Type patches!
#define Bit16u word
#define Bit8u byte
#define Bit32u uint_32
#define Bitu uint_32

typedef byte *PhysPt; //Physical pointer!
#define color pixel
#define mem_readb(off) MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,(((off)>>16)&0xFFFF),((off)&0xFFFF),0)
#define mem_writeb(off,val) MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,(((off)>>16)&0xFFFF),((off)&0xFFFF),0)
#define PhysMake(seg,offs) MMU_ptr(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,seg,offs,0,1)
#define Real2Phys(x) (byte *)MMU_ptr(-1,(((x)>>16)&0xFFFF),((x)&0xFFFF),0,1)
//Physical to real support (for MMU_wb/w/dw)
#define Phys2Real1(x) (((uint_32)x)-((uint_32)MMU_ptr(-1,0,0,0,0)))
#define Phys2Real(x) (Phys2Real1(x)&0xF)|((Phys2Real1(x)&(~0xF))<<16)
//Write/read functions!
#define phys_writew(ptr,val) phys_writew(ptr,val)
#define phys_writeb(ptr,val) phys_writeb(ptr,val)
#define phys_readb(ptr) phys_readb(ptr)
#define RealMake(seg,offs) (((seg)&0xFFFF)<<16)|((offs)&0xFFFF)
#define false 0
#define S3_LFB_BASE 0xC0000000

//Palette only:
#define VGAREG_ACTL_ADDRESS            0x3c0
#define VGAREG_ACTL_WRITE_DATA         0x3c0
#define VGAREG_ACTL_READ_DATA          0x3c1
#define VGAREG_ACTL_RESET              0x3da
#define ACTL_MAX_REG   0x14


//Dosbox Patches! Redirect it all!
#define real_readb(biosseg,offs) MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,biosseg,offs,0)
#define real_writeb(biosseg,offs,val) MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,biosseg,offs,val)
#define real_readw(biosseg,offs) (real_readb(biosseg,offs)+(real_readb(biosseg,offs+1)*256))
#define real_writew(biosseg,offs,val) real_writeb(biosseg,offs,val&0xFF); real_writeb(biosseg,offs+1,((val&0xFF00)>>8));
#define memreal_writew(seg,offs,val) MMU_ww(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,seg,offs,val)
#define IO_WriteB(port,value) PORT_OUT_B(port,value)
#define IO_WriteW(port,value) PORT_OUT_W(port,value)
#define IO_ReadB(port) PORT_IN_B(port)
#define IO_ReadW(port) PORT_IN_W(port)
//Synonym for IO_WriteB:
#define IO_Write(port,value) IO_WriteB(port,value)
#define IO_Read(port) IO_ReadB(port)

#define true 1
#define false 0
#define IS_EGAVGA_ARCH IS_VGA_ARCH
#define Bitu uint_32


/*

Our own switch function!

*/

void switchvideomode(word mode); //For DOSBox way!

//Phys/Real pointer support
void phys_writew(PhysPt ptr, word val);
void phys_writeb(PhysPt ptr, byte val);
byte phys_readb(PhysPt ptr);
void RealSetVec(byte interrupt, word segment, word offset);

#endif