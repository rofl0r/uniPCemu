#Define our default building directory and executable name/title for all output files!
BUILD_NAME = x86EMU

#Now, our modules!

#Exception handler!
OBJS = exception/exception.o

#Memory allocation!
OBJS += support/zalloc.o

#Timers!
OBJS += support/timers.o

#Bitmap support!
OBJS += support/bmp.o

#Logging support!
OBJS += support/log.o

#Lock support!
OBJS += support/locks.o

#WAV file support!
OBJS += support/wave.o

#Basic Callback manager in-memory!
OBJS += cpu/cb_manager.o
#first, interrupts
OBJS += interrupts/interrupt13.o interrupts/interrupt10.o interrupts/interrupt18.o interrupts/interrupt19.o interrupts/interrupt16.o interrupts/interrupt5.o interrupts/interrupt11.o interrupts/interrupt12.o interrupts/interrupt1a.o interrupts/interrupt15.o interrupts/unkint.o
OBJS += cpu/cpu_interrupts.o
#Interrupt 10h VGA help:
OBJS += interrupts/interrupt10_modelist_vga.o interrupts/interrupt10_switchvideomode.o interrupts/interrupt10_romfont.o interrupts/interrupt10_textmodedata.o
#next, support
OBJS += basicio/dynamicimage.o basicio/staticimage.o basicio/dskimage.o support/isoreader.o support/fifobuffer.o support/lba.o support/crc32.o support/state.o
#all main I/O stuff we need (disk I/O and port mapping)
OBJS += basicio/io.o basicio/port_mapper.o
#external chip caller port port i/o (for the CPU and hardware):
OBJS += basicio/port_io.o
#external chips
OBJS += hardware/pit.o
#PS/2 Keyboard
OBJS += hardware/ps2_keyboard_data.o hardware/ps2_keyboard.o
#PS/2 mouse device
OBJS += hardware/ps2_mouse.o
#Main PS/2 controller
OBJS += hardware/8042.o
#Various devices.
OBJS += hardware/dma.o hardware/pic.o hardware/CMOS.o
#VGA Adapter for CPU and EMU (non-view):
OBJS += hardware/vga/vga_colorconversion.o hardware/vga/vga_vram.o hardware/vga/vga_dac.o hardware/vga/vga.o
#VGA MMU&I/O device.
OBJS += hardware/vga/vga_mmu.o hardware/vga/vga_io.o
#VGA view:
# *1*: Sequencer Operation
OBJS += hardware/vga/vga_sequencer.o hardware/vga/vga_sequencer_graphicsmode.o hardware/vga/vga_sequencer_textmode.o
# *2*: Attribute Controller Operation
OBJS += hardware/vga/vga_attributecontroller.o
# *3*: DAC Operation
OBJS += hardware/vga/vga_dacrenderer.o
# *4*: CRTC
OBJS += hardware/vga/vga_crtcontroller.o
# Help: precalcs for VGA rendering!
OBJS += hardware/vga/vga_precalcs.o
#VRAM text-mode data
OBJS += hardware/vga/vga_vramtext.o
#CGA/MDA support!
OBJS += hardware/vga/vga_cga_mda.o hardware/vga/vga_cga_ntsc.o

#Adlib!
OBJS += hardware/adlib.o

#Disney Sound Source
OBJS += hardware/ssource.o

#Floppy
OBJS += hardware/floppy.o

#Software debugger/CPU emulator functions.
OBJS += hardware/softdebugger.o

#PPI
OBJS += hardware/ppi.o

#EMS
OBJS += hardware/ems.o

#BIOS:
OBJS += bios/bios.o bios/biosmenu.o bios/initmem.o bios/biosrom.o
#MMU:
OBJS += mmu/paging.o mmu/mmuhandler.o mmu/mmu.o
#CPU:

#Runromverify: Verifies ROM by execution.
OBJS += emu/debugger/runromverify.o
#MODR/M support:
OBJS += cpu/modrm.o
#Opcodes for CPU 8086,Debugger,Flags
OBJS += cpu/opcodes_8086.o
#Opcodes for CPU 80186,Debugger
OBJS += cpu/opcodes_80186.o
#Opcodes for CPU 80286,Debugger&JMPTbl for 0F opcodes!
OBJS += cpu/cpu_jmptbls0f.o cpu/opcodes_80286.o
#Opcodes for CPU 80386&JMPTbl
#OBJS += cpu/opcodes0F_386.o cpu/opcodes_386.o
#Opcodes for CPU 80486
OBJS += cpu/opcodes_486.o
#Opcodes for CPU 80586 (Pentium)
OBJS += cpu/opcodes_586.o
#Opcodes: unknown opcodes and rest handler functions (386 0x0F, unknown opcode handler for 386&8086)
OBJS += cpu/unkop.o
#Now the jumptables for all CPUs!
OBJS += cpu/cpu_jmptbls.o
#Protection module of the CPU (286+).
OBJS += cpu/protection.o
#Multitasking module of the CPU (286+).
OBJS += cpu/multitasking.o
#finally CPU module itself and flag support (global)
OBJS += cpu/flags.o cpu/cpu.o
#Debugger
OBJS += emu/debugger/debugger.o
#Signed VS unsigned conversion support!
OBJS += support/signedness.o

#MIDI and SF2 support!
OBJS += support/sf2.o hardware/midi/mpu.o hardware/midi/midi.o hardware/midi/mididevice.o hardware/midi/adsr.o
#MID file support!
OBJS += support/mid.o

#UART
OBJS += hardware/uart.o

#Parallel
OBJS += hardware/parallel.o

#Mouse
OBJS += hardware/sermouse.o

#PCI
OBJS += hardware/pci.o

#ATA/IDE
OBJS += hardware/ide.o

#High resolution timer
OBJS += support/highrestimer.o

#Rest and main script
OBJS += basicio/boot.o

#Emulator specific stuff!
#Emulator sound:
OBJS += emu/io/sound.o
#Emulator threads:
OBJS += emu/core/threads.o
#Emulator input:
OBJS += emu/io/keyboard.o emu/io/input.o
#All GPU parts!
OBJS += emu/gpu/gpu.o emu/gpu/gpu_debug.o emu/gpu/gpu_framerate.o emu/gpu/gpu_renderer.o emu/gpu/gpu_emu.o emu/gpu/gpu_text.o
#GPU SDL part!
OBJS += emu/gpu/gpu_sdl.o

#Error handler!
OBJS += emu/core/errors.o

#Sound test utility.
OBJS += emu/debugger/debug_sound.o

#CPU emulation core file.
OBJS += emu/core/emucore.o
#BIOS POST
OBJS += emu/core/emu_bios_post.o

#Main subcores.
OBJS += emu/core/emu_main.o emu/core/emu_misc.o emu/core/number_optimizations.o emu/core/emu_vga_bios.o emu/core/emu_vga.o emu/core/emu_bios_sound.o emu/debugger/debug_graphics.o emu/debugger/debug_files.o

#Directory listing support!
OBJS += emu/io/directorylist.o

#Finally: main script!
OBJS += emu/main.o

#64-bit file support!
OBJS += basicio/fopen64.o

#Support multi platform builds:
include Makefile.multiplatform