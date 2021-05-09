#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "private.h"
#include "uthread.h"

/*
 * Frequency of preemption
 * 100Hz is 100 times per second
 */
#define HZ 100

struct sigaction sa;
struct itimerval timer;
sigset_t blocker;

static void handler(int signum) {
	/* num is useless, doing some useless operation for error free compiling*/
	uthread_yield();
	signum++;
}

void preempt_disable(void)
{
	/* TODO Phase 4 */
	/* stop all the alarm signals */
	sigprocmask(SIG_BLOCK, &blocker, NULL);
}

void preempt_enable(void)
{
	/* TODO Phase 4 */
	/* if alarm is fired during the preemptive session, alarm handler will be called (ie. yield) */
	sigprocmask(SIG_UNBLOCK, &blocker, NULL);
}

void preempt_start(void)
{
	/* TODO Phase 4 */
	/* setting up the timer */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &handler;
	sigaction(SIGVTALRM, &sa, NULL);

	/* set up the blocker */
	sigemptyset(&blocker);
	sigaddset(&blocker, SIGVTALRM);

	/* math time */
	/*
	*	100 HZ = 100 times per sec = 1 time per 0.01 sec
	*	1 time per 0.01sec = 0.01sec per time
	*	0.01sec = 0.01sec * 1000000microsecond = 10000
	*	100 HZ = 1000000micro second / 100 = 10000 micro second per time
	*/

	/* fire the alarm at 1000000 / HZ microsecond */
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 1000000 / HZ;
	/* keep firing the alarm at the same frequency */
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 1000000 / HZ;

	setitimer(ITIMER_VIRTUAL, &timer, NULL);
}

void preempt_stop(void)
{
	/* TODO Phase 4 */
	/* timer will stop once all values in it_value and it_interval becomes zero */
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_usec = 0;
}

