#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "private.h"
#include "uthread.h"
#include "queue.h"

#define STATUS_RUN 1
#define STATUS_READY 2
#define STATUS_BLOCK 3
#define STATUS_EXIT 4

static queue_t ready_queue;						/* the main ready queue */
static queue_t recycle_bin;						/* collect the exited threads*/
static struct uthread_tcb* running_thread;		/* the current running thread, usually the functions */

struct uthread_tcb {
	uthread_ctx_t *exe_context;
	void* stack_ptr;
	int state;
};

struct uthread_tcb *uthread_current(void)
{
	return running_thread;
}

void garbage_cleaner() {
	struct uthread_tcb* exited_thread;

	/* clean up the recycle bin first */
	queue_dequeue(recycle_bin, (void**)&exited_thread);

	while(queue_length(recycle_bin) != 0) {
		uthread_ctx_destroy_stack(exited_thread->stack_ptr);
		free(exited_thread->exe_context);
		free(exited_thread);

		queue_dequeue(recycle_bin, (void**)&exited_thread);
	}

	/* destory all the queues */
	queue_destroy(ready_queue);
	queue_destroy(recycle_bin);
}

void uthread_yield(void)
{
	/* don't want the yield to be interupted */
	preempt_disable();

	struct uthread_tcb *prev_tcb;
	struct uthread_tcb *next_tcb;

	/* save the current thread to the temporaty container */
	prev_tcb = running_thread;

	if (prev_tcb->state == STATUS_RUN)
	{
		/* if the yield comes from a running thread */
		prev_tcb->state = STATUS_READY;
		queue_enqueue(ready_queue, prev_tcb);
	} else if(prev_tcb->state == STATUS_EXIT) {
		/* if the yield comes from a exit thread */
		/* no need to push it back to the ready queue */
		/* throw it to the recycle bin */
		queue_enqueue(recycle_bin, prev_tcb);
	}

	/* throw the next thread in line into the next_tcb */
	queue_dequeue(ready_queue, (void**) &next_tcb);

	/* next thread will be put to run */
	next_tcb->state = STATUS_RUN;

	/* now the currently running thread will be next_tcb */
	running_thread = next_tcb;

	/* go to the next thread */
	uthread_ctx_switch(prev_tcb->exe_context, next_tcb->exe_context);
	
	preempt_enable();
}

void uthread_exit(void)
{
	running_thread->state = STATUS_EXIT;
	/* go to the next thread if there's  any */
	uthread_yield();
}

int uthread_create(uthread_func_t func, void *arg)
{
	preempt_disable();
	/* malloc a new_thread */
	struct uthread_tcb* new_thread = malloc(sizeof(struct uthread_tcb));
	new_thread->stack_ptr   = uthread_ctx_alloc_stack();
	new_thread->state   = STATUS_READY;
	new_thread->exe_context = malloc(sizeof(uthread_ctx_t));

	/* error checking */
	if(new_thread == NULL || new_thread->stack_ptr == NULL || new_thread->exe_context == NULL)
		return -1;

	/* initialize the new_thread  and throw it to the ready queue*/
	uthread_ctx_init(new_thread->exe_context, new_thread->stack_ptr, func, arg);

	queue_enqueue(ready_queue, new_thread);

	preempt_enable();

	return 0;
}

int uthread_start(uthread_func_t func, void *arg)
{
	ready_queue = queue_create();
	recycle_bin = queue_create();

	/* build an idle thread to come back from */
	struct uthread_tcb* idle_thread = malloc(sizeof(struct uthread_tcb));
	idle_thread->state   = STATUS_RUN;
	idle_thread->exe_context = malloc(sizeof(uthread_ctx_t));

	if(idle_thread->exe_context == NULL || idle_thread->exe_context == NULL)
		return -1;

	/* techniquelly the idle thread is the current thread */
	running_thread = idle_thread;

	/* create the initial thread */
	uthread_create(func, arg);

	/* initialize the timer */
	preempt_start();

	while(queue_length(ready_queue) != 0)
		uthread_yield();

	/* terminate the timer */
	preempt_stop();

	/* clean up all ready queue and recycle queue */
	garbage_cleaner();

	return 0;
}

void uthread_block(void)
{
	running_thread->state = STATUS_BLOCK;
	uthread_yield();
}

void uthread_unblock(struct uthread_tcb *uthread)
{
	uthread->state = STATUS_READY;
	queue_enqueue(ready_queue, uthread);
}
