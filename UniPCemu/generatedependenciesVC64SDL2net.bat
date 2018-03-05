@echo off
xcopy C:\SDL\SDL2\VisualC\x64\Release\SDL2.dll ..\..\projects_build\UniPCemu /Y>nul
xcopy C:\SDL\SDL2_net\VisualC\x64\Release\SDL2_net.dll ..\..\projects_build\UniPCemu /Y>nul
echo Dependencies are updated!