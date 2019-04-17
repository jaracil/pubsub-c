#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//#define PS_USE_GETTIMEOFDAY // Use gettimeofday instead of monotonic clock_gettime

enum msg_flags {
	FL_STICKY = 1 << 0,
	FL_NONRECURSIVE = 1 << 1,
	INT_TYP = 0x00000100u,
	DBL_TYP = 0x00000200u,
	PTR_TYP = 0x00000300u,
	STR_TYP = 0x00000400u,
	BUF_TYP = 0x00000500u,
	BOOL_TYP = 0x00000600u,
	ERR_TYP = 0x00000700u,
	MSK_TYP = 0x00000F00u,
};

typedef const char *strlist_t[];

#define STRLIST(...)                                                                                                   \
	(strlist_t) {                                                                                                      \
		__VA_ARGS__, NULL                                                                                              \
	}

typedef void (*ps_dtor_t)(void *);

typedef struct ps_buf_s {
	void *ptr;
	size_t sz;
	ps_dtor_t dtor;
} ps_buf_t;

typedef struct ps_err_s {
	int id;
	char *desc;
} ps_err_t;

typedef struct ps_msg_s {
	uint32_t _ref; // Ref counter
	char *topic;   // Message topic
	char *rtopic;  // Response topic
	uint32_t flags;
	union {
		double dbl_val;
		int64_t int_val;
		int bool_val;
		void *ptr_val;
		char *str_val;
		ps_buf_t buf_val;
		ps_err_t err_val;
	};
} ps_msg_t;

typedef struct ps_subscriber_s ps_subscriber_t; // Private definition

void ps_init(void);

ps_msg_t *ps_new_msg(const char *topic, uint32_t flags, ...);
ps_msg_t *ps_ref_msg(ps_msg_t *msg);
void ps_unref_msg(ps_msg_t *msg);
void ps_msg_set_rtopic(ps_msg_t *msg, const char *rtopic);

ps_subscriber_t *ps_new_subscriber(size_t queue_size, strlist_t subs);
void ps_free_subscriber(ps_subscriber_t *s);

ps_msg_t *ps_get(ps_subscriber_t *su, int64_t timeout);
int ps_subscribe(ps_subscriber_t *su, const char *topic);
int ps_subscribe_many(ps_subscriber_t *su, strlist_t subs);
int ps_unsubscribe(ps_subscriber_t *su, const char *topic);
int ps_unsubscribe_many(ps_subscriber_t *su, strlist_t subs);
size_t ps_unsubscribe_all(ps_subscriber_t *su);
size_t ps_flush(ps_subscriber_t *su);
size_t ps_num_subs(ps_subscriber_t *su);
size_t ps_waiting(ps_subscriber_t *su);
size_t ps_overflow(ps_subscriber_t *su);

size_t ps_publish(ps_msg_t *msg);
ps_msg_t *ps_call(ps_msg_t *msg, int64_t timeout);
ps_msg_t *ps_wait_one(const char *topic, int64_t timeout);

size_t ps_stats_live_msg(void);
size_t ps_stats_live_subscribers(void);
void ps_clean_sticky(void);

#define PUB_INT_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | INT_TYP, (int64_t)(val)))
#define PUB_DBL_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | DBL_TYP, (double) (val)))
#define PUB_PTR_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | PTR_TYP, (void *) (val)))
#define PUB_STR_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | STR_TYP, (char *) (val)))
#define PUB_BOOL_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | BOOL_TYP, (int) (val)))
#define PUB_BUF_FL(topic, ptr, sz, dtor, fl)                                                                           \
	ps_publish(ps_new_msg(topic, (fl) | BUF_TYP, (void *) (ptr), (size_t)(sz), (ps_dtor_t)(dtor)));
#define PUB_ERR_FL(topic, id, desc, fl) ps_publish(ps_new_msg(topic, (fl) | ERR_TYP, (int) (id), (char *) (desc)))

#define PUB_INT(topic, val) PUB_INT_FL(topic, val, 0)
#define PUB_DBL(topic, val) PUB_DBL_FL(topic, val, 0)
#define PUB_PTR(topic, val) PUB_PTR_FL(topic, val, 0)
#define PUB_STR(topic, val) PUB_STR_FL(topic, val, 0)
#define PUB_BOOL(topic, val) PUB_BOOL_FL(topic, val, 0)
#define PUB_BUF(topic, ptr, sz, dtor) PUB_BUF_FL(topic, ptr, sz, dtor, 0)
#define PUB_ERR(topic, id, desc) PUB_ERR_FL(topic, id, desc, 0)

#define CALL_INT(topic, val, timeout) ps_call(ps_new_msg(topic, INT_TYP, (int64_t)(val)), (timeout))
#define CALL_DBL(topic, val, timeout) ps_call(ps_new_msg(topic, DBL_TYP, (double) (val)), (timeout))
#define CALL_PTR(topic, val, timeout) ps_call(ps_new_msg(topic, PTR_TYP, (void *) (val)), (timeout))
#define CALL_STR(topic, val, timeout) ps_call(ps_new_msg(topic, STR_TYP, (char *) (val)), (timeout))
#define CALL_BOOL(topic, val, timeout) ps_call(ps_new_msg(topic, BOOL_TYP, (int) (val)), (timeout))
#define CALL_BUF(topic, ptr, sz, dtor, timeout)                                                                        \
	ps_call(ps_new_msg(topic, BUF_TYP, (void *) (ptr), (size_t)(sz), (ps_dtor_t)(dtor)), (timeout))

#define IS_INT(m) (((m)->flags & MSK_TYP) == INT_TYP)
#define IS_DBL(m) (((m)->flags & MSK_TYP) == DBL_TYP)
#define IS_PTR(m) (((m)->flags & MSK_TYP) == PTR_TYP)
#define IS_STR(m) (((m)->flags & MSK_TYP) == STR_TYP)
#define IS_BOOL(m) (((m)->flags & MSK_TYP) == BOOL_TYP)
#define IS_BUF(m) (((m)->flags & MSK_TYP) == BUF_TYP)
#define IS_ERR(m) (((m)->flags & MSK_TYP) == ERR_TYP)