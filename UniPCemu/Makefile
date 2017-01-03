# Makefile to build and install the SDL library

top_builddir = .
srcdir  = .
objects = build
gen = gen
prefix = /mingw
exec_prefix = 
bindir	= bin
libdir  = lib
includedir = $(prefix)/include
datarootdir = data
datadir	= data
auxdir	= aux
distpath = $(srcdir)/..
distdir = UniPCemu-@SDL_VERSION@
distfile = $(distdir).tar.gz

CC      = gcc
INCLUDE = -I$(includedir)
CFLAGS  = 
EXTRA_CFLAGS = 
LDFLAGS = 
EXTRA_LDFLAGS = 
LIBTOOL = libtool
INSTALL = install
AR	= ar
RANLIB	= ranlib
WINDRES	= windres

ifeq (,$(ROOTPATH))
ROOTPATH = .
endif

#Support multi platform builds, also the list of used files:
include $(ROOTPATH)/Makefile.files