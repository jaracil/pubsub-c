#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>

#include "pubsub.h"
#include "uthash.h"
#include "utlist.h"

typedef struct ps_queue_s {
	ps_msg_t **messages;
	size_t size;
	size_t count;
	size_t head;
	size_t tail;
	pthread_mutex_t mux;
	pthread_cond_t not_empty;
} ps_queue_t;

typedef struct subscriber_list_s {
	ps_subscriber_t *su;
	bool hidden;
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
};

static pthread_mutex_t lock;
static topic_map_t *topic_map = NULL;

static uint32_t uuid_ctr;

static uint32_t stat_live_msg;
static uint32_t stat_live_subscribers;

void ps_init(void) {
	pthread_mutex_init(&lock, NULL);
}

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

ps_queue_t *ps_new_queue(size_t sz) {

	ps_queue_t *q = calloc(1, sizeof(ps_queue_t));
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
	return q;
}

void ps_free_queue(ps_queue_t *q) {
	free(q->messages);
	pthread_mutex_destroy(&q->mux);
	pthread_cond_destroy(&q->not_empty);
	free(q);
}

int ps_queue_push(ps_queue_t *q, ps_msg_t *msg) {
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
}

ps_msg_t *ps_queue_pull(ps_queue_t *q, int64_t timeout) {
	ps_msg_t *msg = NULL;
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
}

size_t ps_queue_waiting(ps_queue_t *q) {
	size_t res = 0;
	pthread_mutex_lock(&q->mux);
	res = q->count;
	pthread_mutex_unlock(&q->mux);
	return res;
}

ps_msg_t *ps_new_msg(const char *topic, uint32_t flags, ...) {
	if (topic == NULL)
		return NULL;

	ps_msg_t *msg = calloc(1, sizeof(ps_msg_t));
	va_list args;
	va_start(args, flags);

	msg->_ref = 1;
	msg->flags = flags;
	msg->topic = strdup(topic);
	msg->rtopic = NULL;

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

void ps_msg_set_rtopic(ps_msg_t *msg, const char *rtopic) {
	if (msg->rtopic != NULL) {
		free(msg->rtopic); // Free previous rtopic
		msg->rtopic = NULL;
	}
	if (rtopic != NULL) {
		msg->rtopic = strdup(rtopic);
	}
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
	topic_map_t *tm;
	tm = calloc(1, sizeof(*tm));
	tm->topic = strdup(topic);
	HASH_ADD_KEYPTR(hh, topic_map, tm->topic, strlen(tm->topic), tm);
	return tm;
}

static topic_map_t *fetch_topic(const char *topic) {
	topic_map_t *tm;
	HASH_FIND_STR(topic_map, topic, tm);
	return tm;
}

static topic_map_t *fetch_topic_create_if_not_exist(const char *topic) {
	topic_map_t *tm;
	tm = fetch_topic(topic);
	if (tm == NULL) {
		tm = create_topic(topic);
	}
	return tm;
}

ps_subscriber_t *ps_new_subscriber(size_t queue_size, strlist_t subs) {
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

int ps_subscribe_many(ps_subscriber_t *su, strlist_t subs) {
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

	pthread_mutex_lock(&lock);
	tm = fetch_topic_create_if_not_exist(topic);
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
		if (ps_queue_push(su->q, tm->sticky) != 0) {
			__sync_add_and_fetch(&su->overflow, 1);
			ps_unref_msg(tm->sticky);
		}
	}

exit_fn:
	pthread_mutex_unlock(&lock);
	return ret;
}

int ps_unsubscribe(ps_subscriber_t *su, const char *topic) {
	int ret = 0;
	topic_map_t *tm;
	subscriber_list_t *sl;
	subscriptions_list_t *subs;

	pthread_mutex_lock(&lock);
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
	pthread_mutex_unlock(&lock);
	return ret;
}

int ps_unsubscribe_many(ps_subscriber_t *su, strlist_t subs) {
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

	pthread_mutex_lock(&lock);
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
	pthread_mutex_unlock(&lock);
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

void ps_clean_sticky(void) {
	topic_map_t *tm, *tm_tmp;

	pthread_mutex_lock(&lock);
	HASH_ITER(hh, topic_map, tm, tm_tmp) {
		if (tm->sticky != NULL) {
			ps_unref_msg(tm->sticky);
			tm->sticky = NULL;
			free_topic_if_empty(tm);
		}
	}
	pthread_mutex_unlock(&lock);
}

int ps_publish(ps_msg_t *msg) {
	if (msg == NULL)
		return 0;
	topic_map_t *tm;
	subscriber_list_t *sl;
	size_t ret = 0;
	char *topic = strdup(msg->topic);
	pthread_mutex_lock(&lock);
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
				ps_ref_msg(msg);
				if (ps_queue_push(sl->su->q, msg) != 0) {
					__sync_add_and_fetch(&sl->su->overflow, 1);
					ps_unref_msg(msg);
				} else {
					if (!sl->hidden)
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
	pthread_mutex_unlock(&lock);
	return ret;
}

ps_msg_t *ps_call(ps_msg_t *msg, int64_t timeout) {
	ps_msg_t *ret_msg = NULL;
	char rtopic[32] = {0};

	snprintf(rtopic, sizeof(rtopic), "$r.%u", __sync_add_and_fetch(&uuid_ctr, 1));
	ps_msg_set_rtopic(msg, rtopic);
	ps_subscriber_t *su = ps_new_subscriber(1, STRLIST(rtopic));
	ps_publish(msg);
	ret_msg = ps_get(su, timeout);
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