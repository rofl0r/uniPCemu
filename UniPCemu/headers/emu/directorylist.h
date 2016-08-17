#ifndef __DIRECTORYLIST_H
#define __DIRECTORYLIST_H

#include "headers/types.h" //Basic typedefs!

#if defined(IS_PSP) || defined (ANDROID)
#include <dirent.h> //PSP only required?
#endif//Are we disabled?

typedef struct
{
	char path[256]; //Full path!
#if !( defined(IS_PSP) || defined(ANDROID) )
	//Visual C++ and MinGW?
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