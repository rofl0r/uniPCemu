#ifndef DOSBOX_PATCHES_H
#define DOSBOX_PATCHES_H

//Dosbox Patches! Redirect it all!
#define real_readb(biosseg,offs) MMU_rb(biosseg,offs)
#define real_writeb(biosseg,offs,val) MMU_wb(biosseg,offs,val)
#define real_readw(biosseg,offs) (real_readb(biosseg,offs)+(real_readb(biosseg,offs+1)*256))
#define real_writew(biosseg,offs,val) real_writeb(biosseg,offs,val&0xFF); real_writeb(biosseg,offs+1,((val&0xFF00)>>8));
#define memreal_writew(seg,offs,val) MMU_ww(seg,offs,val)
#define IO_WriteB(port,value) PORT_OUT_B(port,value)
#define IO_WriteW(port,value) PORT_OUT_W(port,value)
#define IO_ReadB(port) PORT_IN_B(port)
#define IO_ReadW(port) PORT_IN_W(port)
//Synonym for IO_WriteB:
#define IO_Write(port,value) IO_WriteB(port,value)
#define IO_Read(port) IO_ReadB(port)

#endif