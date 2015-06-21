# README #

This README would normally document whatever steps are necessary to get your application up and running.

### What is this repository for? ###

* Quick summary
* Version
* [Learn Markdown](https://bitbucket.org/tutorials/markdowndemo)

### How do I get set up? ###

* Summary of set up
- This repository goes into \psp-projects\x86EMU
- Install Minimalist PSPSDK devkit(PSP) or Visual C++(Windows)
- Install SDL packages for the devkit, in C:\SDL for Windows, installers for MinPSPW.
- Install tools (scripts) required for compilation (Tools repository) to \psp-projects\tools.
* Configuration
- Make sure there is a compile directory two directories above the source directory (\psp-projects_build).
* Dependencies
- See set up.
* Database configuration
None
* How to run tests
- Run the remake.bat file in the project directory and use a PSP emulator to verify. On Windows, open the Visual C++ project, build and run.
* Deployment instructions
- Simply build using the devkit or Visual C++, copy the executable (x86EMU.exe for windows or EBOOT.PBP for the PSP) to the executable directory, add disk images and run. Setting up is integrated into the executable, and it will automatically open the BIOS for setting up during first execution. The settings are saved into a BIOS.DAT file.

### Contribution guidelines ###

* Writing tests
* Code review
* Other guidelines

### Who do I talk to? ###

* Repo owner or admin
* Other community or team contact