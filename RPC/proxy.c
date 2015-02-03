//OMKAR BELLARE, VISHNU VENKATARAMAN
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include "queue.h"
#include "compress.h"

//Number of maximum connections that can be queued
#define QUEUE_LENGTH 10

//Maximum length of data that can be stored in the buffer
#define MAXDATALENGTH 10000

//Maximum number of worker threads
#define NUMBER_OF_WORKERS 1000

//Request line length
#define REQUEST_LINE_LENGTH 10000

//Status line length
#define STATUS_LINE_LENGTH 100

//Setting length to temporarily store part of line to check..
//if it contains the word GET to later get filename
#define FILE_NAME_LENGTH 100

//Length of buffer required between server and proxy
#define BUFFER_LENGTH 100000

int server_port=7351;

pthread_mutex_t queueLock, serviceCntMutex;
pthread_cond_t notEmpty, notFull;
//char *rpcIP[20];
int no_RPC = 0;
int optBit = 0;

int service_count = 0;

struct RPCMap{
	char rpcIP[20];
};

struct RPCMap rpcMap[20];

//Cache to be used for optimization
struct Cache
{
	char cache_content[4000000];
	char cache_fname[100];
	int cache_size;
}cache;

void sigchld_handler(int s){
	while(wait(NULL) > 0);
}

//Prints any errors
void error(char *msg){
	perror(msg);
	exit(1);
}

/*Multithreading function to handle multiple requests
Takes a socket_id which is the socket that the worker
thread works on
*/
void* handleHTTPRequest(void* ThreadQueue){
	//Socket descriptor [QUEUE]
	myQueue* q = (myQueue*)ThreadQueue;

	//Worker socket using file descriptor;
	int client_socket;
	
	int received = -1, i = 0, sd, j = 0, k = 0;

	char *line, *buf, *hostname;
	char outputBuffer[BUFFER_LENGTH];
	char filename[FILE_NAME_LENGTH];// hostname[FILE_NAME_LENGTH];

	FILE *fr;

	struct hostent *hp;

	struct sockaddr_in server_addr;

	CLIENT *clnt;
	enum clnt_stat retval_1;
	int browserRequest = 0;
	int contentSize;
	int rpcSelect = 0;
	int count_slash = 0;
	image *result_1 = NULL;
	char *isJPG;
	
	char numBytesFromServer[10];
	int numBytesServer;

	while(1){
		//Buffers [CHAR Array]
		char check_get[STATUS_LINE_LENGTH]="";

		hostname = (char*)malloc(FILE_NAME_LENGTH*sizeof(char));

		i = 0;

		//Dequeuing to get client_socket
		//Lock mutex and wait till queue is not empty
		pthread_mutex_lock(&queueLock);
	
		//Waiting on condition variable notEmpty
		while(queueEmpty(q)){
			pthread_cond_wait(&notEmpty, &queueLock);
		}

		//Getting value in dequeue
		client_socket = dequeue(q);

		//Unlocking the mutex
		pthread_mutex_unlock(&queueLock);

		//Signalling notFull to boss
		pthread_cond_signal(&notFull);

		//Yielding the scheduler
		sched_yield();

		buf = (char*)calloc(MAXDATALENGTH, sizeof(char));
	
		//Check if receive data is successful	
		//Data is read into buffer 'buf'
		if((received = recv(client_socket, buf, REQUEST_LINE_LENGTH, 0)) < 0)
			error("Failed to receive data from client");	

		//As received has number of bytes read, we set received-1(th)
		//byte as '\0'
		buf[received] = '\0';

		printf("%s\n", buf);

		line = (char*)calloc(REQUEST_LINE_LENGTH, sizeof(char));

		//Now we store the data in received line
		while(buf[i] != '\n' && buf[i] != '\0'){
			line[i] = buf[i];
			i++;
		}
		line[i] = '\0';
		i=0;

		//Now we get the filename which follows GET word
		while(line[i] != ' '){
			check_get[i] = line[i];
			i++;
		}

		if (strcmp(check_get, "GET") == 0){
			i = 4;
		}
		else if (strcmp(check_get, "POST") == 0){
			i = 5;
		}

		/*Extracting filename, so splitting it with first /
		Then breaking second part again on another / of HTTP/
		Then using the space between filename and HTTP
		This also allows for privacy as only 1 / can be used*/
		//Also getting the hostname from
		k = 0;
		count_slash = 0;

		while(count_slash != 3){
			if(line[i++]=='/')
				count_slash++;
			if(count_slash == 2 && line[i]!='/')
				hostname[k++] = line[i];				
		}

		hostname[k] = '\0';

		j=0;

		//Get the filename			
		while(line[i] != ' '){
			filename[j++] = line[i++];
		}

		filename[j] = '\0';
		
		isJPG = strstr(filename, "jpg");

		if(isJPG!=NULL && optBit == 1){
			//Select an RPC
			rpcSelect = rand()%no_RPC;
			printf("RPC Server number: %d picked whose address is: %s for file: %s\n", rpcSelect, rpcMap[rpcSelect].rpcIP, filename);

			clnt = clnt_create(rpcMap[rpcSelect].rpcIP, COMPRESS_PROG, COMPRESS_VERS, "tcp");

			if (clnt == NULL) {
				clnt_pcreateerror(rpcMap[rpcSelect].rpcIP);
				exit(1);
			}

			result_1 = compress_1(&buf, clnt);

			if (result_1 == NULL) {
				clnt_perror(clnt, "call failed:");
			}
			else{
				char headerJPG[MAXDATALENGTH];
				int headerSize;
				char *compressedImage;
				int compressedImageSize;
				char contentLength[10];
				FILE *compressImageFile;

				compressImageFile = fopen("proxy.jpg","w");

				//Creating Image
				strcpy(headerJPG, "");
				strcat(headerJPG, "HTTP/1.1 200 OK\r\n");
				strcat(headerJPG, "Content-Type: image/jpeg\r\n");
				strcat(headerJPG, "Content-Length: ");

				//Extracting image and size from result
				compressedImageSize = result_1->img.img_len;

				compressedImage = (char*)calloc(compressedImageSize+1, sizeof(char));
				memcpy(compressedImage, result_1->img.img_val, compressedImageSize);
				compressedImage[compressedImageSize] = '\0';
				sprintf(contentLength,"%d", compressedImageSize);

				strcat(headerJPG, contentLength);
				strcat(headerJPG, "\r\n\r\n");
				headerSize = strlen(headerJPG);

				fwrite(compressedImage, compressedImageSize, 1, compressImageFile);
				fclose(compressImageFile);

				//Sending Header
				send(client_socket, headerJPG, headerSize, 0);
				printf("\nSent Header from RPC\n");
		
				//Sending compressed image
				send(client_socket, compressedImage, compressedImageSize, 0);
				printf("Sent Image from RPC of length %d\n", compressedImageSize);

				free(compressedImage);				
			}    

			clnt_destroy(clnt); 
		}
		else{
			int server_port = 80, sd;
			struct hostent *hp;
			struct sockaddr_in server_addr;
			struct timeval timeout;
	
			//Not JPG content so no RPC, just sockets
			if((hp = gethostbyname(hostname)) == NULL){
					printf("Could not connect to host: %s", hostname);
					printf("\nLine that caused the error was: %s\n", line);
					error("Host name not valid\n");
			}
		

			//Set server details for socket
			memset(&server_addr, 0, sizeof(server_addr));
			server_addr.sin_family = AF_INET;
			memcpy(&server_addr.sin_addr, hp->h_addr_list[0], hp->h_length);
			server_addr.sin_port = htons(server_port);
			
			sd = socket(AF_INET, SOCK_STREAM, 0);
    
			//timeout.tv_sec = 1;
			//timeout.tv_usec = 0;

			//if (setsockopt (sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
			//	error("setsockopt failed\n");

			//Connecting to port on host
			if((connect(sd, (struct sockaddr *) &server_addr, sizeof(server_addr))) == -1){
				error("Connect to port on host failed\n");
			}

			//Sending the request
			send(sd, buf, strlen(buf), 0);
			printf("Sent request for file: %s\n", filename);

			while((received = recv(sd, outputBuffer, BUFFER_LENGTH, 0)) > 0)
			{
				//printf("%d bytes read from server\n", received);

				send(client_socket, outputBuffer, received, 0);		
			}
		}

		free(hostname);
		free(line);
		free(buf);		
	
		//Closing socket between proxy and client
		close(client_socket);			
	}
	pthread_exit(NULL);
}

int main(int argc, char **argv){
	int sockfd, worker_fd;
	struct sockaddr_in proxy_addr;
	struct sockaddr_in client_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int flag = 1;
	pthread_t workers[NUMBER_OF_WORKERS];
	int i = 0, addParam = 0;
	char* rpcFile;
	FILE *fr;
	char fileline[80];
	int part1, part2, part3, part4;

	//Creating a queue for boss
	myQueue *q = newQueue();
	
	//Creating proxy socket
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		error("Socket creation failed\n");

	//Setting socket options
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) == -1)
		error("Socket option failed\n");

	//Check number of arguments
	if(argc < 5){
		printf("Usage: %s PROXY_PORT NO_PROXY_THREADS OPTIMIZE_BIT RPC_FILE\n",argv[0]);
		exit(1);
	}

	//Extract arguments
	int port_number = atoi(argv[1]);
	int number_of_workers = atoi(argv[2]);
	optBit = atoi(argv[3]);
	rpcFile = argv[4];

	fr = fopen (rpcFile, "rt");
	if(!fr)
		error("RPC File could not be opened");

	//Getting mapping of requested server to server IP address
	while(fgets(fileline, 80, fr) != NULL){
		sscanf (fileline, "%d.%d.%d.%d", &part1, &part2, &part3, &part4);
		sprintf(rpcMap[no_RPC].rpcIP, "%d.%d.%d.%d", part1, part2, part3, part4);
		no_RPC++;
	}
	
	//Setting proxy address
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_port = htons(port_number);
	proxy_addr.sin_addr.s_addr = INADDR_ANY;
	memset(&(proxy_addr.sin_zero), '\0', 8);

	//Binding socket to address
	if(bind(sockfd, (struct sockaddr*)&proxy_addr, sizeof(struct sockaddr)) == -1)
		error("Socket binding failed\n");

	//Make listen to socket
	if(listen(sockfd, QUEUE_LENGTH) == -1)
		error("Socket listen failed\n");

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	if(sigaction(SIGCHLD, &sa, NULL) == -1)
		error("SIGACTION failed\n");

	//Create worker threads
	for(i = 0; i < number_of_workers; i++)
		pthread_create(&workers[i], NULL, handleHTTPRequest, q);

	while(1){
		//Accept new connection
		sin_size = sizeof(struct sockaddr_in);
		if((worker_fd = accept(sockfd, (struct sockaddr*)&client_addr, &sin_size)) == -1){
			perror("Connection accepting failed\n");
			continue;
		}

		//Queue thread onto queue
		//Lock the mutex
		pthread_mutex_lock(&queueLock);
		
		//Check for condition and wait on condition variable
		//Because boss is producer wait on notFull
		while(queueFull(q)){
			pthread_cond_wait(&notFull, &queueLock);
		}

		//Enqueue after lock acquired
		enqueue(q, worker_fd);

		//Unlock the mutex
		pthread_mutex_unlock(&queueLock);
		
		//Signal worker threads that queue is not empty now
		pthread_cond_signal(&notEmpty);

		//Yielding the scheduler
		sched_yield();	
	}

	//Freeing the queue
	deleteQueue(q);

	//Exiting main thread
	pthread_exit(NULL);	

	return 0;
}
