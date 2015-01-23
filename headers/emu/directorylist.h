#ifndef __DIRECTORYLIST_H
#define __DIRECTORYLIST_H

#include "headers/types.h" //Basic typedefs!

#ifdef _WIN32
//Windows specific structures!
#include <direct.h>
#include <windows.h>
#include <tchar.h> 
#include <strsafe.h>
#pragma comment(lib, "User32.lib")
#else
//PSP?
#include <dirent.h>
#endif//Are we disabled?

typedef struct
{
	char path[256]; //Full path!
#ifdef _WIN32
	TCHAR szDir[MAX_PATH];
	WIN32_FIND_DATA ffd;
	HANDLE hFind;
#else
	//PSP?
	DIR *dir;
	struct dirent *dirent;
#endif
} DirListContainer_t, *DirListContainer_p;

byte isext(char *filename, char *extension); //Are we a certain extension?
byte opendirlist(DirListContainer_p dirlist, char *path, char *entry, byte *isfile); //Open a directory for reading, give the first entry if any!
byte readdirlist(DirListContainer_p dirlist, char *entry, byte *isfile); //Read an entry from the directory list! Gives the next entry, if any!
void closedirlist(DirListContainer_p dirlist); //Close an opened directory list!

#endif