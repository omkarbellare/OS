CC = gcc
DEBUG = -g
CFLAGS = -m32 -Wall
LFLAGS = -lpthread 

all: server_sem proxy_sem client

client:
	$(CC) $(CFLAGS) client.c $(DEBUG) -o client $(LFLAGS)

server_sem: 
	$(CC) $(CFLAGS) server_sem.c $(DEBUG) -o server_sem $(LFLAGS)

proxy_sem:  
	$(CC) $(CFLAGS) proxy_sem.c $(DEBUG) -o proxy_sem $(LFLAGS)

clean: 
	rm server_sem proxy_sem client
