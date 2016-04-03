#ifndef VGA_CGA_MDA_H
#define VGA_CGA_MDA_H

#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //Basic VGA support!

void setVGA_CGA(byte enabled); //0=Disabled, 1=Enable with disabled VGA, 2=Enabled with enabled VGA!
void setCGA_NewCGA(byte enabled); //Use New-style CGA emulation?
void setCGA_NTSC(byte enabled); //Use NTSC CGA signal output?
void setVGA_MDA(byte enabled); //0=Disabled, 1=Enable with disabled VGA, 2=Enabled with enabled VGA!

//Initialization call for registering us on the VGA!
void initCGA_MDA();

//CGA/MDA emulation enabled on the CRTC registers&timing?
#define CGAMDAEMULATION_ENABLED_CRTC(VGA) (((VGA->registers->specialCGAflags&0x81)==1) || ((VGA->registers->specialMDAflags&0x81)==1) || (VGA->registers->CRTControllerRegisters.REGISTERS.VERTICALRETRACEENDREGISTER.Protect && (((VGA->registers->specialCGAflags&0xC1)==0xC1) || ((VGA->registers->specialMDAflags&0xC1)==0xC1))))
//CGA/MDA emulation enabled?
#define CGAMDAEMULATION_ENABLED(VGA) (((VGA->registers->specialCGAflags&0x81)==1) || ((VGA->registers->specialMDAflags&0x81)==1) || ((VGA->registers->specialCGAflags&0xC1)==0xC1) || ((VGA->registers->specialMDAflags&0xC1)==0xC1))

//To perform CGA/MDA to display conversion?
#define CGAMDAEMULATION_RENDER(VGA) (((VGA->registers->specialCGAflags|VGA->registers->specialMDAflags)&1) && ((VGA->registers->specialCGAflags&0xC0)!=0x80) || ((VGA->registers->specialMDAflags&0xC0)!=0x80))

#endif
