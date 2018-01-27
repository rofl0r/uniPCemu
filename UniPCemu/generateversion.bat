@echo off
git describe --tags>gitversion2.txt
set /p version1=<gitversion1.txt
set /p version2=<gitversion2.txt
set /p version3=<gitversion3.txt
set whatversion=%version1%%version2%%version3%
echo %version%
rem copy /b gitversion1.txt + gitversion2.txt + gitversion3.txt gitcommitversiontest.tmp
rem findstr /R /C:. gitcommitversiontest.tmp>gitcommitversion.h
if NOT "%whatversion%"=="" echo %whatversion%>gitcommitversion.h
rem Make sure the file isn't committed to the repository anymore!
if NOT "%whatversion%"=="" git update-index --assume-unchanged 