/*---------------------------------------------------------------------------
   rainthread.cpp   handle rain gauge
         Ted Hale
	2014-11-27   initial edits
	2014-12-05   add db update

	NOTE: to get total rainfall from MySQL
	select dt,sum(value+0.0) as total from data where name="rainfall"
	
---------------------------------------------------------------------------*/

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/timeb.h>
#include <pthread.h>
#include <wiringPi.h>

#include "weatherstation.h"

int rainCounter;	// counter for rain guage clicks

//************************************************************************
// rainInterrupt:  called for rain gauge counter
void rainInterrupt(void) {
   rainCounter++;
}

//**************************************************************************
// Thread entry point, param is not used
void *rainthread(void *param)
{
	char tmp[80];
	time_t now, lastUpdate=0;
	double rainFall;
	
	// set up rain gauge interrupt
	rainCounter = 0;
	if ( wiringPiISR (RAIN_PIN, INT_EDGE_FALLING, &rainInterrupt) < 0 ) {
		Log("Unable to setup ISR: %s\n", strerror (errno));
	}	
		
	// start polling loop
	Log("rainthread> start polling loop.");
	time(&lastUpdate);
    do
    {
		time(&now);
		// check count every minute
		if ((now-lastUpdate)>=60)
		{
			// process data
			rainFall = rainCounter*0.000045;
			rainToday += rainFall;
			sprintf(tmp,"rainthread> rainFall = %6.3f   today = %4.1f",rainFall,rainToday);
			Log(tmp);
			rainCounter = 0;
			// update database
			sprintf(tmp,"%f",rainFall);
			StoreToDB("rainfall",tmp);;
			sprintf(tmp,"%f",rainToday);
			StoreToDB("rainfall_today",tmp);
			lastUpdate = now;
		}
		// let other threads run
		Sleep(5);
	} while (kicked==0);  // exit loop if flag set
	
	Log("rainthread> thread exiting");
	return 0;
}
