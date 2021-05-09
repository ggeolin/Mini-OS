#include <stddef.h>
#include <stdlib.h>

#include "queue.h"
#include "sem.h"
#include "private.h"

struct semaphore {
	/* TODO Phase 3 */
	int key;
	queue_t block_queue;
};

sem_t sem_create(size_t count)
{
	/* TODO Phase 3 */
	sem_t new_sem = malloc(sizeof(struct semaphore));

	if(new_sem == NULL)
		return NULL;

	new_sem->key = count;
	new_sem->block_queue = queue_create();

	return new_sem;
}

int sem_destroy(sem_t sem)
{
	/* TODO Phase 3 */
	if(sem == NULL || queue_length(sem->block_queue) == 0)
		return -1;
	
	queue_destroy(sem->block_queue);
	free(sem);

	return 0;
}

int sem_down(sem_t sem)
{
	/* TODO Phase 3 */
	if(sem == NULL)
		return -1;

	/* making sure the sem_down will not be interupted */
	preempt_disable();

	while(sem->key == 0) {
		struct uthread_tcb* tmp = uthread_current();

		queue_enqueue(sem->block_queue, tmp);
		
		uthread_block();
	}

	sem->key -= 1;
	
	/* if alarm is fired within this function, run yield */
	preempt_enable();
	
	return 0;
}

int sem_up(sem_t sem)
{
	/* TODO Phase 3 */
	if(sem == NULL)
		return -1;

	/* making sure the sem_up will not be interupted */
	preempt_disable();
	
	/* if there are blocked thread in the block_queue, release them back to the schedule queue in main */
	if(queue_length(sem->block_queue) != 0) {
		struct uthread_tcb* tmp;

		queue_dequeue(sem->block_queue, (void**)&tmp);

		uthread_unblock(tmp);
	}

	sem->key += 1;
	
	/* if alarm is fired within this function, run yield */
	preempt_enable();

	return 0;
}
