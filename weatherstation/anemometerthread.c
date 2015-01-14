/*---------------------------------------------------------------------------
   anemometerthread.cpp   read pulse count for anemometer
         Ted Hale
	2014-11-29   initial edits
	2014-12-05   add rolling avg and gust processing

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

#define AVGSIZE 5
#define BUFSIZE 24

int windCounter;	// counter for wind guage clicks

//************************************************************************
// windInterrupt:  called for anemometer counter
void windInterrupt(void) {
   windCounter++;
}

//**************************************************************************
void getAvg(int *dat, int sz, double *result)
{
	int i, tot=0;
	for (i=0; i<sz; i++)
		tot+=dat[i];
	*result = 1.0*tot/sz;
}

//**************************************************************************
void getAvgDouble(double *dat, int sz, double *result)
{
	int i;
	double tot=0;
	for (i=0; i<sz; i++)
		tot+=1.0*dat[i];
	*result = tot/sz;
}

//**************************************************************************
void getMax(double *dat, int sz, double *result)
{
	int i;
	double high = dat[0];
	for (i=1; i<sz; i++)
		if (dat[i]>high)
			high = dat[i];
	*result = high;
}
	
//**************************************************************************
// Thread entry point, param is not used
void *anemometerthread(void *param)
{
	char tmp[80];
	time_t now, lastCount = 0;
	int avgbuf[AVGSIZE];
	int avgptr = 0;
	double bvgbuf[BUFSIZE];		// last 2 minutes of values
	int bvgptr = 0;
	double x,y;
	
	memset(avgbuf,0,sizeof(avgbuf));
	memset(bvgbuf,0,sizeof(bvgbuf));;
	
	// set up anemometer interrupt
	windCounter = 0;
	if ( wiringPiISR (WIND_PIN, INT_EDGE_FALLING, &windInterrupt) < 0 ) {
		Log("Unable to setup ISR: %s\n", strerror (errno));
	}	
		
	// start polling loop
	Log("anemometerthread> start polling loop.");
	time(&lastCount);	
    do
    {
		time(&now);
		// process every second
		if ((now-lastCount)>=1)
		{
			//Log("anemometerthread> a=%d   b=%d",avgptr,bvgptr);
			avgbuf[avgptr] = windCounter;
			avgptr++;
			windCounter = 0;
			lastCount = now;
		}			
		if (avgptr>AVGSIZE)
		{
			// instantaneous from 5 second avg
			getAvg(avgbuf,AVGSIZE,&x);
			y = 3.6528*x;
			bvgbuf[bvgptr] = y;
			bvgptr++;
			// gust
			getMax(bvgbuf,BUFSIZE,&y);
			if (y>x)
				windGust=y;
			avgptr=0;
		}
		if (bvgptr>BUFSIZE)
		{
			// averaged windspeed
			getAvgDouble(bvgbuf, BUFSIZE, &x);
			windSpeed = x;
			sprintf(tmp,"anemometerthread> wind speed = %4.1f   wind gust = %4.1f",windSpeed,windGust);
			Log(tmp);
			// save to DB
			sprintf(tmp,"%5.1f",windSpeed);
			StoreToDB("wind_speed",tmp);
			sprintf(tmp,"%5.1f",windGust);
			StoreToDB("wind_gust",tmp);
			windGust = 0;
			bvgptr=0;
		}

		// let other threads run
		Sleep(5);	
	} while (kicked==0);  // exit loop if flag set
	
	Log("anemometerthread> thread exiting");
	return 0;
}
