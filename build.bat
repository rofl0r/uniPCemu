@echo off
if NOT "%1"=="/?" goto exec
rem Help
echo Android profile usage:
echo build [apk [debug] [install]]
echo Execute this from the android-project folder itself. Builds with default settings and SDL2_net, if present in the jni\SDL2_net folder.
echo apk specifies to build an APK file.
echo debug specifies the APK to use a debug configuration instead of release.
echo install specifies the APK to be installed to the default emulator/device.
goto finishup
:exec
if "%1"=="apk" goto doapk
if exist jni\SDL2_net\SDL_net.h ndk-build NDK_DEBUG=0 useSDL2_net=1
if not exist jni\SDL2_net\SDL_net.h ndk-build NDK_DEBUG=0
goto finishup
:doapk
if "%2"=="debug" if "%3"=="" ant debug
if "%2"=="debug" if "%3"=="install" ant debug install
if not "%2"=="debug" if "%2"=="" ant release
if not "%2"=="debug" if "%2"=="install" ant release install
:finishup
