#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>

#include "pubsub.h"
#include "uthash.h"
#include "utlist.h"

#ifdef PS_FREE_RTOS
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#else
#include <pthread.h>
#endif

typedef struct ps_queue_s {
#ifdef PS_FREE_RTOS
	QueueHandle_t queue;
#else
	ps_msg_t **messages;
	size_t size;
	size_t count;
	size_t head;
	size_t tail;
	pthread_mutex_t mux;
	pthread_cond_t not_empty;
#endif
} ps_queue_t;

typedef struct subscriber_list_s {
	ps_subscriber_t *su;
	bool hidden;
	bool on_empty;
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
	ps_queue_t *q;
	subscriptions_list_t *subs;
	uint32_t overflow;
	new_msg_cb_t new_msg_cb;
	void *userData;
};

#ifdef PS_FREE_RTOS
static SemaphoreHandle_t lock;
#else
static pthread_mutex_t lock;
#endif

static topic_map_t *topic_map = NULL;

static uint32_t uuid_ctr;

static uint32_t stat_live_msg;
static uint32_t stat_live_subscribers;

#ifdef PS_FREE_RTOS
#define PORT_LOCK xSemaphoreTake(lock, portMAX_DELAY);
#define PORT_UNLOCK xSemaphoreGive(lock);
#else
#define PORT_LOCK pthread_mutex_lock(&lock);
#define PORT_UNLOCK pthread_mutex_unlock(&lock);
#endif

void ps_init(void) {
#ifdef PS_FREE_RTOS
	lock = xSemaphoreCreateMutex();
#else
	pthread_mutex_init(&lock, NULL);
#endif
}

#ifndef PS_FREE_RTOS
static int deadline_ms(int64_t ms, struct timespec *tout) {
#ifdef PS_USE_GETTIMEOFDAY
	struct timeval tv;
	gettimeofday(&tv, NULL);
	tout->tv_sec = tv.tv_sec;
	tout->tv_nsec = tv.tv_usec * 1000;
#else
	clock_gettime(CLOCK_MONOTONIC, tout);
#endif
	tout->tv_sec += (ms / 1000);
	tout->tv_nsec += ((ms % 1000) * 1000000);
	if (tout->tv_nsec > 1000000000) {
		tout->tv_sec++;
		tout->tv_nsec -= 1000000000;
	}
	return 0;
}
#endif

static ps_queue_t *ps_new_queue(size_t sz) {

	ps_queue_t *q = calloc(1, sizeof(ps_queue_t));
#ifdef PS_FREE_RTOS
	q->queue = xQueueCreate(sz, sizeof(void *));
#else
	q->size = sz;
	q->messages = calloc(sz, sizeof(void *));
	pthread_mutex_init(&q->mux, NULL);

#ifdef PS_USE_GETTIMEOFDAY
	pthread_cond_init(&q->not_empty, NULL);
#else
	pthread_condattr_t cond_attr;
	pthread_condattr_init(&cond_attr);
	pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
	pthread_cond_init(&q->not_empty, &cond_attr);
	pthread_condattr_destroy(&cond_attr);
#endif
#endif
	return q;
}

static void ps_free_queue(ps_queue_t *q) {
#ifdef PS_FREE_RTOS
	vQueueDelete(q->queue);
#else
	free(q->messages);
	pthread_mutex_destroy(&q->mux);
	pthread_cond_destroy(&q->not_empty);
#endif
	free(q);
}

static int ps_queue_push(ps_queue_t *q, ps_msg_t *msg) {

#ifdef PS_FREE_RTOS
	if (xQueueSend(q->queue, &msg, 0) != pdTRUE) {
		return -1;
	}
	return 0;
#else
	int ret = 0;
	pthread_mutex_lock(&q->mux);
	if (q->count >= q->size) {
		ret = -1;
		goto exit_fn;
	}
	q->messages[q->head] = msg;
	if (++q->head >= q->size)
		q->head = 0;
	q->count++;
	pthread_cond_signal(&q->not_empty);

exit_fn:
	pthread_mutex_unlock(&q->mux);
	return ret;
#endif
}

static ps_msg_t *ps_queue_pull(ps_queue_t *q, int64_t timeout) {
	ps_msg_t *msg = NULL;

#ifdef PS_FREE_RTOS
	if (timeout < 0) {
		timeout = portMAX_DELAY;
	}
	if (xQueueReceive(q->queue, &msg, timeout / portTICK_PERIOD_MS) != pdTRUE) {
		return NULL;
	}
	return msg;

#else
	struct timespec tout = {0};
	bool init_tout = false;

	pthread_mutex_lock(&q->mux);
	while (q->count == 0) {
		if (timeout == 0) {
			goto exit_fn;
		} else if (timeout < 0) {
			pthread_cond_wait(&q->not_empty, &q->mux);
		} else {
			if (!init_tout) {
				init_tout = true;
				deadline_ms(timeout, &tout);
			}
			if (pthread_cond_timedwait(&q->not_empty, &q->mux, &tout) != 0)
				goto exit_fn;
		}
	}
	msg = q->messages[q->tail];
	if (++q->tail >= q->size)
		q->tail = 0;
	q->count--;

exit_fn:
	pthread_mutex_unlock(&q->mux);
	return msg;
#endif
}

static size_t ps_queue_waiting(ps_queue_t *q) {
#ifdef PS_FREE_RTOS
	return uxQueueMessagesWaiting(q->queue);
#else
	size_t res = 0;
	pthread_mutex_lock(&q->mux);
	res = q->count;
	pthread_mutex_unlock(&q->mux);
	return res;
#endif
}

static void ps_msg_free_value(ps_msg_t *msg) {
	if (IS_STR(msg)) {
		free(msg->str_val);
	} else if (IS_BUF(msg)) {
		if (msg->buf_val.ptr && msg->buf_val.dtor) {
			msg->buf_val.dtor(msg->buf_val.ptr);
		}
	} else if (IS_ERR(msg)) {
		free(msg->err_val.desc);
	}

	msg->flags = (msg->flags & ~MSK_VALUE) | NIL_TYP;
}

static void ps_msg_set_vvalue(ps_msg_t *msg, uint32_t flags, va_list args) {
	ps_msg_free_value(msg);

	msg->flags = (msg->flags & ~MSK_VALUE) | (flags & MSK_VALUE);
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
}

ps_msg_t *ps_new_msg(const char *topic, uint32_t flags, ...) {
	if (topic == NULL)
		return NULL;

	ps_msg_t *msg = calloc(1, sizeof(ps_msg_t));

	msg->_ref = 1;
	msg->flags = flags;
	msg->topic = strdup(topic);
	msg->rtopic = NULL;

	va_list args;
	va_start(args, flags);

	ps_msg_set_vvalue(msg, flags, args);

	va_end(args);

	__sync_add_and_fetch(&stat_live_msg, 1);
	return msg;
}

ps_msg_t *ps_dup_msg(ps_msg_t const *msg_orig) {

	ps_msg_t *msg = malloc(sizeof(ps_msg_t));
	memcpy(msg, msg_orig, sizeof(ps_msg_t));
	msg->_ref = 1;
	if (msg_orig->topic != NULL) {
		msg->topic = strdup(msg_orig->topic);
	}
	if (msg_orig->rtopic != NULL) {
		msg->rtopic = strdup(msg_orig->rtopic);
	}

	if (IS_STR(msg_orig)) {
		if (msg_orig->str_val != NULL) {
			msg->str_val = strdup(msg_orig->str_val);
		}
	} else if (IS_BUF(msg_orig)) {
		msg->buf_val.ptr = malloc(msg_orig->buf_val.sz);
		memcpy(msg->buf_val.ptr, msg_orig->buf_val.ptr, msg_orig->buf_val.sz);
		msg->buf_val.dtor = free;
	} else if (IS_ERR(msg_orig)) {
		if (msg_orig->err_val.desc != NULL) {
			msg->err_val.desc = strdup(msg_orig->err_val.desc);
		}
	}
	__sync_add_and_fetch(&stat_live_msg, 1);
	return msg;
}

void ps_msg_set_topic(ps_msg_t *msg, const char *topic) {
	if (msg->topic != NULL) {
		free(msg->topic); // Free previous topic
		msg->topic = NULL;
	}
	if (topic != NULL) {
		msg->topic = strdup(topic);
	}
}

void ps_msg_set_rtopic(ps_msg_t *msg, const char *rtopic) {
	if (msg->rtopic != NULL) {
		free(msg->rtopic); // Free previous rtopic
		msg->rtopic = NULL;
	}
	if (rtopic != NULL) {
		msg->rtopic = strdup(rtopic);
	}
}

void ps_msg_set_value(ps_msg_t *msg, uint32_t flags, ...) {
	va_list args;
	va_start(args, flags);

	ps_msg_set_vvalue(msg, flags, args);

	va_end(args);
}

int64_t ps_msg_value_int(const ps_msg_t *msg) {
	if (IS_INT(msg))
		return msg->int_val;
	else if (IS_DBL(msg))
		return msg->dbl_val;
	else if (IS_BOOL(msg))
		return msg->bool_val;
	return 0;
}

double ps_msg_value_double(const ps_msg_t *msg) {
	if (IS_INT(msg))
		return msg->int_val;
	else if (IS_DBL(msg))
		return msg->dbl_val;
	else if (IS_BOOL(msg))
		return msg->bool_val;
	return 0;
}

bool ps_msg_value_bool(const ps_msg_t *msg) {
	if (IS_INT(msg))
		return msg->int_val != 0;
	else if (IS_DBL(msg))
		return msg->dbl_val != 0.0;
	else if (IS_BOOL(msg))
		return msg->bool_val;
	return false;
}

ps_msg_t *ps_ref_msg(ps_msg_t *msg) {
	if (msg != NULL)
		__sync_add_and_fetch(&msg->_ref, 1);
	return msg;
}

void ps_unref_msg(ps_msg_t *msg) {
	if (msg == NULL)
		return;
	if (__sync_sub_and_fetch(&msg->_ref, 1) == 0) {
		free(msg->topic);
		if (msg->rtopic != NULL) {
			free(msg->rtopic);
		}
		ps_msg_free_value(msg);

		free(msg);
		__sync_sub_and_fetch(&stat_live_msg, 1);
	}
}

int ps_stats_live_msg(void) {
	return __sync_fetch_and_add(&stat_live_msg, 0);
}

static int free_topic_if_empty(topic_map_t *tm) {
	if (tm->subscribers == NULL && tm->sticky == NULL) {
		HASH_DEL(topic_map, tm);
		free(tm->topic);
		free(tm);
		return 1;
	}
	return 0;
}

static topic_map_t *create_topic(const char *topic) {
	topic_map_t *tm = NULL;
	tm = calloc(1, sizeof(*tm));
	tm->topic = strdup(topic);
	HASH_ADD_KEYPTR(hh, topic_map, tm->topic, strlen(tm->topic), tm);
	return tm;
}

static topic_map_t *fetch_topic(const char *topic) {
	topic_map_t *tm = NULL;
	HASH_FIND_STR(topic_map, topic, tm);
	return tm;
}

static topic_map_t *fetch_topic_create_if_not_exist(const char *topic) {
	topic_map_t *tm = NULL;
	tm = fetch_topic(topic);
	if (tm == NULL) {
		tm = create_topic(topic);
	}
	return tm;
}

static int push_subscriber_queue(ps_subscriber_t *su, ps_msg_t *msg) {
	ps_ref_msg(msg);
	if (ps_queue_push(su->q, msg) != 0) {
		__sync_add_and_fetch(&su->overflow, 1);
		ps_unref_msg(msg);
		return -1;
	}
	if (su->new_msg_cb != NULL) {
		(su->new_msg_cb)(su);
	}

	return 0;
}

static void push_child_sticky(ps_subscriber_t *su, const char *prefix) {
	topic_map_t *tm, *tm_tmp;

	size_t pl = strlen(prefix);
	HASH_ITER(hh, topic_map, tm, tm_tmp) {
		if (pl == 0 || (strncmp(prefix, tm->topic, pl) == 0 && (tm->topic[pl] == 0 || tm->topic[pl] == '.'))) {
			if (tm->sticky != NULL) {
				push_subscriber_queue(su, tm->sticky);
			}
		}
	}
}

ps_subscriber_t *ps_new_subscriber(size_t queue_size, const strlist_t subs) {
	ps_subscriber_t *su = calloc(1, sizeof(ps_subscriber_t));
	su->q = ps_new_queue(queue_size);
	su->overflow = false;
	ps_subscribe_many(su, subs);
	__sync_add_and_fetch(&stat_live_subscribers, 1);
	return su;
}

void ps_free_subscriber(ps_subscriber_t *su) {
	ps_unsubscribe_all(su);
	ps_flush(su);
	ps_free_queue(su->q);
	free(su);
	__sync_sub_and_fetch(&stat_live_subscribers, 1);
}

void ps_subscriber_user_data_set(ps_subscriber_t *s, void *userData) {
	s->userData = userData;
}
void *ps_subscriber_user_data(ps_subscriber_t *s) {
	return s->userData;
}

void ps_set_new_msg_cb(ps_subscriber_t *su, new_msg_cb_t cb) {
	PORT_LOCK
	su->new_msg_cb = cb;
	if (ps_queue_waiting(su->q) > 0) {
		if (su->new_msg_cb != NULL) {
			(su->new_msg_cb)(su);
		}
	}
	PORT_UNLOCK
}

int ps_stats_live_subscribers(void) {
	return __sync_fetch_and_add(&stat_live_subscribers, 0);
}

int ps_flush(ps_subscriber_t *su) {
	int flushed = 0;
	ps_msg_t *msg = NULL;
	while ((msg = ps_queue_pull(su->q, 0)) != NULL) {
		ps_unref_msg(msg);
		flushed++;
	}
	return flushed;
}

int ps_subscribe_many(ps_subscriber_t *su, const strlist_t subs) {
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

int ps_subscribe(ps_subscriber_t *su, const char *topic_orig) {
	int ret = 0;
	topic_map_t *tm;
	subscriber_list_t *sl;
	subscriptions_list_t *subs;

	char *topic = strdup(topic_orig);

	bool hidden_flag = false;
	bool on_empty_flag = false;
	bool no_sticky_flag = false;
	bool child_sticky_flag = false;

	char *fl_str = strchr(topic, ' ');
	if (fl_str != NULL) {
		*fl_str = '\0';
		fl_str++;
		while (*fl_str != '\0') {
			switch (*fl_str) {
			case 'h':
				hidden_flag = true;
				break;
			case 's':
				no_sticky_flag = true;
				break;
			case 'S':
				child_sticky_flag = true;
				break;
			case 'e':
				on_empty_flag = true;
				break;
			}
			fl_str++;
		}
	}

	PORT_LOCK
	tm = fetch_topic_create_if_not_exist(topic);
	DL_SEARCH_SCALAR(tm->subscribers, sl, su, su);
	if (sl != NULL) {
		ret = -1;
		goto exit_fn;
	}
	sl = calloc(1, sizeof(*sl));
	sl->su = su;
	sl->hidden = hidden_flag;
	sl->on_empty = on_empty_flag;
	DL_APPEND(tm->subscribers, sl);
	subs = calloc(1, sizeof(*subs));
	subs->tm = tm;
	DL_APPEND(su->subs, subs);
	if (!no_sticky_flag) {
		if (child_sticky_flag) {
			push_child_sticky(su, topic);
		} else {
			if (tm->sticky != NULL) {
				push_subscriber_queue(su, tm->sticky);
			}
		}
	}

exit_fn:
	PORT_UNLOCK
	free(topic);
	return ret;
}

int ps_unsubscribe(ps_subscriber_t *su, const char *topic) {
	int ret = 0;
	topic_map_t *tm;
	subscriber_list_t *sl;
	subscriptions_list_t *subs;

	PORT_LOCK
	tm = fetch_topic(topic);
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
	free_topic_if_empty(tm);
	DL_SEARCH_SCALAR(su->subs, subs, tm, tm);
	if (subs != NULL) {
		DL_DELETE(su->subs, subs);
		free(subs);
	}

exit_fn:
	PORT_UNLOCK
	return ret;
}

int ps_unsubscribe_many(ps_subscriber_t *su, const strlist_t subs) {
	int n = 0;
	if (subs != NULL) {
		size_t idx = 0;
		while (subs[idx] != NULL) {
			if (ps_unsubscribe(su, subs[idx++]) == 0)
				n++;
		}
	}
	return n;
}

int ps_unsubscribe_all(ps_subscriber_t *su) {
	subscriptions_list_t *s, *ps;
	subscriber_list_t *sl;
	size_t count = 0;

	PORT_LOCK
	s = su->subs;
	while (s != NULL) {
		DL_SEARCH_SCALAR(s->tm->subscribers, sl, su, su);
		if (sl != NULL) {
			DL_DELETE(s->tm->subscribers, sl);
			free(sl);
			free_topic_if_empty(s->tm);
		}
		ps = s;
		s = s->next;
		free(ps);
		count++;
	}
	su->subs = NULL;
	PORT_UNLOCK
	return count;
}

ps_msg_t *ps_get(ps_subscriber_t *su, int64_t timeout) {
	return ps_queue_pull(su->q, timeout);
}

int ps_num_subs(ps_subscriber_t *su) {
	int count;
	subscriptions_list_t *elt;
	DL_COUNT(su->subs, elt, count);
	return count;
}

int ps_waiting(ps_subscriber_t *su) {
	return ps_queue_waiting(su->q);
}

int ps_overflow(ps_subscriber_t *su) {
	uint32_t n = __sync_fetch_and_sub(&su->overflow, 0);
	__sync_fetch_and_sub(&su->overflow, n);
	return n;
}

void ps_clean_sticky(const char *prefix) {
	topic_map_t *tm, *tm_tmp;

	size_t pl = strlen(prefix);
	PORT_LOCK
	HASH_ITER(hh, topic_map, tm, tm_tmp) {
		if (pl == 0 || (strncmp(prefix, tm->topic, pl) == 0 && (tm->topic[pl] == 0 || tm->topic[pl] == '.'))) {
			if (tm->sticky != NULL) {
				ps_unref_msg(tm->sticky);
				tm->sticky = NULL;
				free_topic_if_empty(tm);
			}
		}
	}
	PORT_UNLOCK
}

int ps_publish(ps_msg_t *msg) {
	if (msg == NULL)
		return 0;
	topic_map_t *tm = NULL;
	subscriber_list_t *sl = NULL;
	size_t ret = 0;
	char *topic = strdup(msg->topic);
	PORT_LOCK
	bool first = true;
	while (strlen(topic) > 0) {
		tm = fetch_topic(topic);
		if (first) {
			first = false;
			if (msg->flags & FL_STICKY) {
				if (tm == NULL) {
					tm = create_topic(topic);
				}
				if (tm->sticky != NULL) {
					ps_unref_msg(tm->sticky);
				}
				tm->sticky = ps_ref_msg(msg);
			} else {
				if (tm != NULL && tm->sticky != NULL) {
					ps_unref_msg(tm->sticky);
					tm->sticky = NULL;
					if (free_topic_if_empty(tm)) {
						tm = NULL;
					}
				}
			}
		}
		if (tm != NULL) {
			DL_FOREACH (tm->subscribers, sl) {
				if (sl->on_empty && ps_waiting(sl->su) != 0) {
					continue;
				}
				if (push_subscriber_queue(sl->su, msg) == 0 && !sl->hidden) {
					ret++;
				}
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
	PORT_UNLOCK
	return ret;
}

int ps_subs_count(char *topic_) {
	if (topic_ == NULL || strlen(topic_) == 0)
		return 0;
	char *topic = strdup(topic_);

	topic_map_t *tm = NULL;
	subscriber_list_t *sl = NULL;
	size_t count = 0;

	PORT_LOCK
	while (strlen(topic) > 0) {
		tm = fetch_topic(topic);
		if (tm != NULL) {
			DL_FOREACH (tm->subscribers, sl) {
				if (!sl->hidden)
					count++;
			}
		}

		for (size_t n = strlen(topic); n > 0; n--) {
			if (topic[n - 1] == '.') {
				topic[n - 1] = 0;
				break;
			}
			topic[n - 1] = 0;
		}
	}
	free(topic);
	PORT_UNLOCK
	return count;
}

ps_msg_t *ps_call(ps_msg_t *msg, int64_t timeout) {
	ps_msg_t *ret_msg = NULL;
	char rtopic[32] = {0};

	snprintf(rtopic, sizeof(rtopic), "$r.%u", __sync_add_and_fetch(&uuid_ctr, 1));
	ps_msg_set_rtopic(msg, rtopic);
	ps_subscriber_t *su = ps_new_subscriber(1, STRLIST(rtopic));
	if (ps_publish(msg) == 0) {
		goto exit_fn;
	}
	ret_msg = ps_get(su, timeout);
exit_fn:
	ps_free_subscriber(su);
	return ret_msg;
}

ps_msg_t *ps_wait_one(const char *topic, int64_t timeout) {
	ps_msg_t *ret_msg = NULL;

	ps_subscriber_t *su = ps_new_subscriber(1, STRLIST(topic));
	ret_msg = ps_get(su, timeout);
	ps_free_subscriber(su);
	return ret_msg;
}

bool ps_has_topic_prefix(ps_msg_t *msg, const char *pre) {
	if (msg == NULL) {
		return false;
	}
	return strncmp(pre, msg->topic, strlen(pre)) == 0;
}

bool ps_has_topic_suffix(ps_msg_t *msg, const char *suf) {
	if (msg == NULL) {
		return false;
	}
	size_t lsuf = strlen(suf), ltopic = strlen(msg->topic);
	if (lsuf <= ltopic) {
		return strcmp(suf, &msg->topic[ltopic - lsuf]) == 0;
	}
	return false;
}

bool ps_has_topic(ps_msg_t *msg, const char *topic) {
	if (msg == NULL) {
		return false;
	}
	return strcmp(topic, msg->topic) == 0;
}
