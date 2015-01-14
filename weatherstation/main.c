/*---------------------------------------------------------------------------
  main.c	weather station for the Raspberry Pi	
    
	By Ted B. Hale

  2014-11-27  initial edits
  
---------------------------------------------------------------------------*/

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

// this defines the pre-processor variable EXTERN to be nothing
// it results in the variables in RPiHouse.h being defined only here 
#define EXTERN
#include "weatherstation.h"

//************************************************************************
// read various configuration values for program
void readConfig(char *fname)
{
	char temp[100];
	
	ReadConfigString("debug","0",temp,sizeof(temp),fname);
	debug = atoi(temp);	
	ReadConfigString("database","weather",dbdatabase,sizeof(dbdatabase),fname);
	ReadConfigString("dbhost","localhost",dbhost,sizeof(dbhost),fname);
	ReadConfigString("dbuser","ted",dbuser,sizeof(dbuser),fname);
	ReadConfigString("dbpass","secret",dbpass,sizeof(dbpass),fname);

	ReadConfigString("tempA","",tempA_ID,sizeof(tempA_ID),fname);
	
	Log("%s %s %s %s",dbhost,dbdatabase,dbuser,dbpass);
}


//************************************************************************
// handles signals to restart or shutdown
void sig_handler(int signo)
{
    switch (signo) {
      case SIGPWR:
        break;

      case SIGHUP:
	    // do a restart
	    Log("SIG restart\n");
		kicked = 1;
        break;

      case SIGINT:
      case SIGTERM:
		// do a clean exit
	    Log("SIG exit\n");
	    kicked = 2;
        break;
    }
}
//************************************************************************
// and finally, the main program
// a cmd line parameter of "f" will cause it to run in the foreground 
// instead of as a daemon 
int main(int argc, char *argv[])
{
    pid_t		pid;
	FILE		*f;
	pthread_t	tid1,tid2,tid3,tid4;		// thread IDs
	int x;
	
	// check cmd line param
	if ((argc==1) || strncmp(argv[1],"f",1))
	{
		printf("running in daemon mode\n");
		// Spawn off a child, then kill the parent.
		// child will then have no controlling terminals, 
		// and will become adopted by the init proccess.
		if ((pid = fork()) < 0) {
			perror("Error forking process ");
			exit (-1);
		}
		else if (pid != 0) {
			exit (0);  // parent process goes bye bye
		}

		// The child process continues from here
		setsid();  // Become session leader;
	}

	// trap some signals 
	signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGPWR, sig_handler);
    signal(SIGHUP, sig_handler);

    // save the pid in a file
	pid = getpid();
	f = fopen(PIDFILE,"w");
	if (f) {
		fprintf(f,"%d",pid);
		fclose(f);
	}

	// open the log file
	LogOpen("/opt/projects/logs/weatherstation");

	// read config values
	readConfig(CONFFILE);
	
	// set log debug flag
	LogSetDebug(debug);
	
	// initialize the WiringPi interface
	Log("Main> init wiringPi");
	x = wiringPiSetup ();
	if (x == -1)
	{
		Log("Main> Error on wiringPiSetup.  weatherstation quitting.");
		return 0;
	}	
	// config heartbeat pin
	pinMode (11, OUTPUT);

	// open database
	conn = mysql_init(NULL);
	ConnectToDb();
	
	// start the main loop
	do
	{
		// start the various threads
		Log("Main> start threads");
		tid1 = tid2 = tid3 = tid4 = 0;
		pthread_create(&tid1, NULL, i2cthread, NULL);
		Sleep(100);
		pthread_create(&tid2, NULL, w1thread, NULL);
		Sleep(100);
		pthread_create(&tid3, NULL, rainthread, NULL);
		Sleep(100);
		pthread_create(&tid4, NULL, anemometerthread, NULL);
	
		// wait for signal to restart or exit
		int i=0;
		do
		{
			// blink the heartbeat LED
			if (i==0)
			{
				digitalWrite(11,1);
				i=20;
			} else if (i==10)
			{
				digitalWrite(11,0);
			}
			Sleep(50);
			i--;
		} while (kicked==0); 
		
		// wait for running threads to stop
		if (tid1!=0) pthread_join(tid1, NULL);
		if (tid2!=0) pthread_join(tid2, NULL);
		if (tid3!=0) pthread_join(tid3, NULL);
		if (tid4!=0) pthread_join(tid4, NULL);

		// exit?
		if (kicked==2) break;
		// else restart, set flag back to 0
		kicked = 0;

	} while (1); // forever

	// delete the PID file
    unlink(PIDFILE);

	Log("Program Exit *****");
	Log(" ");

	return 0;
}
