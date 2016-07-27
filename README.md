# README #

This README would normally document whatever steps are necessary to get your application up and running.

### What is this repository for? ###

* Quick summary
* Version

### How do I get set up? ###

* Summary of set up
- This repository goes into a folder named x86EMU. Place the tools support repository(https://bitbucket.org/superfury/tools.git) parralel to this repository folder(Required for PSP builds).
- Install Minimalist PSPSDK devkit(PSP) or Visual C++, MinGW(Windows) or GNU C++ toolchsin(Linux).
- Install SDL packages for the devkit, in C:\SDL for Windows, installers for MinPSPW and /mingw(SDL or SDL2).

* Configuration
- Make sure there is a compile directory parallel to the project directory(projects_build\x86emu) with a duplicate directory tree of the project repository(automatically createn by remake.bat on Windows).
* Dependencies
- See set up.
* Database configuration
None
* How to run tests
- Run the remake.bat file in the project directory(Requires tools repository) and use a PSP emulator to test(like JPCSP, which is supported by the batch file). On Windows, open the Visual C++ project, build and run.
* Deployment instructions
- Simply build using the devkit(Makefile command "make psp/win/linux [re]build [SDL2[-static]](to (re)build SDL2 with(out) static linking)" or Visual C++, copy the executable (x86EMU.exe for windows or EBOOT.PBP for the PSP) to the executable directory, add SDL dll when needed, add disk images to use and run the executable. Setting up is integrated into the executable, and it will automatically open the BIOS for setting up during first execution. The settings are saved into a BIOS.DAT file.

### Contribution guidelines ###

* Writing tests
* Code review
* Other guidelines

### Who do I talk to? ###

* Repo owner or admin