#include "headers/types.h"
#include "headers/support/highrestimer.h" //Our own typedefs etc.
TicksHolder logticksholder; //Log ticks holder!
void initlog()
{
	startHiresCounting(&logticksholder); //Init our timer to the starting point!
}

void dolog(char *filename, const char *format, ...) //Logging functionality!
{
	char emptystring = '\0'; //Empty string!
	if (!filename) filename = &emptystring;
	char filenametmp[256];
	bzero(filenametmp,sizeof(filenametmp)); //Init filename!
	char logtext[256];
	bzero(logtext,sizeof(logtext)); //Init logging text!
	char timestamp[256];
	bzero(timestamp,sizeof(timestamp)); //Init timestamp text!
	
	strcpy(filenametmp,"logs/"); //Base directory!
	strcat(filenametmp,filename); //Add the filename to the directory!
	if (!strcmp(filename,"")) //Empty filename?
	{
		strcpy(filenametmp,"unknown"); //Empty filename = unknown.log!
	}
	strcat(filenametmp,".log"); //Do log here!
	
	mkdir("logs"); //Create a logs directory if needed!
	
	va_list args; //Going to contain the list!
	va_start (args, format); //Start list!
	vsprintf (logtext, format, args); //Compile list!
	va_end (args); //Destroy list!
	
	if (safe_strlen(logtext,sizeof(logtext))) //Got length?
	{
		uint_64 time = getuspassed_k(&logticksholder); //Get the current time!
		convertTime(time,&timestamp[0]); //Convert the time!
		strcat(timestamp,": "); //Suffix!
	}

	FILE *f; //The file to use!
	f = fopen(filenametmp,"r"); //Open for testing!
	if (f) //Existing?
	{
		fclose(f); //Close it!
		f = fopen(filenametmp,"a"); //Reopen for appending!
	}
	else
	{
		f = fopen(filenametmp,"w"); //Reopen for writing new!
	}

	//Now log!
	if (f) //Opened?
	{
		if (safe_strlen(logtext,sizeof(logtext))) //Got length?
		{
			fwrite(&timestamp,1,safe_strlen(timestamp,sizeof(timestamp)),f); //Write the timestamp!
			fwrite(&logtext,1,safe_strlen(logtext,sizeof(logtext)),f); //Write string to file!
		}
		char CRLF[2] = "\r\n"; //CRLF!
		fwrite(&CRLF,1,sizeof(CRLF),f); //Write line feed!
		fclose(f); //Close the log!
	}
}