#include "headers/types.h" //Basic types!
#include "headers/support/zalloc.h" //Zero allocation support!

/*

This is a custom PSP&PC library for adding universal 64-bit fopen support to the project using platform-specific calls.

*/

typedef struct
{
	FILE filedummy; //For compatibility only!
	char filename[256]; //The full filename!
	uint_64 position; //The position!
	uint_64 size; //The size!
#ifdef IS_PSP
	//PSP only data:
	SceUID f; //The file opened using sceIoOpen!
	SceMode mode; //The mode!
#else
	//Windows/linux?
	FILE *f; //We use normal file operations combined with 64-bit call alternatives!
#endif
} BIGFILE; //64-bit fopen result!

int64_t emuftell64(FILE *stream)
{
	if (!stream) return -1LL; //Error!
	BIGFILE *b = (BIGFILE *)stream; //Convert!
	return b->position; //Our position!
}

int emufeof64(FILE *stream)
{
	if (!stream) return 1; //EOF!
	BIGFILE *b = (BIGFILE *)stream; //Convert!
	return (b->position >= b->size) ? 1 : 0; //Our eof marker is set with non-0 values!
}

int emufseek64(FILE *stream, int64_t pos, int direction)
{
	if (!stream) return -1; //Error!
	BIGFILE *b = (BIGFILE *)stream; //Convert!
#ifdef IS_PSP
									//Convert new direction!
	int newdir = PSP_SEEK_CUR;
	if (direction == SEEK_CUR) newdir = PSP_SEEK_CUR;
	if (direction == SEEK_END) newdir = PSP_SEEK_END;
	if (direction == SEEK_SET) newdir = PSP_SEEK_SET;
	b->position = sceIoLseek(b->f, pos, newdir); //Update position!
	return (b->position == pos) ? 0 : 1; //Give error (non-0) or OK(0)!
#else
									//Windows
	int result;
#ifdef VISUALC
	if (!(result = _fseeki64(b->f, pos, direction))) //Direction is constant itself!
	{
		b->position = _ftelli64(b->f); //Use our own position indicator!
	}
#elif defined(IS_WINDOWS) && defined(fseeko64)
	if (!(result = fseeko64(b->f, pos, direction))) //Direction is constant itself!
	{
		b->position = ftello64(b->f); //Use our own position indicator!
	}
#else
	if (!(result = fseek(b->f, pos, direction))) //Direction is constant itself!
	{
		b->position = ftell(b->f); //Use our own position indicator!
	}
#endif
	return result; //Give the result!
#endif
}

int emufflush64(FILE *stream)
{
	if (!stream) return 1; //EOF!
	BIGFILE *b = (BIGFILE *)stream; //Convert!
#ifdef IS_PSP
	//return sceIoSync("ms0:", 0); //Synchronize data on the device!
	return 1; //Seems to be fine without it?
#else
	//Windows/linux?
	return fflush(b->f); //Give the fflush result!
#endif
}

FILE *emufopen64(char *filename, char *mode)
{
	BIGFILE *stream;
	int length;
	if (!filename) //Invalid filename?
	{
		return NULL; //Invalid filename!
	}
	if (!safe_strlen(filename,255)) //Empty filename?
	{
		return NULL; //Invalid filename!
	}

	stream = (BIGFILE *)zalloc(sizeof(BIGFILE),"BIGFILE",NULL); //Allocate the big file!
	if (!stream) return NULL; //Nothing to be done!

	char *modeidentifier = mode; //First character of the mode!
	if (strcmp(modeidentifier,"")==0) //Nothing?
	{
		freez((void **)&stream,sizeof(BIGFILE),"Unused BIGFILE");
		return NULL; //Failed!
	}
	length = safe_strlen(mode, 255); //Safe length!
	while (length--) //Process the mode string!
	{
			switch (*modeidentifier) //What identifier have we found?
			{
			#ifdef IS_PSP
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
					case 'b': //Binary mode?
						break; //Ignore binary mode: we're always binary mode!
					case '+': //Mixed mode?
						stream->mode &= ~(PSP_O_WRONLY|PSP_O_RDONLY); //Disable write/read only and truncate flags!
						stream->mode |= PSP_O_RDWR; //Set read/write flag!
						break;
			#else
				//Windows uses almost the default API.
				case 'w':
				case 'r':
				case 'a':
				case 'b':
				case '+':
					break; //Ignore the flags!
			#endif
			default: //Unknown modifier?
				freez((void **)&stream,sizeof(BIGFILE),"fopen64@UnknownModifier"); //Cleanup!
				return NULL; //Failed!
				break;
		}
		++modeidentifier; //Next identifier!
	}
#ifdef IS_PSP
	stream->f = sceIoOpen(filename,stream->mode,0777); //Open the file!
	if (!stream->f || ((stream->f&0x8F000000)==0x80000000)) //Failed?
	{
		freez((void **)&stream,sizeof(*stream),"fopen@InvalidStream"); //Free it!
		return NULL; //Failed!
	}
#else
	//Windows?
	stream->f = fopen(filename, mode); //Just call fopen!
	if (!stream->f) //Failed?
	{
		freez((void **)&stream, sizeof(*stream), "fopen@InvalidStream"); //Free it!
		return NULL; //Failed!
	}
#endif
	bzero(&stream->filename,sizeof(stream->filename)); //Init filename buffer!
	strcpy(&stream->filename[0],filename); //Set the filename!

	//Detect file size!
	if (!emufseek64((FILE *)stream,0,SEEK_END)) //Seek to eof!
	{
		stream->size = emuftell64((FILE *)stream); //File size detected!
		//Windows?
		emufseek64((FILE *)stream,0,SEEK_SET); //Seek to bof again!
	}
	return (FILE *)stream; //Opened!
}

int64_t emufwrite64(void *data,int64_t multiplication,int64_t size,FILE *stream)
{
	if (!stream) return -1; //Error!
	BIGFILE *b = (BIGFILE *)stream; //Convert!
#ifdef IS_PSP
	SceSize numwritten = sceIoWrite(b->f,data,multiplication*size); //Try to write!
#else
	//Windows?
	uint_64 numwritten = fwrite(data,(size_t)multiplication,(size_t)size,b->f); //nothing written!
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

int64_t emufread64(void *data,int64_t multiplication,int64_t size,FILE *stream)
{
	if (!stream) return -1; //Error!
	BIGFILE *b = (BIGFILE *)stream; //Convert!
#ifdef IS_PSP
	SceSize numread = sceIoRead(b->f,data,multiplication*size); //Try to write!
#else
	uint_32 numread = fread(data,(size_t)multiplication,(size_t)size,b->f); //Nothing read!
#endif
	if (numread>0) //No error?
	{
		b->position += numread; //Add to the position!
	}
	return numread; //The size written!
}

int emufclose64(FILE *stream)
{
	if (!stream)
	{
		return -1; //Error!
	}
	BIGFILE *b = (BIGFILE *)stream; //Convert!
	char filename[256];
	bzero(filename,sizeof(filename));
	strcpy(filename,b->filename); //Set filename!
#ifdef IS_PSP
	if (sceIoClose(b->f)<0) //Error?
	{
		return EOF; //Error!
	}
	b->f = 0; //Nothing!
#else
	//Windows?
	if (fclose(b->f) == EOF)
	{
		return EOF; //Error closing the file!
	}
#endif
	freez((void **)&b,sizeof(*b),"fclose@Free_BIGFILE"); //Free the object safely!
	if (b) //Still set?
	{
		return EOF; //Error!
	}
	return 0; //OK!
}
