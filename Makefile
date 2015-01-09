#Define our building directory for all output files!
BUILD_DIR = ../../psp-projects_build/x86EMU

TARGET = eboot

#need prx to be able to run the exception handler!
#BUILD_PRX = 1
#atm. build_prx causes multithreading to fail?

#Enable large memory detection? Disabled during developing.
#PSP_LARGE_MEMORY = 1

#Init for below:
OBJS = 
CFLAGS = 
CXXFLAGS = 
LIBS = 
#optimization flags: Nothing: Debugging, -O3 normal operations.
OPTIMIZATIONFLAG = -O3
#OPTIMIZATIONFLAG = 

#Exception handler!
OBJS += exception/exception.o

#Memory allocation!
OBJS += support\zalloc.o

#Timers!
OBJS += support\timers.o

#Bitmap support!
OBJS += support\bmp.o

#Logging support!
OBJS += support\log.o

#Basic Callback manager in-memory!
OBJS += cpu\cb_manager.o
#first, interrupts
OBJS += interrupts\interrupt13.o interrupts\interrupt10.o interrupts\interrupt18.o interrupts\interrupt19.o interrupts\interrupt16.o interrupts\interrupt5.o interrupts\interrupt11.o interrupts\unkint.o
OBJS += cpu\cpu_interrupts.o
#Interrupt 10h VGA help:
OBJS += interrupts\interrupt10_modelist_vga.o interrupts\interrupt10_switchvideomode.o interrupts\interrupt10_romfont.o interrupts\interrupt10_textmodedata.o
#next, support
OBJS += basicio\dynamicimage.o basicio\staticimage.o support\isoreader.o support\fifobuffer.o support\lba.o support\crc32.o support\state.o
#all main I/O stuff we need (disk I/O and port mapping)
OBJS += basicio\io.o basicio\port_mapper.o
#external chip caller port port i/o (for the CPU and hardware):
OBJS += basicio\port_io.o
#external chips
OBJS += hardware\pit.o
#PS/2 Keyboard
OBJS += hardware\ps2_keyboard_data.o hardware\ps2_keyboard.o
#PS/2 mouse device
OBJS += hardware\ps2_mouse.o
#Main PS/2 controller
OBJS += hardware\8042.o
#Various devices.
OBJS += hardware\dma.o hardware\pic.o hardware\CMOS.o
#VGA Adapter for CPU and EMU (non-view):
OBJS += hardware\vga\vga_colorconversion.o hardware\vga\vga_vram.o hardware\vga\vga_dac.o hardware\vga\vga.o
#VGA MMU&I/O device.
OBJS += hardware\vga\vga_mmu.o hardware\vga\vga_io.o
#VGA view:
# *1*: Sequencer Operation
OBJS += hardware\vga\vga_screen\vga_sequencer.o hardware\vga\vga_screen\vga_sequencer_graphicsmode.o hardware\vga\vga_screen\vga_sequencer_textmode.o
# *2*: Attribute Controller Operation
OBJS += hardware\vga\vga_screen\vga_attributecontroller.o
# *3*: DAC Operation
OBJS += hardware\vga\vga_screen\vga_dac.o
# *4*: CRTC
OBJS += hardware\vga\vga_screen\vga_crtcontroller.o
# Help: precalcs for VGA rendering!
OBJS += hardware\vga\vga_screen\vga_precalcs.o
#VRAM text-mode data
OBJS += hardware\vga\vga_vramtext.o

#Adlib!
OBJS += hardware\adlib.o

#Software debugger/CPU emulator functions.
OBJS += hardware\softdebugger.o

#BIOS:
OBJS += bios\bios.o bios\biosmenu.o bios\initmem.o bios\biosrom.o
#MMU:
OBJS += mmu\paging.o mmu\mmuhandler.o mmu\mmu.o
#CPU:
#Interrupts used.
OBJS += cpu\hardware_interrupts.o
#Runromverify: Verifies ROM by execution.
OBJS += emu\debugger\runromverify.o
#MODR/M support:
OBJS += cpu\modrm.o
#8086,80186 GRP Opcodes:
OBJS += cpu\8086\8086_grpOPs.o
#Opcodes for CPU 8086,Debugger,Flags
OBJS += cpu\8086\opcodes_8086.o cpu\8086\flags.o
#Opcodes for CPU 80186,Debugger
OBJS += cpu\80186\opcodes_80186.o
#Opcodes for CPU 80286,Debugger&JMPTbl for 0F opcodes!
OBJS += cpu\cpu_jmptbls0f.o cpu\80286\opcodes_80286.o
#Opcodes for CPU 80386&JMPTbl
#OBJS += cpu\80386\opcodes0F_386.o cpu\80386\opcodes_386.o
#Opcodes: unknown opcodes and rest handler functions (386 0x0F, unknown opcode handler for 386&8086)
OBJS += cpu\unkop.o
#Now the jumptables for all CPUs!
OBJS += cpu\cpu_jmptbls.o
#Protection module of the CPU (286+).
OBJS += cpu\80286\protection.o
#Multitasking module of the CPU (286+).
OBJS += cpu\80286\multitasking.o
#finally CPU module itself
OBJS += cpu\cpu.o
#Debugger
OBJS += emu\debugger\debugger.o
#Signed VS unsigned conversion support!
OBJS += support\signedness.o

#PC Speaker&Std sound output!
OBJS += hardware\pcspeaker.o

#MIDI and SF2 support!
OBJS += support\sf2.o hardware\midi\mpu.o hardware\midi\midi.o hardware\midi\mididevice.o

#High resolution timer
OBJS += support\highrestimer.o

#Rest and main script
OBJS += basicio\boot.o

#Emulator specific stuff!
#Emulator sound:
OBJS += emu\io\sound.o
#Emulator threads:
OBJS += emu\support\threads.o
#Emulator input:
OBJS += emu\io\keyboard.o emu\io\input.o
#All GPU parts!
OBJS += emu\gpu\gpu.o emu\gpu\gpu_debug.o emu\gpu\gpu_framerate.o emu\gpu\gpu_renderer.o emu\gpu\gpu_emu.o emu\gpu\gpu_text.o
#GPU SDL part!
OBJS += emu\gpu\gpu_sdl.o

#Error handler!
OBJS += emu\support\errors.o

#Sound test utility.
OBJS += emu\debugger\debug_sound.o

#CPU emulation core file.
OBJS += emu\core\emucore.o
#BIOS POST
OBJS += emu\core\emu_bios_post.o

#Main subcores.
OBJS += emu\core\emu_main.o emu\core\emu_misc.o emu\support\number_optimizations.o emu\core\emu_vga_bios.o emu\core\emu_vga.o emu\core\emu_bios_sound.o emu\debugger\debug_graphics.o emu\debugger\debug_files.o

#Finally: main script!
OBJS += emu\main.o

#64-bit file support!
OBJS += basicio\fopen64.o

#Finally, everything PSP!

CFLAGS += $(OPTIMIZATIONFLAG) -G0

#SDL
CXXFLAGS += $(CFLAGS)
ASFLAGS = $(CFLAGS)
LIBS = -lpng -lz -lm -lstdc++
#PSPLIBS1 = -lpspgu -lpsppower -lpspdebugkb
PSPLIBS1 = -lpspdebugkb
#Audio
PSPLIBS1 += -lpspaudiolib -lpspaudio
#Timing/RTC
PSPLIBS1 += -lpsprtc

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = x86 emulator

#Turn this line off when using SDL!

#SDL Specific!
PSPBIN = $(PSPSDK)/../bin
#CFLAGS = $(shell $(PSPBIN)/sdl-config --cflags)
#LIBS += -lSDL  -lglut -lGLU -lGL -lc
#PSPLIBS2 = -lpsputility -lpspdebug -lpspge -lpspdisplay -lpspctrl -lpspsdk -lpspvfpu -lpsplibc -lpspuser -lpspkernel -lpsphprm -lpspirkeyb -lpsppower
PSPLIBS2 = -lSDL -lSDL_ttf -lSDL_image -lSDL_gfx -lpspirkeyb -lpspwlan -lpsppower -lGL -l freetype -ljpeg -lpng -lz -lm -lSDL -lpspgu -l psphprm -lpspaudio -lstdc++ -lpspvfpu -lpsprtc

#Link the final PSP libaries!
LIBS += $(PSPLIBS2) $(PSPLIBS1)

#Rest PSP!
PSPSDK=$(shell psp-config --pspsdk-path)
include ../tools/build.mak