#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

struct queue_node {
	struct queue_node* next;
	void* data;
};

struct queue {
	/* TODO Phase 1 */
	int length;
	struct queue_node* first;
	struct queue_node* last;
};

queue_t queue_create(void)
{
	/* TODO Phase 1 */
	queue_t new_queue = malloc(sizeof(struct queue));

	if(new_queue != NULL) {
		/* initalizize the struct if successful*/
		new_queue->first = NULL;
		new_queue->last = NULL;
		new_queue->length = 0;

		return new_queue;
	}

	return NULL;
}

int queue_destroy(queue_t queue)
{
	/* TODO Phase 1 */
	if(queue == NULL || queue->first != NULL)
		return -1;

	free(queue);
	
	return 0;
}

int queue_enqueue(queue_t queue, void *data)
{
	/* TODO Phase 1 */
	struct queue_node* new_node = malloc(sizeof(struct queue_node));

	if(queue == NULL || data == NULL || new_node == NULL)
		return -1;

	/* assign values into the new_node */
	new_node->next = NULL;
	new_node->data = data;

	if(queue->first == NULL) {
		/* if the queue is empty: special treatment */

		/* put the node into queue */
		queue->first = new_node;
		queue->last = new_node;
		queue->length += 1;
	} else {
		/* else the queue is not empty */

		/* point the last_node pointer to the new_node */
		queue->last->next = new_node;
		queue->last = new_node;
		queue->length += 1;
	}

	return 0;
}

int queue_dequeue(queue_t queue, void **data)
{
	/* TODO Phase 1 */
	if(queue == NULL || data == NULL || queue->first == NULL)
		return -1;

	/* store the adreess of data into the *data */
	*data = queue->first->data;

	queue->first = queue->first->next;

	/* if the queue is empty */
	if(queue->first == NULL)
		queue->last = NULL;

	queue->length -= 1;

	return 0;
}

int queue_delete(queue_t queue, void *data)
{
	/* TODO Phase 1 */
	if(queue == NULL || data == NULL || queue->first == NULL)
		return -1;

	struct queue_node* current_node = queue->first;
	struct queue_node* previous_node = NULL;
	while(current_node != NULL) {
		if(current_node->data == data) {
			if(current_node->next == NULL && previous_node == NULL) {
				/* if the node is the lonly node within the queue */
				queue->first = NULL;
				queue->last = NULL;
			} else if(previous_node == NULL) {
				/* if the node is at the beginning of the queue */
				queue->first = current_node->next;
			} else if (current_node->next == NULL && previous_node != NULL) {
				/* if the node is at the end of the queue */
				previous_node->next = NULL;
				queue->last = previous_node;
			} else {
				/* if the node is in the middle of the queue */
				previous_node->next = current_node->next;
			}

			free(current_node);
			queue->length -= 1;
			return 0;
		}

		previous_node = current_node;
		current_node = current_node->next;
	}

	return -1;
}

int queue_iterate(queue_t queue, queue_func_t func)
{
	/* TODO Phase 1 */
	if(queue == NULL || func == NULL)
		return -1;

	struct queue_node* current_node = queue->first;

	while(current_node != NULL) {
		func(current_node->data);

		current_node = current_node->next;
	}

	return 0;
}

int queue_length(queue_t queue)
{
	if(queue == NULL)
		return -1;

	return queue->length;
}
