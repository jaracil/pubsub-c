#include <stdarg.h>

#include "pubsub.h"
#include "uthash.h"
#include "utlist.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

typedef struct subscriber_list_s {
	ps_subscriber_t *su;
	struct subscriber_list_s *next;
	struct subscriber_list_s *prev;
} subscriber_list_t;

typedef struct topic_map_s {
	char *topic;
	subscriber_list_t *subscribers;
	ps_msg_t *sticky;
	UT_hash_handle hh;
} topic_map_t;

typedef struct subscriptions_list_s {
	topic_map_t *tm;
	struct subscriptions_list_s *next;
	struct subscriptions_list_s *prev;
} subscriptions_list_t;

struct ps_subscriber_s {
	QueueHandle_t q;
	subscriptions_list_t *subs;
	uint32_t overflow;
};

static SemaphoreHandle_t sem;
static topic_map_t *topic_map = NULL;

static uint32_t stat_live_msg;
static uint32_t stat_live_subscribers;

void ps_init(void) {
	sem = xSemaphoreCreateMutex();
	xSemaphoreGive(sem);
}

ps_msg_t *ps_new_msg(const char *topic, uint32_t flags, ...) {
	ps_msg_t *msg = calloc(1, sizeof(ps_msg_t));
	va_list args;
	va_start(args, flags);

	msg->_ref = 1;
	msg->flags = flags;
	msg->topic = strdup(topic);
	msg->rtopic = NULL;

	if (flags & FL_RESP) {
		msg->rtopic = strdup((va_arg(args, char *)));
	}

	if (IS_INT(msg)) {
		msg->int_val = (va_arg(args, int64_t));
	} else if (IS_DBL(msg)) {
		msg->dbl_val = (va_arg(args, double));
	} else if (IS_PTR(msg)) {
		msg->ptr_val = (va_arg(args, void *));
	} else if (IS_STR(msg)) {
		msg->str_val = strdup((va_arg(args, char *)));
	} else if (IS_BOOL(msg)) {
		msg->bool_val = (va_arg(args, int));
	} else if (IS_BUF(msg)) {
		msg->buf_val.ptr = (va_arg(args, void *));
		msg->buf_val.sz = (va_arg(args, size_t));
		msg->buf_val.dtor = (va_arg(args, ps_dtor_t));
	} else if (IS_ERR(msg)) {
		msg->err_val.id = (va_arg(args, int));
		msg->err_val.desc = strdup((va_arg(args, char *)));
	}
	__sync_add_and_fetch(&stat_live_msg, 1);
	return msg;
}

ps_msg_t *ps_ref_msg(ps_msg_t *msg) {
	__sync_add_and_fetch(&msg->_ref, 1);
	return msg;
}

void ps_unref_msg(ps_msg_t *msg) {
	if (__sync_sub_and_fetch(&msg->_ref, 1) == 0) {
		free(msg->topic);
		if (msg->rtopic != NULL) {
			free(msg->rtopic);
		}
		if (IS_STR(msg)) {
			free(msg->str_val);
		} else if (IS_BUF(msg)) {
			if (msg->buf_val.dtor != NULL) {
				msg->buf_val.dtor(msg->buf_val.ptr);
			}
		} else if (IS_ERR(msg)) {
			free(msg->err_val.desc);
		}
		free(msg);
		__sync_sub_and_fetch(&stat_live_msg, 1);
	}
}

size_t ps_stats_live_msg(void) {
	return __sync_fetch_and_add(&stat_live_msg, 0);
}

ps_subscriber_t *ps_new_subscriber(size_t queue_size, char *subs[]) {
	ps_subscriber_t *su = calloc(1, sizeof(ps_subscriber_t));
	su->q = xQueueCreate(queue_size, sizeof(ps_msg_t *));
	su->overflow = false;
	ps_subscribe_many(su, subs);
	__sync_add_and_fetch(&stat_live_subscribers, 1);
	return su;
}

void ps_free_subscriber(ps_subscriber_t *su) {
	ps_unsubscribe_all(su);
	ps_flush(su);
	vQueueDelete(su->q);
	free(su);
	__sync_sub_and_fetch(&stat_live_subscribers, 1);
}

size_t ps_stats_live_subscribers(void) {
	return __sync_fetch_and_add(&stat_live_subscribers, 0);
}

size_t ps_flush(ps_subscriber_t *su) {
	size_t flushed = 0;
	ps_msg_t *msg = NULL;
	while (xQueueReceive(su->q, &msg, 0) == pdTRUE) {
		ps_unref_msg(msg);
		flushed++;
	}
	return flushed;
}

int ps_subscribe_many(ps_subscriber_t *su, char *subs[]) {
	int n = 0;
	if (subs != NULL) {
		size_t idx = 0;
		while (subs[idx] != NULL) {
			if (ps_subscribe(su, subs[idx++]) == 0)
				n++;
		}
	}
	return n;
}

int ps_subscribe(ps_subscriber_t *su, const char *topic) {
	int ret = 0;
	topic_map_t *tm;
	subscriber_list_t *sl;
	subscriptions_list_t *subs;

	while (!xSemaphoreTake(sem, portMAX_DELAY))
		;
	HASH_FIND_STR(topic_map, topic, tm);
	if (tm == NULL) {
		tm = calloc(1, sizeof(*tm));
		tm->topic = strdup(topic);
		HASH_ADD_KEYPTR(hh, topic_map, tm->topic, strlen(tm->topic), tm);
	}
	DL_SEARCH_SCALAR(tm->subscribers, sl, su, su);
	if (sl != NULL) {
		ret = -1;
		goto exit_fn;
	}
	sl = calloc(1, sizeof(*sl));
	sl->su = su;
	DL_APPEND(tm->subscribers, sl);
	subs = calloc(1, sizeof(*subs));
	subs->tm = tm;
	DL_APPEND(su->subs, subs);
	if (tm->sticky != NULL) {
		ps_ref_msg(tm->sticky);
		if (xQueueSend(su->q, &tm->sticky, 0) != pdTRUE) {
			__sync_add_and_fetch(&su->overflow, 1);
			ps_unref_msg(tm->sticky);
		}
	}

exit_fn:
	xSemaphoreGive(sem);
	return ret;
}

int ps_unsubscribe(ps_subscriber_t *su, const char *topic) {
	int ret = 0;
	topic_map_t *tm;
	subscriber_list_t *sl;
	subscriptions_list_t *subs;

	while (!xSemaphoreTake(sem, portMAX_DELAY))
		;
	HASH_FIND_STR(topic_map, topic, tm);
	if (tm == NULL) {
		ret = -1;
		goto exit_fn;
	}
	DL_SEARCH_SCALAR(tm->subscribers, sl, su, su);
	if (sl == NULL) {
		ret = -1;
		goto exit_fn;
	}
	DL_DELETE(tm->subscribers, sl);
	free(sl);
	if (tm->subscribers == NULL && tm->sticky == NULL) { // Empty list
		HASH_DEL(topic_map, tm);
		free(tm->topic);
		free(tm);
	}
	DL_SEARCH_SCALAR(su->subs, subs, tm, tm);
	if (subs != NULL) {
		DL_DELETE(su->subs, subs);
		free(subs);
	}

exit_fn:
	xSemaphoreGive(sem);
	return ret;
}

size_t ps_unsubscribe_all(ps_subscriber_t *su) {
	subscriptions_list_t *s, *ps;
	subscriber_list_t *sl;
	size_t count = 0;

	while (!xSemaphoreTake(sem, portMAX_DELAY))
		;
	s = su->subs;
	while (s != NULL) {
		DL_SEARCH_SCALAR(s->tm->subscribers, sl, su, su);
		if (sl != NULL) {
			DL_DELETE(s->tm->subscribers, sl);
			free(sl);
			if (s->tm->subscribers == NULL && s->tm->sticky == NULL) { // Empty list
				HASH_DEL(topic_map, s->tm);
				free(s->tm->topic);
				free(s->tm);
			}
		}
		ps = s;
		s = s->next;
		free(ps);
		count++;
	}
	su->subs = NULL;
	xSemaphoreGive(sem);
	return count;
}

ps_msg_t *ps_get(ps_subscriber_t *su, int tout) {
	ps_msg_t *msg = NULL;
	if (tout < 0)
		tout = portMAX_DELAY;
	xQueueReceive(su->q, &msg, tout);
	return msg;
}

size_t ps_num_subs(ps_subscriber_t *su) {
	size_t count;
	subscriptions_list_t *elt;
	DL_COUNT(su->subs, elt, count);
	return count;
}

size_t ps_waiting(ps_subscriber_t *su) {
	return uxQueueMessagesWaiting(su->q);
}

size_t ps_overflow(ps_subscriber_t *su) {
	uint32_t n = __sync_fetch_and_sub(&su->overflow, 0);
	__sync_fetch_and_sub(&su->overflow, n);
	return n;
}

void ps_clean_sticky(void) {
	topic_map_t *tm, *tm_tmp;

	while (!xSemaphoreTake(sem, portMAX_DELAY))
		;
	HASH_ITER(hh, topic_map, tm, tm_tmp) {
		if (tm->sticky != NULL) {
			ps_unref_msg(tm->sticky);
			tm->sticky = NULL;
		}
	}
	xSemaphoreGive(sem);
}

size_t ps_publish(ps_msg_t *msg) {
	topic_map_t *tm;
	subscriber_list_t *sl;
	size_t ret = 0;
	char *topic = strdup(msg->topic);
	while (!xSemaphoreTake(sem, portMAX_DELAY))
		;
	if (msg->flags & FL_STICKY) {
		HASH_FIND_STR(topic_map, topic, tm);
		if (tm == NULL) {
			tm = calloc(1, sizeof(*tm));
			tm->topic = strdup(topic);
			HASH_ADD_KEYPTR(hh, topic_map, tm->topic, strlen(tm->topic), tm);
		}
		if (tm->sticky != NULL) {
			ps_unref_msg(tm->sticky);
		}
		tm->sticky = ps_ref_msg(msg);
	}
	while (strlen(topic) > 0) {
		HASH_FIND_STR(topic_map, topic, tm);
		if (tm != NULL) {
			DL_FOREACH (tm->subscribers, sl) {
				ps_ref_msg(msg);
				if (xQueueSend(sl->su->q, &msg, 0) != pdTRUE) {
					__sync_add_and_fetch(&sl->su->overflow, 1);
					ps_unref_msg(msg);
				}
				ret++;
			}
		}
		if (msg->flags & FL_NONRECURSIVE)
			break;
		for (size_t n = strlen(topic); n > 0; n--) {
			if (topic[n - 1] == '.') {
				topic[n - 1] = 0;
				break;
			}
			topic[n - 1] = 0;
		}
	}
	ps_unref_msg(msg);
	free(topic);
	xSemaphoreGive(sem);
	return ret;
}
