/***** Routines to read profile strings --  by Joseph J. Graf, modified by Superfury ******/
#include "headers/types.h" //Basic support!
#include "headers/support/iniparser.h"   /* function prototypes in here */

/*****************************************************************
* Function:     read_line()
* Arguments:    <FILE *> fp - a pointer to the file to be read from
*               <char *> bp - a pointer to the copy buffer
* Returns:      TRUE if successful FALSE otherwise
******************************************************************/
int read_line(FILE *fp, char *bp)
{
	int c = (int)'\0';
    int i = 0;
    /* Read one line from the source file */
    while( (c = getc(fp)) != '\n' )
    {
		if (i>(MAX_LINE_LENGTH-1)) return (-1); //Overflow detected!
		if( c == EOF )         /* return FALSE on unexpected EOF */
            return(0);
		bp[i++] = (char)c;
    }
    bp[i] = '\0';
    return(1);
}

/**************************************************************************
* Function:     get_private_profile_string()
* Arguments:    <char *> section - the name of the section to search for
*               <char *> entry - the name of the entry to find the value of
*               <char *> def - default string in the event of a failed read
*               <char *> buffer - a pointer to the buffer to copy into
*               <int> buffer_len - the max number of characters to copy
*               <char *> file_name - the name of the .ini file to read from
* Returns:      the number of characters copied into the supplied buffer
***************************************************************************/
int get_private_profile_string(char *section, char *entry, char *def,
    char *buffer, int buffer_len, char *file_name)
{   FILE *fp = fopen(file_name,"r");
    char buff[MAX_LINE_LENGTH];
    char *ep;
    char t_section[MAX_LINE_LENGTH];
    int len = strlen(entry);
    if( !fp ) return(0);
    sprintf(t_section,"[%s]",section);    /* Format the section name */
    /*  Move through file 1 line at a time until a section is matched or EOF */
    do
    {
		commentline: //Skip a comment line!
		if( (read_line(fp,buff)<=0) )
        {   fclose(fp);
            strncpy(buffer,def,buffer_len);
            return(strlen(buffer));
        }
		if (buff[0]==';') //Comment?
		{
			goto commentline;
		}
    }
    while( strcmp(buff,t_section) );
    /* Now that the section has been found, find the entry.
     * Stop searching upon leaving the section's area. */
    do
    {   if( (read_line(fp,buff)<=0) || buff[0] == '\0' )
        {   fclose(fp);
            strncpy(buffer,def,buffer_len);
            return(strlen(buffer));
        }
    }  while( strncmp(buff,entry,len) );
    ep = strrchr(buff,'=');    /* Parse out the equal sign */
    ep++;
    /* Copy up to buffer_len chars to buffer */
    strncpy(buffer,ep,buffer_len - 1);

    buffer[buffer_len] = '\0';
    fclose(fp);               /* Clean up and return the amount copied */
    return(strlen(buffer));
}

/************************************************************************
* Function:     get_private_profile_int()
* Arguments:    <char *> section - the name of the section to search for
*               <char *> entry - the name of the entry to find the value of
*               <int> def - the default value in the event of a failed read
*               <char *> file_name - the name of the .ini file to read from
* Returns:      the value located at entry
*************************************************************************/
int_64 get_private_profile_int64(char *section,
    char *entry, int_64 def, char *file_name)
{
    char value[MAX_LINE_LENGTH];
    char ep[MAX_LINE_LENGTH];
	memset(&value,0,sizeof(value));
	memset(&ep,0,sizeof(ep));
	int i;
	byte isnegative=0;
	byte length = 0;
	get_private_profile_string(section,entry,"",&ep[0],sizeof(ep)-1,file_name); //Read the entry, with default being empty!
    for(i = 0; (isdigit((int)ep[i]) || ((ep[i]=='-') && (!i))); i++ )
		if (ep[i]=='-') //Negative sign?
		{
			isnegative = 1; //Negative sign!
		}
		else
		{
	        value[length++] = ep[i];
		}
    value[length] = '\0';
	LONG64SPRINTF result;
	if (sscanf(&value[0],LONGLONGSPRINTF,&result)==1) //Convert to our result!
	{
		return result*(isnegative?-1:1); //Give the result!
	}
	else return def; //Default otherwise!
}

/************************************************************************
* Function:     get_private_profile_int()
* Arguments:    <char *> section - the name of the section to search for
*               <char *> entry - the name of the entry to find the value of
*               <int> def - the default value in the event of a failed read
*               <char *> file_name - the name of the .ini file to read from
* Returns:      the value located at entry
*************************************************************************/
uint_64 get_private_profile_uint64(char *section,
    char *entry, uint_64 def, char *file_name)
{
    char value[MAX_LINE_LENGTH];
	memset(&value,0,sizeof(value));
	int i;
	get_private_profile_string(section,entry,"",&value[0],sizeof(value)-1,file_name); //Read the entry, with default being empty!
    for(i = 0; isdigit((int)value[i]); i++ ); //Scan until invalid characters!
    value[i] = '\0';
	LONG64SPRINTF result;
	if (sscanf(&value[0],LONGLONGSPRINTF,&result)==1) //Convert to our result!
	{
		return result; //Give the result!
	}
	else return def; //Return default!
}

void createTempFileName(char *dst,uint_32 dstsize, char *file_name)
{
	memset(dst,0,dstsize); //init!
	strcpy(dst,file_name); //Filename!
	strcat(dst,".tmp"); //Temporary filename!
}

void writesectioncomment(char *comment, FILE *wfp)
{
	if (comment!=NULL) //Gotten a comment to create as well?
	{
		fprintf(wfp,"; "); //Start a comment!
		for (;*comment;) //Process the comment!
		{
			if (*comment=='\n') //Newline?
			{
				++comment; //Skip the newline!
				fprintf(wfp,"\n; "); //Newline in the comment!
			}
			else
			{
				fprintf(wfp,"%c",*comment++); //Write the character!
			}
		}
		fprintf(wfp,"\n"); //End with a newline!
	}
}

/***** Routine for writing private profile strings --- by Joseph J. Graf *****/

/*************************************************************************
 * Function:    write_private_profile_string()
 * Arguments:   <char *> section - the name of the section to search for
 *			    <char *> section_comment - the comment for use with the section. NULL for none.
 *              <char *> entry - the name of the entry to find the value of
 *              <char *> buffer - pointer to the buffer that holds the string
 *              <char *> file_name - the name of the .ini file to read from
 * Returns:     TRUE if successful, otherwise FALSE
 *************************************************************************/
int write_private_profile_string(char *section, char *section_comment,
    char *entry, char *buffer, char *file_name)

{   FILE *rfp, *wfp;
    char tmp_name[256];
    char buff[MAX_LINE_LENGTH];
    char t_section[MAX_LINE_LENGTH];
    int len = strlen(entry);
    createTempFileName(tmp_name,sizeof(tmp_name),file_name); /* Get a temporary file name to copy to */
    sprintf(t_section,"[%s]",section);/* Format the section name */
    if( !(rfp = fopen(file_name,"r")) )  /* If the .ini file doesn't exist */
    {   if( !(wfp = fopen(file_name,"w")) ) /*  then make one */
        {   return(0);   }
        fprintf(wfp,"%s\n",t_section);
		writesectioncomment(section_comment,wfp); //Write the comment!
        fprintf(wfp,"%s=%s\n",entry,buffer);
        fclose(wfp);
        return(1);
    }
    if( !(wfp = fopen(tmp_name,"w")) )
    {   fclose(rfp);
        return(0);
    }

    /* Move through the file one line at a time until a section is
     * matched or until EOF. Copy to temp file as it is read. */

    do
    {   if( (read_line(rfp,buff)<=0) )
        {   /* Failed to find section, so add one to the end */
            fprintf(wfp,"\n%s\n",t_section);
			writesectioncomment(section_comment,wfp); //Write the comment!
            fprintf(wfp,"%s=%s\n",entry,buffer);
            /* Clean up and rename */
            fclose(rfp);
            fclose(wfp);
            delete_file(NULL,file_name);
            rename(tmp_name,file_name);
            return(1);
        }
        fprintf(wfp,"%s\n",buff);
    } while( strcmp(buff,t_section) );

    /* Now that the section has been found, find the entry. Stop searching
     * upon leaving the section's area. Copy the file as it is read
     * and create an entry if one is not found.  */
    for (;;)
    { 
		if( (read_line(rfp,buff)<=0) )
        {
		    /* EOF without an entry so make one */
            fprintf(wfp,"%s=%s\n",entry,buffer);
            /* Clean up and rename */
            fclose(rfp);
            fclose(wfp);
            delete_file(NULL,file_name);
            rename(tmp_name,file_name);
            return(1);

        }

        if((!strncmp(buff,entry,len) || buff[0] == '\0') && (buff[0]!=';')) //Found or empty file and not a comment?
            break;
        fprintf(wfp,"%s\n",buff);
    }

    if( buff[0] == '\0' )
    {   fprintf(wfp,"%s=%s\n",entry,buffer);
        do
        {
            fprintf(wfp,"%s\n",buff);
        } while( (read_line(rfp,buff)>0) );
    }
    else
    {   fprintf(wfp,"%s=%s\n",entry,buffer);
        while( (read_line(rfp,buff)>0) )
        {
             fprintf(wfp,"%s\n",buff);
        }
    }

    /* Clean up and rename */
    fclose(wfp);
    fclose(rfp);
    delete_file(NULL,file_name);
    rename(tmp_name,file_name);
    return(1);
}

int write_private_profile_int64(char *section, char *section_comment,
    char *entry, int_64 value, char *file_name)
{
	uint_64 datasigned;
	char s[256];
	memset(&s,0,sizeof(s)); //Init!
	if (value<0) datasigned = (uint_64)-value; //Make positive if needed!
	else datasigned = (uint_64)value; //Alreadty positive!
	if (value<0) //negative?
	{
		sprintf(&s[0],"-" LONGLONGSPRINTF,(LONG64SPRINTF)datasigned);
	}
	else
	{
		sprintf(&s[0],LONGLONGSPRINTF,(LONG64SPRINTF)datasigned);
	}
	return write_private_profile_string(section,section_comment,entry,&s[0],file_name); //Write to the file, give the result!
}

int write_private_profile_uint64(char *section, char *section_comment,
    char *entry, uint_64 value, char *file_name)
{
	char s[256];
	memset(&s,0,sizeof(s)); //Init!
	sprintf(&s[0],LONGLONGSPRINTF,(LONG64SPRINTF)value);
	return write_private_profile_string(section,section_comment,entry,&s[0],file_name); //Write to the file, give the result!
}