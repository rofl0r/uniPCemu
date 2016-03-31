#include "headers/types.h" //Basic types!
#include "headers/hardware/vga/vga.h" //VGA/CGA support!
#include "headers/header_dosbox.h" //Dosbox support!
#include "headers/emu/gpu/gpu_emu.h" //RGBI support!
#include "headers/hardware/vga/vga_cga_ntsc.h" //Our own definitions!

double hue_offset = 0.0f; //Hue offset!

//Main functions for rendering NTSC and RGBI by superfury:

byte CGA_RGB = 1; //Are we a RGB monitor(1) or Composite monitor(0)?
byte New_CGA = 0; //Emulate a new-style CGA?

void setCGA_NTSC(byte enabled) //Use NTSC CGA signal output?
{
	byte needupdate = 0;
	needupdate = (CGA_RGB^(enabled?0:1)); //Do we need to update the palette?
	CGA_RGB = enabled?0:1; //RGB or NTSC monitor!
	if (needupdate) RENDER_updateCGAColors(); //Update colors if we're changed!
}

void setCGA_NewCGA(byte enabled)
{
	byte needupdate = 0;
	needupdate = (New_CGA^(enabled?1:0)); //Do we need to update the palette?
	New_CGA = enabled?1:0; //Use New Style CGA as set with protection?
	if (needupdate) RENDER_updateCGAColors(); //Update colors if we're changed!
}

//Our main conversion functions!
//RGBI conversion
OPTINLINE static void RENDER_convertRGBI(byte *pixels, uint_32 *renderdestination, uint_32 size) //Convert a row of data to NTSC output!
{
	uint_32 current;
	for (current=0;current<size;current++) //Process all pixels!
		renderdestination[current] = getemucol16(pixels[current]); //Just use RGBI colors!
}

//NTSC conversion
uint_32 NTSCPAL[0x100]; //NTSC palette for all specified indexes(8-bit indexes with 16-bit RGB)!
OPTINLINE static void RENDER_convertNTSC(byte *pixels, uint_32 *renderdestination, uint_32 size) //Convert a row of data to NTSC output!
{
	//RENDER_convertRGBI(pixels,renderdestination,size); return; //Test by converting to RGBI instead!
	uint_32 current;
	for (current=0;current<size;current++) //Process all pixels!
	{
		renderdestination[current] = NTSCPAL[((current&0xF)<<4)|pixels[current]]; //Render using the NTSC palette!
	}
}

OPTINLINE static void RENDER_SetPal(Bit8u index, int r, int g, int b) //Dosbox compatibility function by Superfury: Apparently this simply sets the NTSC 256-color palette, according to https://github.com/joncampbell123/dosbox-x/blob/a6ef524002c560254bc705e8629df72ff173c2eb/src/hardware/hardware.cpp!
{
	//Convert the renderer data to data for ourselves! Just store for now!
	NTSCPAL[index] = RGB(r,g,b); //Set the RGB palette of NTSC mode!
}

//Functions to call to update our data and render it according to our settings!
void RENDER_convertCGAOutput(byte *pixels, uint_32 *renderdestination, uint_32 size) //Convert a row of data to NTSC output!
{
	if (CGA_RGB) //RGB monitor?
	{
		RENDER_convertRGBI(pixels, renderdestination, size); //Convert the pixels as RGBI!
	}
	else //NTSC monitor?
	{
		RENDER_convertNTSC(pixels, renderdestination, size); //Convert the pixels as NTSC!
	}
}

//Dosbox conversion function itself, Converted from Dosbox-X(Parameters added with information from the emulated CGA)!

OPTINLINE static void update_cga16_color(byte CGA_paletteregister, byte CGA_modecontrolregister) {
// New algorithm based on code by reenigne
// Works in all CGA graphics modes/color settings and can simulate older and newer CGA revisions
	static const double tau = 6.28318531; // == 2*pi
	static const double ns = 567.0/440;  // degrees of hue shift per nanosecond

	double tv_brightness = 0.0; // hardcoded for simpler implementation
	double tv_saturation = (New_CGA ? 0.7 : 0.6);

	bool bw = (CGA_modecontrolregister&4) != 0;
	bool color_sel = (CGA_paletteregister&0x20) != 0;
	bool background_i = (CGA_paletteregister&0x10) != 0;	// Really foreground intensity, but this is what the CGA schematic calls it.
	bool bpp1 = (CGA_modecontrolregister&0x10) != 0;
	Bit8u overscan = CGA_paletteregister&0x0f;  // aka foreground colour in 1bpp mode

	double chroma_coefficient = New_CGA ? 0.29 : 0.72;
	double b_coefficient = New_CGA ? 0.07 : 0;
	double g_coefficient = New_CGA ? 0.22 : 0;
	double r_coefficient = New_CGA ? 0.1 : 0;
	double i_coefficient = New_CGA ? 0.32 : 0.28;
	double rgbi_coefficients[0x10];
	for (int c = 0; c < 0x10; c++) {
		double v = 0;
		if ((c & 1) != 0)
			v += b_coefficient;
		if ((c & 2) != 0)
			v += g_coefficient;
		if ((c & 4) != 0)
			v += r_coefficient;
		if ((c & 8) != 0)
			v += i_coefficient;
		rgbi_coefficients[c] = v;
	}

	// The pixel clock delay calculation is not accurate for 2bpp, but the difference is small and a more accurate calculation would be too slow.
	static const double rgbi_pixel_delay = 15.5*ns;
	static const double chroma_pixel_delays[8] = {
		0,        // Black:   no chroma
		35*ns,    // Blue:    no XORs
		44.5*ns,  // Green:   XOR on rising and falling edges
		39.5*ns,  // Cyan:    XOR on falling but not rising edge
		44.5*ns,  // Red:     XOR on rising and falling edges
		39.5*ns,  // Magenta: XOR on falling but not rising edge
		44.5*ns,  // Yellow:  XOR on rising and falling edges
		39.5*ns}; // White:   XOR on falling but not rising edge
	double pixel_clock_delay;
	int o = overscan == 0 ? 15 : overscan;
	if (overscan == 8)
		pixel_clock_delay = rgbi_pixel_delay;
	else {
		double d = rgbi_coefficients[o];
		pixel_clock_delay = (chroma_pixel_delays[o & 7]*chroma_coefficient + rgbi_pixel_delay*d)/(chroma_coefficient + d);
	}
	pixel_clock_delay -= 21.5*ns;  // correct for delay of color burst

	double hue_adjust = (-(90-33)-hue_offset+pixel_clock_delay)*tau/360.0;
	double chroma_signals[8][4];
	for (Bit8u i=0; i<4; i++) {
		chroma_signals[0][i] = 0;
		chroma_signals[7][i] = 1;
		for (Bit8u j=0; j<6; j++) {
			static const double phases[6] = {
				270 - 21.5*ns,  // blue
				135 - 29.5*ns,  // green
				180 - 21.5*ns,  // cyan
				  0 - 21.5*ns,  // red
				315 - 29.5*ns,  // magenta
				 90 - 21.5*ns}; // yellow/burst
			// All the duty cycle fractions are the same, just under 0.5 as the rising edge is delayed 2ns more than the falling edge.
			static const double duty = 0.5 - 2*ns/360.0;

			// We have a rectangle wave with period 1 (in units of the reciprocal of the color burst frequency) and duty
			// cycle fraction "duty" and phase "phase". We band-limit this wave to frequency 2 and sample it at intervals of 1/4.
			// We model our band-limited wave with 4 frequency components:
			//   f(x) = a + b*sin(x*tau) + c*cos(x*tau) + d*sin(x*2*tau)
			// Then:
			//   a =   integral(0, 1, f(x)*dx) = duty
			//   b = 2*integral(0, 1, f(x)*sin(x*tau)*dx) = 2*integral(0, duty, sin(x*tau)*dx) = 2*(1-cos(x*tau))/tau
			//   c = 2*integral(0, 1, f(x)*cos(x*tau)*dx) = 2*integral(0, duty, cos(x*tau)*dx) = 2*sin(duty*tau)/tau
			//   d = 2*integral(0, 1, f(x)*sin(x*2*tau)*dx) = 2*integral(0, duty, sin(x*4*pi)*dx) = 2*(1-cos(2*tau*duty))/(2*tau)
			double a = duty;
			double b = 2.0*(1.0-cos(duty*tau))/tau;
			double c = 2.0*sin(duty*tau)/tau;
			double d = 2.0*(1.0-cos(duty*2*tau))/(2*tau);

			double x = (phases[j] + 21.5*ns + pixel_clock_delay)/360.0 + i/4.0;

			chroma_signals[j+1][i] = a + b*sin(x*tau) + c*cos(x*tau) + d*sin(x*2*tau);
		}
	}
	Bitu CGApal[4] = {
		overscan,
		(Bitu)(2 + (color_sel||bw ? 1 : 0) + (background_i ? 8 : 0)),
		(Bitu)(4 + (color_sel&&!bw? 1 : 0) + (background_i ? 8 : 0)),
		(Bitu)(6 + (color_sel||bw ? 1 : 0) + (background_i ? 8 : 0))
	};
	for (Bit8u x=0; x<4; x++) {	 // Position of pixel in question
		bool even = (x & 1) == 0;
		for (Bit8u bits=0; bits<(even ? 0x10 : 0x40); ++bits) {
			double Y=0, I=0, Q=0;
			for (Bit8u p=0; p<4; p++) {  // Position within color carrier cycle
				// generate pixel pattern.
				Bit8u rgbi;
				if (bpp1)
					rgbi = ((bits >> (3-p)) & (even ? 1 : 2)) != 0 ? overscan : 0;
				else
					if (even)
						rgbi = CGApal[(bits >> (2-(p&2)))&3];
					else
						rgbi = CGApal[(bits >> (4-((p+1)&6)))&3];
				Bit8u c = rgbi & 7;
				if (bw && c != 0)
					c = 7;

				// calculate composite output
				double chroma = chroma_signals[c][(p+x)&3]*chroma_coefficient;
				double composite = chroma + rgbi_coefficients[rgbi];

				Y+=composite;
				if (!bw) { // burst on
					I+=composite*2*cos(hue_adjust + (p+x)*tau/4.0);
					Q+=composite*2*sin(hue_adjust + (p+x)*tau/4.0);
				}
			}

			double contrast = 1 - tv_brightness;

			Y = (contrast*Y/4.0) + tv_brightness; if (Y>1.0) Y=1.0; if (Y<0.0) Y=0.0;
			I = (contrast*I/4.0) * tv_saturation; if (I>0.5957) I=0.5957; if (I<-0.5957) I=-0.5957;
			Q = (contrast*Q/4.0) * tv_saturation; if (Q>0.5226) Q=0.5226; if (Q<-0.5226) Q=-0.5226;

			static const double gamma = 2.2;

			double R = Y + 0.9563*I + 0.6210*Q;	R = (R - 0.075) / (1-0.075); if (R<0) R=0; if (R>1) R=1;
			double G = Y - 0.2721*I - 0.6474*Q;	G = (G - 0.075) / (1-0.075); if (G<0) G=0; if (G>1) G=1;
			double B = Y - 1.1069*I + 1.7046*Q;	B = (B - 0.075) / (1-0.075); if (B<0) B=0; if (B>1) B=1;
			R = pow(R, gamma);
			G = pow(G, gamma);
			B = pow(B, gamma);

			int r = (int)(255*pow( 1.5073*R -0.3725*G -0.0832*B, 1/gamma)); if (r<0) r=0; if (r>255) r=255;
			int g = (int)(255*pow(-0.0275*R +0.9350*G +0.0670*B, 1/gamma)); if (g<0) g=0; if (g>255) g=255;
			int b = (int)(255*pow(-0.0272*R -0.0401*G +1.1677*B, 1/gamma)); if (b<0) b=0; if (b>255) b=255;

			Bit8u index = bits | ((x & 1) == 0 ? 0x30 : 0x80) | ((x & 2) == 0 ? 0x40 : 0);
			RENDER_SetPal(index,r,g,b);
		}
	}
}

void RENDER_updateCGAColors() //Update CGA rendering NTSC vs RGBI conversion!
{
	if (!CGA_RGB) update_cga16_color(getActiveVGA()->registers->Compatibility_CGAPaletteRegister, getActiveVGA()->registers->Compatibility_CGAModeControl); //Update us if we're used!
}