#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#define TRUE (1)
void testTimerSign();
void SignHandler(int iSignNo);
void printTime();
int main() {
    testTimerSign();
    while(TRUE) {
    }
    return 0;
}
void SignHandler(int iSignNo){
    printTime();
    if(iSignNo == SIGUSR1){
        printf("Capture sign No.=SIGUSR1\n");
    }else{
        printf("Capture sign No.=%d\n",iSignNo);
    }
}
void testTimerSign(){
    struct sigevent evp;  
    struct itimerspec ts;  
    timer_t timer;  
    int ret;  
    evp.sigev_value.sival_ptr = &timer;  
    evp.sigev_notify = SIGEV_SIGNAL;  
    evp.sigev_signo = SIGUSR1;  
    signal(evp.sigev_signo, SignHandler);
    ret = timer_create(CLOCK_REALTIME, &evp, &timer);  
    if(ret) {
        perror("timer_create");
    }
    ts.it_interval.tv_sec = 2; // the spacing time，間隔時間
    ts.it_interval.tv_nsec = 0;  
    ts.it_value.tv_sec = 2;  // the delay time start，倒計時時間
    ts.it_value.tv_nsec = 0;  
    printTime();
    printf("start\n");
    ret = timer_settime(timer, 0, &ts, NULL);  
    if(ret) {
        perror("timer_settime");
    }
}
void printTime(){
    struct tm *cursystem;
    time_t tm_t;
    time(&tm_t);
    cursystem = localtime(&tm_t);
    char tszInfo[2048] ;
    sprintf(tszInfo, "%02d:%02d:%02d",
            cursystem->tm_hour, cursystem->tm_min,
            cursystem->tm_sec);
    printf("[%s]",tszInfo);
}