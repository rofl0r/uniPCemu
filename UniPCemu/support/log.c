#include "headers/types.h"
#include "headers/support/highrestimer.h" //Our own typedefs etc.

#ifdef IS_PSP
//The PSP uses a 1KB buffer, because it doesn't have much memory left!
#define __LOGBUFFER 1024
#else
//Log buffer size in bytes. The system we're running on will flush it every X bytes to disk. We use a 1MB buffer on Windows!
#define __LOGBUFFER 1024000
#endif

TicksHolder logticksholder; //Log ticks holder!
SDL_sem *log_Lock = NULL;
SDL_sem *log_stampLock = NULL;
byte log_timestamp = 1; //Are we to log the timestamp?
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
	SDL_DestroySemaphore(log_stampLock);
}

char log_filenametmp[256];
char log_logtext[8196], log_logtext2[8196]; //Original and prepared text!
char log_thetimestamp[256];


void initlog()
{
	initTicksHolder(&logticksholder); //Initialize our time!
	startHiresCounting(&logticksholder); //Init our timer to the starting point!
	log_Lock = SDL_CreateSemaphore(1); //Create our sephamore!
	log_stampLock = SDL_CreateSemaphore(1); //Create our sephamore!
	atexit(&donelog); //Our cleanup function!
	cleardata(&log_filenametmp[0],sizeof(log_filenametmp)); //Init filename!
	cleardata(&log_logtext[0],sizeof(log_logtext)); //Init logging text!
	cleardata(&log_logtext2[0],sizeof(log_logtext2)); //Init logging text!
	cleardata(&log_thetimestamp[0],sizeof(log_thetimestamp)); //Init timestamp text!
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

byte log_logtimestamp(byte logtimestamp)
{
	byte result;
	WaitSem(log_stampLock) //Only one instance allowed!
	result = log_timestamp; //Are we loggin the timestamp?
	if (logtimestamp<2) //Valid?
	{
		log_timestamp = (logtimestamp!=0)?1:0; //Set the new timestamp setting!
	}
	//Unlock
	PostSem(log_stampLock)
	return result; //Give the result!
}

void closeLogFile(byte islocked)
{
	if (islocked==0) WaitSem(log_Lock) //Only one instance allowed!
	//PSP doesn't buffer, because it's too slow! Android is still in the making, so constantly log, don't buffer!
	if (unlikely(logfile)) //Are we logging?
	{
		fclose(logfile);
		logfile = NULL; //We're finished!
	}
	if (islocked==0) PostSem(log_Lock)
}

void dolog(char *filename, const char *format, ...) //Logging functionality!
{
	word i;
	uint_32 logtextlen = 0;
	char c, newline=0;
	va_list args; //Going to contain the list!
	float time;

	//Lock
	WaitSem(log_Lock) //Only one instance allowed!

	//First: init variables!
	strcpy(&log_filenametmp[0],""); //Init filename!
	strcpy(&log_logtext[0],""); //Init logging text!
	strcpy(log_thetimestamp,""); //Init timestamp text!
	
	strcpy(log_filenametmp,logpath); //Base directory!
	strcat(log_filenametmp,"/");
	strcat(log_filenametmp,filename); //Add the filename to the directory!
	if (!*filename) //Empty filename?
	{
		strcpy(log_filenametmp,"unknown"); //Empty filename = unknown.log!
	}
	#ifdef ANDROID
	strcat(log_filenametmp,".txt"); //Do log here!
	#else
	strcat(log_filenametmp,".log"); //Do log here!
	#endif

	va_start (args, format); //Start list!
	vsprintf (log_logtext, format, args); //Compile list!
	va_end (args); //Destroy list!

	strcpy(log_logtext2,""); //Clear the data to dump!

	logtextlen = safe_strlen(log_logtext, sizeof(log_logtext)); //Get our length to log!
	for (i=0;i<logtextlen;) //Process the log text!
	{
		c = log_logtext[i++]; //Read the character to process!
		if ((c == '\n') || (c == '\r')) //Newline character?
		{
			//we count \n, \r, \n\r and \r\n as the same: newline!
			if (!newline) //First newline character?
			{
				addnewline(&log_logtext2[0]); //Flush!
				newline = c; //Detect for further newlines!
			}
			else //Second newline+?
			{
				if (newline == c) //Same newline as before?
				{
					addnewline(&log_logtext2[0]); //Flush!
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
			sprintf(log_logtext2, "%s%c", log_logtext2, c); //Add to the debugged data!
		}
	}

	if (safe_strlen(log_logtext2,sizeof(log_logtext2)) && log_logtimestamp(2)) //Got length and logging timestamp?
	{
		time = getuspassed_k(&logticksholder); //Get the current time!
		convertTime(time,&log_thetimestamp[0]); //Convert the time!
		strcat(log_thetimestamp,": "); //Suffix!
	}

	if ((!logfile) || (strcmp(lastfile,log_filenametmp)!=0)) //Other file or new file?
	{
		closeLogFile(1); //Close the old log if needed!
		domkdir(logpath); //Create a logs directory if needed!
		logfile = fopen(log_filenametmp, "rb"); //Open for testing!
		if (logfile) //Existing?
		{
			fclose(logfile); //Close it!
			logfile = fopen(log_filenametmp, "ab"); //Reopen for appending!
		}
		else
		{
			logfile = fopen(log_filenametmp, "wb"); //Reopen for writing new!
		}
		strcpy(lastfile, log_filenametmp); //Set the last file we've opened!
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
		if (safe_strlen(log_logtext2,sizeof(log_logtext2))) //Got length?
		{
			fwrite(&log_thetimestamp,1,safe_strlen(log_thetimestamp,sizeof(log_thetimestamp)),logfile); //Write the timestamp!
			fwrite(&log_logtext2,1,safe_strlen(log_logtext2,sizeof(log_logtext2)),logfile); //Write string to file!
		}
		fwrite(&lineending,1,sizeof(lineending),logfile); //Write the line feed appropriate for the system after any write operation!
#if defined(IS_PSP) || defined(ANDROID)
		closeLogFile(1); //Close the current log file!
#endif

	}

	//Unlock
	PostSem(log_Lock)
}