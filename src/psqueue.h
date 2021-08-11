#pragma once

#include <stddef.h>
#include "pubsub.h"

enum {
	PS_QUEUE_OK = 0,
	PS_QUEUE_EFULL = -1,
	PS_QUEUE_EOVERFLOW = -2,
};

typedef struct ps_queue_s ps_queue_t;

ps_queue_t *ps_new_queue(size_t sz);
void ps_free_queue(ps_queue_t *q);
int ps_queue_push(ps_queue_t *q, ps_msg_t *msg, uint8_t priority);
ps_msg_t *ps_queue_pull(ps_queue_t *q, int64_t timeout);
size_t ps_queue_waiting(ps_queue_t *q);