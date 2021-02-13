# Small Multithread programming practice
### This is a multithread task from gogoro's interview:
- To create a POSIX thread (thread_A) to continuously print “HELLO\n” as mutex (mutex_A) unlocked, and lock mutex (mutex_A) after printing
- To create another POSIX thread (thread_B) with a message queue server(queue_B). To unlock mutex_A once get any message from queue_B
- To create a timer (timer_C) with an interval of 2 seconds. As timer_C timeout, to send a message to queue_B.

### Reference
Example of POSIX timer: timer.c, sigev_thread2.c
Example of POSIX message queue: c_priority_queue_thread.c
