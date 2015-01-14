/*---------------------------------------------------------------------------
   common.cpp   misc shared functions
         Ted Hale
	
	26-Dec-2014  add lock on all MySQL functions to prevent crashes
	
---------------------------------------------------------------------------*/

#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/timeb.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <wiringPi.h>
#include "weatherstation.h"

//***************************************************************************

int Sleep(int millisecs)
{
	return usleep(1000*millisecs);
}

//************************************************************************
// read chars into buffer until EOF or newline
int read_line(FILE *fp, char *bp, int mx)
{   char c = '\0';
    int i = 0;
	memset(bp,0,mx);
    /* Read one line from the source file */
    while ( ( (c = getc(fp)) != '\n' ) && (i<mx) )
    {   
		if ((c == EOF)||(c == 255))         /* return -1 on EOF */
		{
			LogDbg("read_line> got EOF");
			sleep(1);
			if (i>0) break;
            else return(-1);
		}
        bp[i++] = c;
    }
    bp[i] = '\0';
    return(i);
}

//************************************************************************
// get var=value from config file
int ReadConfigString(char *var, char *defaultVal, char *out, int sz, char *file)
{
	FILE	*f;
	char	line[100];
	char	*p;

	LogDbg("ReadConfigString> get %s from %s ",var,file);
	piLock(1);
	f = fopen(file,"r");
	if (!f)
	{
		Log("ReadConfigString> error %d opening %s",errno,file);
		piUnlock(1);
		return 1;
	}

	while (read_line(f,line,sizeof(line))>=0)
	{
		LogDbg("ReadConfigString> read: %s",line);
		if ((line[0]==';')||(line[0]=='#')) 
			continue;
		p = strchr(line,'=');
		if (!p)
			continue;
		*p=0;
		p++;
		if (!strcmp(line,var))
		{
			strncpy(out,p,sz);
			fclose(f);
			Log("ReadConfigString> return %s=%s",var,out);
			piUnlock(1);
			return 1;
		}
	}
	fclose(f);
	strncpy(out,defaultVal,sz);
	Log("ReadConfigString> return %s=%s",var,out);
	piUnlock(1);
	return 0;
}


//************************************************************************
// connect/reconnect to MySQL database

int ConnectToDb()
{
	piLock(0);
	///Log("ConnectToDb");
	if (mysql_real_connect(conn, dbhost, dbuser, dbpass, dbdatabase, 0, NULL, 0) == NULL) {
		Log("MySQL connect error %u: %s\n", mysql_errno(conn), mysql_error(conn));
		mysql_close(conn);
		return 1;
	}
	else
	{
		Log(" ***** Connected to MySQL on %s",dbhost);
	}
	piUnlock(0);
	return 0;
}

//************************************************************************
// store a value in the database
void StoreToDB(char* var, char* val)
{
	char sql[100];
	int cnt=0;
	
	// ignore if no MySQL connection
	if (conn!=NULL) 
	{
		piLock(0);
		///Log("StoreToDB begin");
		sprintf(sql,"insert into data (name,value) VALUES ('%s',%s)",var,val);
		while (cnt<5)
		{
			if (mysql_query(conn, sql)&&(mysql_errno(conn)!=0)) {
				Log("mysql_query Error sql: %s\n         errno = %u:   %s", 
							sql, mysql_errno(conn), mysql_error(conn));
				// reconnect
				ConnectToDb();
			}
			else
			{
				///Log("StoreToDB  %s  %s",var,val);
				cnt=999;
			}
			cnt++;
		}
		piUnlock(0);
	}
}