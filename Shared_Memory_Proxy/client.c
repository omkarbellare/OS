//OMKAR BELLARE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>
#include <wait.h>
#include <unistd.h>
#include <time.h>

//Maximum number of files
#define MAX_FILE 328

//Maximum number of threads for client
//378 because Virtual Machine Ubuntu limit
#define MAX_NUMBER_OF_THREADS 2000

//File name length 
#define FILE_NAME_LENGTH 50

//Request line length
#define RQ_LENGTH 100

//Buffer length
#define BUFFER_LENGTH 10000

int flagForThreadStart = 0;
int numberOfRQ = 1;
//unsigned long long numberOfBytesRead[MAX_NUMBER_OF_THREADS];
unsigned long long numberOfBytesRead = 0;
struct sockaddr_in server_addr;

pthread_mutex_t threadSync;
pthread_cond_t flagSet;

void* sendHTTPRequest(void* thread_no){
	int thread_id = (int)thread_no+1, sd[numberOfRQ];
	unsigned long num_bytes_read = 0;
	char filename[FILE_NAME_LENGTH]="";
	char requestLine[RQ_LENGTH]="";
	char temp[3];
	int i;

	char *buf;

	srand(time(NULL));

	//Locking the thread sync mutex
	pthread_mutex_lock(&threadSync);

	//Waiting for all threads to get created
	while(flagForThreadStart == 0)
		pthread_cond_wait(&flagSet, &threadSync);

	//Unlocking the mutex
	pthread_mutex_unlock(&threadSync);

	//Handling multiple requests from same thread
	for(i = 0; i < numberOfRQ; i++){
		sd[i] = socket(AF_INET, SOCK_STREAM, 0);
	
		strcpy(filename, "");
		strcpy(requestLine, "");

		//Connecting to port on host
		if((connect(sd[i], (struct sockaddr *) &server_addr, sizeof(server_addr))) == -1){
			perror("Connect to port on host failed\n");
			exit(1);
		}
	
		thread_id = rand()%MAX_FILE + 1;
		
		//Getting name of file
		sprintf(temp, "%d", thread_id);

//		strcpy(filename, strcat(filename, "client_file_"));
//		strcpy(filename, strcat(filename, temp));
		strcpy(filename, strcat(filename, "file2"));
		strcpy(filename, strcat(filename, ".txt"));

		//Building the request
		strcpy(requestLine, strcat(requestLine, "GET "));
		strcpy(requestLine, strcat(requestLine, " http://mylocalserver.com/"));
		strcpy(requestLine, strcat(requestLine, filename));
		strcpy(requestLine, strcat(requestLine, " HTTP/1.1\r\n"));

		buf = (char*)malloc(BUFFER_LENGTH);

		//Sending the request
		if(send(sd[i], requestLine, strlen(requestLine), 0) == -1){
			perror("Sending the request failed\n");
			exit(1);
		}
	
		//Receiving the response
		while((num_bytes_read = read(sd[i], buf, BUFFER_LENGTH)) > 0){
			numberOfBytesRead += num_bytes_read;
		}
		
		free(buf);
		close(sd[i]);
	}

		
	//Yielding the scheduler
	sched_yield();

	//Exiting client thread
	pthread_exit(NULL);
}

int main(int argc, char** argv){
	char hostname[100];
	int port;
	int i = 0, rc;
	struct hostent *hp;
	int number_of_threads = 1;
	//unsigned long long number_of_bytes = 0;
	pthread_attr_t attr;
	pthread_t client_threads[MAX_NUMBER_OF_THREADS];
	struct timeval tv1, tv2;
	float runtime;

	//Check number of argument
	if(argc < 5){
		printf("Usage: %s HOSTNAME PORT NUMBER_OF_THREADS NUMBER_OF_RQ_PER_THREAD\n", argv[0]);
		exit(1);
	}

	pthread_attr_init(&attr);
	
	//Extract arguments
	strcpy(hostname, argv[1]);
	port = atoi(argv[2]);
	number_of_threads = atoi(argv[3]);
	numberOfRQ = atoi(argv[4]);

	//Checking for number of threads
	if(number_of_threads > MAX_NUMBER_OF_THREADS){
		perror("Number of threads entered is too high!\n");
		exit(1);
	}

	//Check if host name is valid
	if((hp = gethostbyname(hostname)) == NULL){
		perror("Host name not valid\n");
		exit(1);
	}

	//Set server details for socket
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	memcpy(&server_addr.sin_addr, hp->h_addr_list[0], hp->h_length);
	server_addr.sin_port = htons(port);

	//Creating client threads
	for(i = 0; i < number_of_threads; i++){
		rc = pthread_create(&client_threads[i], &attr, sendHTTPRequest, (void*)i);
		if(rc == -1){
			perror("Client thread creation failed\n");
			exit(1);
		}	
	}	

	//Locking thread sync mutex
	pthread_mutex_lock(&threadSync);

	//Setting go flag
	flagForThreadStart = 1;

	//Unlocking mutex
	pthread_mutex_unlock(&threadSync);

	//Broadcasting to start all threads at same time
	pthread_cond_broadcast(&flagSet);

	sched_yield();

	//Start timer
	gettimeofday(&tv1, NULL);

	//Waiting for all threads to finish
	//Also computing number of bytes read
	for(i = 0; i < number_of_threads; i++){
		pthread_join(client_threads[i], NULL);	
		//number_of_bytes += numberOfBytesRead[i];
	}

	//End the timer
	gettimeofday(&tv2, NULL);

	runtime = (double) (tv2.tv_usec - tv1.tv_usec)/1000000 +
		  (double) (tv2.tv_sec - tv1.tv_sec);

	//Display information
	//printf("\tNumber of bytes read: %lld\n", number_of_bytes);
	printf("\tNumber of bytes read: %lld\n", numberOfBytesRead);
	printf("\tTotal time = %f seconds\n", runtime);
	printf("\tThroughput = %f (Mb/s)\n", numberOfBytesRead / (runtime * 1000000));

	//Exit main thread of client program
	pthread_exit(NULL);

	return 0;
}
