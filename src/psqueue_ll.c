#include <stdlib.h>
#include "psqueue.h"
#include "sync.h"

#ifdef PS_QUEUE_LL

struct ps_queue_s {
	ps_msg_t **messages;
	size_t size;
	size_t count;
	size_t head;
	size_t tail;
	mutex_t mux;
	semaphore_t not_empty;
};

ps_queue_t *ps_new_queue(size_t sz) {
	ps_queue_t *q = calloc(1, sizeof(ps_queue_t));
	q->size = sz;
	q->messages = calloc(sz, sizeof(void *));
	mutex_init(&q->mux);
	semaphore_init(&q->not_empty, 0);

	return q;
}

void ps_free_queue(ps_queue_t *q) {
	free(q->messages);
	mutex_destroy(&q->mux);
	semaphore_destroy(&q->not_empty);
	free(q);
}

int ps_queue_push(ps_queue_t *q, ps_msg_t *msg, uint8_t priority) {
	(void) priority; // This implementation has no priority
	int ret = 0;
	mutex_lock(q->mux);
	if (q->count >= q->size) {
		ret = -1;
		goto exit_fn;
	}
	q->messages[q->head] = msg;
	if (++q->head >= q->size)
		q->head = 0;
	q->count++;
	semaphore_post(q->not_empty);

exit_fn:
	mutex_unlock(q->mux);
	return ret;
}

ps_msg_t *ps_queue_pull(ps_queue_t *q, int64_t timeout) {
	ps_msg_t *msg = NULL;

	if (semaphore_wait(q->not_empty, timeout) < 0)
		return NULL;

	mutex_lock(q->mux);
	msg = q->messages[q->tail];
	if (++q->tail >= q->size)
		q->tail = 0;
	q->count--;
	mutex_unlock(q->mux);

	return msg;
}

size_t ps_queue_waiting(ps_queue_t *q) {
	size_t res = 0;
	mutex_lock(q->mux);
	res = q->count;
	mutex_unlock(q->mux);
	return res;
}

#endif