@echo off
if NOT "%1"=="/?" goto exec
rem Help
echo Android profile usage:
echo build [install [debug]]
echo Execute this from the android-project folder itself. Builds with default settings and SDL2_net, if present in the jni\SDL2_net folder.
goto finishup
:exec
if "%1"=="install" goto doinstall
if exist jni\SDL2_net\SDL_net.h ndk-build NDK_DEBUG=0 useSDL2_net=1
if not exist jni\SDL2_net\SDL_net.h ndk-build NDK_DEBUG=0
goto finishup
:doinstall
if "%2"=="debug" ant debug
if not "%2"=="debug" ant release
:finishup
