SRCS=main.c logfile.c common.c rainthread.c i2cthread.c anemometerthread.c w1thread.c
OBJS=$(SRCS:.c=.o)

CC=gcc
#DEBUG  = -g -O0
DEBUG   = -O3
INCLUDE = -I/usr/local/include -I/usr/include/mysql
LDFLAGS = -L/usr/local/lib -L/usr/local/lib/mysql
LDLIBS  = -lmysqlclient -lwiringPi -lwiringPiDev -lpthread -lm 
CFLAGS  = $(DEBUG) -Wall $(INCLUDE) -Winline -pipe

LD=gcc

all: weatherstation

weatherstation: $(OBJS)
	$(CC) -o weatherstation $(OBJS) $(LDFLAGS) $(LDLIBS) 

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@
	
clean:
	rm -f $(OBJS) weatherstation core 

