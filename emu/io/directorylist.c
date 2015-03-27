#include "headers/emu/directorylist.h" //Our typedefs!
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/support/log.h" //Debugging!

byte isext(char *filename, char *extension)
{
	char ext[256]; //Temp extension holder!
	if (filename == NULL) return FALSE; //No ptr!
	if (extension == NULL) return FALSE; //No ptr!
	char temp[256];
	bzero(temp, sizeof(temp));
	strcat(temp, "|"); //Starting delimiter!
	strcat(temp, extension);
	strcat(temp, "|"); //Finishing delimiter!
	char *curchar;
	byte result;
	int counter; //Counter!
	extension = strtok(temp, "|"); //Start token!
	for (;safe_strlen(extension,256);) //Not an empty string?
	{
		bzero(ext, sizeof(ext)); //Init!
		strcpy(ext, "."); //Init!
		strcat(ext, extension); //Add extension to compare!
		int startpos = safe_strlen(filename, 256) - safe_strlen(ext, 256); //Start position of the extension!
		result = 0; //Default: not there yet!
		if (startpos >= 0) //Available?
		{
			char *comparedata;
			comparedata = &ext[0]; //Start of the comparision!
			curchar = &filename[startpos]; //Start of the extension!
			//Now we're at the startpos. MUST MATCH ALL CHARACTERS!
			result = 1; //Default: match!
			counter = 0; //Process the complete extension!
			while (counter < safe_strlen(ext, 256)) //Not end of string?
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
		}
		if (result) return 1; //Found an existing extension!
		extension = strtok(NULL, "|"); //Next token!
	}

	return 0; //NOt he extension!
}

void get_filename(const wchar_t *src, char *dest)
{
	wcstombs(dest,src , 256); //Convert to viewable size!
}

byte opendirlist(DirListContainer_p dirlist, char *path, char *entry, byte *isfile) //Open a directory for reading, give the first entry if any!
{
	memset(dirlist->path,0,sizeof(dirlist->path)); //Clear the path!
#ifdef _WIN32
	char pathtmp[256]; //Temp data!
	//Windows?
	strcpy(pathtmp,path); //Initialise the path!
	strcat(pathtmp,"\\*.*"); //Add the wildcard to the filename to search!

	//Create the path variable!
	mbstowcs((wchar_t *)&dirlist->szDir, (const char *)pathtmp, sizeof(dirlist->szDir) / sizeof(wchar_t)); //Convert to TCHAR variable!

	dirlist->hFind = FindFirstFile(dirlist->szDir, &dirlist->ffd); //Find the first file!
	if (dirlist->hFind==INVALID_HANDLE_VALUE) //Invalid?
	{
		return 0; //Invalid handle: not usable!
	}
	//We now have the first entry, so give it!
	//get_filename(dirlist->ffd.cFileName, entry); //Convert filename found!
	get_filename(dirlist->ffd.cFileName, entry); //Copy the filename!
	*isfile = ((dirlist->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0); //Are we a file?
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
		get_filename(dirlist->ffd.cFileName,entry); //Copy the filename!
		*isfile = ((dirlist->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0); //Are we a file?
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
