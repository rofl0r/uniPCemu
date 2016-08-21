#include "headers/types.h"
#include "headers/support/highrestimer.h" //Our own typedefs etc.

#ifdef IS_PSP
//The PSP uses a 1KB buffer, because it doesn't have much memory left!
#define __LOGBUFFER 1024
#else
//Log buffer size in bytes. The system we're running on will flush it every X bytes to disk. We use a 1MB buffer on Windows!
//#define __LOGBUFFER 1024000
#endif

TicksHolder logticksholder; //Log ticks holder!
SDL_sem *log_Lock = NULL;
char lastfile[256] = ""; //Last file we've been logging to!
FILE *logfile = NULL; //The log file to use!

char logpath[256] = "logs"; //Log path!

//Windows line-ending = \r\n. Wordpad line-ending is \n.

#ifdef WINDOWS_LINEENDING
char lineending[2] = {'\r','\n'}; //CRLF!
#else
char lineending[1] = {'\n'}; //Line-ending used in other systems!
#endif

void donelog(void)
{
	if (logfile) fclose(logfile); //Close the last log file!
	SDL_DestroySemaphore(log_Lock);
}

void initlog()
{
	initTicksHolder(&logticksholder); //Initialize our time!
	startHiresCounting(&logticksholder); //Init our timer to the starting point!
	log_Lock = SDL_CreateSemaphore(1); //Create our sephamore!
	atexit(&donelog); //Our cleanup function!
}

OPTINLINE void addnewline(char *s)
{
	char s2[2] = {'\0','\0'}; //String to add!
	byte i;
	for (i = 0;i < sizeof(lineending);i++) //Process the entire line-ending!
	{
		s2[0] = lineending[i]; //The character to add!
		strcat(s,s2); //Add the line-ending character(s) to the text!
	}
}

void dolog(char *filename, const char *format, ...) //Logging functionality!
{
	static char filenametmp[256];
	static char logtext[256], logtext2[512]; //Original and prepared text!
	static char timestamp[256];
	word i, logtextlen = 0;
	char c, newline=0;
	va_list args; //Going to contain the list!
	float time;

	//Lock
	WaitSem(log_Lock) //Only one instance allowed!

	//First: init variables!
	bzero(filenametmp,sizeof(filenametmp)); //Init filename!
	bzero(logtext,sizeof(logtext)); //Init logging text!
	bzero(timestamp,sizeof(timestamp)); //Init timestamp text!
	
	strcpy(filenametmp,logpath); //Base directory!
	strcat(filenametmp,"/");
	strcat(filenametmp,filename); //Add the filename to the directory!
	if (!*filename) //Empty filename?
	{
		strcpy(filenametmp,"unknown"); //Empty filename = unknown.log!
	}
	#ifdef ANDROID
	strcat(filenametmp,".txt"); //Do log here!
	#else
	strcat(filenametmp,".log"); //Do log here!
	#endif

	va_start (args, format); //Start list!
	vsprintf (logtext, format, args); //Compile list!
	va_end (args); //Destroy list!

	memset(&logtext2,0,sizeof(logtext2)); //Clear the data to dump!

	logtextlen = safe_strlen(logtext, 256); //Get our length to log!
	for (i=0;i<logtextlen;) //Process the log text!
	{
		c = logtext[i++]; //Read the character to process!
		if ((c == '\n') || (c == '\r')) //Newline character?
		{
			//we count \n, \r, \n\r and \r\n as the same: newline!
			if (!newline) //First newline character?
			{
				addnewline(&logtext2[0]); //Flush!
				newline = c; //Detect for further newlines!
			}
			else //Second newline+?
			{
				if (newline == c) //Same newline as before?
				{
					addnewline(&logtext2[0]); //Flush!
					//Continue counting newlines!
				}
				else //No newline, clear the newline flag!
				{
					newline = 0; //Not a newline anymore!
				}
			}
		}
		else //Normal character?
		{
			newline = 0; //Not a newline character anymore!
			sprintf(logtext2, "%s%c", logtext2, c); //Add to the debugged data!
		}
	}

	if (safe_strlen(logtext2,sizeof(logtext2))) //Got length?
	{
		//Lock
		time = getuspassed_k(&logticksholder); //Get the current time!
		convertTime(time,&timestamp[0]); //Convert the time!
		strcat(timestamp,": "); //Suffix!
	}

	if ((!logfile) || (strcmp(lastfile,filenametmp)!=0)) //Other file or new file?
	{
		if (logfile) fclose(logfile); //Close the old log if needed!
		domkdir(logpath); //Create a logs directory if needed!
		logfile = fopen(filenametmp, "rb"); //Open for testing!
		if (logfile) //Existing?
		{
			fclose(logfile); //Close it!
			logfile = fopen(filenametmp, "ab"); //Reopen for appending!
		}
		else
		{
			logfile = fopen(filenametmp, "wb"); //Reopen for writing new!
		}
		strcpy(lastfile, filenametmp); //Set the last file we've opened!
#ifdef __LOGBUFFER
		if (logfile) //We're opened?
		{
			setvbuf(logfile, NULL, _IOFBF, __LOGBUFFER); //Use this buffer!
		}
#endif
	}

	//Now log!
	if (logfile) //Opened?
	{
		if (safe_strlen(logtext2,sizeof(logtext2))) //Got length?
		{
			fwrite(&timestamp,1,safe_strlen(timestamp,sizeof(timestamp)),logfile); //Write the timestamp!
			fwrite(&logtext2,1,safe_strlen(logtext2,sizeof(logtext2)),logfile); //Write string to file!
		}
		fwrite(&lineending,1,sizeof(lineending),logfile); //Write the line feed appropriate for the system after any write operation!
#if defined(IS_PSP) || defined(ANDROID)
		//PSP doesn't buffer, because it's too slow!
		fclose(logfile);
		logfile = NULL; //We're finished!
#endif

	}

	//Unlock
	PostSem(log_Lock)
}