/**************************************************************************************************
* F(X) = 1193181 / (1000000/(X/100))
* F(X): divisor needed for a thread to wait at least X ns when there are 100 threads running
* the divisor must be a 16bit integer, therefore:
* it is impossible for a thread to wait more than 5492500 nanoseconds when there are 100 threads
* it is impossible for a thread to wait less than 84 nanoseconds when there are 100 threads
*
* Since the divisor is an integer, we must recalculate the real waiting time:
* W(D) = (D*100*1000000)/1193181
* Therefore, all threads will run W(D)/100 nanoseconds
***************************************************************************************************/

/*1second for 100 thread: F(1000000) = 11931*/
#define TIMER_RELOAD_VALUE 11931			/*real waiting time is 999932 nanoseconds*/



