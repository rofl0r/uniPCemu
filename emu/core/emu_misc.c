#include "headers/types.h" //Basic types!
#include "headers/emu/gpu/gpu_text.h" //Emulator support!
#include "headers/emu/gpu/gpu_framerate.h" //Framerate support!
#include "headers/emu/threads.h" //Multithreading support!
#include "headers/emu/directorylist.h" //Directory listing support!

int convertrel(int src, int fromres, int tores) //Relative int conversion!
{
	double data;
	data = src; //Load src!
	if (fromres!=0)
	{
		data /= (double)fromres; //Divide!
	}
	else
	{
		data = 0.0f; //Clear!
	}
	data *= (double)tores; //Generate the result!
	return (int)data; //Give the result!
}

//CONCAT support!
char concatinations_constsprintf[256];
char *constsprintf(char *text, ...)
{
	bzero(concatinations_constsprintf,sizeof(concatinations_constsprintf)); //Init!
	va_list args; //Going to contain the list!
	va_start (args, text); //Start list!
	vsprintf (concatinations_constsprintf, text, args); //Compile list!
	va_end (args); //Destroy list!
	
	return &concatinations_constsprintf[0]; //Give the concatinated string!
}

extern GPU_TEXTSURFACE *frameratesurface; //The framerate surface!
void BREAKPOINT() //Break point!
{
	termThreads(); //Terminate all other threads!
	GPU_textgotoxy(frameratesurface,0,0); //Left-up!
	GPU_textprintf(frameratesurface,RGB(0xFF,0xFF,0xFF),RGB(0,0,0),"Breakpoint reached!");
	renderFramerateOnly(); //Render the framerate surface only!
	sleep(); //Stop!
}

/*

FILE_EXISTS

*/

int FILE_EXISTS(char *filename)
{
	FILE *f = fopen(filename,"r"); //Try to open!
	if (!f) //Failed?
	{
		return 0; //Doesn't exist!
	}
	fclose(f); //Close the file!
	return 1; //Exists!
}

char *substr(char *s,int startpos) //Simple substr function, with string and starting position!
{
	if (strlen(s)<(startpos+1)) //Out of range?
	{
		return NULL; //Nothing!
	}
	return (char *)s+startpos; //Give the string from the start position!
}

void delete_file(char *directory, char *filename)
{
	if (!filename || !directory) return; // Not a file?
	if (*filename=='*') //Wildcarding?
	{
		char *f2 = substr(filename,1); //Take off the *!

		char direntry[256];
		byte isfile;
		DirListContainer_t dir;
		if (!opendirlist(&dir,directory,&direntry[0],&isfile))
		{
			return; //Nothing found!
		}
		
		/* open directory */
		do //Files left to check?
		{
			if ( (direntry[0] == '.') ) continue; //. or ..?
			if (strcmp(substr(direntry,strlen(direntry)-strlen(f2)),f2)==0) //Match?
			{
				delete_file(directory,direntry); //Delete the file!
			}
		}
		while (readdirlist(&dir,&direntry[0],&isfile)); //Files left to check?)
		closedirlist(&dir); //Close the directory!
		return; //Don't process normally!
	}
	//Compose the full path!
	char fullpath[256];
	memset(fullpath,0,sizeof(fullpath)); //Clear the directory!
	strcpy(fullpath,directory);
	strcat(fullpath,"/");
	strcat(fullpath,filename); //Full filename!

	FILE *f = fopen(fullpath,"r"); //Try to open!
	if (f) //Found?
	{
		fclose(f); //Close it!
		remove(fullpath); //Remove it, ignore the result!
	}
}


/*

Safe strlen function.

*/

uint_32 safe_strlen(char *str, int limit)
{
	if (str) //Valid?
	{
		//Valid address determined!
		if (!limit) //Unlimited?
		{
			return strlen(str); //Original function with unlimited length!
		}
		/*uint_32 address = (uint_32)str; //Address!
		if (!((address>=0x08800000) && (address<0x01800000))) //Invalid: not user memory?
		{
			return 0; //Invalid memory!
		}*/
		char *c = str; //Init to first character!
		int length = 0; //Init length to first character!
		for (;length<limit;) //Continue to the limit!
		{
			//Valid, check for EOS!
			if (*c=='\0' || !*c) //EOS?
			{
				return length; //Give the length!
			}
			++length; //Increase the current position!
			++c; //Increase the position!
		}
		return limit; //We're at the limit!
	}
	return 0; //No string = no length!
}

//Same as FILE_EXISTS?
int file_exists(char *filename)
{
	return FILE_EXISTS(filename); //Alias!
}

int move_file(char *fromfile, char *tofile)
{
	int result;
	if (file_exists(fromfile)) //Original file exists?
	{
		if (file_exists(tofile)) //Destination file exists?
		{
			if ((result = remove(tofile))!=0) //Error removing destination?
			{
				return result; //Error code!
			}
		}
		return rename(fromfile,tofile); //0 on success, anything else is an error code..
	}
	return 0; //Error: file doesn't exist, so success!
}

float RandomFloat(float min, float max)
{
    if (min>max) //Needs to be swapped?
    {
	float temp;
	temp = min;
	min = max;
	max = temp;
    }

    float random;
    random = ((float) rand()) / (float) RAND_MAX;

    double range = max - min;  
    return (float)((double)((random*range) + (double)min)); //Give the result within range!
}

float frand() //Floating point random
{
	return RandomFloat(FLT_MIN,FLT_MAX); //Generate a random float!
}

short RandomShort(short min, short max)
{
	return (short)RandomFloat(min,max); //Short random generator!
}

short shortrand() //Short random
{
	return (short)RandomShort(-SHRT_MAX,SHRT_MAX); //Short random generator!
}