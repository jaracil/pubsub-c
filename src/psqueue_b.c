#include "psqueue.h"

#ifdef PS_QUEUE_BUCKET

#include <stdlib.h>
#include "sync.h"
#include "utlist.h"
#include "pubsub.h"

#define PRIORITIES 10 // 0-9 priorities

typedef struct node_s {
	struct node_s *prev;
	struct node_s *next;
	ps_msg_t *msg;
} node_t;

struct ps_queue_s {
	node_t *priorities[PRIORITIES];
	node_t *available;
	mutex_t mux;
	semaphore_t not_empty;
};

ps_queue_t *ps_new_queue(size_t sz) {
	ps_queue_t *q = calloc(1, sizeof(ps_queue_t));
	mutex_init(&q->mux);
	semaphore_init(&q->not_empty, 0);

	for (size_t i = 0; i < sz; i++) {
		node_t *n = (node_t *) calloc(1, sizeof(node_t));
		DL_APPEND(q->available, n);
	}

	return q;
}

static int bqueue_get_available(ps_queue_t *q, node_t **n, uint8_t max_prio) {
	if (q->available != NULL) {
		*n = q->available;
		DL_DELETE(q->available, q->available);
		return PS_QUEUE_OK;
	}

	// There is none available, we need to drop the one with the lowest priority
	for (size_t i = 0; i < max_prio; i++) {
		if (q->priorities[i] != NULL) {
			*n = q->priorities[i]->prev;
			ps_unref_msg((*n)->msg);
			(*n)->msg = NULL;
			DL_DELETE(q->priorities[i], *n);
			return PS_QUEUE_EOVERFLOW;
		}
	}

	return PS_QUEUE_EFULL;
}

static void bqueue_get(ps_queue_t *q, ps_msg_t **msg) {
	for (int i = PRIORITIES - 1; i >= 0; i--) {
		if (q->priorities[i] != NULL) {
			node_t *n = q->priorities[i];
			*msg = n->msg;
			DL_DELETE(q->priorities[i], n);
			n->msg = NULL;
			DL_APPEND(q->available, n);
			return;
		}
	}
	*msg = NULL;
	return;
}

static void bqueue_insert(ps_queue_t *q, node_t *n, uint8_t priority) {
	DL_APPEND(q->priorities[priority], n);
}

void ps_free_queue(ps_queue_t *q) {
	node_t *n = NULL;
	node_t *e = NULL;

	for (size_t i = 0; i < PRIORITIES; i++) {
		DL_FOREACH_SAFE (q->priorities[i], n, e) {
			ps_unref_msg(n->msg);
			DL_DELETE(q->priorities[i], n);
			free(n);
		}
	}

	DL_FOREACH_SAFE (q->available, n, e) {
		DL_DELETE(q->available, n);
		free(n);
	}

	mutex_destroy(&q->mux);
	semaphore_destroy(&q->not_empty);
	free(q);
}

int ps_queue_push(ps_queue_t *q, ps_msg_t *msg, uint8_t priority) {
	int ret = 0;
	mutex_lock(q->mux);

	node_t *n = NULL;
	ret = bqueue_get_available(q, &n, priority);

	if (ret != PS_QUEUE_EFULL) {
		n->msg = msg;
		bqueue_insert(q, n, priority);

		if (ret == PS_QUEUE_OK)
			semaphore_post(q->not_empty);
	}

	mutex_unlock(q->mux);
	return ret;
}

ps_msg_t *ps_queue_pull(ps_queue_t *q, int64_t timeout) {
	ps_msg_t *msg = NULL;

	if (semaphore_wait(q->not_empty, timeout) < 0)
		return NULL;

	mutex_lock(q->mux);
	bqueue_get(q, &msg);
	mutex_unlock(q->mux);

	return msg;
}

size_t ps_queue_waiting(ps_queue_t *q) {
	int ret = semaphore_get(q->not_empty);
	return ret < 0 ? 0 : ret;
}

#endif