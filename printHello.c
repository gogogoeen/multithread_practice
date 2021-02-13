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

#define QUEUE_NAME  "/message_queue" /* Queue name. */
#define QUEUE_PERMS ((int)(0644))
#define QUEUE_MAXMSG  16 /* Maximum number of messages. */
#define QUEUE_MSGSIZE 1024 /* Length of message. */
#define QUEUE_ATTR_INITIALIZER ((struct mq_attr){0, QUEUE_MAXMSG, QUEUE_MSGSIZE, 0, {0}})

#define QUEUE_MAX_PRIO ((int)(9))

static bool th_server_running = true;
static bool th_printer_running = true;

bool receive = false;

pthread_mutex_t mutex;
pthread_cond_t cv;




// signal for exit the program
void signal_handler(int signum) {
	fprintf(stderr, "\n\nReceived signal: %d.\nStopping threads...\n", signum);
	th_server_running = false;
	th_printer_running = false;
}




// 	printer waiting for the message server to print out the "hello message"

void* printer_thread(void* args) {
	(void) args;
	while(th_printer_running) {
		pthread_mutex_lock(&mutex);
		while (!receive) {
			pthread_cond_wait(&cv, &mutex);
		}
		if (th_printer_running)
			printf("[PRINTER]: Hello\n");

		// after printer prints "Hello", set the receive value to false
		// so the printer will be locked until the server signals it to wake
		// it up again
		receive = false;
		pthread_mutex_unlock(&mutex);
	}
	/* Cleanup */
	printf("[PRINTER]: Cleanup...\n");
	pthread_exit(NULL);
	
}



// server for receive the message

void* queue_server(void * args) {
	(void) args; /* Suppress -Wunused-parameter warning. */
	/* Initialize the queue attributes */
	struct mq_attr attr = QUEUE_ATTR_INITIALIZER;
	
	/* Create the message queue. The queue reader is NONBLOCK. */
	mqd_t mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY | O_NONBLOCK, QUEUE_PERMS, &attr);
	if(mq < 0) {
		fprintf(stderr, "[SERVER]: Error, cannot open the queue: %s.\n", strerror(errno));
		exit(1);
	}
	
	printf("[SERVER]: Queue opened, queue descriptor: %d.\n", mq);
	
	unsigned int prio;
	ssize_t bytes_read;
	char buffer[QUEUE_MSGSIZE + 1];
	while(th_server_running) {
		memset(buffer, 0x00, sizeof(buffer));
		bytes_read = mq_receive(mq, buffer, QUEUE_MSGSIZE, &prio);
		if(bytes_read >= 0) {	// if receive message
			printf("[SERVER]: Receive Message\n");
			pthread_mutex_lock(&mutex);
			if (!receive)
				pthread_cond_signal(&cv);
			receive = true;
			pthread_mutex_unlock(&mutex);
		}
		
	
	}
	
	/* Cleanup */
	printf("[SERVER]: Cleanup...\n");

 	// Need to wake up the condition variable at the printer thread
	// , otherwise printer thread can not exit
	if (!receive)
		pthread_cond_signal(&cv);
	receive = true;
	pthread_mutex_unlock(&mutex);

	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cv);

	mq_close(mq);
	mq_unlink(QUEUE_NAME);
		
	pthread_exit(NULL);
}



// time thread

void printTime(){
    struct tm *cursystem;
    time_t tm_t;
    time(&tm_t);
    cursystem = localtime(&tm_t);
    char tszInfo[2048] ;
    sprintf(tszInfo, "%02d:%02d:%02d",
            cursystem->tm_hour, cursystem->tm_min,
            cursystem->tm_sec);
    printf("%s\n",tszInfo);
}


void timer_thread() {
	mqd_t mq;
	mq = mq_open(QUEUE_NAME, O_WRONLY);
	if(mq < 0) {
		printf("[TIMER]: The queue is not ready yet. Waiting...\n");
		return;
	}

	printf("[TIMER]: send message at ");
	printTime();

	unsigned int prio = 1;
	char buffer[QUEUE_MSGSIZE] = "send";
	mq_send(mq, buffer, QUEUE_MSGSIZE, prio);
	fflush(stdout);
	mq_close(mq);
	return;

}


void create_timer() {
	timer_t timer; 
	struct sigevent evp;  
    struct itimerspec ts;
	int ret;
	
	evp.sigev_value.sival_ptr = &timer;
	evp.sigev_notify = SIGEV_THREAD;
	evp.sigev_notify_function = timer_thread;
    evp.sigev_notify_attributes = NULL;    

	ret = timer_create(CLOCK_REALTIME, &evp, &timer);  
    if(ret) {
        perror("timer_create");
    } 
	
	// set the timer period to be 2 second and start

	ts.it_value.tv_sec = 2;  // the delay time start
    ts.it_value.tv_nsec = 0;  
	ts.it_interval.tv_sec = 2; // the spacing time
    ts.it_interval.tv_nsec = 0;  

	ret = timer_settime(timer, 0, &ts, NULL);  
    if(ret) {
        perror("timer_settime");
    }
}



	

int main() {
	pthread_t th_server;		//server thread for receiving message
	pthread_t th_printer;		//thread for printing
	int ret;

	signal(SIGINT, signal_handler);

	uid_t user_id = getuid();					
	if(user_id > 0) {
		printf("Run as root.\n");
		exit(EXIT_FAILURE);
	}
	
	//	create thread for the message queue server and the printer
	printf("Start...\n");
	ret = pthread_create(&th_server, NULL, &queue_server, NULL);
	if (ret) {
		perror("pthread_create");
	}
	ret = pthread_create(&th_printer, NULL, &printer_thread, NULL);
	if (ret) {
		perror("pthread_create");
	}

	ret = pthread_mutex_init(&mutex, NULL);
	if (ret) {
		perror("mutex initialization");
	}

	ret = pthread_cond_init(&cv, NULL);
	if (ret)
	{
		perror("condition variable");
	}

	// create timer for sending message
	create_timer();								

	pthread_join(th_printer, NULL);
	pthread_join(th_server, NULL);
	
	printf("Done...\n");
	
	return (EXIT_SUCCESS);

}
