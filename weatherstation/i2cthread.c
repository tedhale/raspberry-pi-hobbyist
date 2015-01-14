/*---------------------------------------------------------------------------
   i2cthread.cpp   read I2C temp/humidity/baro
                   (am2315 and MPL115A2)
         Ted Hale
	2014-11-27   initial edits
	2014-12-27   add I2C code for the devices
	2015-01-11   fix sign on celsius (am2315) 

---------------------------------------------------------------------------*/

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <sys/timeb.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#include "weatherstation.h"

double BADTEMP = -999.0;

// conversion coefficients for MPL115A2
float a0;
float b1;
float b2;
float c12;

//**************************************************************************
// get temperature and humidity from am2315 device
void read_am2315(int fd, float *temp, float *humid)
{
	unsigned char read_request[3] = {3, 0, 4};
	unsigned char response[8];
	unsigned char dummy[1] = {0};
	float humidity, celsius;
	
	// wake it up
	write(fd, dummy, 1);
	write(fd, dummy, 1);
	// request data
	write(fd, read_request, 3);
	delay(2);
	// get response
	read(fd, response, 8);
	// validity check
	if ((response[0]!=3) || (response[1]!=4))
	{
		Log("i2cthread> read_am2315 i2c response invalid\n");
		humidity = BADTEMP;
		celsius = BADTEMP;
	}
	else 
	{
		humidity = (256*response[2] + response[3])/10.1;
		celsius = (256 * (response[4] & 0x7F) + response[5]) / 10.0;
		if ((response[4]&0x80)!=0)
			celsius *= -1.0;
	}
	// return results
	*humid = humidity;
	// convert C to F
	*temp = (celsius * 1.8) + 32.0;
}

//**************************************************************************
// get temperature and barometric from mpl115a2 device
void read_mpl115a2(int fd, float *t, float *b)
{
	int pressure;
	int temp;
	float pressureComp;
	float baro, celsius;
	
	// start conversion
	wiringPiI2CWriteReg8(fd,0x12,0);
	delay(5);
	// get results from device registers
	pressure = (( (uint16_t) wiringPiI2CReadReg8(fd,0) << 8) | wiringPiI2CReadReg8(fd,1)) >> 6;
	temp = (( (uint16_t) wiringPiI2CReadReg8(fd,2) << 8) | wiringPiI2CReadReg8(fd,3)) >> 6;
	// apply coefficients
	pressureComp = a0 + (b1 + c12 * temp ) * pressure + b2 * temp;
	// get pressure and temperature in the native units
	baro = ((65.0F / 1023.0F) * pressureComp) + 50.0F;   // kPa
	celsius = ((float) temp - 498.0F) / -5.35F + 25.0F;  // C
	// return results in more useful units
	// convert kPa to inches mercury
	*b = baro * 0.295299830714;
	// convert C to F
	*t = (celsius * 1.8) + 32.0;
}

//**************************************************************************
// get conversion coefficients from mpl115a2 device
void read_mpl115a2_coef(int fd)
{
	int16_t a0coeff;
	int16_t b1coeff;
	int16_t b2coeff;
	int16_t c12coeff;
	
	// get coef values from device registers
	a0coeff = (( (uint16_t) wiringPiI2CReadReg8(fd,4) << 8) | wiringPiI2CReadReg8(fd,5));
	b1coeff = (( (uint16_t) wiringPiI2CReadReg8(fd,6) << 8) | wiringPiI2CReadReg8(fd,7));
	b2coeff = (( (uint16_t) wiringPiI2CReadReg8(fd,8) << 8) | wiringPiI2CReadReg8(fd,9));
	c12coeff = (( (uint16_t) (wiringPiI2CReadReg8(fd,10) << 8) | wiringPiI2CReadReg8(fd,11))) >> 2;
	// compute the floating point coefficients
	a0 = (float)a0coeff / 8;
	b1 = (float)b1coeff / 8192;
	b2 = (float)b2coeff / 16384;
	c12 = (float)c12coeff;
	c12 /= 4194304.0;	
}

//**************************************************************************
// log to database and debug log
void DataLog(char *name, double *data)
{
	char tmp[80];
	sprintf(tmp,"i2cthread> %12s %6.1f",name,*data);
	Log(tmp);
	sprintf(tmp,"%5.1f",*data);
	StoreToDB(name,tmp);
}

//**************************************************************************
// Thread entry point, param is not used
void *i2cthread(void *param)
{
	time_t now, lastUpdate=0;
	float t1, t2, hum, baro;
	float t1tot, t2tot, humtot, barotot;
	int nsamples;
	int fd_am2315, fd_mpl115a2;

	// open am2315 i2c device
	fd_am2315 = wiringPiI2CSetup(0x5c);  // 0x5C is bus address of am2315
	if (fd_am2315==-1)
	{
		printf("i2cthread> wiringPiI2CSetup for am2315 failed\n");
		return 0;
	}

	// open mpl115a2 i2c device
	fd_mpl115a2 = wiringPiI2CSetup(0x60);  // 0x60 is bus address of mpl115a2
	if (fd_mpl115a2==-1)
	{
		printf("i2cthread> wiringPiI2CSetup for fd_mpl115a2 failed\n");
		return 0;
	}
	read_mpl115a2_coef(fd_mpl115a2);
	
	time(&lastUpdate);
	nsamples = 0;
	t1tot = 0;
	t2tot = 0;
	humtot = 0;
	barotot = 0;
	
	// start polling loop
	Log("i2cthread> start polling loop.");
    do
    {
		// read outside temperature and humidity
		read_am2315(fd_am2315, &t1, &hum);
		t1tot += t1;
		humtot += hum;
		
		// read board temp and barometric
		read_mpl115a2(fd_mpl115a2, &t2, &baro);
		t2tot += t2;
		barotot += baro;
		
		nsamples++;
		
		// log averaged data once a minute
		time(&now);
		if ((now-lastUpdate)>60)
		{
			outsideTemp = t1tot / nsamples;
			DataLog("outsideTemp",&outsideTemp);
			
			humidity = humtot / nsamples;
			DataLog("humidity",&humidity);
			
			boardTemp = t2tot / nsamples;
			DataLog("boardTemp",&boardTemp);
			
			barometric = barotot / nsamples;
			DataLog("barometric",&barometric);

			lastUpdate = now;
			nsamples = 0;
			t1tot = 0;
			t2tot = 0;
			humtot = 0;
			barotot = 0;
		}
	
		// let other threads run
		Sleep(3000);
    } while (kicked==0);  // exit loop if flag set
	
	Log("i2cthread> thread exiting");
	return 0;
}
