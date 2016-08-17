#!/bin/sh

LOCAL_PATH=`dirname $0`
LOCAL_PATH=`cd $LOCAL_PATH && pwd`

JOBS=1

../setEnvironment-$1.sh sh -c "cd x86emu && make --makefile=Makefile PLATFORM=android -j1" && mv -f x86emu/x86EMU libapplication-$1.so

