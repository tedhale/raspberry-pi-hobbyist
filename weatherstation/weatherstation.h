/*---------------------------------------------------------------------------
   weatherstation.h - include file for the Raspberry Pi brew controller project
         Ted Hale
   2014-11-27  initial edits

   
---------------------------------------------------------------------------*/

#define PIDFILE "/var/run/weatherstation.pid"
#define CONFFILE "/etc/weatherstation.conf"

#define BYTE unsigned char
#define WIND_PIN 1
#define RAIN_PIN 4
#define HEARTBEAT_PIN 11

#include "mysql.h"
#include "mysqld_error.h"

// prototype definitions for the worker threads
void *w1thread(void *param);
void *rainthread(void *param);
void *anemometerthread(void *param);
void *i2cthread(void *param);
void *wuthread(void *param);

// prototypes from common.c
int Sleep(int millisecs);
int read_line(FILE *fp, char *bp, int mx);
int ReadConfigString(char *var, char *defaultVal, char *out, int sz, char *file);
int WriteConfigString(char *var, char *out, char *file);
int LogOpen(char *filename);
void Log(char *format, ... );
void LogDbg(char *format, ... );
void StoreToDB(char* var, char* val);
int ConnectToDb();


// causes Global variables to be defined in the main
// and referenced as extern in all the other source files
#ifndef EXTERN
#define EXTERN extern
#endif

// GLOBAL variables.  A lock needs to be used to prevent any 
// simultaneous access from multiple threads
EXTERN int			kicked;						// flag for shutdown or restart
EXTERN int 			debug;						// flag to allow debug log output

EXTERN double 		outsideTemp;				// outside temperature degrees F
EXTERN double 		boardTemp;					// interface board temperature degrees F
EXTERN double 		windSpeed;					// wind speed MPH
EXTERN double 		windGust;					// wind gust MPH
EXTERN double 		rainToday;					// amount of rain today
EXTERN double 		rainPeriod;					// amount of rain since last update 
EXTERN double 		humidity;					// relative humidity percent
EXTERN double 		barometric;					// barometric prossure inches mercury
EXTERN double		tempA;						// 1-wire temp probe (if used)
		
// database 
EXTERN MYSQL		*conn;						// the DB connection
EXTERN char			dbhost[50];
EXTERN char			dbuser[50];
EXTERN char			dbpass[50];
EXTERN char			dbdatabase[50];
EXTERN char			tempA_ID[32];
