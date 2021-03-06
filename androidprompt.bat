@echo off
set ANDROIDNDK=C:/androiddevdir/android-ndk
set path=%path%;%ANDROIDNDK%;C:\androiddevdir\apache-ant\bin;C:\androiddevdir\android-sdk-windows\platform-tools
rem Add support for the arm-linux-androideabi-gprof program!
set path=%path%;%ANDROIDNDK%\toolchains\arm-linux-androideabi-4.9\prebuilt\windows-x86_64\bin
rem Add support for our extra batch files
set path=%path%;%cd%
rem Add Java path as well to support keytool.
set path=%path%;c:\Program Files\Java\jre\bin
set JAVA_HOME=C:\Program Files\Java\jdk
set ANDROID_HOME=C:/androiddevdir/android-sdk-windows
set NDK_MODULE_PATH=%ANDROIDNDK%/sources
rem Set us up in the used project folder and start a command line session to work in!
cd android-project
if "%1"=="profile" ndk-build profile=%ANDROIDNDK% NDK_MODULE_PATH=%NDK_MODULE_PATH%
cmd