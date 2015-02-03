//OMKAR BELLARE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <netdb.h>
#include "queue.h"

//Size of shared segment
#define SHMSZ 512

//Number of maximum connections that can be queued
#define QUEUE_LENGTH 10

//Maximum length of data that can be stored in the buffer
#define MAXDATALENGTH 1000

//Maximum number of worker threads
#define NUMBER_OF_WORKERS 1000

//Request line length
#define REQUEST_LINE_LENGTH 200

//Status line length
#define STATUS_LINE_LENGTH 100

//Setting length to temporarily store part of line to check..
//if it contains the word GET to later get filename
#define FILE_NAME_LENGTH 30

//Length of buffer required between server and proxy
#define BUFFER_LENGTH 100000

#define SEMKEYPATH "/dev/null"  /* Path used on ftok for semget key  */
#define SEMKEYID 1              /* Id used on ftok for semget key    */
#define NUMSEMS 2               /* Num of sems in created sem set    */

//Semaphore variables
int semid;
key_t semkey;
struct sembuf operations[2];
struct shmid_ds shmid_struct;
short  sarray[NUMSEMS];

pthread_mutex_t queueLock, serviceCntMutex;
pthread_cond_t notEmpty, notFull;
struct sockaddr_in server_addr;
pthread_mutexattr_t sharedMemAttr;
pthread_condattr_t reader;
pthread_condattr_t writer;

//Creating a queue for shared memory
myKeyQueue* sharedMemQueue;

int service_count = 0, isShared = 0;

void sigchld_handler(int s){
	while(wait(NULL) > 0);
}

//Prints any errors
void error(char *msg){
	perror(msg);
	exit(1);
}

//Function to perform cleanup
void shmCleanup(int sig){
	int i, shmid;
	printf("\nStarting Cleanup of Shared Memory");
	for(i = 0; i < QUEUE_LENGTH; i++){
		int key = 5678+i;
		if((shmid = shmget(key, SHMSZ, 0666)) < 0){
			error("SHMGET in cleanup failed");
		}
		shmctl(shmid, IPC_RMID, NULL);
	}
	printf("\nCleanup performed\n");
	exit(0);
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
	
	int port = 7351;

	int received = -1, i = 0, isExternalServer = 1, sd;

	char buf[MAXDATALENGTH]="", line[REQUEST_LINE_LENGTH]="";

	FILE *fr;

	struct hostent *hp;

	while(1){
		//Buffers [CHAR Array]
		char check_get[STATUS_LINE_LENGTH]="";

		char fileline[80];
	
		i = 0;


		//Opening server mapping file
		fr = fopen ("serverMapping.txt", "rt");

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
	
		//Check if receive data is successful	
		//Data is read into buffer 'buf'
		if((received = recv(client_socket, buf, REQUEST_LINE_LENGTH, 0)) < 0)
			error("Failed to receive data from client");	

		//As received has number of bytes read, we set received-1(th)
		//byte as '\0'
		buf[received+1] = '\0';

		//Now we store the data in received line
		while(buf[i] != '\n'){
			line[i] = buf[i];
			i++;
		}

		i=0;

		//Now we get the filename which follows GET word
		while(line[i] != ' '){
			check_get[i] = line[i];
			i++;
		}

		if (strcmp(check_get, "GET") == 0){
			//Get the filename
			char filename[FILE_NAME_LENGTH],hostname[FILE_NAME_LENGTH],hostmap[FILE_NAME_LENGTH],hostaddr[FILE_NAME_LENGTH];
			int j=0,k=0;
			int count_slash = 0;

			/*Extracting filename, so splitting it with first /
			Then breaking second part again on another / of HTTP/
			Then using the space between filename and HTTP
			This also allows for privacy as only 1 / can be used*/
			//Also getting the hostname from 
			i = 4;
			while(count_slash != 3){
				if(line[i++]=='/')
					count_slash++;
				if(count_slash == 2 && line[i]!='/')
					hostname[k++] = line[i];				
			}
			
			hostname[k] = '\0';

			//Getting mapping of requested server to server IP address
			while(fgets(fileline, 80, fr) != NULL){
				sscanf (fileline, "%s %s", hostmap, hostaddr);
				if(strcmp(hostmap,hostname) == 0)
					break;
   			}

			//Closing server mapping file
			fclose(fr);

			//Checking if request is coming to internal or external server
			if(strcmp(hostaddr,"127.0.0.1")==0)
				isExternalServer = 0;

			while(line[i] != ' '){
				filename[j++] = line[i++];
			}

			filename[j] = '\0';
	
			char requestLine[REQUEST_LINE_LENGTH]="";
			char *outputbuf;
			int num_bytes_recv, total_bytes_recv = 0;
			char sizeRQLine[3];

			//Check if host name is valid
			if((hp = gethostbyname(hostaddr)) == NULL){
				error("Host name not valid\n");
			}
			
			//Set server details for socket
			memset(&server_addr, 0, sizeof(server_addr));
			server_addr.sin_family = AF_INET;
			memcpy(&server_addr.sin_addr, hp->h_addr_list[0], hp->h_length);
			server_addr.sin_port = htons(port);

			sd = socket(AF_INET, SOCK_STREAM, 0);

			//Connecting to port on host
			if((connect(sd, (struct sockaddr *) &server_addr, sizeof(server_addr))) == -1){
				error("Connect to port on host failed\n");
			}

			//If external server = true or don't want shared, use ports
			if( isExternalServer == 1 || !isShared){
				outputbuf = (char*)malloc(BUFFER_LENGTH);
				
				strcpy(requestLine, "");

				//Building the request
				strcpy(requestLine, strcat(requestLine, "GET "));
				strcpy(requestLine, strcat(requestLine, filename));
				strcpy(requestLine, strcat(requestLine, " HTTP/1.1\r\n"));

				//Sending size of requestLine first
				sprintf(sizeRQLine, "%d", strlen(requestLine));
				send(sd, sizeRQLine, 3, 0);

				//Sending the request
				send(sd, requestLine, strlen(requestLine), 0);
				
				//Read data into output buffer
				while((num_bytes_recv = read(sd, outputbuf, BUFFER_LENGTH))>0){
					total_bytes_recv += num_bytes_recv;
					//Forwarding server's response
					if(write(client_socket, outputbuf, num_bytes_recv) == -1){
						error("Failed to send response message to client");	
					}
				}

				//Freeing the output buffer
				free(outputbuf);
			}
			else{ //isExternalServer = 0
				int shmid;
				int countBytes;
				char *shm, *moreDataPtr;
				char* cread;
				int *localCountPtr;
				int sentBytes = 0;
	
				//Have to associate a mutex with this TODO
				key_t senddata = dequeueKey(sharedMemQueue);

				//Creating the shared memory segment with dequeued key
				if((shmid = shmget(senddata, SHMSZ, 0666)) < 0){
					error("SHMGET failed!");
				}
		
				//Attaching to the shared memory segment
				if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
					error("SHMAT failed!");
   			    	}

				strcpy(requestLine, "");

				//Building the request
				strcpy(requestLine, strcat(requestLine, "LGET "));
				strcpy(requestLine, strcat(requestLine, filename));
				strcpy(requestLine, strcat(requestLine, " HTTP/1.1\r\n"));

				//Sending size of requestLine first
				sprintf(sizeRQLine, "%d", strlen(requestLine));
				send(sd, sizeRQLine, 3, 0);

				//Sending the request
				send(sd, requestLine, strlen(requestLine), 0);

				//Send the shared memory segment key being used to server
				send(sd, &senddata, sizeof(senddata),0);

				localCountPtr = (int*)((char *)shm+10);
				moreDataPtr =  ((char *)shm+20);

				//Actually read data and send to client
				while(1){
					//Wait if shared memory being written to by server
					operations[0].sem_num = 0;
					operations[0].sem_op =  0;
					operations[0].sem_flg = 0;

					//Bump up zero semaphore when shared memory is obtained
					operations[1].sem_num = 0;
					operations[1].sem_op =  1;
					operations[1].sem_flg = 0;

					//Wait for server to write and free shared memory
					operations[2].sem_num = 1;
					operations[2].sem_op = -1;
					operations[2].sem_flg = 0;

					if((semop( semid, operations, 3 ) == -1))
						error("SEMOP failed");

					//Number of bytes to read from shared memory
				     	countBytes = *localCountPtr;

					cread = (char*)(malloc(countBytes));

					memset(cread, 0, countBytes);
				     
					//Read and write to socket of client
				     	for(j=0; j<countBytes; j++){
						cread[j] = shm[30+j];
				     	}

					cread[j] = '\0';

					//Send data to client
					sentBytes = send(client_socket, cread, countBytes, 0);

					free(cread);
					
					operations[0].sem_num = 0;  //Bump down zero semaphore to signal shared memory can be freed
					operations[0].sem_op  = -1;
					operations[0].sem_flg = 0;

					if((semop( semid, operations, 1 ) == -1))
				    		error("SEMOP failed\n");

					//Seeing if there is more data to be put in shared memory segment
				      	if((*moreDataPtr) == 0)
						break;
				}		

				//Have to associate a mutex with this TODO
				enqueueKey(sharedMemQueue, senddata);

				shmdt(shm);
			}			

			//Close socket between proxy and server
			close(sd);

			//Closing socket between proxy and client
			close(client_socket);			
			
			//Service counter
			pthread_mutex_lock(&serviceCntMutex);
			service_count += 1;
			pthread_mutex_unlock(&serviceCntMutex);
		}
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
	int i = 0;

	//Creating a queue for boss
	myQueue* q = newQueue();
	sharedMemQueue = newKeyQueue();
	
	//Creating proxy socket
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		error("Socket creation failed\n");

	//Setting socket options
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) == -1)
		error("Socket option failed\n");

	//Check number of arguments
	if(argc < 2){
		printf("Usage: %s PORT\n",argv[0]);
		exit(1);
	}

	//Checking for shared memory flag
	for(i = 0; i < argc; i++){
		if(strcmp(argv[i],"-s") == 0)
			isShared = 1;
	}

	//Extract arguments
	int port_number = atoi(argv[1]);
	int number_of_workers = 10;
	int shmid;
	
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

	//Signal Handlers for cleanup
	signal(SIGINT, shmCleanup);
	signal(SIGTERM, shmCleanup);

	//Create worker threads
	for(i = 0; i < number_of_workers; i++)
		pthread_create(&workers[i], NULL, handleHTTPRequest, q);

	//Create shared memory queue
	for(i = 0; i < QUEUE_LENGTH; i++){
		if ((shmid = shmget((i+5678), SHMSZ, IPC_CREAT | 0666)) < 0) {
			error("SHMGET failed!");
		}
		enqueueKey(sharedMemQueue, (key_t)(i+5678));
	}

	//Semaphore setup
	semkey = ftok(SEMKEYPATH,SEMKEYID);
	if ( semkey == (key_t)-1 )
		error("FTOK failed");

	//Creating semaphore set using semkey
	semid = semget( semkey, NUMSEMS, 0666 | IPC_CREAT);
    	if ( semid == -1 )
        	error("SEMGET failed");

	sarray[0] = 0;
	sarray[1] = 0;

	if((semctl( semid, 1, SETALL, sarray)) == -1)
		error("SEMCTL failed");

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
	deleteKeyQueue(sharedMemQueue);

	//Exiting main thread
	pthread_exit(NULL);	

	return 0;
}
