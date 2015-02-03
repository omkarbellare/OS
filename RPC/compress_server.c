/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */

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
#include <sys/shm.h>
#include <sys/sem.h>
#include <netdb.h>
#include "queue.h"
#include "jpeg-6b/lowres.h"
#include <fcntl.h>
#include "compress.h"

#define HEADER_SIZE 1000

#define IMAGE_BUFFER 100000

//Maximum length of data that can be stored in the buffer
#define MAXDATALENGTH 1000

//Status line length
#define STATUS_LINE_LENGTH 100

//Setting length to temporarily store part of line to check..
//if it contains the word GET to later get filename
#define FILE_NAME_LENGTH 1000

//Length of buffer required between server and proxy
#define BUFFER_LENGTH 100000

//Finding size of file
off_t fsize(const char *filename){
	struct stat st;
	
	if(stat(filename, &st) == 0)
		return st.st_size;
	
	return -1;
}

//Prints any errors
void error(char *msg){
	perror(msg);
	exit(1);
}

//Function to compress image
image compressImage(const char *filename, int size){
	int file = -1, j = 0, i = 0;
	size_t old_size = 0;
	int sizeOfImage = 0;
	char message[FILE_NAME_LENGTH];
	char *img_buf_ptr = NULL;
	FILE *smallfp;
	static image compressedImageStruct;

	FILE *tmpfile = NULL;

	if((file = open(filename, O_RDONLY)) < 0){
		sprintf(message, "Could not open %s", filename);
		error(message);
	}

	tmpfile = fdopen(file,"r");
	
	old_size = size;

	img_buf_ptr = (char *)malloc(size * sizeof(char));
	if(!img_buf_ptr)
		error("failed mem alloc \n");

	sizeOfImage = size;

	printf("Changing image to lowres with size:%d\n", sizeOfImage);
	if( change_res_JPEG_F (tmpfile, &img_buf_ptr, &sizeOfImage ) == 0 )
		error("Grabbing image failed!\n");
	else {
	       	printf("image lowres completed successfully!, old image size %ld , new image size %ld\n",(long)old_size, (long)sizeOfImage);
		compressedImageStruct.img.img_val = (char*)malloc(sizeOfImage*sizeof(char));
		compressedImageStruct.img.img_len = sizeOfImage;
		memcpy(compressedImageStruct.img.img_val, img_buf_ptr, sizeOfImage);
		compressedImageStruct.length = sizeOfImage;				
	 }

	return compressedImageStruct;
}

image *
compress_1_svc(char **argp, struct svc_req *rqstp)
{
	static image result;
	char line[MAXDATALENGTH], check_get[STATUS_LINE_LENGTH], hostname[FILE_NAME_LENGTH], filename[FILE_NAME_LENGTH], buf[MAXDATALENGTH];
	char tempFileName[FILE_NAME_LENGTH];
	int i = 0, k = 0, count_slash = 0, server_port, sd, total_bytes_recv = 0, num_bytes_recv = 0, headerSize = 0;
	int contentSize = 0, j = 0, sizeHeader = 0;	

	struct hostent *hp;

	struct sockaddr_in server_addr;

	struct timeval timeout;

	FILE *fp;

	char *contentSizeBuffer,*jpgError = NULL;
	//char jpgBuffer[BUFFER_LENGTH];
	char *jpgBuffer;
	char *jpgBufferPointer, *jpgBody;
	char headerJPG[HEADER_SIZE];
	char *posHeader;

	jpgBuffer = (char*)malloc(BUFFER_LENGTH);
	
	//Getting request
	strcpy(buf, *argp);

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
		/*Extracting filename, so splitting it with first /
		Then breaking second part again on another / of HTTP/
		Then using the space between filename and HTTP
		This also allows for privacy as only 1 / can be used*/
		//Also getting the hostname from
		i = 4;
		k = 0;
		while(count_slash != 3){
			if(line[i++]=='/')
				count_slash++;
			if(count_slash == 2 && line[i]!='/')
				hostname[k++] = line[i];				
		}
		
		count_slash = 0;

		hostname[k] = '\0';

		j=0;

		//Get the filename			
		while(line[i] != ' '){
			filename[j++] = line[i++];
		}

		filename[j] = '\0';

		//Have to make HTTP request
		server_port = 80;

		printf("\nHost being contacted is: %s\n", hostname);

		if((hp = gethostbyname(hostname)) == NULL){
				printf("Line that caused the error was: %s\n", line);
				error("Host name not valid\n");
		}

		//Set server details for socket
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		memcpy(&server_addr.sin_addr, hp->h_addr_list[0], hp->h_length);
		server_addr.sin_port = htons(server_port);
		
		sd = socket(AF_INET, SOCK_STREAM, 0);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		if (setsockopt (sd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
			error("setsockopt failed\n");

		//Connecting to port on host
		if((connect(sd, (struct sockaddr *) &server_addr, sizeof(server_addr))) == -1){
			error("Connect to port on host failed\n");
		}

		printf("Line contains: %s\n", line);

		//Sending the request
		send(sd, buf, strlen(buf), 0);
		printf("Sent request for file: %s \n", filename);
				
		sprintf(tempFileName, "temp%d.jpg",sd);
		fp = fopen(tempFileName,"wb");

		//Initializing jpg buffers
		//jpgBuffer=(char*)calloc(BUFFER_LENGTH,sizeof(char));
   	    	jpgBufferPointer=jpgBuffer;
		total_bytes_recv = 0;
		
		printf("Starting to read file: %s from %s using port %d\n",filename, hostname, server_port);
		//Read data into output buffer
		//Works for most cases
		while((num_bytes_recv = read(sd, jpgBufferPointer, BUFFER_LENGTH))>0){
			total_bytes_recv += num_bytes_recv;
			//printf("Read %d bytes\n",num_bytes_recv);

			//Check for error
			if(jpgError == NULL)
				jpgError = strstr(jpgBufferPointer, "404 Not Found");

			if(jpgError != NULL){
				//Forwarding error response
				if(send(sd, jpgBuffer, num_bytes_recv, 0) == -1){
					error("Failed to send response message to client");	
				}
			}	
			else{
				//Find content size
				contentSizeBuffer = strstr(jpgBufferPointer,"Content-Length");

				if(contentSizeBuffer != NULL){
					contentSizeBuffer=contentSizeBuffer+16;
					contentSize=atoi(contentSizeBuffer);					
					jpgBuffer=(char*)realloc(jpgBuffer,(contentSize+FILE_NAME_LENGTH*2)*sizeof(char));
					jpgBufferPointer=jpgBuffer;
				}
				jpgBufferPointer+=num_bytes_recv;
			}
		}
	
		printf("Total Read %d bytes\n",total_bytes_recv);

		if(jpgError == NULL && total_bytes_recv > 0){
			//TODO Change to macros
			char tempHeader[HEADER_SIZE],fileSize[10];
			int smallFileSize;
			char *ImageFileBuffer;
			FILE *compressImageFile;
			int fileBytesRead = 0;

			compressImageFile = fopen("server.jpg","w");
		
			printf("Now displaying image\n");
			//Getting only jpg image into buffer
			headerSize = total_bytes_recv - contentSize;

			//headerJPG = (char*)realloc(headerJPG, headerSize);
			//memset(headerJPG, '0', headerSize);
			
			jpgBody = jpgBuffer;
		
			jpgBody += headerSize;

			//Writing temporary jpg file
			fwrite(jpgBody, contentSize, 1, fp);
			fclose(fp);		

			printf("Header size: %d\n", headerSize);	

			result = compressImage(tempFileName, contentSize);

			smallFileSize = result.length;
			ImageFileBuffer = (char*)calloc(smallFileSize+1, sizeof(char));
			result.img.img_len = smallFileSize;
			memcpy(ImageFileBuffer, result.img.img_val, smallFileSize);
			printf("%d is the length of the file\n", result.img.img_len);
			ImageFileBuffer[smallFileSize] = '\0';

			//Write compressed image back on to server
			fwrite(ImageFileBuffer, smallFileSize, 1, compressImageFile);
			fclose(compressImageFile);

			//Removing temporary jpg file
			remove(tempFileName);
			free(ImageFileBuffer);
		}
	}	

	free(jpgBuffer);		

	return &result;
}
