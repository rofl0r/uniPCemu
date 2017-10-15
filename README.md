# README #

This README would normally document whatever steps are necessary to get your application up and running.

### What is this repository for? ###

* Quick summary
* Version

### How do I get set up? ###

* Summary of set up
- This repository goes into a folder named x86EMU. Place the tools support repository(https://bitbucket.org/superfury/tools.git) parralel to this repository folder(Required for PSP builds).
- Install Minimalist PSPSDK devkit(PSP) or Visual C++, MinGW(Windows) or GNU C++ toolchsin(Linux).
- Install SDL packages for the devkit, in C:\SDL for Windows(copy SDL2-2.* folder contents to C:\SDL\SDL2 (and the SDL2_net folder to C:\SDL\SDL2_net), to use SDL 1.2.*, copy the folder contents to C:\SDL\SDL1.2.15(and the SDL_net folder to C:\SDL\SDL_net-1.2.8)), installers for MinPSPW and /mingw(SDL or SDL2). Open the projects within the VisualC subfolders(the solution file) and compile SDL2 and SDL2main. Also compile the SDL2_net project when used(after compiling SDL2 itself). Don't forget to add the paths C:\SDL\SDL2\include to both Win32 and x64 target include directories, as well as the C:\SDL\SDL2\VisualC\Win32\Release to the Win32 library directories and C:\SDL\SDL2\VisualC\x64\Release to the x64 library directories.
- Set the Visual C++ Local Windows Debugger to use "$(TargetDir)" for it's working directory, to comply with the other paths set in the project.

* Configuration
- Make sure there is a compile directory parallel to the project directory(projects_build\x86emu) with a duplicate directory tree of the project repository(automatically createn by remake.bat on Windows).
* Dependencies
- See set up.
* Adding the Android SDL2 build to the project
- Download the latest version of SDL2 from the project homepage. Copy the android-project\src\org directory to android-project/src. Copy the include and source directories, as well as the Android.mk file to the android-project/jni/SDL2 folder.
* Adding Android SDL2_net to the project
- Download the latest version of SDL2_net from the project homepage. Copy all c/h files and Android.mk to a newly created directory android-project\jni\SDL2_net folder. Edit android-project\src\org\libsdl\app\SDLActivity.java, removing // before '// "SDL2_net",'.
* How to run tests
- Run the remake.bat file in the project directory(Requires tools repository) and use a PSP emulator to test(like JPCSP, which is supported by the batch file). On Windows, open the Visual C++ project, build and run.
* Deployment instructions
- Simply build using the devkit(Makefile command "make psp/win/linux [re]build [SDL2[-static]](to (re)build SDL2 with(out) static linking)" or Visual C++, copy the executable (x86EMU.exe for windows or EBOOT.PBP for the PSP) to the executable directory, add SDL dll when needed, add disk images to use and run the executable. Setting up is integrated into the executable, and it will automatically open the BIOS for setting up during first execution. The settings are saved into a BIOS.DAT file.
- To make the Android NDK use the SDL2_net library and compile with internet support, add " useSDL2_net=1" to the usual ndk-build command line or equivalent. Otherwise, it isn't enabled/used.

### Contribution guidelines ###

* Writing tests
* Code review
* Other guidelines

### Who do I talk to? ###

* Repo owner or admin