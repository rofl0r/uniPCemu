#include "headers/emu/directorylist.h" //Our typedefs!
#include "headers/support/zalloc.h" //Zero allocation support!

byte isext(char *filename, char *extension)
{
	if (filename == NULL) return FALSE; //No ptr!
	if (extension == NULL) return FALSE; //No ptr!
	int startpos = safe_strlen(filename, 256) - safe_strlen(extension, 256) - safe_strlen(".", 2); //Start position of the extension!
	if (startpos<0) //Not available?
	{
		return 0; //Not this extension!
	}
	char *curchar;
	int counter = 0; //Counter!
	curchar = filename; //Take over!
	while (counter<startpos) //Goto start pos!
	{
		++curchar; //Next character!
		++counter; //Next position!
	}
	char ext[256]; //Temp extension holder!
	bzero(ext, sizeof(ext)); //Init!
	strcpy(ext, "."); //Init!
	strcat(ext, extension); //Add extension to compare!
	char *comparedata;
	comparedata = &ext[0]; //Start of the comparision!
	//Now we're at the startpos. MUST MATCH ALL CHARACTERS!
	int result = 1; //Default: match!
	while (counter<safe_strlen(filename, 256)) //Not end of string?
	{
		//Are we equal or not?
		if (toupper((int)*curchar) != toupper((int)*comparedata)) //Not equal (case insensitive)?
		{
			result = 0; //Not extension!
			break; //Stop comparing!
		}
		++comparedata; //Next character to compare!
		++curchar; //Next character in string to compare!
		++counter; //Next position!
	}

	return result; //Give the result: 1 for is extension, 0 for not extension!
}

byte opendirlist(DirListContainer_p dirlist, char *path, char *entry, byte *isfile) //Open a directory for reading, give the first entry if any!
{
	DirListContainer_t dirlist2; //Temp data!
	memset(dirlist->path,0,sizeof(dirlist->path)); //Clear the path!
#ifdef _WIN32
	//Windows?
	strcpy(dirlist->path,path); //Initialise the path!
	memcpy(&dirlist2,dirlist,sizeof(dirlist2)); //Copy the path!
	strcat(dirlist2.path,"\\*"); //Add the wildcard to the filename to search!
	StringCchCopy(dirlist->szDir, MAX_PATH, (STRSAFE_LPCWSTR)&dirlist2.path);
	dirlist->hFind = FindFirstFile(dirlist->szDir, &dirlist->ffd); //Find the first file!
	if (dirlist->hFind==INVALID_HANDLE_VALUE) //Invalid?
	{
		return 0; //Invalid handle: not usable!
	}
	//We now have the first entry, so give it!
	wcstombs(entry, dirlist->ffd.cFileName, wcslen(&dirlist->ffd.cFileName[0])); //Convert to viewable size!
	*isfile = ((dirlist->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)>0); //Are we a file?
	return 1; //We have a valid file loaded!
#else
	//PSP?
    if ((dirlist->dir = opendir (path)) == NULL) //Not found?
    {
		return 0; //No directory list: cannot open!
	}
	dirlist->dirent = NULL; //No entry yet!
	strcpy(dirlist->path,path); //Save the path!
	return readdirlist(dirlist,entry,isfile); //When opened, give the first entry, if any!
#endif
}
byte readdirlist(DirListContainer_p dirlist, char *entry, byte *isfile) //Read an entry from the directory list! Gives the next entry, if any!
{
	char filename[256]; //Full filename!
#ifdef _WIN32
	//Windows?
	if (FindNextFile(dirlist->hFind, &dirlist->ffd) != 0) //Found a next file?
	{
		wcstombs(entry, dirlist->ffd.cFileName, wcslen(&dirlist->ffd.cFileName[0])); //Convert to viewable size!
		*isfile = ((dirlist->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)>0); //Are we a file?
		return 1; //We have a valid file loaded!
	}
	return 0; //No file found!
#else
	//PSP?
	dirlist->dirent = readdir( dirlist->dir ); //Try to read the next entry!
	if (dirlist->dirent != NULL) //Valid entry?
	{
		strcpy(entry,dirlist->dirent->d_name); //Set the filename!
		memset(filename,0,sizeof(filename)); //Init!
		strcpy(filename,dirlist->path); //init to path!
		strcat(filename,"/"); //Add a directory seperator!
		strcat(filename,entry); //Add the filename!
		*isfile = file_exists(filename); //Does it exist as a file?
		return 1; //We're valid!
	}
	return 0; //We're invalid: we don't have entries anymore!
#endif
}
void closedirlist(DirListContainer_p dirlist) //Close an opened directory list!
{
#ifdef _WIN32
	FindClose(dirlist->hFind); //Close the directory!
#else
	//PSP?
	closedir(dirlist->dir); //Close the directory!
	memset(dirlist,0,sizeof(*dirlist)); //Clean the directory list information!
#endif
}
