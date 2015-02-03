CC = gcc
DEBUG = -g
CFLAGS = -m32 -Wall
LFLAGS = -lpthread 

all: server client

server: 
	$(CC) $(CFLAGS) server.c $(DEBUG) -o server $(LFLAGS)

client:  
	$(CC) $(CFLAGS) client.c $(DEBUG) -o client $(LFLAGS)

clean: 
	\rm server client
