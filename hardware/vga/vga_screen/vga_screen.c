#include "headers/types.h" //Basic types!
//#include "headers/emu/gpu/gpu.h" //GPU support!
//#include "headers/cpu/interrupts.h" //Interrupt support!
#include "headers/hardware/vga.h" //VGA support!
//#include "headers/hardware/vga_rest/colorconversion.h" //Color conversion support!
//#include "headers/support/log.h" //Logging support!

//Finally all subparts, in order of occurence:
#include "headers/hardware/vga_screen/vga_sequencer.h" //Sequencer!
//#include "headers/hardware/vga_screen/vga_sequencer_textmode.h" //Text-mode!
//#include "headers/hardware/vga_screen/vga_sequencer_textmode_cursor.h" //Text-mode cursor!
//#include "headers/hardware/vga_screen/vga_attributecontroller.h" //Attribute controller!
//#include "headers/hardware/vga_screen/vga_dac.h" //DAC operations!
//#include "headers/hardware/vga_screen/vga_displaygeneration_crtcontroller.h" //CRT Controller
//#include "headers/emu/gpu/gpu_text.h" //GPU text support!
#include "headers/support/zalloc.h" //Zalloc protection support!

//Are we disabled?
#define __HW_DISABLED 0

//extern GPU_type GPU; //GPU contents!
//To generate the screen, we use the CRTC registers!
byte active_screen = 0; //Active screen?

//Video padding functions:

//Display row info:
//Start Vertical Retrace=X width
//Vertical Total - Vertical Blank End = Y height

//Display type functions:

void setVGAFrameskip(VGA_Type *VGA, byte Frameskip) //Set frameskip or 0 for none!
{
	if (Frameskip) //Got frameskip?
	{
		VGA->framenr = 0; //Reset frame nr to draw correctly!
	}
	VGA->frameskip = Frameskip; //Set frameskip!
}

void VGA_generateScreenLine(VGA_Type *VGA) //Generate a screen line!
{
	if (__HW_DISABLED) return; //Abort!
	if (!VGA) //No VGA to work with?
	{
		return; //No VGA set, so we can't work!
	}
	if (!memprotect(VGA,sizeof(*VGA),NULL))
	{
		return; //Invalid VGA!
	}

	/*
	int do_render = 1; //Do render?
	static uint_32 framenr = 0; //Frame counter!

	if (VGA->frameskip) //Got frameskip?
	{
		do_render = !VGA->framenr; //To render the current frame each <frameskip> frames!
		if (!VGA->registers->Scanline) //First scanline (after VBlank)?
		{
			VGA->framenr = (VGA->framenr+1)%(VGA->frameskip+1); //Next frame detection!
		}
	} //Always render!

	if (!VGA->registers->Scanline) //New screen?
	{
		++framenr; //Start at 1 with increment!
	}
	
	if (do_render) //To render this frame?
	{
	*/ //For now, no frameskip!
		//We work by changing one scan line at a time!
		/*static byte firstres = 1; //Cleared during resolution init!
		word oldxres=GPU.xres, oldyres=GPU.yres; //Old rresolutions!
		GPU.xres = getxresfull(VGA); //Update x size!
		GPU.yres = getyresfull(VGA); //Update y size!
		if ((GPU.xres!=oldxres) || (GPU.yres!=oldyres) || firstres) //Updated resolution or first call?
		{
			GPU.emu_buffer_dirty = 1; //Needs to update the full screen!
			firstres = 0; //Not first call anymore, so no init!
		}*/

		//if (VGA->precalcs.verticalcharacterclocks) //Got something to render?
		//{
			VGA_Sequencer(VGA,!active_screen); //Sequencer, attribute controller and special effects and finally DAC: render 1 scanline!
		//}
	/*}
	else
	{
		VGA->registers->Scanline = SAFEMOD((VGA->registers->Scanline+1),GPU.yres); //Next wrap-arround frame handler!
	}*/ //For now, no frameskip!

//Now a scanline may have been rendered!
}