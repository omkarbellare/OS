To compile the files:
1. make clean removes all existing object face
2. make compiles the code to server and client

To run the program:
1. Server
./server PORT SERVER_ADDRESS NUMBER_OF_WORKER_THREADS

2. Client
./client HOSTNAME PORT NUMBER_OF_THREADS NUMBER_OF_RQ_PER_THREAD

To create random files:
sh createRandom.sh

Inside this shell script we can provide the number of files we want to create, the filesize in bytes and the location where the files need to be created.
