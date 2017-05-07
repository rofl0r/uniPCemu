@echo off
if NOT "%1"=="" goto exec
rem Help
echo Android profile usage:
echo profile [MODULENAME]
echo Execute this from the android-project folder itself. Output is stored in profiler.txt in the android-project folder.
goto finishup
:exec
cd bin
arm-linux-androideabi-gprof ..\obj\local\armeabi-v7a\%1 -PprofCount -QprofCount -P__gnu_mcount_nc -Q__gnu_mcount_nc>..\profiler.txt
cd ..
:finishup
