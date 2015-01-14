/*---------------------------------------------------------------------------
   w1thread.cpp   read 1-wire temperature probe
         Ted Hale
	2014-12-01   initial edit, from brewcontroller project
	
NOTES:	
sudo modprobe w1-gpio
sudo modprobe w1-therm
cd /sys/bus/w1/devices/
ls  (to get ID string for sensor)   28-000004610260

cd (ID String)
cat w1_slave	
first line must end with YES
second line will end with t=12345    milli degrees C

28-000004fcf3ce    tempA

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

#define BADTEMP -999.0

//**************************************************************************
// get temperature in Degrees F
int getTemperature(char *id, double *value)
{
	char buff[80];
	FILE *fd;
	char *p;
	double c;
	
	if (strlen(id)==0)
	{
		*value = BADTEMP;
		return 3;
	}
	sprintf(buff,"/sys/bus/w1/devices/%s/w1_slave",id);
	///Log(buff);
	fd = fopen(buff,"r");
	if (fd==NULL)
	{
		Log("w1thread> Error %d opening %s",errno,buff);
		*value = BADTEMP;
		return 2;
	}
	read_line(fd,buff,sizeof(buff));
	///Log(buff);
	if (strcmp(&buff[36],"YES")!=0)
	{
		Log("w1thread> error on 1-wire read: %s",buff);
		fclose(fd);
		*value = BADTEMP;
		return 1;
	}
	read_line(fd,buff,sizeof(buff));
	p = strstr(buff,"t=");
	///Log(buff);
	if (p==NULL)
	{
		Log("w1thread> error on 1-wire read: %s",buff);
		fclose(fd);
		*value = BADTEMP;
		return 1;
	}
	fclose(fd);
	c = atof(p+2);
	*value = ((c/1000.0) * 1.8) + 32.0;
	return 0;
}

//**************************************************************************
// Thread entry point, param is not used
void *w1thread(void *param)
{
	char tmp[80];
	time_t now, lastUpdate=0;
	double tot=0, x=0;
	int err, samples=0;

	if (strlen(tempA_ID)<1)
	{
		Log("w1thread> disabled");
		return 0;
	}
	
	// start polling loop
	Log("w1thread> start polling loop.");

    do
    {
		// read temperature
		err = getTemperature(tempA_ID,&x);
		if (err==2)
			break;  	// quit if this device if not found
		
		// add to avg if OK
		if (err==0)
		{
			tot += x;
			samples++;
		}
		
		// update each minute
		time(&now);
		if (((now-lastUpdate)>60)&&(samples>0))
		{
			tempA = tot/samples;
			// save to DB
			sprintf(tmp,"%5.1f",tempA);
			StoreToDB("tempA",tmp);

			sprintf(tmp,"w1thread> tempA = %6.1f ",	tempA);
			Log(tmp);
			lastUpdate = now;
			tot = samples = 0;
		}
		
		// let other threads run
		// short delay since reading temp takes a second
		Sleep(100);
    } while (kicked==0);  // exit loop if flag set
	Log("w1thread> thread exiting");
	return 0;
}
