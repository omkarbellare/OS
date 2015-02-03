#ifndef QUEUE_H_
#define QUEUE_H_

#define MAX_QUEUE_SIZE 10

#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 

typedef struct queueImplementation{
	int contents[MAX_QUEUE_SIZE];
	int front;
	int count;
} myQueue;

typedef struct KeyqueueImplementation{
	key_t contents[MAX_QUEUE_SIZE];
	int front;
	int count;
} myKeyQueue;

myQueue* newQueue();
void enqueue(myQueue*, int element);
int dequeue(myQueue*);
int queueFull(myQueue*);
int queueEmpty(myQueue*);
void deleteQueue(myQueue*);

myKeyQueue* newKeyQueue();
void enqueueKey(myKeyQueue*, key_t element);
key_t dequeueKey(myKeyQueue*);
int KeyqueueFull(myKeyQueue*);
int KeyqueueEmpty(myKeyQueue*);
void deleteKeyQueue(myKeyQueue*);

//Function to create new Queue
myQueue* newQueue(){
	myQueue* queue;
	
	queue = (myQueue*)malloc(sizeof(myQueue));

  	if (queue == NULL) {
    		perror("Error creating queue");
    		exit(1);  /* Exit program, returning error code. */
  	}

  	queue->front = 0;
  	queue->count = 0;
  
	return queue;
} 

//Function to destroy Queue
void deleteQueue(myQueue *queue){
	free(queue);
}

//Function to enqueue
void enqueue(myQueue *queue, int element)
{
  int newElementIndex;

  if (queue->count >= MAX_QUEUE_SIZE) {
    perror("QueueEnter on Full Queue.\n");
    exit(1);
  }

  newElementIndex = (queue->front + queue->count)% MAX_QUEUE_SIZE;
  queue->contents[newElementIndex] = element;

  queue->count++;
}

int dequeue(myQueue *queue){
  int old_element;

  if (queue->count <= 0) {
    perror("QueueDelete on Empty Queue.\n");
    exit(1);  /* Exit program, returning error code. */
  }

  //Element to be returned
  old_element = queue->contents[queue->front];

  queue->front++;
  queue->front %= MAX_QUEUE_SIZE;

  queue->count--;

  return old_element;
}

int queueFull(myQueue *queue){
	return (queue->front + queue->count == MAX_QUEUE_SIZE);
}

int queueEmpty(myQueue *queue){
	return (queue->count == 0);
}

//Start of key queue functions

//Function to create new Queue
myKeyQueue* newKeyQueue(){
	myKeyQueue* queue;
	
	queue = (myKeyQueue*)malloc(sizeof(myKeyQueue));

  	if (queue == NULL) {
    		perror("Error creating queue");
    		exit(1);  /* Exit program, returning error code. */
  	}

  	queue->front = 0;
  	queue->count = 0;

  return queue;
} 

//Function to destroy Queue
void deleteKeyQueue(myKeyQueue *queue){
	free(queue);
}

//Function to enqueue
void enqueueKey(myKeyQueue *queue, key_t element)
{
  int newElementIndex;

  if (queue->count >= MAX_QUEUE_SIZE) {
    perror("QueueEnter on Full Queue.\n");
    exit(1);
  }

  newElementIndex = (queue->front + queue->count)% MAX_QUEUE_SIZE;
  queue->contents[newElementIndex] = element;

  queue->count++;
}

key_t dequeueKey(myKeyQueue *queue){
  key_t old_element;

  if (queue->count <= 0) {
    perror("QueueDelete on Empty Queue.\n");
    exit(1);  /* Exit program, returning error code. */
  }

  //Element to be returned
  old_element = queue->contents[queue->front];

  queue->front++;
  queue->front %= MAX_QUEUE_SIZE;

  queue->count--;

  return old_element;
}

int KeyqueueFull(myKeyQueue *queue){
	return (queue->front + queue->count == MAX_QUEUE_SIZE);
}

int KeyqueueEmpty(myKeyQueue *queue){
	return (queue->count == 0);
}

#endif
