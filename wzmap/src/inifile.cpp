#include <stdio.h>
#include <stdlib.h>
#include <shlobj.h>
#include <string>

using namespace std;

#include "inifile.h"   /* function prototypes in here */
#include "q3map2.h"

#ifdef _WIN32
#define unlink _unlink
#endif

/*char FS_GetC(FILE *fp)
{
	char c = '\0';
	fread(&c, sizeof(char), 1, fp);
	return c;
}*/

/*****************************************************************
* Function:     read_line()
* Arguments:    <FILE *> fp - a pointer to the file to be read from
*               <char *> bp - a pointer to the copy buffer
* Returns:      TRUE if successful FALSE otherwise
******************************************************************/
int read_line(FILE *fp, char *bp)
{   
	char c = '\0';
	int i = 0;
	/* Read one line from the source file */
	while( (c = getc(fp)) != '\n' )
	//while( (c = FS_GetC(fp)) != '\n' )
	{   
		if( c == EOF/* || c == '\0'*/ )         /* return FALSE on unexpected EOF */
			return(0);

		bp[i++] = c;
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
size_t get_private_profile_string(char *section, char *entry, char *def, char *buffer, int buffer_len, char *file_name)
{   
	FILE *fp = fopen(file_name,"r");
	char buff[MAX_LINE_LENGTH];
	char *ep;
	char t_section[MAX_LINE_LENGTH];
	size_t len = strlen(entry);
	if( !fp ) return(0);
	sprintf(t_section,"[%s]",section);    /* Format the section name */
	/*  Move through file 1 line at a time until a section is matched or EOF */
	do
	{   
		if( !read_line(fp,buff) )
		{   
			fclose(fp);
			strncpy(buffer,def,buffer_len);
			return(strlen(buffer));
		}
	}
	while( strncmp(buff,t_section, strlen(t_section)) );

	/* Now that the section has been found, find the entry.
	* Stop searching upon leaving the section's area. */
	do
	{   
		if( !read_line(fp,buff) /*|| buff[0] == '\0'*/ )
		{   
			fclose(fp);
			strncpy(buffer,def,buffer_len);
			return(strlen(buffer));
		}
	}  while( strncmp(buff,entry,strlen(entry)) );

	ep = strrchr(buff,'=');    /* Parse out the equal sign */
	ep++;
	/* Copy up to buffer_len chars to buffer */
	strncpy(buffer,ep,buffer_len - 1);

	buffer[buffer_len] = '\0';
	fclose(fp);               /* Clean up and return the amount copied */
	return(strlen(buffer));
}

/*************************************************************************
* Function:    write_private_profile_string()
* Arguments:   <char *> section - the name of the section to search for
*              <char *> entry - the name of the entry to find the value of
*              <char *> buffer - pointer to the buffer that holds the string
*              <char *> file_name - the name of the .ini file to read from
* Returns:     TRUE if successful, otherwise FALSE
*************************************************************************/
int write_private_profile_string(char *section, char *entry, char *buffer, char *file_name)
{   
	FILE *rfp, *wfp;
	char tmp_name[15];
	char buff[MAX_LINE_LENGTH];
	char t_section[MAX_LINE_LENGTH];
	size_t len = strlen(entry);
	tmpnam(tmp_name); /* Get a temporary file name to copy to */
	sprintf(t_section,"[%s]",section);/* Format the section name */
	if( !(rfp = fopen(file_name,"r")) )  /* If the .ini file doesn't exist */
	{   
		if( !(wfp = fopen(file_name,"w")) ) /*  then make one */
		{   
			return(0);   
		}
		fprintf(wfp,"%s\n",t_section);
		fprintf(wfp,"%s=%s\n",entry,buffer);
		fclose(wfp);
		return(1);
	}
	if( !(wfp = fopen(tmp_name,"w")) )
	{   
		fclose(rfp);
		return(0);
	}

	/* Move through the file one line at a time until a section is
	* matched or until EOF. Copy to temp file as it is read. */

	do
	{   
		if( !read_line(rfp,buff) )
		{   /* Failed to find section, so add one to the end */
			fprintf(wfp,"\n%s\n",t_section);
			fprintf(wfp,"%s=%s\n",entry,buffer);
			/* Clean up and rename */
			fclose(rfp);
			fclose(wfp);
			unlink(file_name);
			rename(tmp_name,file_name);
			return(1);
		}
		fprintf(wfp,"%s\n",buff);
	} while( strcmp(buff,t_section) );

	/* Now that the section has been found, find the entry. Stop searching
	* upon leaving the section's area. Copy the file as it is read
	* and create an entry if one is not found.  */
	while( 1 )
	{   
		if( !read_line(rfp,buff) )
		{   /* EOF without an entry so make one */
			fprintf(wfp,"%s=%s\n",entry,buffer);
			/* Clean up and rename */
			fclose(rfp);
			fclose(wfp);
			unlink(file_name);
			rename(tmp_name,file_name);
			return(1);

		}

		if( !strncmp(buff,entry,len) || buff[0] == '\0' )
			break;
		fprintf(wfp,"%s\n",buff);
	}

	if( buff[0] == '\0' )
	{   
		fprintf(wfp,"%s=%s\n",entry,buffer);
		do
		{
			fprintf(wfp,"%s\n",buff);
		} while( read_line(rfp,buff) );
	}
	else
	{   
		fprintf(wfp,"%s=%s\n",entry,buffer);
		while( read_line(rfp,buff) )
		{
			fprintf(wfp,"%s\n",buff);
		}
	}

	/* Clean up and rename */
	fclose(wfp);
	fclose(rfp);
	unlink(file_name);
	rename(tmp_name,file_name);
	return(1);
}


// ==============================================================================================================
// C++ Interface Layer Functions - For simple usage from C++ code...
// ==============================================================================================================

const char *IniReadCPP(char *aFilespec, char *aSection, char *aKey, char *aDefault)
{
	if (!aDefault || !*aDefault)
		aDefault = "";

	if (!aKey || !aKey[0])
		return "";

	char	szBuffer[65535] = { 0 };					// Max ini file size is 65535 under 95

	get_private_profile_string(aSection, aKey, aDefault, szBuffer, sizeof(szBuffer), aFilespec);

	return va(szBuffer);
}

void IniWriteCPP(char *aFilespec, char *aSection, char *aKey, char *aValue)
{
	write_private_profile_string(aSection, aKey, aValue, aFilespec);
}

// ==============================================================================================================
// C Interface Layer Functions - For simple access from Q3 code...
// ==============================================================================================================

/*
// Example usage
PLAYER_FACTION = IniRead("general.ini","MISC_SETTINGS","PLAYER_FACTION","federation");
PLAYER_FACTION_AUTODETECT = atoi(IniRead("general.ini","MISC_SETTINGS","PLAYER_FACTION_AUTODETECT","1"));
*/

const char *IniRead(char *aFilespec, char *aSection, char *aKey, char *aDefault)
{
	const char *value = IniReadCPP(aFilespec, aSection, aKey, aDefault);
	//Sys_Printf("[section] %s [key] %s [value] %s\n", aSection, aKey, value);
	return value;
}

void IniWrite(char *aFilespec, char *aSection, char *aKey, char *aValue)
{
	IniWriteCPP(aFilespec, aSection, aKey, aValue);
}
