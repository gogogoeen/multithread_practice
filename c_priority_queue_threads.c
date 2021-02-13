/**
 * Compile:
 *     gcc -std=gnu11 -Wall -Wextra c_priority_queue_threads.c -o priority_queue_threads -lpthread -lrt
 */

#include <errno.h>
#include <mqueue.h>
#include <fcntl.h>    /* For O_* constants. */
#include <sys/stat.h> /* For mode constants. */
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define QUEUE_NAME  "/test_queue" /* Queue name. */
#define QUEUE_PERMS ((int)(0644))
#define QUEUE_MAXMSG  16 /* Maximum number of messages. */
#define QUEUE_MSGSIZE 1024 /* Length of message. */
#define QUEUE_ATTR_INITIALIZER ((struct mq_attr){0, QUEUE_MAXMSG, QUEUE_MSGSIZE, 0, {0}})

/* The consumer is faster than the publisher. */
#define QUEUE_POLL_CONSUMER ((struct timespec){2, 500000000})
#define QUEUE_POLL_PUBLISHER ((struct timespec){5, 0})

#define QUEUE_MAX_PRIO ((int)(9))

static bool th_consumer_running = true;
static bool th_publisher_running = true;

void signal_handler(int signum) {
	fprintf(stderr, "\n\nReceived signal: %d.\nStopping threads...\n", signum);
	th_consumer_running = false;
	th_publisher_running = false;
}

void * queue_consumer(void * args) {
	(void) args; /* Suppress -Wunused-parameter warning. */
	/* Initialize the queue attributes */
	struct mq_attr attr = QUEUE_ATTR_INITIALIZER;
	
	/* Create the message queue. The queue reader is NONBLOCK. */
	mqd_t mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY | O_NONBLOCK, QUEUE_PERMS, &attr);
	if(mq < 0) {
		fprintf(stderr, "[CONSUMER]: Error, cannot open the queue: %s.\n", strerror(errno));
		exit(1);
	}
	
	printf("[CONSUMER]: Queue opened, queue descriptor: %d.\n", mq);
	
	unsigned int prio;
	ssize_t bytes_read;
	char buffer[QUEUE_MSGSIZE + 1];
	struct timespec poll_sleep;
	while(th_consumer_running) {
		memset(buffer, 0x00, sizeof(buffer));
		bytes_read = mq_receive(mq, buffer, QUEUE_MSGSIZE, &prio);
		if(bytes_read >= 0) {
			printf("[CONSUMER]: Received message: \"%s\"\n", buffer);
		} else {
			printf("[CONSUMER]: No messages yet.\n");
			poll_sleep = QUEUE_POLL_CONSUMER;
			nanosleep(&poll_sleep, NULL);
		}
		
		fflush(stdout);
	}
	
	/* Cleanup */
	printf("[CONSUMER]: Cleanup...\n");
	mq_close(mq);
	mq_unlink(QUEUE_NAME);
	
	return NULL;
}

void * queue_publisher(void * args) {
	(void) args; /* Suppress -Wunused-parameter warning. */
	/* Open the created queue by the consumer. */
	mqd_t mq;
	struct timespec poll_sleep;
	do {
		mq = mq_open(QUEUE_NAME, O_WRONLY);
		if(mq < 0) {
			printf("[PUBLISHER]: The queue is not created yet. Waiting...\n");
			
			poll_sleep = QUEUE_POLL_PUBLISHER;
			nanosleep(&poll_sleep, NULL);
		}
	} while(mq == -1);
	
	printf("[PUBLISHER]: Queue opened, queue descriptor: %d.\n", mq);
	
	/* Intializes random number generator. */
	srand((unsigned)time(NULL));
	
	unsigned int prio = 0;
	int count = 1;
	char buffer[QUEUE_MSGSIZE];
	while(th_publisher_running) {
		/* Send a burst of three messages */
		for(int i=0; i<3; i++) {
			prio = rand() % QUEUE_MAX_PRIO;
			snprintf(buffer, sizeof(buffer), "MESSAGE NUMBER %d, PRIORITY %d", count, prio);
			printf("[PUBLISHER]: Sending message %d with priority %d...\n", count, prio);
			mq_send(mq, buffer, QUEUE_MSGSIZE, prio);
			count++;
		}
		
		poll_sleep = QUEUE_POLL_PUBLISHER;
		nanosleep(&poll_sleep, NULL);
		
		fflush(stdout);
	}
	
	/* Cleanup */
	printf("[PUBLISHER]: Cleanup...\n");
	mq_close(mq);
	
	return NULL;
}

int main() {
	pthread_t th_publisher, th_consumer;
	
	signal(SIGINT, signal_handler);
	
	uid_t user_id = getuid();
	if(user_id > 0) {
		printf("Run as root.\n");
		exit(EXIT_FAILURE);
	}
	
	printf("Start...\n");
	
	pthread_create(&th_publisher, NULL, &queue_publisher, NULL);
	pthread_create(&th_consumer, NULL, &queue_consumer, NULL);
	
	pthread_join(th_publisher, NULL);
	pthread_join(th_consumer, NULL);
	
	printf("Done...\n");
	
	return (EXIT_SUCCESS);
}