@echo off
set path=%path%;C:\androiddevdir\android-ndk-r12b;C:\androiddevdir\apache-ant-1.9.7\bin;C:\androiddevdir\android-sdk-windows\platform-tools
set JAVA_HOME=C:\Program Files\Java\jdk1.8.0_102
set ANDROID_HOME=C:\androiddevdir\android-sdk-windows
rem Set us up in the used project folder and start a command line session to work in!
cd android-project
cmd