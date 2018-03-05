@echo off
xcopy C:\SDL\SDL-1.2.15\VisualC\SDL\Release\SDL.dll ..\..\projects_build\UniPCemu /Y>nul
xcopy C:\SDL\SDL_net-1.2.8\VisualC\x64\Release\SDL_net.dll ..\..\projects_build\UniPCemu /Y>nul
echo Dependencies are updated!