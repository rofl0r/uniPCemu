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
#include "headers/cpu/cb_manager.h" //Callback detection!

#include "headers/cpu/80286/protection.h" //For CPU_segment_index!

//Are we disabled for checking?
#define __HW_DISABLED 0

//Text screen height is always 25!

extern GPU_type GPU; //GPU info&adjusting!

int int10loaded = 0; //Default: not loaded yet!

//Screencontents: 0xB800:(row*0x0040:004A)+column*2)
//Screencolorsfont: 0xB800:((row*0x0040:004A)+column*2)+1)
//Screencolorsback: Same as font, high nibble!

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

OPTINLINE void GPU_setresolution(word mode) //Sets the resolution based on current video mode byte!
{
	GPU.showpixels = ALLOW_GPU_GRAPHICS; //Show pixels!

	//Now all BIOS data!
	//dolog("emu","Setting int10 video mode...");
	INT10_Internal_SetVideoMode(mode); //Switch video modes!
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

OPTINLINE byte GPUgetvideomode()
{
	if (__HW_DISABLED) return 0; //Abort!
	return MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,0); //Give mode!
}

OPTINLINE void GPUswitchvideomode(word mode)
{
	GPU_setresolution(mode); //Set the resolution to use&rest data!
}


OPTINLINE int GPU_getpixel(int x, int y, byte page, byte *pixel) //Get a pixel from the real emulated screen buffer!
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
                        RealPt off=RealMake(0xa000,MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,0)*page+
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
        /*case M_LIN8: {
                        //if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)
                        //        LOG(LOG_INT10,LOG_ERROR)("GetPixel_VGA_w: %x!=%x",CurMode->swidth,real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
                        RealPt off=RealMake(S3_LFB_BASE,y*real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8+x);
                        *color = mem_readb(off);
                        break;
                }*/
        default:
                //LOG(LOG_INT10,LOG_ERROR)("GetPixel unhandled mode type %d",CurMode->type);
            return 0; //Error: unknown mode!
                break;
        }
        return 1; //OK!
}

OPTINLINE int GPU_putpixel(int x, int y, byte page, byte color) //Writes a video buffer pixel to the real emulated screen buffer
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
										Bit16u off = (y >> 2) * 160 + ((x >> 2)&(~1));
                                        off+=(8*1024) * (y & 3);

                                        Bit16u old=real_readw(0xb800,off);
                                        if (color & 0x80) {
                                                old^=(color&1) << (7-(x&7));
                                                old^=((color&2)>>1) << ((7-(x&7))+8);
                                        } else {
											old = (old&(~(0x101 << (7 - (x & 7))))) | ((color & 1) << (7 - (x & 7))) | (((color & 2) >> 1) << ((7 - (x & 7)) + 8));
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
                        RealPt off=RealMake(0xa000,real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE)*page+
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
        /*case M_LIN8: {
                        //if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)
                        //        LOG(LOG_INT10,LOG_ERROR)("PutPixel_VGA_w: %x!=%x",CurMode->swidth,real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
                        RealPt off=RealMake(S3_LFB_BASE,y*real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8+x);
                        mem_writeb(off,color);
                        break;
                }*/
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

OPTINLINE void ResetACTL() {
	if (__HW_DISABLED) return; //Abort!
	IO_Read(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS) + 6);
}

OPTINLINE void INT10_SetSinglePaletteRegister(Bit8u reg,Bit8u val) {
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

OPTINLINE void INT10_SetOverscanBorderColor(Bit8u val) {
	/*switch (machine) {
	case TANDY_ARCH_CASE:
		IO_Read(VGAREG_TDY_RESET);
		WriteTandyACTL(0x02,val);
		break;
	case EGAVGA_ARCH_CASE:*/
		ResetACTL();
		IO_Write(VGAREG_ACTL_ADDRESS,0x11);
		IO_Write(VGAREG_ACTL_WRITE_DATA,val);
		IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
	/*	break;
	}*/
}

OPTINLINE void INT10_SetAllPaletteRegisters(PhysPt data) {
	/*switch (machine) {
	case TANDY_ARCH_CASE:
		IO_Read(VGAREG_TDY_RESET);
		// First the colors
		for(Bit8u i=0;i<0x10;i++) {
			WriteTandyACTL(i+0x10,mem_readb(data));
			data++;
		}
		// Then the border
		WriteTandyACTL(0x02,mem_readb(data));
		break;
	case EGAVGA_ARCH_CASE:*/
		ResetACTL();
		// First the colors
		Bit8u i;
		for(i=0;i<0x10;i++) {
			IO_Write(VGAREG_ACTL_ADDRESS,i);
			IO_Write(VGAREG_ACTL_WRITE_DATA,phys_readb(data));
			data++;
		}
		// Then the border
		IO_Write(VGAREG_ACTL_ADDRESS,0x11);
		IO_Write(VGAREG_ACTL_WRITE_DATA,phys_readb(data));
		IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
		/*break;
	}*/
}

OPTINLINE void INT10_SetColorSelect(Bit8u val) {
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

OPTINLINE void INT10_ToggleBlinkingBit(Bit8u state) {
	Bit8u value;
//	state&=0x01;
	//if ((state>1) && (svgaCard==SVGA_S3Trio)) return;
	ResetACTL();
	
	IO_Write(VGAREG_ACTL_ADDRESS,0x10);
	value=IO_Read(VGAREG_ACTL_READ_DATA);
	if (state<=1) {
		value&=0xf7;
		value|=state<<3;
	}

	ResetACTL();
	IO_Write(VGAREG_ACTL_ADDRESS,0x10);
	IO_Write(VGAREG_ACTL_WRITE_DATA,value);
	IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette

	if (state<=1) {
		Bit8u msrval=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR)&0xdf;
		if (state) msrval|=0x20;
		real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_MSR,msrval);
	}
}

OPTINLINE void INT10_GetSinglePaletteRegister(Bit8u reg,Bit8u * val) {
	if(reg<=ACTL_MAX_REG) {
		ResetACTL();
		IO_Write(VGAREG_ACTL_ADDRESS,reg+32);
		*val=IO_Read(VGAREG_ACTL_READ_DATA);
		IO_Write(VGAREG_ACTL_WRITE_DATA,*val);
	}
}

OPTINLINE void INT10_GetOverscanBorderColor(Bit8u * val) {
	ResetACTL();
	IO_Write(VGAREG_ACTL_ADDRESS,0x11+32);
	*val=IO_Read(VGAREG_ACTL_READ_DATA);
	IO_Write(VGAREG_ACTL_WRITE_DATA,*val);
}

OPTINLINE void INT10_GetAllPaletteRegisters(PhysPt data) {
	ResetACTL();
	// First the colors
	Bit8u i;
	for(i=0;i<0x10;i++) {
		IO_Write(VGAREG_ACTL_ADDRESS,i);
		phys_writeb(data,IO_Read(VGAREG_ACTL_READ_DATA));
		ResetACTL();
		data++;
	}
	// Then the border
	IO_Write(VGAREG_ACTL_ADDRESS,0x11+32);
	phys_writeb(data,IO_Read(VGAREG_ACTL_READ_DATA));
	ResetACTL();
}

OPTINLINE void updateCursorLocation()
{
	if (__HW_DISABLED) return; //Abort!
	int x;
	int y;
	x = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0)*2),0); //X
	y = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0)*2)+1,0); //Y
	word address; //Address of the cursor location!
	address = MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_START,0)+
			(y*MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,0))+(x<<1);

	byte oldcrtc = PORT_IN_B(0x3D4); //Save old address!
	PORT_OUT_B(0x3D4,0xF); //Select cursor location low register!
	PORT_OUT_B(0x3D5,address&0xFF); //Low location!
	PORT_OUT_B(0x3D4,0xE); //Select cursor location high register!
	PORT_OUT_B(0x3D5,((address>>8)&0xFF)); //High location!
	PORT_OUT_B(0x3D4,oldcrtc); //Restore old CRTC register!
}

OPTINLINE void EMU_CPU_setCursorXY(byte displaypage, byte x, byte y)
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

OPTINLINE void EMU_CPU_getCursorScanlines(byte *start, byte *end)
{
	if (__HW_DISABLED) return; //Abort!
	*start = MMU_rb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE, 0); //Get start line!
	*end = MMU_rb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE + 1, 0); //Get end line!
}

OPTINLINE void EMU_CPU_setCursorScanlines(byte start, byte end)
{
	if (__HW_DISABLED) return; //Abort!
	byte oldcrtc = PORT_IN_B(0x3D4); //Save old address!

	start &= 0x3F; //Take our usable data only!
	end &= 0x1F; //Take our usable data only!

	PORT_OUT_B(0x3D4,0xA); //Select start register!
	byte cursorStart = PORT_IN_B(0x3D5); //Read current cursor start!

	cursorStart &= ~0x3F; //Clear our data location!
	cursorStart |= start; //Add the usable data!
	PORT_OUT_B(0x3D5,start); //Start!

	//Process cursor end!
	PORT_OUT_B(0x3D4,0xB); //Select end register!
	byte cursorEnd = PORT_IN_B(0x3D5); //Read old end!

	cursorEnd &= ~0x1F; //Clear our data location!
	cursorEnd |= end; //Create the cursor end data!

	PORT_OUT_B(0x3D5,end); //Write new cursor end!
	
	PORT_OUT_B(0x3D4,oldcrtc); //Restore old CRTC register address!

	start &= 0x1F; //Take our usable data only!

	//Update our values!
	MMU_wb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE, start); //Set start line!
	MMU_wb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE + 1, end); //Set end line!
}

OPTINLINE void GPU_clearscreen() //Clears the screen!
{
	if (__HW_DISABLED) return; //Abort!
	byte oldmode;
	oldmode = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,0); //Active video mode!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,oldmode&0x7F); //Clear!
	GPU_setresolution(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,0)); //Reset the resolution!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_MODE,oldmode); //Restore old mode!
}

OPTINLINE void GPU_clearscreen_BIOS() //Clears the screen for BIOS menus etc.!
{
	if (__HW_DISABLED) return; //Abort!
	GPU_clearscreen(); //Forward: we're using official VGA now!
}

OPTINLINE void int10_nextcol(byte thepage)
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

OPTINLINE void int10_vram_writecharacter(byte x, byte y, byte page, byte character, byte attribute) //Write character+attribute!
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
			address += (page*MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,0)); //Start of page!
			address += (((y*MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,0))+x)<<1); //Character offset within page!
			MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,where,address,character); //The character! Write plane 0!
			MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,where,address+1,attribute); //The attribute! Write plane 1!
			return; //Done!
		}
		break;
	default:
		break; //Do nothing: unsupported yet!
	}
}

OPTINLINE void int10_vram_readcharacter(byte x, byte y, byte page, byte *character, byte *attribute) //Read character+attribute!
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
			address += (((y*MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_COLS,0))+x)<<1); //Character offset within page!
			*character = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,where,address,0); //The character!
			*attribute = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,where,address+1,0); //The attribute!
		}
		break;
	default:
		break; //Do nothing: unsupported yet!
	}
}

OPTINLINE void emu_setactivedisplaypage(byte page) //Set active display page!
{
	if (__HW_DISABLED) return; //Abort!
	MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,page); //Active video page!
	MMU_ww(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_START,page*MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_PAGE_SIZE,0)); //Display page offset!
//Now for the VGA!

	byte oldcrtc = PORT_IN_B(0x3D4); //Save old address!
	PORT_OUT_B(0x3D4,0xE); //Select high register!
	PORT_OUT_B(0x3D5,((MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_START,0)>>8)&0xFF));  //High!
	PORT_OUT_B(0x3D4,0xF); //Select low register!
	PORT_OUT_B(0x3D5,(MMU_rw(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_START,0)&0xFF)); //Low!
	PORT_OUT_B(0x3D4,oldcrtc); //Restore old CRTC register!
}

OPTINLINE byte emu_getdisplaypage()
{
	if (__HW_DISABLED) return 0; //Abort!
	return MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE,0); //Active page!
}

/*

Font generator support!

*/


OPTINLINE void int10_ActivateFontBlocks(byte selector) //Activate a font!
{
	if (__HW_DISABLED) return; //Abort!
	IO_Write(0x3c4,0x03); //Character map select register!
	IO_Write(0x3c5,selector); //Activate the font blocks, according to the user!
}

word map_offset[8] = {0x0000,0x4000,0x8000,0xB000,0x2000,0x6000,0xA000,0xE000}; //Where do we map to?

OPTINLINE void int10_LoadFont(word segment, uint_32 offset, //font in dosbox!
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

OPTINLINE void int10_LoadFontSystem(byte *data, //font in dosbox!
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

OPTINLINE void INT10_SetSingleDacRegister(Bit8u index,Bit8u red,Bit8u green,Bit8u blue) {
	IO_Write(VGAREG_DAC_WRITE_ADDRESS,(Bit8u)index);
	IO_Write(VGAREG_DAC_DATA,red);
	IO_Write(VGAREG_DAC_DATA,green);
	IO_Write(VGAREG_DAC_DATA,blue);
}

OPTINLINE void INT10_GetSingleDacRegister(Bit8u index,Bit8u * red,Bit8u * green,Bit8u * blue) {
	IO_Write(VGAREG_DAC_READ_ADDRESS,index);
	*red=IO_Read(VGAREG_DAC_DATA);
	*green=IO_Read(VGAREG_DAC_DATA);
	*blue=IO_Read(VGAREG_DAC_DATA);
}

OPTINLINE void INT10_SetDACBlock(Bit16u index,Bit16u count,PhysPt data) {
 	IO_Write(VGAREG_DAC_WRITE_ADDRESS,(Bit8u)index);
	for (;count>0;count--) {
		IO_Write(VGAREG_DAC_DATA,phys_readb(data++));
		IO_Write(VGAREG_DAC_DATA,phys_readb(data++));
		IO_Write(VGAREG_DAC_DATA,phys_readb(data++));
	}
}

OPTINLINE void INT10_GetDACBlock(Bit16u index,Bit16u count,PhysPt data) {
 	IO_Write(VGAREG_DAC_READ_ADDRESS,(Bit8u)index);
	for (;count>0;count--) {
		phys_writeb(data++,IO_Read(VGAREG_DAC_DATA));
		phys_writeb(data++,IO_Read(VGAREG_DAC_DATA));
		phys_writeb(data++,IO_Read(VGAREG_DAC_DATA));
	}
}

OPTINLINE void INT10_SelectDACPage(Bit8u function,Bit8u mode) {
	ResetACTL();
	IO_Write(VGAREG_ACTL_ADDRESS,0x10);
	Bit8u old10=IO_Read(VGAREG_ACTL_READ_DATA);
	if (!function) {		//Select paging mode
		if (mode) old10|=0x80;
		else old10&=0x7f;
		//IO_Write(VGAREG_ACTL_ADDRESS,0x10);
		IO_Write(VGAREG_ACTL_WRITE_DATA,old10);
	} else {				//Select page
		IO_Write(VGAREG_ACTL_WRITE_DATA,old10);
		if (!(old10 & 0x80)) mode<<=2;
		mode&=0xf;
		IO_Write(VGAREG_ACTL_ADDRESS,0x14);
		IO_Write(VGAREG_ACTL_WRITE_DATA,mode);
	}
	IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
}

OPTINLINE void INT10_GetDACPage(Bit8u* mode,Bit8u* page) {
	ResetACTL();
	IO_Write(VGAREG_ACTL_ADDRESS,0x10);
	Bit8u reg10=IO_Read(VGAREG_ACTL_READ_DATA);
	IO_Write(VGAREG_ACTL_WRITE_DATA,reg10);
	*mode=(reg10&0x80)?0x01:0x00;
	IO_Write(VGAREG_ACTL_ADDRESS,0x14);
	*page=IO_Read(VGAREG_ACTL_READ_DATA);
	IO_Write(VGAREG_ACTL_WRITE_DATA,*page);
	if(*mode) {
		*page&=0xf;
	} else {
		*page&=0xc;
		*page>>=2;
	}
}

OPTINLINE void INT10_SetPelMask(Bit8u mask) {
	IO_Write(VGAREG_PEL_MASK,mask);
}	

OPTINLINE void INT10_GetPelMask(Bit8u *mask) {
	*mask=IO_Read(VGAREG_PEL_MASK);
}	

OPTINLINE void INT10_SetBackgroundBorder(Bit8u val) {
	Bit8u temp=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL);
	temp=(temp & 0xe0) | (val & 0x1f);
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,temp);
	/*if (machine == MCH_CGA || IS_TANDY_ARCH)
		IO_Write(0x3d9,temp);
	else if (IS_EGAVGA_ARCH) {*/
		val = ((val << 1) & 0x10) | (val & 0x7);
		/* Aways set the overscan color */
		INT10_SetSinglePaletteRegister( 0x11, val );
		/* Don't set any extra colors when in text mode */
		if (CurMode->mode <= 3)
			return;
		INT10_SetSinglePaletteRegister( 0, val );
		val = (temp & 0x10) | 2 | ((temp & 0x20) >> 5);
		INT10_SetSinglePaletteRegister( 1, val );
		val+=2;
		INT10_SetSinglePaletteRegister( 2, val );
		val+=2;
		INT10_SetSinglePaletteRegister( 3, val );
	//}
}



















void int10_SetVideoMode()
{
	/*
		AL=Video mode
		result: AL=Video mode flag/controller byte
	*/
	GPUswitchvideomode(REG_AL); //Switch the video mode!
}

void int10_SetTextModeCursorShape()
{
	/*
		CH=Scan row start
		CL=Scan row end
		If bit 5 is used on VGA: hide cursor; else determine by start>end.
	*/
	EMU_CPU_setCursorScanlines(REG_CH,REG_CL); //Set scanline start&end to off by making start higher than end!
}

void int10_SetCursorPosition()
{
	/*
		BH=Page Number
		DH=Row
		DL=Column
	*/
	//dolog("interrupt10","Gotoxy@%02X: %i,%i",REG_BH,REG_DL,REG_DH);
	cursorXY(REG_BH,REG_DL,REG_DH); //Goto x,y!
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
	REG_AX = 0;
	REG_DL = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(REG_BH*2),0); //Cursor x!
	REG_DH = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(REG_BH*2)+1,0); //Cursor y!
	EMU_CPU_getCursorScanlines(&REG_CH,&REG_CL); //Scan lines of the cursor!
	//dolog("interrupt10","GetCursorPositionAndSize:%i,%i; Size: %i-%i",REG_DL,REG_DH,REG_CH,REG_CL);
}

void int10_ReadLightPenPosition()
{
	//Not used on VGA systems!
	REG_AH = 0; //Invalid function!
}

void int10_SelectActiveDisplayPage()
{
	/*
		AL=Page Number
	*/
	emu_setactivedisplaypage(REG_AL); //Set!
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
	int10_ScrollDownWindow_real(REG_AL,REG_BH,emu_getdisplaypage(),REG_CH,REG_CL,REG_DH,REG_DL); //Scroll down this window!
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
	rowstoclear = REG_AL; //Default!
	if (REG_AL==0)
	{
		rowstoclear = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_NB_ROWS,0); /* Clear all! */
	}
	for (c=0; c<rowstoclear; c++) //Process!
	{
		for (y=REG_DH; y>=REG_CH; y--) //Rows!
		{
			for (x=REG_CL; x<REG_DL; x++) //Columns!
			{
				oldchar = 0;
				oldattr = REG_BH; //Init to off-screen!
				if (REG_AL!=0) //Get from coordinates?
				{
					if (y>REG_CH) //Not at top (top fill empty)?
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
	int10_vram_readcharacter(MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(REG_BH*2),0),
				 MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(REG_BH*2)+1,0),REG_BH,&REG_AL,&REG_AH); //Read character REG_AL font REG_AH from page!
}

void int10_WriteCharAttrAtCursor()
{
	/*
	AL=Character
	BH=Page Number
	BL=Color
	CX=Number of times to print character
	*/

	byte tempx,tempy;
	tempx = MMU_rb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_POS + (REG_BH * 2), 0); //Column!
	tempy = MMU_rb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_POS + (REG_BH * 2) + 1, 0); //Row!

	while (REG_CX--) //Times left?
	{
		int10_vram_writecharacter(
			MMU_rb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_POS + (REG_BH * 2), 0),
			MMU_rb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_POS + (REG_BH * 2) + 1, 0)
			, REG_BH, REG_AL, REG_BL); //Write character REG_AL font REG_BL at page!
		int10_nextcol(REG_BH); //Next column!
	}
	cursorXY(REG_BH, tempx, tempy); //Return the cursor to it's original position!
}

void int10_WriteCharOnlyAtCursor()
{
	/*
	AL=Character
	BH=Page Number
	CX=Number of times to print character
	*/

	word tempx, tempy;
	tempx = MMU_rb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_POS + (REG_BH * 2), 0); //Column!
	tempy = MMU_rb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_POS + (REG_BH * 2) + 1, 0); //Row!

	while (REG_CX--)
	{
		byte oldchar = 0;
		byte oldattr = 0;
		byte x = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(REG_BH*2),0);
		byte y = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(REG_BH*2)+1,0);
		int10_vram_readcharacter(x,y,REG_BH,&oldchar,&oldattr); //Get old info!
		int10_vram_writecharacter(x,y,REG_BH,REG_AL,oldattr); //Write character REG_AL with old font at page!
		int10_nextcol(REG_BH); //Next column!
	}

	cursorXY(REG_BH, (byte)tempx, (byte)tempy); //Return the cursor!
}

void int10_SetBackColor() //REG_AH=0B REG_BH=00h
{
	/*
	BL=Background/Border color (border only in text modes)
	*/
	
	PORT_IN_B(0x3DA); //Reset attribute controller!
	byte step7;
	step7 = PORT_IN_B(0x3C0); //Read and save original index!
	PORT_OUT_B(0x3C0,(step7&0x20)|0x11); //Goto index we need, leave toggle intact!
	PORT_OUT_B(0x3C0,REG_BL); //Write the value to use!
	PORT_OUT_B(0x3C0,step7); //Restore the index!
	byte oldindex;
	oldindex = PORT_IN_B(0x3B4); //Read current CRTC index!
	PORT_OUT_B(0x3B4,0x24); //Flipflop register!
	if (!(PORT_IN_B(0x3B5)&0x80)) //Flipflop is to be reset?
	{
		PORT_IN_B(0x3DA); //Reset the flip-flop!
	}
	PORT_OUT_B(0x3B4,oldindex); //Restore CRTC index!
}

void int10_SetPalette() //REG_AH=0B REG_BH!=00h
{
	/*
	BL=Palette ID (was only valid in CGA, but newer cards support it in many or all graphics modes)
	*/
	INT10_SetColorSelect(REG_BL); //Set the palette!
//???
}

void int10_Multi0B()
{
	if (!REG_BH)
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

	GPU_putpixel(REG_CX,REG_DX,REG_BH,REG_AL); //Put the pixel, ignore result!
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

	GPU_getpixel(REG_CX,REG_DX,REG_BH,&REG_AL); //Try to get the pixel, ignore result!
}

OPTINLINE void int10_internal_outputchar(byte videopage, byte character, byte attribute)
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

	byte oldchar = 0;
	byte oldattr = 0;
	byte x = MMU_rb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_POS + (REG_BH * 2), 0);
	byte y = MMU_rb(CB_ISCallback() ? CPU_segment_index(CPU_SEGMENT_DS) : -1, BIOSMEM_SEG, BIOSMEM_CURSOR_POS + (REG_BH * 2) + 1, 0);
	int10_vram_readcharacter(x, y, REG_BH, &oldchar, &oldattr); //Get old info!
	int10_internal_outputchar(REG_BH, REG_AL, oldattr); //Write character REG_AL with old font at page!
}

void int10_GetCurrentVideoMode()
{
	/*
	Returns:
	AL=Video Mode
	*/
	REG_AL = GPUgetvideomode(); //Give video mode!
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
	len = REG_CX; //The length of the string!
	word cur=0; //Current value!

	x = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(REG_BH*2),0); //Old x!
	y = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(REG_BH*2)+1,0); //Old y!

	while (len)
	{
		c = MMU_rb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_ES):-1,REG_ES,REG_BP+cur,0); //Read character from memory!
		int10_internal_outputchar(REG_BH,c,REG_BL); //Output&update!
		--len; //Next item!
	}

	if (!(REG_AL&0x01)) //No Update cursor?
	{
		MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(REG_BH*2),x); //Restore x!
		MMU_wb(CB_ISCallback()?CPU_segment_index(CPU_SEGMENT_DS):-1,BIOSMEM_SEG,BIOSMEM_CURSOR_POS+(REG_BH*2)+1,y); //Restore y!
	}
}

//Extra: EGA/VGA functions:

void int10_Pallette() //REG_AH=10h,REG_AL=subfunc
{
	switch (REG_AL) {
		case 0x00:							/* SET SINGLE PALETTE REGISTER */
			INT10_SetSinglePaletteRegister(REG_BL,REG_BH);
			break;
		case 0x01:							/* SET BORDER (OVERSCAN) COLOR*/
			INT10_SetOverscanBorderColor(REG_BH);
			break;
		case 0x02:							/* SET ALL PALETTE REGISTERS */
			INT10_SetAllPaletteRegisters(Real2Phys(RealMake(REG_ES,REG_DX)));
			break;
		case 0x03:							/* TOGGLE INTENSITY/BLINKING BIT */
			INT10_ToggleBlinkingBit(REG_BL);
			break;
		case 0x07:							/* GET SINGLE PALETTE REGISTER */
			INT10_GetSinglePaletteRegister(REG_BL,&REG_BH);
			break;
		case 0x08:							/* READ OVERSCAN (BORDER COLOR) REGISTER */
			INT10_GetOverscanBorderColor(&REG_BH);
			break;
		case 0x09:							/* READ ALL PALETTE REGISTERS AND OVERSCAN REGISTER */
			INT10_GetAllPaletteRegisters(Real2Phys(RealMake(REG_ES,REG_DX)));
			break;
		case 0x10:							/* SET INDIVIDUAL DAC REGISTER */
			INT10_SetSingleDacRegister(REG_BL,REG_DH,REG_CH,REG_CL);
			break;
		case 0x12:							/* SET BLOCK OF DAC REGISTERS */
			INT10_SetDACBlock(REG_BX,REG_CX,Real2Phys(RealMake(REG_ES,REG_DX)));
			break;
		case 0x13:							/* SELECT VIDEO DAC COLOR PAGE */
			INT10_SelectDACPage(REG_BL,REG_BH);
			break;
		case 0x15:							/* GET INDIVIDUAL DAC REGISTER */
			INT10_GetSingleDacRegister(REG_BL,&REG_DH,&REG_CH,&REG_CL);
			break;
		case 0x17:							/* GET BLOCK OF DAC REGISTER */
			INT10_GetDACBlock(REG_BX,REG_CX,Real2Phys(RealMake(REG_ES,REG_DX)));
			break;
		case 0x18:							/* undocumented - SET PEL MASK */
			INT10_SetPelMask(REG_BL);
			break;
		case 0x19:							/* undocumented - GET PEL MASK */
			INT10_GetPelMask(&REG_BL);
			REG_BH=0;	// bx for get mask
			break;
		case 0x1A:							/* GET VIDEO DAC COLOR PAGE */
			INT10_GetDACPage(&REG_BL,&REG_BH);
			break;
		case 0x1B:							/* PERFORM GRAY-SCALE SUMMING */
			INT10_PerformGrayScaleSumming(REG_BX,REG_CX);
			break;
		default:
			//LOG(LOG_INT10,LOG_ERROR)("Function 10:Unhandled EGA/VGA Palette Function %2X",REG_AL);
			break;
	}
}

OPTINLINE uint_32 RealGetVec(byte interrupt)
{
	word segment, offset;
	CPU_getint(interrupt,&segment,&offset);
	return RealMake(segment,offset); //Give vector!
}

void int10_CharacterGenerator() //REG_AH=11h,REG_AL=subfunc
{
	switch (REG_AL) {
/* Textmode calls */
	case 0x00:			/* Load user font */
	case 0x10:
		INT10_LoadFont(REG_ES,REG_BP,REG_AL==0x10,REG_CX,REG_DX,REG_BL,REG_BH);
		break;
	case 0x01:			/* Load 8x14 font */
	case 0x11:
		INT10_LoadFont(RealSeg(int10.rom.font_14),RealOff(int10.rom.font_14),REG_AL==0x11,256,0,0,14);
		break;
	case 0x02:			/* Load 8x8 font */
	case 0x12:
		INT10_LoadFont(RealSeg(int10.rom.font_8_first),RealOff(int10.rom.font_8_first),REG_AL==0x12,256,0,0,8);
		break;
	case 0x03:			/* Set Block Specifier */
		IO_Write(0x3c4,0x3);IO_Write(0x3c5,REG_BL);
		break;
	case 0x04:			/* Load 8x16 font */
	case 0x14:
		if (!IS_VGA_ARCH) break;
		INT10_LoadFont(RealSeg(int10.rom.font_16),RealOff(int10.rom.font_16),REG_AL==0x14,256,0,0,16);
		break;
/* Graphics mode calls */
	case 0x20:			/* Set User 8x8 Graphics characters */
		RealSetVec(0x1f,REG_ES,REG_BP);
		break;
	case 0x21:			/* Set user graphics characters */
		RealSetVec(0x43,REG_ES,REG_BP);
		real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,REG_CX);
		goto graphics_chars;
	case 0x22:			/* Rom 8x14 set */
		RealSetVec(0x43,RealSeg(int10.rom.font_14),RealOff(int10.rom.font_14));
		real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,14);
		goto graphics_chars;
	case 0x23:			/* Rom 8x8 double dot set */
		RealSetVec(0x43,RealSeg(int10.rom.font_8_first),RealOff(int10.rom.font_8_first));
		real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,8);
		goto graphics_chars;
	case 0x24:			/* Rom 8x16 set */
		if (!IS_VGA_ARCH) break;
		RealSetVec(0x43,RealSeg(int10.rom.font_16),RealOff(int10.rom.font_16));
		real_writew(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT,16);
		goto graphics_chars;
graphics_chars:
		switch (REG_BL) {
		case 0x00:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,REG_DL-1);break;
		case 0x01:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,13);break;
		case 0x03:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,42);break;
		case 0x02:
		default:real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,24);break;
		}
		break;
/* General */
	case 0x30:/* Get Font Information */
		switch (REG_BH) {
		case 0x00:	/* interupt 0x1f vector */
			{
				RealPt int_1f=RealGetVec(0x1f);
				segmentWritten(CPU_SEGMENT_ES,RealSeg(int_1f),0);
				REG_BP=RealOff(int_1f);
			}
			break;
		case 0x01:	/* interupt 0x43 vector */
			{
				RealPt int_43=RealGetVec(0x43);
				segmentWritten(CPU_SEGMENT_ES,RealSeg(int_43),0);
				REG_BP=RealOff(int_43);
			}
			break;
		case 0x02:	/* font 8x14 */
			segmentWritten(CPU_SEGMENT_ES,RealSeg(int10.rom.font_14),0);
			REG_BP=RealOff(int10.rom.font_14);
			break;
		case 0x03:	/* font 8x8 first 128 */
			segmentWritten(CPU_SEGMENT_ES,RealSeg(int10.rom.font_8_first),0);
			REG_BP=RealOff(int10.rom.font_8_first);
			break;
		case 0x04:	/* font 8x8 second 128 */
			segmentWritten(CPU_SEGMENT_ES,RealSeg(int10.rom.font_8_second),0);
			REG_BP=RealOff(int10.rom.font_8_second);
			break;
		case 0x05:	/* alpha alternate 9x14 */
			if (!IS_VGA_ARCH) break;
			segmentWritten(CPU_SEGMENT_ES,RealSeg(int10.rom.font_14_alternate),0);
			REG_BP=RealOff(int10.rom.font_14_alternate);
			break;
		case 0x06:	/* font 8x16 */
			if (!IS_VGA_ARCH) break;
			segmentWritten(CPU_SEGMENT_ES,RealSeg(int10.rom.font_16),0);
			REG_BP=RealOff(int10.rom.font_16);
			break;
		case 0x07:	/* alpha alternate 9x16 */
			if (!IS_VGA_ARCH) break;
			segmentWritten(CPU_SEGMENT_ES,RealSeg(int10.rom.font_16_alternate),0);
			REG_BP=RealOff(int10.rom.font_16_alternate);
			break;
		default:
			//LOG(LOG_INT10,LOG_ERROR)("Function 11:30 Request for font %2X",REG_BH);	
			break;
		}
		if ((REG_BH<=7) /*|| (svgaCard==SVGA_TsengET4K)*/) {
			/*if (machine==MCH_EGA) {
				REG_CX=0x0e;
				REG_DL=0x18;
			} else {*/
				REG_CX=real_readw(BIOSMEM_SEG,BIOSMEM_CHAR_HEIGHT);
				REG_DL=real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS);
			//}
		}
		break;
	default:
		//LOG(LOG_INT10,LOG_ERROR)("Function 11:Unsupported character generator call %2X",REG_AL);
		break;
	}
}

void int10_SpecialFunctions() //REG_AH=12h
{
	switch (REG_BL) {
	case 0x10:							/* Get EGA Information */
		REG_BH=(real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS)==0x3B4);	
		REG_BL=3;	//256 kb
		REG_CL=real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES) & 0x0F;
		REG_CH=real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES) >> 4;
		break;
	case 0x20:							/* Set alternate printscreen */
		break;
	case 0x30:							/* Select vertical resolution */
		{   
			if (!IS_VGA_ARCH) break;
			//LOG(LOG_INT10,LOG_WARN)("Function 12:Call %2X (select vertical resolution)",reg_bl);
			/*if (svgaCard != SVGA_None) {
				if (REG_AL > 2) {
					REG_AL=0;		// invalid subfunction
					break;
				}
			}*/
			Bit8u modeset_ctl = real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL);
			Bit8u video_switches = real_readb(BIOSMEM_SEG,BIOSMEM_SWITCHES)&0xf0;
			switch(REG_AL) {
			case 0: // 200
				modeset_ctl &= 0xef;
				modeset_ctl |= 0x80;
				video_switches |= 8;	// ega normal/cga emulation
				break;
			case 1: // 350
				modeset_ctl &= 0x6f;
				video_switches |= 9;	// ega enhanced
				break;
			case 2: // 400
				modeset_ctl &= 0x6f;
				modeset_ctl |= 0x10;	// use 400-line mode at next mode set
				video_switches |= 9;	// ega enhanced
				break;
			default:
				modeset_ctl &= 0xef;
				video_switches |= 8;	// ega normal/cga emulation
				break;
			}
			real_writeb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL,modeset_ctl);
			real_writeb(BIOSMEM_SEG,BIOSMEM_SWITCHES,video_switches);
			REG_AL=0x12;	// success
			break;
		}
	case 0x31:							/* Palette loading on modeset */
		{   
			if (!IS_VGA_ARCH) break;
			//if (svgaCard==SVGA_TsengET4K) REG_AL&=1;
			if (REG_AL>1) {
				REG_AL=0;		//invalid subfunction
				break;
			}
			Bit8u temp = real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL) & 0xf7;
			if (REG_AL&1) temp|=8;		// enable if al=0
			real_writeb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL,temp);
			REG_AL=0x12;
			break;	
		}		
	case 0x32:							/* Video adressing */
		if (!IS_VGA_ARCH) break;
		//LOG(LOG_INT10,LOG_ERROR)("Function 12:Call %2X not handled",reg_bl);
		//if (svgaCard==SVGA_TsengET4K) REG_AL&=1;
		if (REG_AL>1) REG_AL=0;		//invalid subfunction
		else REG_AL=0x12;			//fake a success call
		break;
	case 0x33: /* SWITCH GRAY-SCALE SUMMING */
		{   
			if (!IS_VGA_ARCH) break;
			//if (svgaCard==SVGA_TsengET4K) REG_AL&=1;
			if (REG_AL>1) {
				REG_AL=0;
				break;
			}
			Bit8u temp = real_readb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL) & 0xfd;
			if (!(REG_AL&1)) temp|=2;		// enable if al=0
			real_writeb(BIOSMEM_SEG,BIOSMEM_MODESET_CTL,temp);
			REG_AL=0x12;
			break;	
		}		
	case 0x34: /* ALTERNATE FUNCTION SELECT (VGA) - CURSOR EMULATION */
		{   
			// bit 0: 0=enable, 1=disable
			if (!IS_VGA_ARCH) break;
			//if (svgaCard==SVGA_TsengET4K) REG_AL&=1;
			if (REG_AL>1) {
				REG_AL=0;
				break;
			}
			Bit8u temp = real_readb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL) & 0xfe;
			real_writeb(BIOSMEM_SEG,BIOSMEM_VIDEO_CTL,temp|REG_AL);
			REG_AL=0x12;
			break;	
		}		
	case 0x35:
		if (!IS_VGA_ARCH) break;
		//LOG(LOG_INT10,LOG_ERROR)("Function 12:Call %2X not handled",reg_bl);
		REG_AL=0x12;
		break;
	case 0x36: {						/* VGA Refresh control */
		if (!IS_VGA_ARCH) break;
		/*if ((svgaCard==SVGA_S3Trio) && (REG_AL>1)) {
			REG_AL=0;
			break;
		}*/
		IO_Write(0x3c4,0x1);
		Bit8u clocking = IO_Read(0x3c5);
		
		if (REG_AL==0) clocking &= ~0x20;
		else clocking |= 0x20;
		
		IO_Write(0x3c4,0x1);
		IO_Write(0x3c5,clocking);

		REG_AL=0x12; // success
		break;
	}
	default:
		//LOG(LOG_INT10,LOG_ERROR)("Function 12:Call %2X not handled",reg_bl);
		/*if (machine!=MCH_EGA)*/ REG_AL=0;
		break;
	}
}

void int10_DCC() //REG_AH=1Ah
{
	if (REG_AL==0) {	// get dcc
		// walk the tables...
		RealPt vsavept=real_readd(BIOSMEM_SEG,BIOSMEM_VS_POINTER);
		RealPt svstable=real_readd(RealSeg(vsavept),RealOff(vsavept)+0x10);
		if (svstable) {
			RealPt dcctable=real_readd(RealSeg(svstable),RealOff(svstable)+0x02);
			Bit8u entries=real_readb(RealSeg(dcctable),RealOff(dcctable)+0x00);
			Bit8u idx=real_readb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX);
			// check if index within range
			if (idx<entries) {
				Bit16u dccentry=real_readw(RealSeg(dcctable),RealOff(dcctable)+0x04+idx*2);
				if ((dccentry&0xff)==0) REG_BX=dccentry>>8;
				else REG_BX=dccentry;
			} else REG_BX=0xffff;
		} else REG_BX=0xffff;
		REG_AX=0x1A;	// high part destroyed or zeroed depending on BIOS
	} else if (REG_AL==1) {	// set dcc
		Bit8u newidx=0xff;
		// walk the tables...
		RealPt vsavept=real_readd(BIOSMEM_SEG,BIOSMEM_VS_POINTER);
		RealPt svstable=real_readd(RealSeg(vsavept),RealOff(vsavept)+0x10);
		if (svstable) {
			RealPt dcctable=real_readd(RealSeg(svstable),RealOff(svstable)+0x02);
			Bit8u entries=real_readb(RealSeg(dcctable),RealOff(dcctable)+0x00);
			if (entries) {
				Bitu ct;
				Bit16u swpidx=REG_BH|(REG_BL<<8);
				// search the ddc index in the dcc table
				for (ct=0; ct<entries; ct++) {
					Bit16u dccentry=real_readw(RealSeg(dcctable),RealOff(dcctable)+0x04+ct*2);
					if ((dccentry==REG_BX) || (dccentry==swpidx)) {
						newidx=(Bit8u)ct;
						break;
					}
				}
			}
		}

		real_writeb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX,newidx);
		REG_AX=0x1A;	// high part destroyed or zeroed depending on BIOS
	}
}

OPTINLINE Bitu INT10_VideoState_GetSize(Bitu state) {
	// state: bit0=hardware, bit1=bios data, bit2=color regs/dac state
	if ((state&7)==0) return 0;

	Bitu size=0x20;
	if (state&1) size+=0x46;
	if (state&2) size+=0x3a;
	if (state&4) size+=0x303;
	//if ((svgaCard==SVGA_S3Trio) && (state&8)) size+=0x43;
	if (size!=0) size=(size-1)/64+1;
	return size;
}

OPTINLINE bool INT10_VideoState_Save(Bitu state,RealPt buffer) {
	Bitu ct;
	if ((state&7)==0) return false;

	Bitu base_seg=RealSeg(buffer);
	Bitu base_dest=RealOff(buffer)+0x20;

	if (state&1)  {
		real_writew(base_seg,RealOff(buffer),base_dest);

		Bit16u crt_reg=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);
		real_writew(base_seg,base_dest+0x40,crt_reg);

		real_writeb(base_seg,base_dest+0x00,IO_ReadB(0x3c4));
		real_writeb(base_seg,base_dest+0x01,IO_ReadB(0x3d4));
		real_writeb(base_seg,base_dest+0x02,IO_ReadB(0x3ce));
		IO_ReadB(crt_reg+6);
		real_writeb(base_seg,base_dest+0x03,IO_ReadB(0x3c0));
		real_writeb(base_seg,base_dest+0x04,IO_ReadB(0x3ca));

		// sequencer
		for (ct=1; ct<5; ct++) {
			IO_WriteB(0x3c4,ct);
			real_writeb(base_seg,base_dest+0x04+ct,IO_ReadB(0x3c5));
		}

		real_writeb(base_seg,base_dest+0x09,IO_ReadB(0x3cc));

		// crt controller
		for (ct=0; ct<0x19; ct++) {
			IO_WriteB(crt_reg,ct);
			real_writeb(base_seg,base_dest+0x0a+ct,IO_ReadB(crt_reg+1));
		}

		// attr registers
		for (ct=0; ct<4; ct++) {
			IO_ReadB(crt_reg+6);
			IO_WriteB(0x3c0,0x10+ct);
			real_writeb(base_seg,base_dest+0x33+ct,IO_ReadB(0x3c1));
		}

		// graphics registers
		for (ct=0; ct<9; ct++) {
			IO_WriteB(0x3ce,ct);
			real_writeb(base_seg,base_dest+0x37+ct,IO_ReadB(0x3cf));
		}

		// save some registers
		IO_WriteB(0x3c4,2);
		Bit8u crtc_2=IO_ReadB(0x3c5);
		IO_WriteB(0x3c4,4);
		Bit8u crtc_4=IO_ReadB(0x3c5);
		IO_WriteB(0x3ce,6);
		Bit8u gfx_6=IO_ReadB(0x3cf);
		IO_WriteB(0x3ce,5);
		Bit8u gfx_5=IO_ReadB(0x3cf);
		IO_WriteB(0x3ce,4);
		Bit8u gfx_4=IO_ReadB(0x3cf);

		// reprogram for full access to plane latches
		IO_WriteW(0x3c4,0x0f02);
		IO_WriteW(0x3c4,0x0704);
		IO_WriteW(0x3ce,0x0406);
		IO_WriteW(0x3ce,0x0105);
		mem_writeb(0xaffff,0);

		for (ct=0; ct<4; ct++) {
			IO_WriteW(0x3ce,0x0004+ct*0x100);
			real_writeb(base_seg,base_dest+0x42+ct,mem_readb(0xaffff));
		}

		// restore registers
		IO_WriteW(0x3ce,0x0004|(gfx_4<<8));
		IO_WriteW(0x3ce,0x0005|(gfx_5<<8));
		IO_WriteW(0x3ce,0x0006|(gfx_6<<8));
		IO_WriteW(0x3c4,0x0004|(crtc_4<<8));
		IO_WriteW(0x3c4,0x0002|(crtc_2<<8));

		for (ct=0; ct<0x10; ct++) {
			IO_ReadB(crt_reg+6);
			IO_WriteB(0x3c0,ct);
			real_writeb(base_seg,base_dest+0x23+ct,IO_ReadB(0x3c1));
		}
		IO_WriteB(0x3c0,0x20);

		base_dest+=0x46;
	}

	if (state&2)  {
		real_writew(base_seg,RealOff(buffer)+2,base_dest);

		real_writeb(base_seg,base_dest+0x00,mem_readb(0x410)&0x30);
		for (ct=0; ct<0x1e; ct++) {
			real_writeb(base_seg,base_dest+0x01+ct,mem_readb(0x449+ct));
		}
		for (ct=0; ct<0x07; ct++) {
			real_writeb(base_seg,base_dest+0x1f+ct,mem_readb(0x484+ct));
		}
		real_writed(base_seg,base_dest+0x26,mem_readd(0x48a));
		real_writed(base_seg,base_dest+0x2a,mem_readd(0x14));	// int 5
		real_writed(base_seg,base_dest+0x2e,mem_readd(0x74));	// int 1d
		real_writed(base_seg,base_dest+0x32,mem_readd(0x7c));	// int 1f
		real_writed(base_seg,base_dest+0x36,mem_readd(0x10c));	// int 43

		base_dest+=0x3a;
	}

	if (state&4)  {
		real_writew(base_seg,RealOff(buffer)+4,base_dest);

		Bit16u crt_reg=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);

		IO_ReadB(crt_reg+6);
		IO_WriteB(0x3c0,0x14);
		real_writeb(base_seg,base_dest+0x303,IO_ReadB(0x3c1));

		Bitu dac_state=IO_ReadB(0x3c7)&1;
		Bitu dac_windex=IO_ReadB(0x3c8);
		if (dac_state!=0) dac_windex--;
		real_writeb(base_seg,base_dest+0x000,dac_state);
		real_writeb(base_seg,base_dest+0x001,dac_windex);
		real_writeb(base_seg,base_dest+0x002,IO_ReadB(0x3c6));

		for (ct=0; ct<0x100; ct++) {
			IO_WriteB(0x3c7,ct);
			real_writeb(base_seg,base_dest+0x003+ct*3+0,IO_ReadB(0x3c9));
			real_writeb(base_seg,base_dest+0x003+ct*3+1,IO_ReadB(0x3c9));
			real_writeb(base_seg,base_dest+0x003+ct*3+2,IO_ReadB(0x3c9));
		}

		IO_ReadB(crt_reg+6);
		IO_WriteB(0x3c0,0x20);

		base_dest+=0x303;
	}

	/*if ((svgaCard==SVGA_S3Trio) && (state&8))  {
		real_writew(base_seg,RealOff(buffer)+6,base_dest);

		Bit16u crt_reg=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);

		IO_WriteB(0x3c4,0x08);
//		Bitu seq_8=IO_ReadB(0x3c5);
		IO_ReadB(0x3c5);
//		real_writeb(base_seg,base_dest+0x00,IO_ReadB(0x3c5));
		IO_WriteB(0x3c5,0x06);	// unlock s3-specific registers

		// sequencer
		for (ct=0; ct<0x13; ct++) {
			IO_WriteB(0x3c4,0x09+ct);
			real_writeb(base_seg,base_dest+0x00+ct,IO_ReadB(0x3c5));
		}

		// unlock s3-specific registers
		IO_WriteW(crt_reg,0x4838);
		IO_WriteW(crt_reg,0xa539);

		// crt controller
		Bitu ct_dest=0x13;
		for (ct=0; ct<0x40; ct++) {
			if ((ct==0x4a-0x30) || (ct==0x4b-0x30)) {
				IO_WriteB(crt_reg,0x45);
				IO_ReadB(crt_reg+1);
				IO_WriteB(crt_reg,0x30+ct);
				real_writeb(base_seg,base_dest+(ct_dest++),IO_ReadB(crt_reg+1));
				real_writeb(base_seg,base_dest+(ct_dest++),IO_ReadB(crt_reg+1));
				real_writeb(base_seg,base_dest+(ct_dest++),IO_ReadB(crt_reg+1));
			} else {
				IO_WriteB(crt_reg,0x30+ct);
				real_writeb(base_seg,base_dest+(ct_dest++),IO_ReadB(crt_reg+1));
			}
		}
	}*/
	return true;
}

OPTINLINE bool INT10_VideoState_Restore(Bitu state,RealPt buffer) {
	Bitu ct;
	if ((state&7)==0) return false;

	Bit16u base_seg=RealSeg(buffer);
	Bit16u base_dest;

	if (state&1)  {
		base_dest=real_readw(base_seg,RealOff(buffer));
		Bit16u crt_reg=real_readw(base_seg,base_dest+0x40);

		// reprogram for full access to plane latches
		IO_WriteW(0x3c4,0x0704);
		IO_WriteW(0x3ce,0x0406);
		IO_WriteW(0x3ce,0x0005);

		IO_WriteW(0x3c4,0x0002);
		mem_writeb(0xaffff,real_readb(base_seg,base_dest+0x42));
		IO_WriteW(0x3c4,0x0102);
		mem_writeb(0xaffff,real_readb(base_seg,base_dest+0x43));
		IO_WriteW(0x3c4,0x0202);
		mem_writeb(0xaffff,real_readb(base_seg,base_dest+0x44));
		IO_WriteW(0x3c4,0x0402);
		mem_writeb(0xaffff,real_readb(base_seg,base_dest+0x45));
		IO_WriteW(0x3c4,0x0f02);
		mem_readb(0xaffff);

		IO_WriteW(0x3c4,0x0100);

		// sequencer
		for (ct=1; ct<5; ct++) {
			IO_WriteW(0x3c4,ct+(real_readb(base_seg,base_dest+0x04+ct)<<8));
		}

		IO_WriteB(0x3c2,real_readb(base_seg,base_dest+0x09));
		IO_WriteW(0x3c4,0x0300);
		IO_WriteW(crt_reg,0x0011);

		// crt controller
		for (ct=0; ct<0x19; ct++) {
			IO_WriteW(crt_reg,ct+(real_readb(base_seg,base_dest+0x0a+ct)<<8));
		}

		IO_ReadB(crt_reg+6);
		// attr registers
		for (ct=0; ct<4; ct++) {
			IO_WriteB(0x3c0,0x10+ct);
			IO_WriteB(0x3c0,real_readb(base_seg,base_dest+0x33+ct));
		}

		// graphics registers
		for (ct=0; ct<9; ct++) {
			IO_WriteW(0x3ce,ct+(real_readb(base_seg,base_dest+0x37+ct)<<8));
		}

		IO_WriteB(crt_reg+6,real_readb(base_seg,base_dest+0x04));
		IO_ReadB(crt_reg+6);

		// attr registers
		for (ct=0; ct<0x10; ct++) {
			IO_WriteB(0x3c0,ct);
			IO_WriteB(0x3c0,real_readb(base_seg,base_dest+0x23+ct));
		}

		IO_WriteB(0x3c4,real_readb(base_seg,base_dest+0x00));
		IO_WriteB(0x3d4,real_readb(base_seg,base_dest+0x01));
		IO_WriteB(0x3ce,real_readb(base_seg,base_dest+0x02));
		IO_ReadB(crt_reg+6);
		IO_WriteB(0x3c0,real_readb(base_seg,base_dest+0x03));
	}

	if (state&2)  {
		base_dest=real_readw(base_seg,RealOff(buffer)+2);

		mem_writeb(0x410,(mem_readb(0x410)&0xcf) | real_readb(base_seg,base_dest+0x00));
		for (ct=0; ct<0x1e; ct++) {
			mem_writeb(0x449+ct,real_readb(base_seg,base_dest+0x01+ct));
		}
		for (ct=0; ct<0x07; ct++) {
			mem_writeb(0x484+ct,real_readb(base_seg,base_dest+0x1f+ct));
		}
		mem_writed(0x48a,real_readd(base_seg,base_dest+0x26));
		mem_writed(0x14,real_readd(base_seg,base_dest+0x2a));	// int 5
		mem_writed(0x74,real_readd(base_seg,base_dest+0x2e));	// int 1d
		mem_writed(0x7c,real_readd(base_seg,base_dest+0x32));	// int 1f
		mem_writed(0x10c,real_readd(base_seg,base_dest+0x36));	// int 43
	}

	if (state&4)  {
		base_dest=real_readw(base_seg,RealOff(buffer)+4);

		Bit16u crt_reg=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);

		IO_WriteB(0x3c6,real_readb(base_seg,base_dest+0x002));

		for (ct=0; ct<0x100; ct++) {
			IO_WriteB(0x3c8,ct);
			IO_WriteB(0x3c9,real_readb(base_seg,base_dest+0x003+ct*3+0));
			IO_WriteB(0x3c9,real_readb(base_seg,base_dest+0x003+ct*3+1));
			IO_WriteB(0x3c9,real_readb(base_seg,base_dest+0x003+ct*3+2));
		}

		IO_ReadB(crt_reg+6);
		IO_WriteB(0x3c0,0x14);
		IO_WriteB(0x3c0,real_readb(base_seg,base_dest+0x303));

		Bitu dac_state=real_readb(base_seg,base_dest+0x000);
		if (dac_state==0) {
			IO_WriteB(0x3c8,real_readb(base_seg,base_dest+0x001));
		} else {
			IO_WriteB(0x3c7,real_readb(base_seg,base_dest+0x001));
		}
	}

	/*if ((svgaCard==SVGA_S3Trio) && (state&8))  {
		base_dest=real_readw(base_seg,RealOff(buffer)+6);

		Bit16u crt_reg=real_readw(BIOSMEM_SEG,BIOSMEM_CRTC_ADDRESS);

		Bitu seq_idx=IO_ReadB(0x3c4);
		IO_WriteB(0x3c4,0x08);
//		Bitu seq_8=IO_ReadB(0x3c5);
		IO_ReadB(0x3c5);
//		real_writeb(base_seg,base_dest+0x00,IO_ReadB(0x3c5));
		IO_WriteB(0x3c5,0x06);	// unlock s3-specific registers

		// sequencer
		for (ct=0; ct<0x13; ct++) {
			IO_WriteW(0x3c4,(0x09+ct)+(real_readb(base_seg,base_dest+0x00+ct)<<8));
		}
		IO_WriteB(0x3c4,seq_idx);

//		Bitu crtc_idx=IO_ReadB(0x3d4);

		// unlock s3-specific registers
		IO_WriteW(crt_reg,0x4838);
		IO_WriteW(crt_reg,0xa539);

		// crt controller
		Bitu ct_dest=0x13;
		for (ct=0; ct<0x40; ct++) {
			if ((ct==0x4a-0x30) || (ct==0x4b-0x30)) {
				IO_WriteB(crt_reg,0x45);
				IO_ReadB(crt_reg+1);
				IO_WriteB(crt_reg,0x30+ct);
				IO_WriteB(crt_reg,real_readb(base_seg,base_dest+(ct_dest++)));
			} else {
				IO_WriteW(crt_reg,(0x30+ct)+(real_readb(base_seg,base_dest+(ct_dest++))<<8));
			}
		}

		// mmio
/		IO_WriteB(crt_reg,0x40);
		Bitu sysval1=IO_ReadB(crt_reg+1);
		IO_WriteB(crt_reg+1,sysval|1);
		IO_WriteB(crt_reg,0x53);
		Bitu sysva2=IO_ReadB(crt_reg+1);
		IO_WriteB(crt_reg+1,sysval2|0x10);

		real_writew(0xa000,0x8128,0xffff);

		IO_WriteB(crt_reg,0x40);
		IO_WriteB(crt_reg,sysval1);
		IO_WriteB(crt_reg,0x53);
		IO_WriteB(crt_reg,sysval2);
		IO_WriteB(crt_reg,crtc_idx); /
	}*/

	return true;
}

OPTINLINE void INT10_GetFuncStateInformation(PhysPt save) {
	/* set static state pointer */
	mem_writed(Phys2Real(save),int10.rom.static_state);
	/* Copy BIOS Segment areas */
	Bit16u i;

	/* First area in Bios Seg */
	for (i=0;i<0x1e;i++) {
		mem_writeb(Phys2Real(save+0x4+i),real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE+i));
	}
	/* Second area */
	mem_writeb(Phys2Real(save+0x22),real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS)+1);
	for (i=1;i<3;i++) {
		mem_writeb(Phys2Real(save+0x22+i),real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS+i));
	}
	/* Zero out rest of block */
	for (i=0x25;i<0x40;i++) mem_writeb(Phys2Real(save+i),0);
	/* DCC */
//	mem_writeb(save+0x25,real_readb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX));
	Bit8u dccode = 0x00;
	RealPt vsavept=real_readd(BIOSMEM_SEG,BIOSMEM_VS_POINTER);
	RealPt svstable=real_readd(RealSeg(vsavept),RealOff(vsavept)+0x10);
	if (svstable) {
		RealPt dcctable=real_readd(RealSeg(svstable),RealOff(svstable)+0x02);
		Bit8u entries=real_readb(RealSeg(dcctable),RealOff(dcctable)+0x00);
		Bit8u idx=real_readb(BIOSMEM_SEG,BIOSMEM_DCC_INDEX);
		// check if index within range
		if (idx<entries) {
			Bit16u dccentry=real_readw(RealSeg(dcctable),RealOff(dcctable)+0x04+idx*2);
			if ((dccentry&0xff)==0) dccode=(Bit8u)((dccentry>>8)&0xff);
			else dccode=(Bit8u)(dccentry&0xff);
		}
	}
	mem_writeb(Phys2Real(save+0x25),dccode);

	Bit16u col_count=0;
	switch (CurMode->type) {
	case M_TEXT:
		if (CurMode->mode==0x7) col_count=1; else col_count=16;break; 
	case M_CGA2:
		col_count=2;break;
	case M_CGA4:
		col_count=4;break;
	case M_EGA:
		if (CurMode->mode==0x11 || CurMode->mode==0x0f) 
			col_count=2; 
		else 
			col_count=16;
		break; 
	case M_VGA:
		col_count=256;
		break;
	default:
		//LOG(LOG_INT10,LOG_ERROR)("Get Func State illegal mode type %d",CurMode->type);
		break;
	}
	/* Colour count */
	mem_writew(Phys2Real(save+0x27),col_count);
	/* Page count */
	mem_writeb(Phys2Real(save+0x29),CurMode->ptotal);
	/* scan lines */
	switch (CurMode->sheight) {
	case 200:
		mem_writeb(Phys2Real(save+0x2a),0);break;
	case 350:
		mem_writeb(Phys2Real(save+0x2a),1);break;
	case 400:
		mem_writeb(Phys2Real(save+0x2a),2);break;
	case 480:
		mem_writeb(Phys2Real(save+0x2a),3);break;
	};
	/* misc flags */
	if (CurMode->type==M_TEXT) mem_writeb(Phys2Real(save+0x2d),0x21);
	else mem_writeb(Phys2Real(save+0x2d),0x01);
	/* Video Memory available */
	mem_writeb(Phys2Real(save+0x31),3);
}

void int10_FuncStatus() //REG_AH=1Bh
{
	switch (REG_BX) {
	case 0x0000:
		INT10_GetFuncStateInformation(Real2Phys(RealMake(REG_ES,REG_DI)));
		REG_AL=0x1B;
		break;
	default:
		//LOG(LOG_INT10,LOG_ERROR)("1B:Unhandled call REG_BX %2X",reg_bx);
		REG_AL=0;
		break;
	}
}

void int10_SaveRestoreVideoStateFns() //REG_AH=1Ch
{
	switch (REG_AL) {
		case 0: {
			Bitu ret=INT10_VideoState_GetSize(REG_CX);
			if (ret) {
				REG_AL=0x1c;
				REG_BX=(Bit16u)ret;
			} else REG_AL=0;
			}
			break;
		case 1:
			if (INT10_VideoState_Save(REG_CX,RealMake(REG_ES,REG_BX))) REG_AL=0x1c;
			else REG_AL=0;
			break;
		case 2:
			if (INT10_VideoState_Restore(REG_CX,RealMake(REG_ES,REG_BX))) REG_AL=0x1c;
			else REG_AL=0;
			break;
		default:
			/*if (svgaCard==SVGA_TsengET4K) reg_ax=0;
			else*/ REG_AL=0;
			break;
	}
}

//AH=4Fh,AL=subfunc; SVGA support?













OPTINLINE byte int2hex(byte b)
{
	byte translatetable[0x10] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'}; //The Hexdecimal notation!
	return translatetable[b&0xF];
}

OPTINLINE byte bytetonum(byte b, byte nibble)
{
	if (!nibble) return int2hex(b&0xF); //Low nibble!
	return int2hex((b>>4)&0xF); //High nibble!
}

OPTINLINE void writehex(FILE *f, byte num) //Write a number (byte) to a file!
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

	char lb[3];
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
	,NULL, //14
	NULL, //15
	NULL, //16
	NULL, //17
	NULL, //18
	NULL, //19
	int10_DCC, //1A
	int10_FuncStatus, //1B
	int10_SaveRestoreVideoStateFns, //1C
}; //Function list!

void init_int10() //Initialises int10&VGA for usage!
{
	//if (__HW_DISABLED) return; //Abort!
	
	INT10_SetupRomMemory(); //Initialise the VGA ROM memory if not initialised yet!
	
	//int10_VGA = getActiveVGA(); //Use the active VGA for int10 by default!
	
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
	dohandle = (REG_AH<NUMITEMS(int10functions)); //handle?

	if (!dohandle) //Not within list to execute?
	{
		REG_AX = 0; //Break!
	}
	else //To handle?
	{
		if (int10functions[REG_AH]!=NULL) //Set?
		{
			int10functions[REG_AH](); //Run the function!
		}
		else
		{
			REG_AH = 0xFF; //Error: unknown command!
			CALLBACK_SCF(1); //Set carry flag to indicate an error!
		}
	}
}