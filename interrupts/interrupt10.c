/*

Interrupt 10h: Video interrupt

*/

#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu.h" //Real ouput module!
#include "headers/cpu/cpu.h" //CPU module!
#include "headers/cpu/easyregs.h" //Easy register access!
#include "headers/hardware/vga_rest/colorconversion.h" //Color conversion compatibility for text output!
#include "headers/cpu/interrupts.h" //Interrupt support for GRAPHIC modes!
#include "headers/hardware/vga.h" //Basic typedefs!
#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Attribute controller for debug screen output!
#include "headers/header_dosbox.h" //Screen modes from DOSBox!
#include "headers/hardware/ports.h" //Port support!
#include "headers/hardware/vga_screen/vga_precalcs.h" //Precalculation support!
#include "headers/hardware/vga_screen/vga_sequencer_textmode.h" //For character size detection!
#include "headers/support/log.h" //Logging support!

#include "headers/interrupts/interrupt10.h" //Our typedefs etc.
#include "headers/emu/gpu/gpu_emu.h" //GPU emulation functionality!
#include "headers/cpu/callback.h" //Callback detection!

#include "headers/cpu/80286/protection.h" //For CPU_segment_index!

//Are we disabled for checking?
#define __HW_DISABLED 0

//Text screen height is always 25!

VGA_Type *int10_VGA; //For direct VGA function access!
extern GPU_type GPU; //GPU info&adjusting!
extern byte LOG_VRAM_WRITES; //Log VRAM writes?

int int10loaded = 0; //Default: not loaded yet!

//Screencontents: 0xB800:(row*0x0040:004A)+column*2)
//Screencolorsfont: 0xB800:((row*0x0040:004A)+column*2)+1)
//Screencolorsback: Same as font, high nibble!

void int10_useVGA(VGA_Type *VGA)
{
	if (__HW_DISABLED) return; //Abort!
	int10_VGA = VGA;
}

extern VideoModeBlock ModeList_VGA[63]; //VGA Modelist!
extern VideoModeBlock *CurMode; //VGA Active Video Mode!

//All palettes:
extern byte text_palette[64][3];
extern byte mtext_palette[64][3];
extern byte mtext_s3_palette[64][3];
extern byte ega_palette[64][3];
extern byte cga_palette[16][3];
extern byte cga_palette_2[64][3];
extern byte vga_palette[256][3];

//Masks for CGA!
extern Bit8u cga_masks[4];
extern Bit8u cga_masks2[8];

void GPU_setresolution(word mode) //Sets the resolution based on current video mode byte!
{
	GPU.showpixels = ALLOW_GPU_GRAPHICS; //Show pixels!

	//Now all BIOS data!
	//dolog("emu","Setting int10 video mode...");
	INT10_Internal_SetVideoMode(int10_VGA,mode); //Switch video modes!
	//dolog("emu","Setting scanlines...");
	//EMU_CPU_setCursorScanlines(getcharacterheight(int10_VGA)-2,getcharacterheight(int10_VGA)-1); //Reset scanlines to bottom!
}

byte getscreenwidth(byte displaypage) //Get the screen width (in characters), based on the video mode!
{
	if (__HW_DISABLED) return 0; //Abort!
	return MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,0);
	byte result;
	switch (MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,0)) //What video mode?
	{
	case 0x00:
	case 0x01: //40x25?
		result = 40; //40 columns a row!
		break;
	default:
	case 0x02:
	case 0x03:
	case 0x07: //80x25?
		result = 80; //80 columns a row!
		break;
	}
	//GPU.showpixels = ((result!=0) && ALLOW_GPU_GRAPHICS); //Show pixels?
	GPU.showpixels = ALLOW_GPU_GRAPHICS; //Always allow!
	return result; //Give the result!
}

byte GPUgetvideomode()
{
	if (__HW_DISABLED) return 0; //Abort!
	return MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,0); //Give mode!
}

void GPUswitchvideomode(word mode)
{
	GPU_setresolution(mode); //Set the resolution to use&rest data!
}


int GPU_getpixel(int x, int y, byte page, byte *pixel) //Get a pixel from the real emulated screen buffer!
{
	if (__HW_DISABLED) return 0; //Abort!
        switch (CurMode->type) {
        case M_CGA4:
                {
                        Bit16u off=(y>>1)*80+(x>>2);
                        if (y&1) off+=8*1024;
                        Bit8u val=MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0xb800,off,0);
                        *color=(val>>(((3-(x&3)))*2)) & 3 ;
                }
                break;
        case M_CGA2:
                {
                        Bit16u off=(y>>1)*80+(x>>3);
                        if (y&1) off+=8*1024;
                        Bit8u val=MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0xb800,off,0);
                        *color=(val>>(((7-(x&7))))) & 1 ;
                }
                break;
        case M_EGA:
                {
                        /* Calculate where the pixel is in video memory */
                        //if (CurMode->plength!=(Bitu)MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,0))
                                //LOG(LOG_INT10,LOG_ERROR)("GetPixel_EGA_p: %x!=%x",CurMode->plength,MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,0));
                        //if (CurMode->swidth!=(Bitu)MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,0)*8)
                                //LOG(LOG_INT10,LOG_ERROR)("GetPixel_EGA_w: %x!=%x",CurMode->swidth,MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,0)*8);
                        RealPt off=RealMake(0xa0000,MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,0)*page+
                                ((y*MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,0)*8+x)>>3));
                        Bitu shift=7-(x & 7);
                        /* Set the read map */
                        *color=0;
                        IO_Write(0x3ce,0x4);IO_Write(0x3cf,1);
                        *color|=((mem_readb(off)>>shift) & 1) << 0;
                        IO_Write(0x3ce,0x4);IO_Write(0x3cf,2);
                        *color|=((mem_readb(off)>>shift) & 1) << 1;
                        IO_Write(0x3ce,0x4);IO_Write(0x3cf,4);
                        *color|=((mem_readb(off)>>shift) & 1) << 2;
                        IO_Write(0x3ce,0x4);IO_Write(0x3cf,8);
                        *color|=((mem_readb(off)>>shift) & 1) << 3;
                        break;
                }
        case M_VGA:
                *color=mem_readb(RealMake(0xa000,320*y+x));
                break;
        case M_LIN8: {
                        //if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)
                        //        LOG(LOG_INT10,LOG_ERROR)("GetPixel_VGA_w: %x!=%x",CurMode->swidth,real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
                        RealPt off=RealMake(S3_LFB_BASE,y*real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8+x);
                        *color = mem_readb(off);
                        break;
                }
        default:
                //LOG(LOG_INT10,LOG_ERROR)("GetPixel unhandled mode type %d",CurMode->type);
            return 0; //Error: unknown mode!
                break;
        }
        return 1; //OK!
}

int GPU_putpixel(int x, int y, byte page, byte color) //Writes a video buffer pixel to the real emulated screen buffer
{
	if (__HW_DISABLED) return 0; //Abort!
        //static bool putpixelwarned = false;

        switch (CurMode->type) {
        case M_CGA4:
                {
                                if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE)<=5) {
                                        Bit16u off=(y>>1)*80+(x>>2);
                                        if (y&1) off+=8*1024;

                                        Bit8u old=real_readb(0xb800,off);
                                        if (color & 0x80) {
                                                color&=3;
                                                old^=color << (2*(3-(x&3)));
                                        } else {
                                                old=(old&cga_masks[x&3])|((color&3) << (2*(3-(x&3))));
                                        }
                                        real_writeb(0xb800,off,old);
                                } else {
                                        Bit16u off=(y>>2)*160+BITOFF((x>>2),1);
                                        off+=(8*1024) * (y & 3);

                                        Bit16u old=real_readw(0xb800,off);
                                        if (color & 0x80) {
                                                old^=(color&1) << (7-(x&7));
                                                old^=((color&2)>>1) << ((7-(x&7))+8);
                                        } else {
                                                old=BITOFF(old,((0x101<<(7-(x&7))))) | ((color&1) << (7-(x&7))) | (((color&2)>>1) << ((7-(x&7))+8));
                                        }
                                        real_writew(0xb800,off,old);
                                }
                }
                break;
        case M_CGA2:
                {
                                Bit16u off=(y>>1)*80+(x>>3);
                                if (y&1) off+=8*1024;
                                Bit8u old=real_readb(0xb800,off);
                                if (color & 0x80) {
                                        color&=1;
                                        old^=color << ((7-(x&7)));
                                } else {
                                        old=(old&cga_masks2[x&7])|((color&1) << ((7-(x&7))));
                                }
                                real_writeb(0xb800,off,old);
                }
                break;
        case M_TANDY16:
                {
                        IO_Write(0x3d4,0x09);
                        Bit8u scanlines_m1=IO_Read(0x3d5);
                        Bit16u off=(y>>((scanlines_m1==1)?1:2))*(CurMode->swidth>>1)+(x>>1);
                        off+=(8*1024) * (y & scanlines_m1);
                        Bit8u old=real_readb(0xb800,off);
                        Bit8u p[2];
                        p[1] = (old >> 4) & 0xf;
                        p[0] = old & 0xf;
                        Bitu ind = 1-(x & 0x1);

                        if (color & 0x80) {
                                p[ind]^=(color & 0x7f);
                        } else {
                                p[ind]=color;
                        }
                        old = (p[1] << 4) | p[0];
                        real_writeb(0xb800,off,old);
                }
                break;
        case M_LIN4:
                //if ((machine!=MCH_VGA) || (svgaCard!=SVGA_TsengET4K) ||
                //                (CurMode->swidth>800)) {
                        // the ET4000 BIOS supports text output in 800x600 SVGA (Gateway 2)
                        // putpixel warining?
                        break;
                //}
        case M_EGA:
                {
                        /* Set the correct bitmask for the pixel position */
                        IO_Write(0x3ce,0x8);Bit8u mask=128>>(x&7);IO_Write(0x3cf,mask);
                        /* Set the color to set/reset register */
                        IO_Write(0x3ce,0x0);IO_Write(0x3cf,color);
                        /* Enable all the set/resets */
                        IO_Write(0x3ce,0x1);IO_Write(0x3cf,0xf);
                        /* test for xorring */
                        if (color & 0x80) { IO_Write(0x3ce,0x3);IO_Write(0x3cf,0x18); }
                        //Perhaps also set mode 1 
                        /* Calculate where the pixel is in video memory */
                        //if (CurMode->plength!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE))
                        //        LOG(LOG_INT10,LOG_ERROR)("PutPixel_EGA_p: %x!=%x",CurMode->plength,real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE));
                        //if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)
                        //        LOG(LOG_INT10,LOG_ERROR)("PutPixel_EGA_w: %x!=%x",CurMode->swidth,real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
                        RealPt off=RealMake(0xa0000,real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE)*page+
                                ((y*real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8+x)>>3));
                        /* Bitmask and set/reset should do the rest */
                        mem_readb(off);
                        mem_writeb(off,0xff);
                        /* Restore bitmask */   
                        IO_Write(0x3ce,0x8);IO_Write(0x3cf,0xff);
                        IO_Write(0x3ce,0x1);IO_Write(0x3cf,0);
                        /* Restore write operating if changed */
                        if (color & 0x80) { IO_Write(0x3ce,0x3);IO_Write(0x3cf,0x0); }
                        break;
                }

        case M_VGA:
                mem_writeb(RealMake(0xa000,y*320+x),color);
                break;
        case M_LIN8: {
                        //if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)
                        //        LOG(LOG_INT10,LOG_ERROR)("PutPixel_VGA_w: %x!=%x",CurMode->swidth,real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
                        RealPt off=RealMake(S3_LFB_BASE,y*real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8+x);
                        mem_writeb(off,color);
                        break;
                }
        default:
                //if(GCC_UNLIKELY(!putpixelwarned)) {
                        //putpixelwarned = true;          
                        //LOG(LOG_INT10,LOG_ERROR)("PutPixel unhandled mode type %d",CurMode->type);
                //}
                return 0; //Error!
                break;
        }
        return 1; //OK!
}

static void ResetACTL(void) {
	if (__HW_DISABLED) return; //Abort!
	IO_Read(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS) + 6);
}

void INT10_SetSinglePaletteRegister(Bit8u reg,Bit8u val) {
	if (__HW_DISABLED) return; //Abort!
	//switch (machine) {
/*	case MCH_PCJR:
		reg&=0xf;
		IO_Read(VGAREG_TDY_RESET);
		WriteTandyACTL(reg+0x10,val);
		IO_Write(0x3da,0x0); // palette back on
		break;
	case MCH_TANDY:
		// TODO waits for vertical retrace
		switch(vga.mode) {
		case M_TANDY2:
			if (reg >= 0x10) break;
			else if (reg==1) reg = 0x1f;
			else reg |= 0x10;
			WriteTandyACTL(reg+0x10,val);
			break;
		case M_TANDY4: {
			if (CurMode->mode!=0x0a) {
				// Palette values are kept constand by the BIOS.
				// The four colors are mapped to special palette values by hardware.
				// 3D8/3D9 registers influence this mapping. We need to figure out
				// which entry is used for the requested color.
				if (reg > 3) break;
				if (reg != 0) { // 0 is assumed to be at 0
					Bit8u color_select=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL);
					reg = reg*2+8; // Green Red Brown
					if (color_select& 0x20) reg++; // Cyan Magenta White
				}
				WriteTandyACTL(reg+0x10,val);
			} 
			// 4-color high resolution mode 0x0a isn't handled specially
			else WriteTandyACTL(reg+0x10,val);
			break;
		}
		default:
			WriteTandyACTL(reg+0x10,val);
			break;
		}
		IO_Write(0x3da,0x0); // palette back on
		break;
*/
	//case EGAVGA_ARCH_CASE:
		if (!IS_VGA_ARCH) reg&=0x1f;
		if(reg<=ACTL_MAX_REG) {
			ResetACTL();
			IO_Write(VGAREG_ACTL_ADDRESS,reg);
			IO_Write(VGAREG_ACTL_WRITE_DATA,val);
		}
		IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
		//break;
	//}
}

void INT10_SetColorSelect(Bit8u val) {
	if (__HW_DISABLED) return; //Abort!
	Bit8u temp=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL);
	temp=(temp & 0xdf) | ((val & 1) ? 0x20 : 0x0);
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,temp);
/*	if (machine == MCH_CGA || machine==MCH_TANDY)
		IO_Write(0x3d9,temp);
	else if (machine == MCH_PCJR) {
		IO_Read(VGAREG_TDY_RESET); // reset the flipflop
		switch(vga.mode) {
		case M_TANDY2:
			IO_Write(VGAREG_TDY_ADDRESS, 0x11);
			IO_Write(VGAREG_PCJR_DATA, val&1? 0xf:0);
			break;
		case M_TANDY4:
			for(Bit8u i = 0x11; i < 0x14; i++) {
				const Bit8u t4_table[] = {0,2,4,6, 0,3,5,0xf};
				IO_Write(VGAREG_TDY_ADDRESS, i);
				IO_Write(VGAREG_PCJR_DATA, t4_table[(i-0x10)+(val&1? 4:0)]);
			}
			break;
		default:
			// 16-color modes: always write the same palette
			for(Bit8u i = 0x11; i < 0x20; i++) {
				IO_Write(VGAREG_TDY_ADDRESS, i);
				IO_Write(VGAREG_PCJR_DATA, i-0x10);
			}
			break;
		}
		IO_Write(VGAREG_TDY_ADDRESS, 0); // enable palette
	}
	else if (IS_EGAVGA_ARCH) {
*/
		if (CurMode->mode <= 3) //Maybe even skip the total function!
			return;
		val = (temp & 0x10) | 2 | val;
		INT10_SetSinglePaletteRegister( 1, val );
		val+=2;
		INT10_SetSinglePaletteRegister( 2, val );
		val+=2;
		INT10_SetSinglePaletteRegister( 3, val );
//	}
}

void updateCursorLocation()
{
	if (__HW_DISABLED) return; //Abort!
	int x;
	int y;
	x = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0)*2),0); //X
	y = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0)*2)+1,0); //Y
	word address; //Address of the cursor location!
	address = MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_START,0)+
			(y*MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,0))+x;

	byte oldcrtc = PORT_IN_B(0x3D4); //Save old address!
	PORT_OUT_B(0x3D4,0xF); //Select cursor location low register!
	PORT_OUT_B(0x3D5,address&0xFF); //Low location!
	PORT_OUT_B(0x3D4,0xE); //Select cursor location high register!
	PORT_OUT_B(0x3D5,((address>>8)&0xFF)); //High location!
	PORT_OUT_B(0x3D4,oldcrtc); //Restore old CRTC register!
}

void EMU_CPU_setCursorXY(byte displaypage, byte x, byte y)
{
	if (__HW_DISABLED) return; //Abort!
//First: BDA entry update!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0)*2),x); //X
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0)*2)+1,y); //Y

	if (MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0)==displaypage) //Current page?
	{
		//Apply the cursor position to the VGA!
		updateCursorLocation(); //Update the cursor's location!
	}
}

void EMU_CPU_setCursorScanlines(byte start, byte end)
{
	if (__HW_DISABLED) return; //Abort!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE,start); //Set start line!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE+1,end); //Set end line!

	/*int10_VGA->registers->CRTControllerRegisters.DATA[0xA] = start;
	int10_VGA->registers->CRTControllerRegisters.DATA[0xB] = end;*/
	byte oldcrtc = PORT_IN_B(0x3D4); //Save old address!
	PORT_OUT_B(0x3D4,0xA); //Select start register!
	PORT_OUT_B(0x3D5,start); //Start!
	PORT_OUT_B(0x3D4,0xB); //Select end register!
	PORT_OUT_B(0x3D5,end); //End!
	PORT_OUT_B(0x3D4,oldcrtc); //Restore old CRTC register!
}

void EMU_CPU_getCursorScanlines(byte *start, byte *end)
{
	if (__HW_DISABLED) return; //Abort!
	*start = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE,0); //Get start line!
	*end = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE+1,0); //Get end line!
}

void GPU_clearscreen() //Clears the screen!
{
	if (__HW_DISABLED) return; //Abort!
	byte oldmode;
	oldmode = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,0); //Active video mode!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,oldmode&0x7F); //Clear!
	GPU_setresolution(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,0)); //Reset the resolution!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,oldmode); //Restore old mode!
}

void GPU_clearscreen_BIOS() //Clears the screen for BIOS menus etc.!
{
	if (__HW_DISABLED) return; //Abort!
	GPU_clearscreen(); //Forward: we're using official VGA now!
}

void int10_nextcol(byte thepage)
{
	if (__HW_DISABLED) return; //Abort!
	byte x = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0)*2),0);
	byte y = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0)*2)+1,0);
	//dolog("interrupt10","Nextcol: %i,%i becomes:",x,y);
	++x; //Next X!
	if (x>=getscreenwidth(thepage)) //Overflow?
	{
		x = 0; //Reset!
		++y; //Next Y!
		if (y>MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_ROWS,0)) //Overflow?
		{
			y = 0; //Reset!
		}
	}
	EMU_CPU_setCursorXY(thepage,x,y); //Give x,y of cursor!
}

void cursorXY(byte displaypage, byte x, byte y)
{
	if (__HW_DISABLED) return; //Abort!
	EMU_CPU_setCursorXY(displaypage,x,y); //Give x,y of cursor!
}

























//Below read/writecharacter based upon: https://code.google.com/p/dosbox-wii/source/browse/trunk/src/ints/int10_char.cpp

void int10_vram_writecharacter(byte x, byte y, byte page, byte character, byte attribute) //Write character+attribute!
{
	if (__HW_DISABLED) return; //Abort!
	//dolog("interrupt10","int10_writecharacter: %i,%i@%02X=%02X=>%02X",x,y,page,character,attribute);
	switch (CurMode->type)
	{
	case M_TEXT: //Text mode?
		{
		//+ _4KB * vdupage + 160 * y + 2 * x
			uint_32 where = (CurMode->pstart>>4); //Position of the character, all above the fourth bit (pstart=segment<<4)!
			word address = (CurMode->pstart&0xF); //Rest address!
			address += page*MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,0); //Start of page!
			address += ((y*MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,0))+x)*2; //Character offset within page!
			MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,where,address,character); //The character! Write plane 0!
			MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,where,address+1,attribute); //The attribute! Write plane 1!
			return; //Done!
		}
		break;
	default:
		break; //Do nothing: unsupported yet!
	}
}

void int10_vram_readcharacter(byte x, byte y, byte page, byte *character, byte *attribute) //Read character+attribute!
{
	if (__HW_DISABLED) return; //Abort!
	switch (CurMode->type)
	{
	case M_TEXT: //Text mode?
		{
		//+ _4KB * vdupage + 160 * y + 2 * x
			uint_32 where = (CurMode->pstart>>4); //Position of the character, all above the fourth bit (pstart=segment<<4)!
			word address = (CurMode->pstart&0xF); //Rest address!
			address += page*MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,0); //Start of page!
			address += ((y*MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,0))+x)*2; //Character offset within page!
			*character = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,where,address,0); //The character!
			*attribute = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,where+1,address,0); //The attribute!
		}
		break;
	default:
		break; //Do nothing: unsupported yet!
	}
}

void emu_setactivedisplaypage(byte page) //Set active display page!
{
	if (__HW_DISABLED) return; //Abort!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,page); //Active video page!
	MMU_ww(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_START,page*MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,0)); //Display page offset!
//Now for the VGA!

	int10_VGA->registers->CRTControllerRegisters.REGISTERS.STARTADDRESSHIGHREGISTER = ((MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_START,0)>>8)&0xFF); //High!
	int10_VGA->registers->CRTControllerRegisters.REGISTERS.STARTADDRESSLOWREGISTER = (MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_START,0)&0xFF); //Low!
}

byte emu_getdisplaypage()
{
	if (__HW_DISABLED) return 0; //Abort!
	return MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0); //Active page!
}

/*

Font generator support!

*/

/*void INT10_LoadFont(PhysPt font,bool reload,Bitu count,Bitu offset,Bitu map,Bitu height) {
	PhysPt ftwhere=PhysMake(0xa000,map_offset[map & 0x7]+(Bit16u)(offset*32));
	IO_Write(0x3c4,0x2);IO_Write(0x3c5,0x4);	//Enable plane 2
	IO_Write(0x3ce,0x6);Bitu old_6=IO_Read(0x3cf);
	IO_Write(0x3cf,0x0);	//Disable odd/even and a0000 adressing
	for (Bitu i=0;i<count;i++) {
		MEM_BlockCopy(ftwhere,font,height);
		ftwhere+=32;
		font+=height;
	}
	IO_Write(0x3c4,0x2);IO_Write(0x3c5,0x3);	//Enable textmode planes (0,1)
	IO_Write(0x3ce,0x6);
	if (IS_VGA_ARCH) IO_Write(0x3cf,(Bit8u)old_6);	//odd/even and b8000 adressing
	else IO_Write(0x3cf,0x0e);
	/ Reload tables and registers with new values based on this height /
	if (reload) {
		//Max scanline 
		Bit16u base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
		IO_Write(base,0x9);
		IO_Write(base+1,(IO_Read(base+1) & 0xe0)|(height-1));
		//Vertical display end bios says, but should stay the same?
		//Rows setting in bios segment
		real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,(CurMode->sheight/height)-1);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,(Bit8u)height);
		//TODO Reprogram cursor size?
	}
}*/


void int10_ActivateFontBlocks(byte selector) //Activate a font!
{
	if (__HW_DISABLED) return; //Abort!
	IO_Write(0x3c4,0x03); //Character map select register!
	IO_Write(0x3c5,selector); //Activate the font blocks, according to the user!
}

word map_offset[8] = {0x0000,0x4000,0x8000,0xB000,0x2000,0x6000,0xA000,0xE000}; //Where do we map to?

void int10_LoadFont(word segment, uint_32 offset, //font in dosbox!
				byte reload,
				uint_32 count,
				uint_32 vramoffset, uint_32 map, uint_32 height) //Load a custom font!
{
	if (__HW_DISABLED) return; //Abort!
	IO_Write(0x3c4,0x2);IO_Write(0x3c5,0x4);	//Enable plane 2
	IO_Write(0x3ce,0x6);Bitu old_6=IO_Read(0x3cf);
	IO_Write(0x3cf,0x0);	//Disable odd/even and a0000 adressing
	uint_32 i;
	for (i=0;i<count;i++) {
		MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0xa000,map_offset[map]+map_offset[map&0x7]+(vramoffset*32),MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,segment,offset,0)); //Write to VRAM plane 2!
	}
	IO_Write(0x3c4,0x2);IO_Write(0x3c5,0x3);	//Enable textmode planes (0,1)
	IO_Write(0x3ce,0x6);
	if (IS_VGA_ARCH) IO_Write(0x3cf,(Bit8u)old_6);	//odd/even and b8000 adressing
	else IO_Write(0x3cf,0x0e);	
	if (reload)
	{
//Max scanline 
		Bit16u base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
		IO_Write(base,0x9);
		IO_Write(base+1,(IO_Read(base+1) & 0xe0)|(height-1));
		//Vertical display end bios says, but should stay the same?
		//Rows setting in bios segment
		real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,(CurMode->sheight/height)-1);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,(Bit8u)height);
		//TODO Reprogram cursor size?
	}
}

void int10_LoadFontSystem(byte *data, //font in dosbox!
				byte reload,
				uint_32 count,
				uint_32 offset, uint_32 map, uint_32 height) //Load a custom font!
{
	if (__HW_DISABLED) return; //Abort!
	IO_Write(0x3c4,0x2);IO_Write(0x3c5,0x4);	//Enable plane 2
	IO_Write(0x3ce,0x6);Bitu old_6=IO_Read(0x3cf);
	IO_Write(0x3cf,0x0);	//Disable odd/even and a0000 adressing
	uint_32 i;
	for (i=0;i<count;i++) {
		MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,0xa000,map_offset[map]+map_offset[map&0x7]+(offset*32),data[i]); //Write to VRAM plane 2!
	}
	IO_Write(0x3c4,0x2);IO_Write(0x3c5,0x3);	//Enable textmode planes (0,1)
	IO_Write(0x3ce,0x6);
	if (IS_VGA_ARCH) IO_Write(0x3cf,(Bit8u)old_6);	//odd/even and b8000 adressing
	else IO_Write(0x3cf,0x0e);	
	if (reload)
	{
//Max scanline 
		Bit16u base=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
		IO_Write(base,0x9);
		IO_Write(base+1,(IO_Read(base+1) & 0xe0)|(height-1));
		//Vertical display end bios says, but should stay the same?
		//Rows setting in bios segment
		real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,(CurMode->sheight/height)-1);
		real_writeb(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,(Bit8u)height);
		//TODO Reprogram cursor size?
	}
}

/*void INT10_ReloadFont()
{
	if (__HW_DISABLED) return; //Abort!
	VGALoadCharTable(getActiveVGA(),getActiveVGA()->registers->CRTControllerRegisters.REGISTERS.MAXIMUMSCANLINEREGISTER.MaximumScanLine+1,0x0000); //Reload the font at 0x0000!
}*/ //Seperate!



















void int10_SetVideoMode()
{
	/*
		AL=Video mode
		result: AL=Video mode flag/controller byte
	*/
	GPUswitchvideomode(AL); //Switch the video mode!
}

void int10_SetTextModeCursorShape()
{
	/*
		CH=Scan row start
		CL=Scan row end
		If bit 5 is used on VGA: hide cursor; else determine by start>end.
	*/
	EMU_CPU_setCursorScanlines(CH,CL); //Set scanline start&end to off by making start higher than end!
}

void int10_SetCursorPosition()
{
	/*
		BH=Page Number
		DH=Row
		DL=Column
	*/
	//dolog("interrupt10","Gotoxy@%02X: %i,%i",BH,DL,DH);
	cursorXY(BH,DL,DH); //Goto x,y!
}

void int10_GetCursorPositionAndSize()
{
	/*
		BH=Page Number
		result:
		AX=0
		CH=Start scan line
		CL=End scan line
		DH=Row
		DL=Column
	*/
	AX = 0;
	DL = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2),0); //Cursor x!
	DH = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2)+1,0); //Cursor y!
	EMU_CPU_getCursorScanlines(&CH,&CL); //Scan lines of the cursor!
	//dolog("interrupt10","GetCursorPositionAndSize:%i,%i; Size: %i-%i",DL,DH,CH,CL);
}

void int10_ReadLightPenPosition()
{
	//Not used on VGA systems!
}

void int10_SelectActiveDisplayPage()
{
	/*
		AL=Page Number
	*/
	emu_setactivedisplaypage(AL); //Set!
}

void int10_ScrollDownWindow_real(byte linestoscroll, byte backgroundcolor, byte page, byte x1, byte x2, byte y1, byte y2)
{
	int x; //Current x!
	int y; //Current y!
	int c; //Rows scrolled!
	byte oldchar;
	byte oldattr;
	int rowstoclear;
	rowstoclear = linestoscroll; //Default!
	if (linestoscroll==0)
	{
		rowstoclear = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_ROWS,0); /* Clear all! */
	}
	for (c=0; c<rowstoclear; c++) //Process!
	{
		for (y=y1; y<=y2; y++) //Rows!
		{
			for (x=x1; x<x2; x++) //Columns!
			{
				oldchar = 0;
				oldattr = backgroundcolor; //Init to off-screen!
				if (linestoscroll!=0) //Get from coordinates?
				{
					if (y<y2) //Not at bottom (bottom fill empty)?
					{
						int10_vram_readcharacter(x,y+1,page,&oldchar,&oldattr); //Use character below this one!
					}
				}
				int10_vram_writecharacter(x,y,page,oldchar,oldattr); //Set our character!
			}
		}
	}
}

void int10_ScrollDownWindow() //Top off screen is lost, bottom goes up.
{
	/*
		AL=Lines to scroll (0=clear: CH,CL,DH,DL are used)
		BH=Background color

		CH=Upper row number
		DH=Lower row number
		CL=Left column number
		DL=Right column number
	*/
	int10_ScrollDownWindow_real(AL,BH,emu_getdisplaypage(),CH,CL,DH,DL); //Scroll down this window!
}

void int10_ScrollUpWindow() //Bottom off screen is lost, top goes down.
{
	/*
		AL=Lines to scroll (0=clear; CH,CL,DH,DL are used)
		BH=Background Color

		CH=Upper row number
		DH=Lower row number
		CL=Left column number
		DL=Right column number
	*/
	int x; //Current x!
	int y; //Current y!
	int c; //Rows scrolled!
	byte oldchar;
	byte oldattr;
	int rowstoclear;
	rowstoclear = AL; //Default!
	if (AL==0)
	{
		rowstoclear = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_ROWS,0); /* Clear all! */
	}
	for (c=0; c<rowstoclear; c++) //Process!
	{
		for (y=DH; y>=CH; y--) //Rows!
		{
			for (x=CL; x<DL; x++) //Columns!
			{
				oldchar = 0;
				oldattr = BH; //Init to off-screen!
				if (AL!=0) //Get from coordinates?
				{
					if (y>CH) //Not at top (top fill empty)?
					{
						int10_vram_readcharacter(x,y-1,emu_getdisplaypage(),&oldchar,&oldattr); //Use character above this one!
					}
				}
				int10_vram_writecharacter(x,y,emu_getdisplaypage(),oldchar,oldattr); //Clear!
			}
		}
	}
}










void int10_ReadCharAttrAtCursor()
{
	/*
	BH=Page Number

	Result:
	AH=Color
	AL=Character!
	*/
	int10_vram_readcharacter(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2),0),
				 MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2)+1,0),BH,&AL,&AH); //Read character AL font AH from page!
}

void int10_WriteCharAttrAtCursor()
{
	/*
	AL=Character
	BH=Page Number
	BL=Color
	CX=Number of times to print character
	*/
	while (CX--) //Times left?
	{
		int10_vram_writecharacter(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2),0),
					  MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2)+1,0),BH,AL,BL); //Write character AL font BL at page!
		int10_nextcol(BH); //Next column!
	}
}

void int10_WriteCharOnlyAtCursor()
{
	/*
	AL=Character
	BH=Page Number
	CX=Number of times to print character
	*/
	while (CX--)
	{
		byte oldchar = 0;
		byte oldattr = 0;
		byte x = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2),0);
		byte y = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2)+1,0);
		int10_vram_readcharacter(x,y,BH,&oldchar,&oldattr); //Get old info!
		int10_vram_writecharacter(x,y,BH,AL,oldattr); //Write character AL with old font at page!
		int10_nextcol(BH); //Next column!
	}
}



void int10_SetBackColor() //AH=0B BH=00h
{
	/*
	BL=Background/Border color (border only in text modes)
	*/
	
	PORT_IN_B(0x3DA); //Reset attribute controller!
	byte step7;
	step7 = PORT_IN_B(0x3C0); //Read and save original index!
	PORT_OUT_B(0x3C0,(step7&0x20)|0x11); //Goto index we need, leave toggle intact!
	PORT_OUT_B(0x3C0,BL); //Write the value to use!
	PORT_OUT_B(0x3C0,step7); //Restore the index!
	byte oldindex;
	oldindex = PORT_IN_B(0x3B4); //Read current CRTC index!
	PORT_OUT_B(0x3B4,0x24); //Flipflop register!
	if (!PORT_IN_B(0x3B5)&0x80) //Flipflop is to be reset?
	{
		PORT_IN_B(0x3DA); //Reset the flip-flop!
	}
	PORT_OUT_B(0x3B4,oldindex); //Restore CRTC index!
}

void int10_SetPalette() //AH=0B BH!=00h
{
	/*
	BL=Palette ID (was only valid in CGA, but newer cards support it in many or all graphics modes)
	*/
	INT10_SetColorSelect(BL); //Set the palette!
//???
}

void int10_Multi0B()
{
	if (!BH)
	{
		int10_SetBackColor();
	}
	else //All other cases = function 01h!
	{
		int10_SetPalette();
	}
}

void int10_PutPixel()
{
	/*
	GRAPHICS
	AL=Color
	BH=Page Number
	CX=x
	DX=y
	*/

	int ok;
	ok = GPU_putpixel(CX,DX,BH,AL); //Put the pixel, ignore result!
}

void int10_GetPixel()
{
	/*
	GRAPHICS
	BH=Page Number
	CX=x
	DX=y

	Returns:
	AL=Color
	*/

	int ok;
	ok = GPU_getpixel(CX,DX,BH,&AL); //Try to get the pixel, ignore result!
}

void int10_internal_outputchar(byte videopage, byte character, byte attribute)
{
	//dolog("interrupt10","Output character@%02X: %02X;attr=%02X;(%c)",videopage,character,attribute,character);
	//dolog("interrupt10","Total rows: %i",MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_ROWS,0));
	//dolog("interrupt10","Cursor pos: %i,%i",MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2),0),
	//					  MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2)+1,0)); //Show coordinates!
	switch (character) //What character?
	{
		//Control character?
	//case 0x00: //NULL?
	//	break;
	case 0x07: //Bell?
		//TODO BEEP
		break;
	case 0x08: //Backspace?
		if (MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2),0)>0)
		{
			MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2),MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2),0)-1); //Decrease only!
		}
		EMU_CPU_setCursorXY(videopage,
					MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2),0),
					MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2)+1,0)); //Refresh x,y of cursor!
		break;
	case 0x09: //Tab (8 horizontal, 6 vertical)?
		do
		{
			int10_nextcol(videopage); //Next column!
		}
		while (MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2),0)%8);   //Loop to next 8th position!
		break;
	case 0x0A: //LF?
		EMU_CPU_setCursorXY(videopage,
				    MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2),0), //Same X!
				    MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2)+1,0)+1 //Next row!
				    ); //Give x,y+1 of cursor!
		break;
	case 0x0B: //Vertical tab?
		do //Move some to the bottom!
		{
			int10_internal_outputchar(videopage,0x0A,attribute); //Next row!
		}
		while (MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2)+1,0)%6);   //Loop to next 6th row!
		break;
	case 0x0D: //CR?
		EMU_CPU_setCursorXY(videopage,0,MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2)+1,0)); //Give 0,y of cursor!
		break;
	default: //Normal character?
		int10_vram_writecharacter(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2),0),
					  MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2)+1,0),
					  videopage,character,attribute); //Write character & font at page!
		int10_nextcol(videopage); //Next column!
		break;
	}
	byte maxrows = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_ROWS,0); //Maximum number of rows!
	if (maxrows) //Have max?
	{
		while (MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2)+1,0)>=maxrows) //Past limit: scroll one down!
		{
			byte currow = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2)+1,0); //Current row!
			switch (MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,0)) //Active video mode?
			{
			case 2:
			case 3:
			case 6:
			case 7:
			case 0:
			case 1:
				int10_ScrollDownWindow_real(1,0,videopage,0,0,MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,0),
										  MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_ROWS,0)); //XxY rows?
				MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,
					BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(videopage*2)+1, //Row=...
					currow-1 //One row up!
					);
				break;
			default: //Not supported: graphics mode?
				break;
			}
		}
	}
}

void int10_TeleTypeOutput()
{
	/*
	AL=Character
	BH=Page Number
	BL=Color (only in graphic mode)
	*/
	int10_internal_outputchar(BH,AL,BL); //Output&update!
}

void int10_GetCurrentVideoMode()
{
	/*
	Returns:
	AL=Video Mode
	*/
	AL = GPUgetvideomode(); //Give video mode!
}

void int10_WriteString()
{
	/*
	AL=Write mode
	BH=Page Number
	BL=Color
	CX=String length
	DH=Row
	DL=Column
	ES:BP=Offset of string
	*/
	byte c;
	byte x;
	byte y;

	word len; //Length!
	len = CX; //The length of the string!
	word cur=0; //Current value!

	x = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2),0); //Old x!
	y = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2)+1,0); //Old y!

	while (len)
	{
		c = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_ES):-1,ES,BP+cur,0); //Read character from memory!
		int10_internal_outputchar(BH,c,BL); //Output&update!
		--len; //Next item!
	}

	if (!(AL&0x01)) //No Update cursor?
	{
		MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2),x); //Restore x!
		MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(BH*2)+1,y); //Restore y!
	}
}



//Extra: EGA/VGA functions:

void int10_Pallette() //AH=10h,AL=subfunc
{
    byte index; //Index of the pallette!
    byte data[17]; //Full data for the list!
	switch (AL) //Subfunc?
	{
	case 0x00: //Set one pallette register?
            int10_VGA->registers->AttributeControllerRegisters.REGISTERS.PALETTEREGISTERS[BL&0xF].DATA = BH; //Set palette register!
            VGA_calcprecalcs(int10_VGA,WHEREUPDATED_ATTRIBUTECONTROLLER|(BL&0xF)); //Update precalcs!
            break;
	case 0x01: //Set overscan/border color?
            int10_VGA->registers->AttributeControllerRegisters.REGISTERS.OVERSCANCOLORREGISTER = BH; //Set!
            break;
	case 0x02: //Set all pallette registers&border color?
            //ES:DX=List (byte 0-15=Pallete registers, 16=Border color register))
            for (index=0;index<18;index++)
            {
                data[index] = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,ES,DX+index,0); //Move to our buffer!
            }
            for (index=0;index<=0x10;index++) //Process all!
            {
                int10_VGA->registers->AttributeControllerRegisters.REGISTERS.PALETTEREGISTERS[BL&0xF].DATA = data[index]; //Set palette register!
            }
            int10_VGA->registers->AttributeControllerRegisters.REGISTERS.OVERSCANCOLORREGISTER = data[0x10]; //Set!
            VGA_calcprecalcs(int10_VGA,WHEREUPDATED_ATTRIBUTECONTROLLER); //Update full attribute precalcs!
            break;
	case 0x03: //Select foreground blink or bold background?
	case 0x07: //Read one palette register?
	case 0x08: //Read overscan/border color?
	case 0x09: //Read all pallet registers & border color?
			AL = 0; //Unknown command!
            break;
	case 0x10: //Set one DAC color register?
                {
                    DACEntry entry;
                    entry.r = DH;
                    entry.g = CH;
                    entry.b = CL;
                    writeDAC(int10_VGA,(BX&0xFF),&entry); //Write the entry!
                }
            break;
	case 0x12: //Set block of DAC color registers?
	case 0x13: //DAC color paging functions?
	case 0x15: //Read one DAC color register?
	case 0x17: //Read block of DAC color registers?
	case 0x1A: //Query DAC color paging state?
	case 0x1B: //Convert DAC colors to grey scale?
		AL = 0; //Unknown command!
		break;
	default: //Unknown subfunction?
		AL = 0; //Unknown command!
		break;
	}
}

void int10_CharacterGenerator() //AH=11h,AL=subfunc
{
	AH = 0xFF; //Not supported!
	return; //We're disabled!
	switch (AL) //Subfunc?
	{
	case 0x00: //Load user-defined font?
	case 0x10: //Load and activate user-defined font?
		int10_LoadFont(ES,BP,AL==0x10,CX,DX,BL,BH);
		break;
	case 0x01: //Load ROM 8x14 font?
	case 0x11: //Load and activate ROM 8x14 font?
		//int10_LoadFontSystem(
		break;
	case 0x02: //Load ROM 8x8 font?
	case 0x12: //Load and activate ROM 8x8 font?
		break;
	case 0x03: //Activate font block; 512-character set?
		break;
	case 0x04: //Load ROM 8x16 font?
		break;
		break;
		break;
		break;
	case 0x13: //Load and activate ROM 8x16 font?
		break;
	case 0x20: //Setup INT 1Fh graphics font pointer?
	case 0x21: //Setup user-defined font for graphics?
		break;
	case 0x22: //ROM 8x14 font for graphics modes?
		break;
	case 0x23: //ROM 8x8 font for graphics modes?
		break;
	case 0x24: //ROM 8x16 font for graphics modes?
		break;
	case 0x30: //Get video font information?
		break;
	default: //Unknown subfunction?
		AL = 0; //Unknown subfunction!
		break;
	}
}

void int10_SpecialFunctions() //AH=12h
{
	switch (BL) //Subfunction?
	{
	case 0x10: //get EGA info?
	case 0x20: //use alternate print screen?
	case 0x30: //set text-mode scan-lines?
	case 0x31: //enable default pallette loading?
	case 0x32: //enable access to video?
	case 0x33: //enable gray-scale summing?
	case 0x34: //enable cursor emulation?
	case 0x35: //PS/2 display switching?
		AL = 0; //Unknown command!
		break; //Unimplemented?
	case 0x36: //Screen refresh on/off?
		switch (AL) //Status to switch to?
		{
		case 0x00: //Enable refresh?
			AH = 0x12; //Correct!
			int10_VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.ScreenDisable = 0; //bit 5 of VGA's Sequencer Clocking mode Register off for enable!
			break;
		case 0x01: //Disable refresh?
			AH = 0x12; //Correct!
			int10_VGA->registers->SequencerRegisters.REGISTERS.CLOCKINGMODEREGISTER.ScreenDisable = 1; //bit 5 of VGA's Sequencer Clocking mode Register on for disable!
			break;
		default: //Unknown status?
			AH = 0; //Error: unknown data?
			break;
		}
		break;
	default: //Unknown subfunction?
		AL = 0; //Unknown subfunction!
		break;
	}
}

void int10_DCC() //AH=1Ah
{
	AX = 0; //Unknown command!
}

void int10_FuncStatus() //AH=1Bh
{
	AX = 0; //Unknown command!
}

void int10_SaveRestoreVideoStateFns() //AH=1Ch
{
	AX = 0; //Unknown command!
}

//AH=4Fh,AL=subfunc; SVGA support?













byte int2hex(byte b)
{
	byte translatetable[0x10] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'}; //The Hexdecimal notation!
	return translatetable[b&0xF];
}

byte bytetonum(byte b, byte nibble)
{
	if (!nibble) return int2hex(b&0xF); //Low nibble!
	return int2hex((b>>4)&0xF); //High nibble!
}

void writehex(FILE *f, byte num) //Write a number (byte) to a file!
{
	byte low = bytetonum(num,0); //Low!
	byte high = bytetonum(num,1); //High!
	fwrite(&high,1,sizeof(high),f); //High!
	fwrite(&low,1,sizeof(low),f); //High!
}





void int10_dumpscreen() //Dump screen to file!
{
	if (__HW_DISABLED) return; //Abort!
	int x;
	int y;
	FILE *f;
	int firstrow = 1;
	f = fopen("INT10.TXT","w"); //Open file!
	byte displaypage;
	displaypage = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0); //Active video page!

	writehex(f,MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,0)); //Current display mode!
	writehex(f,displaypage); //Write the display page first!
	writehex(f,getscreenwidth(displaypage)); //Screen width!

	char lb[2];
	bzero(lb,sizeof(lb));
	strcpy(lb,"\r\n"); //Line break!

	fwrite(&lb,1,safe_strlen(lb,sizeof(lb)),f); //Write a line break first!

	for (y=0; y<MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_ROWS,0); y++) //Process rows!
	{
		if (!firstrow)
		{
			fwrite(&lb,1,safe_strlen(lb,sizeof(lb)),f); //Line break!
		}
		else
		{
			firstrow = 0; //Reset!
		}
		for (x=0; x<getscreenwidth(displaypage); x++) //Process columns!
		{
			byte c,a; //Character&attribute!
			int10_vram_readcharacter(x,y,0,&c,&a);
			fwrite(&c,1,sizeof(c),f); //The character at the page!
		}
	}
	fclose(f); //Close the file!
}























void int10_refreshscreen() //Refresh a text-screen to debug screen on PSP!
{
//No debug screen!
}

Handler int10functions[] =
{
	int10_SetVideoMode, //00
	int10_SetTextModeCursorShape, //01
	int10_SetCursorPosition, //02
	int10_GetCursorPositionAndSize, //03
	int10_ReadLightPenPosition, //04
	int10_SelectActiveDisplayPage, //05
	int10_ScrollUpWindow, //06
	int10_ScrollDownWindow, //07
	int10_ReadCharAttrAtCursor, //08
	int10_WriteCharAttrAtCursor, //09
	int10_WriteCharOnlyAtCursor, //0A
	int10_Multi0B, //0B
	int10_PutPixel, //0C
	int10_GetPixel, //0D
	int10_TeleTypeOutput, //0E
	int10_GetCurrentVideoMode, //0F
	int10_Pallette, //10
	int10_CharacterGenerator, //11
	int10_SpecialFunctions, //12
	int10_WriteString //13
	/*
	,NULL, //14
	NULL, //15
	NULL, //16
	NULL, //17
	NULL, //18
	NULL, //19
	int10_DCC, //1A
	int10_FuncStatus, //1B
	int10_SaveRestoreVideoStateFns, //1C
	*/ //VGA functions. Disabled for now!
}; //Function list!

void init_int10() //Initialises int10&VGA for usage!
{
	//if (__HW_DISABLED) return; //Abort!
	
	INT10_SetupRomMemory(); //Initialise the VGA ROM memory if not initialised yet!
	
	int10_VGA = getActiveVGA(); //Use the active VGA for int10 by default!
	
//Initialise variables!
	//dolog("int10","Switching video mode to mode #0...");
	GPUswitchvideomode(0); //Init video mode #0!
	//dolog("int10","Ready.");
}

void initint10() //Fully initialise interrupt 10h!
{
	int10loaded = TRUE; //Interrupt presets loaded!
	init_int10(); //Initialise!
}

void BIOS_int10() //Handler!
{
	if (!int10loaded) //First call?
	{
		initint10();
	}

	if (__HW_DISABLED) return; //Disabled!
//Now, handle the interrupt!

//First, function protection!

	int dohandle = 0;
	dohandle = (AH<NUMITEMS(int10functions)); //handle?

	if (!dohandle) //Not within list to execute?
	{
		AX = 0; //Break!
	}
	else //To handle?
	{
		if (int10functions[AH]!=NULL) //Set?
		{
			int10functions[AH](); //Run the function!
		}
		else
		{
			AL = 0; //Error: unknown command!
		}
	}
}