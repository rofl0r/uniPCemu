# This Makefile will build the MinGW Win32 application to convert the emulator into a proper executable (Windows executable).

OBJS = patchx86emu.o
INCLUDE_DIRS = 
BUILD_DIR = ../../projects_build/x86EMU
TARGET = x86emuWindowFix.exe

WARNS = -Wall

CC = gcc
LDFLAGS = -s -Wl,--subsystem,windows
RC = 

# Compile ANSI build only if CHARSET=ANSI
CFLAGS= -O3 ${WARNS}

#Apply directories!
OBJS := $(patsubst %.o,$(BUILD_DIR)/%.o,$(OBJS))
TARGET := $(BUILD_DIR)/$(TARGET)

all: $(TARGET)

$(TARGET): ${OBJS}
	${CC} -o "$@" ${OBJS} ${LDFLAGS}

clean:
	rm -f $(OBJS) $(TARGET)

$(BUILD_DIR)/%.o: %.c
	${CC} ${CFLAGS} ${INCLUDE_DIRS} -c $< -o $@