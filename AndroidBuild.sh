#!/bin/sh

LOCAL_PATH=`dirname $0`
LOCAL_PATH=`cd $LOCAL_PATH && pwd`

JOBS=1

../setEnvironment-$1.sh sh -c "cd UniPCemu && make --makefile=Makefile PLATFORM=android -j1" && mv -f UniPCemu/UniPCemu libapplication-$1.so

