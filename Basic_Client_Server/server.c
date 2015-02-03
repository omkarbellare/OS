//OMKAR BELLARE
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
#include "queue.h"

//Number of maximum connections that can be queued
#define QUEUE_LENGTH 500

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

//Finding size of file
off_t fsize(const char *filename){
	struct stat st;
	
	if(stat(filename, &st) == 0)
		return st.st_size;
	
	return -1;
}

pthread_mutex_t queueLock, serviceCntMutex;
pthread_cond_t notEmpty, notFull;
char* server_path;

int service_count = 0;

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
	int child_socket;

	unsigned int filesize;

	int received = -1, i = 0;

	size_t bytes_read, bytes_left;

	char buf[MAXDATALENGTH]="", line[REQUEST_LINE_LENGTH]="";

	while(1){
		//Buffers [CHAR Array]
		char check_get[STATUS_LINE_LENGTH]="";
	
		i = 0;

		//Dequeuing to get child_socket
		//Lock mutex and wait till queue is not empty
		pthread_mutex_lock(&queueLock);
	
		//Waiting on condition variable notEmpty
		while(queueEmpty(q)){
			pthread_cond_wait(&notEmpty, &queueLock);
		}

		//Getting value in dequeue
		child_socket = dequeue(q);

		//Unlocking the mutex
		pthread_mutex_unlock(&queueLock);

		//Signalling notFull to boss
		pthread_cond_signal(&notFull);

		//Yielding the scheduler
		sched_yield();
	
		//Check if receive data is successful	
		//Data is read into buffer 'buf'
		if((received = recv(child_socket, buf, REQUEST_LINE_LENGTH, 0)) < 0)
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
			char filename[FILE_NAME_LENGTH];
			int j=0;

			//Provide valid folder from which server can read
			//Implemented for privacy
			char document_root[STATUS_LINE_LENGTH]="";

			strcpy(document_root, server_path);
		
			/*Extracting filename, so splitting it with first /
			Then breaking second part again on another / of HTTP/
			Then using the space between filename and HTTP
			This also allows for privacy as only 1 / can be used*/

			i = 4;
			while(line[i] != ' '){
				filename[j++] = line[i++];
			}
			filename[j] = '\0';
	
			//Return index.html if no filename mentioned
			if(strcmp(filename, "/") == 0)
				strcpy(filename, strcat(document_root, "index.html"));
			else
				strcpy(filename, strcat(document_root, filename));

			//Calculate file size
			filesize = fsize(filename);
			
			//Open the requested file
			FILE *fp;
			int file_exist = 1;

			fp = fopen(filename, "r");

			if(fp == NULL)
				file_exist = 0;

			//Get extension for file
			char *filetype;
			char *content_type;
			int s = '.';

			//Extracting the filetype
			filetype = strchr(filename, s);

			//Setting content type
			if(filetype == NULL || (strcmp(filetype, ".htm")) ==0 || (strcmp(filetype, ".html") == 0))
				content_type = "text/html";
			else if((strcmp(filetype, ".jpg")) ==0)
				content_type = "image/jpeg";
			else if((strcmp(filetype, ".gif")) ==0)
				content_type = "image/gif";
			else if((strcmp(filetype, ".txt")) ==0)
				content_type = "text/plain";
			else if((strcmp(filetype, ".png")) == 0)
				content_type = "image/png";
			else if((strcmp(filetype, ".pdf")) == 0)
				content_type = "application/pdf";
			else
				content_type = "application/octet-stream";

			char statusLine[STATUS_LINE_LENGTH] = "HTTP/1.0 ";
			char contentTypeLine[STATUS_LINE_LENGTH] = "Content-type: ";
			char body[REQUEST_LINE_LENGTH] = "<html>";

			if(file_exist == 1){
				//Send response saying OK
				strcpy(statusLine, strcat(statusLine, "200 OK"));
				strcpy(statusLine, strcat(statusLine, "\r\n"));
				strcpy(contentTypeLine, strcat(contentTypeLine, content_type));
				strcpy(contentTypeLine, strcat(contentTypeLine, "\r\n"));
			}
			else{
				//Send response saying NOT FOUND
				strcpy(statusLine, strcat(statusLine, "404 Not Found"));
				strcpy(statusLine, strcat(statusLine, "\r\n"));
				strcpy(contentTypeLine, strcat(contentTypeLine, "NONE"));
				strcpy(contentTypeLine, strcat(contentTypeLine, "\r\n"));
				
				//Send HTML response for Not Found
				strcpy(body, strcat(body, "<head><title>404 Not Found</title></head>"));
				strcpy(body, strcat(body, "<body>OOPS! File "));
				strcpy(body, strcat(body, filename));
				strcpy(body, strcat(body, " was not found on this server :( <br>"));
				strcpy(body, strcat(body, "Are you sure you have the right filename?</body></html>\r\n"));
	}

			//Send HTTP headers
			if(send(child_socket, statusLine, strlen(statusLine), 0) == -1 ||
				send(child_socket, contentTypeLine, strlen(contentTypeLine), 0) == -1 ||
					send(child_socket, "\r\n", strlen("\r\n"), 0) == -1)
				error("Failed to send http headers to client");

			bytes_left = filesize;

			//Send HTTP body
			if(file_exist){
				while(bytes_left > 0){
					//Allocating buffer for sending message
					char* message = (char*)malloc(filesize);
				
					//Reading and counting number of bytes read
					bytes_read = fread(message, 1, filesize, fp);

					//Having send pointer point to start of buffer
					char* send_ptr = message;
					size_t bytes_written = 0, bw = 0;
				
					//Check if full file has been read
					bytes_left -= bytes_read;
				
					//While all the contents of buffer are sent, keep sending
					while(bytes_written < bytes_read){
						//Send from send_ptr
						bw = send(child_socket, send_ptr, bytes_read, 0);
						//Increment bytes written
						bytes_written += bw;
						//Move send pointer
						send_ptr = send_ptr + bw;
					}

					free(message);
				}
			}
			else{
				if(send(child_socket, body, REQUEST_LINE_LENGTH, 0) == -1)
					error("Failed to send error message to client");	
			}

			//Closing socket
			close(child_socket);
			
			//Closing open file
			if(fp!=NULL)
				fclose(fp);

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
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int flag = 1;
	pthread_t workers[NUMBER_OF_WORKERS];
	int i = 0;

	//Creating a queue for boss
	myQueue* q = newQueue();

	//Creating boss socket
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		error("Socket creation failed\n");

	//Setting socket options
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) == -1)
		error("Socket option failed\n");

	//Check number of arguments
	if(argc < 4){
		printf("Usage: %s PORT SERVER_ADDRESS NUMBER_OF_WORKER_THREADS\n",argv[0]);
		exit(1);
	}

	//Extract arguments
	int port_number = atoi(argv[1]);
	int number_of_workers = atoi(argv[3]);
	server_path = argv[2];

	//Setting server address
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_number);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	memset(&(server_addr.sin_zero), '\0', 8);

	//Binding socket to address
	if(bind(sockfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1)
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
