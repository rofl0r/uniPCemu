#include "headers/types.h" //Basic types!
#include "headers/support/zalloc.h" //Zero allocation support!
#include "headers/fopen64.h" //64-bit fopen support!

#ifdef __psp__
//This is for the PSP only: we're missing normal 64-bit support!
#include <pspkernel.h> //Kernel support!
#endif
/*

This is a custom PSP library for adding 64-bit fopen support to the project.

*/

typedef struct
{
	FILE filedummy; //For compatibility only!
	char filename[256]; //The full filename!
	uint_64 position; //The position!
	uint_64 size; //The size!
#ifdef __psp__
	//PSP only data:
	SceUID f; //The file opened using sceIoOpen!
	SceMode mode; //The mode!
#endif
} BIGFILE; //64-bit fopen result!

FILE *fopen64(char *filename, char *mode)
{
	if (!filename) //Invalid filename?
	{
		return NULL; //Invalid filename!
	}
	if (!filename[0]) //Empty filename?
	{
		return NULL; //Invalid filename!
	}

	BIGFILE *stream;
	stream = (BIGFILE *)zalloc(sizeof(BIGFILE),"BIGFILE"); //Allocate the big file!
	if (!stream) return NULL; //Nothing to be done!
	
	char *modeidentifier = mode; //First character of the mode!
	if (*modeidentifier=='\0') //Nothing?
	{
		freez((void **)&stream,sizeof(BIGFILE),"Unused BIGFILE");
		return NULL; //Failed!
	}
	while (*modeidentifier!='\0') //Process the mode string!
	{
			switch (*modeidentifier++) //What identifier have we found?
			{
			#ifdef __psp__
				//PSP-only flags!
					case 'w': //Write?
						stream->mode |= PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC; //Write, create and truncate!
						break;
					case 'r': //Read?
						stream->mode |= PSP_O_RDONLY; //Read only, file must exist!
						break;
					case 'a': //Append?
						stream->mode |= PSP_O_APPEND|PSP_O_CREAT|PSP_O_WRONLY; //Append, create and write only!
						break;
					case '+': //Mixed mode?
						stream->mode &= ~(PSP_O_WRONLY|PSP_O_RDONLY); //Disable write/read only and truncate flags!
						stream->mode |= PSP_O_RDWR; //Set read/write flag!
						break;
			#endif
			default: //Unknown modifier?
				freez((void **)&stream,sizeof(BIGFILE),"fopen64@UnknownModifier"); //Cleanup!
				return NULL; //Failed!
				break;
		}
	}
#ifdef __psp
	stream->f = sceIoOpen(filename,stream->mode,0777); //Open the file!
	if (!stream->f || ((stream->f&0x8F000000)==0x80000000)) //Failed?
	{
		freez((void **)&stream,sizeof(*stream),"fopen@InvalidStream"); //Free it!
		return NULL; //Failed!
	}
#else
	//Windows?
		freez((void **)&stream,sizeof(*stream),"fopen@InvalidStream"); //Free it!
		return NULL; //Failed!
#endif
	bzero(&stream->filename,sizeof(stream->filename)); //Init filename buffer!
	strcpy(stream->filename,filename); //Set the filename!
	
	//Detect file size!
	if (!fseek64((FILE *)stream,0,SEEK_END)) //Seek to eof!
	{
#ifdef __psp__
		stream->size = ftell64((FILE *)stream); //File size detected!
#else
		//Windows?
#endif
		fseek64((FILE *)stream,0,SEEK_SET); //Seek to bof again!
	}
	return (FILE *)stream; //Opened!
}

int fseek64(FILE *stream, int64_t pos, int direction)
{
	if (!stream) return -1; //Error!
	BIGFILE *b = (BIGFILE *)stream; //Convert!
#ifdef __psp__
	//Convert new direction!
	int newdir = PSP_SEEK_CUR;
	if (direction==SEEK_CUR) newdir = PSP_SEEK_CUR;
	if (direction==SEEK_END) newdir = PSP_SEEK_END;
	if (direction==SEEK_SET) newdir = PSP_SEEK_SET; 
	b->position = sceIoLseek(b->f,pos,newdir); //Update position!
	return (b->position==pos)?0:1; //Give error (non-0) or OK(0)!
#else
	//Windows
	return 0; //Skip!
#endif
}

int64_t fwrite64(void *data,int64_t multiplication,int64_t size,FILE *stream)
{
	if (!stream) return -1; //Error!
	BIGFILE *b = (BIGFILE *)stream; //Convert!
#ifdef __psp__
	SceSize numwritten = sceIoWrite(b->f,data,multiplication*size); //Try to write!
#else
	//Windows?
	uint_64 numwritten = 0; //nothing written!
#endif
	if (numwritten>0) //No error?
	{
		b->position += numwritten; //Add to the position!
	}
	if (b->position>b->size) //Overflow?
	{
		b->size = b->position; //Update the size!
	}
	return numwritten; //The size written!
}

int64_t fread64(void *data,int64_t multiplication,int64_t size,FILE *stream)
{
	if (!stream) return -1; //Error!
	BIGFILE *b = (BIGFILE *)stream; //Convert!
#ifdef __psp__
	SceSize numread = sceIoRead(b->f,data,multiplication*size); //Try to write!
#else
	uint_32 numread = 0; //Nothing read!
#endif
	if (numread>0) //No error?
	{
		b->position += numread; //Add to the position!
	}
	return numread; //The size written!
}

int64_t ftell64(FILE *stream)
{
	if (!stream) return -1LL; //Error!
	BIGFILE *b = (BIGFILE *)stream; //Convert!
#ifdef __psp__
	return b->position; //Our position!
#else
	return 0; //Nothing: not supported!
#endif
}

int feof64(FILE *stream)
{
	if (!stream) return 1; //EOF!
	BIGFILE *b = (BIGFILE *)stream; //Convert!
#ifdef __psp__
	return (b->position>=b->size)?1:0; //Our eof marker is set with non-0 values!
#else
	return 0; //Nothing: not supported!
#endif
}

int fclose64(FILE *stream)
{
	if (!stream)
	{
		return -1; //Error!
	}
	BIGFILE *b = (BIGFILE *)stream; //Convert!
	char filename[256];
	bzero(filename,sizeof(filename));
	strcpy(filename,b->filename); //Set filename!
#ifdef __psp__
	if (sceIoClose(b->f)<0) //Error?
	{
		return EOF; //Error!
	}
	b->f = 0; //Nothing!
#else
	//Windows?
	//Not supported!
#endif
	freez((void **)&b,sizeof(*b),"fclose@Free_BIGFILE"); //Free the object safely!
	if (b) //Still set?
	{
		return EOF; //Error!
	}
	return 0; //OK!
}