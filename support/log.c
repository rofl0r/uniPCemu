#include "headers/types.h"
#include "headers/support/highrestimer.h" //Our own typedefs etc.

#ifdef __psp__
//The PSP uses a 1KB buffer, because it doesn't have much memory left!
#define __LOGBUFFER 1024
#else
//Log buffer size in bytes. The system we're running on will flush it every X bytes to disk. We use a 1MB buffer on Windows!
#define __LOGBUFFER 1024000
#endif

TicksHolder logticksholder; //Log ticks holder!
SDL_sem *log_Lock = NULL;
char lastfile[256] = ""; //Last file we've been logging to!
FILE *logfile = NULL; //The log file to use!

void donelog(void)
{
	if (logfile) fclose(logfile); //Close the last log file!
	SDL_DestroySemaphore(log_Lock);
}

void initlog()
{
	startHiresCounting(&logticksholder); //Init our timer to the starting point!
	log_Lock = SDL_CreateSemaphore(1); //Create our sephamore!
	atexit(&donelog); //Our cleanup function!
}

void dolog(char *filename, const char *format, ...) //Logging functionality!
{
	static char filenametmp[256];
	static char logtext[256];
	static char timestamp[256];
	static char CRLF[2] = {'\r','\n'}; //CRLF!
	va_list args; //Going to contain the list!
	uint_64 time;
	int dummy;

	//Lock
	SDL_SemWait(log_Lock); //Only one instance allowed!

	//First: init variables!
	bzero(filenametmp,sizeof(filenametmp)); //Init filename!
	bzero(logtext,sizeof(logtext)); //Init logging text!
	bzero(timestamp,sizeof(timestamp)); //Init timestamp text!
	
	strcpy(filenametmp,"logs/"); //Base directory!
	strcat(filenametmp,filename); //Add the filename to the directory!
	if (!*filename) //Empty filename?
	{
		strcpy(filenametmp,"unknown"); //Empty filename = unknown.log!
	}
	strcat(filenametmp,".log"); //Do log here!

	va_start (args, format); //Start list!
	vsprintf (logtext, format, args); //Compile list!
	va_end (args); //Destroy list!
	
	if (safe_strlen(logtext,sizeof(logtext))) //Got length?
	{
		//Lock
		time = getuspassed_k(&logticksholder); //Get the current time!
		convertTime(time,&timestamp[0]); //Convert the time!
		strcat(timestamp,": "); //Suffix!
	}

	if ((!logfile) || (strcmp(lastfile,filenametmp)!=0)) //Other file or new file?
	{
		if (logfile) fclose(logfile); //Close the old log if needed!
		dummy = mkdir("logs"); //Create a logs directory if needed!
		logfile = fopen(filenametmp, "r"); //Open for testing!
		if (logfile) //Existing?
		{
			fclose(logfile); //Close it!
			logfile = fopen(filenametmp, "a"); //Reopen for appending!
		}
		else
		{
			logfile = fopen(filenametmp, "w"); //Reopen for writing new!
		}
		strcpy(lastfile, filenametmp); //Set the last file we've opened!
		if (logfile) //We're opened?
		{
			setvbuf(logfile, NULL, _IOFBF, __LOGBUFFER); //Use this buffer!
		}
	}

	//Now log!
	if (logfile) //Opened?
	{
		if (safe_strlen(logtext,sizeof(logtext))) //Got length?
		{
			fwrite(&timestamp,1,safe_strlen(timestamp,sizeof(timestamp)),logfile); //Write the timestamp!
			fwrite(&logtext,1,safe_strlen(logtext,sizeof(logtext)),logfile); //Write string to file!
		}
		fwrite(&CRLF,1,sizeof(CRLF),logfile); //Write line feed!
	}

	//Unlock
	SDL_SemPost(log_Lock);
}