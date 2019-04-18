#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//#define PS_USE_GETTIMEOFDAY // Use gettimeofday instead of monotonic clock_gettime

/**
 * @brief Flags associated to the message:
 * FL_STICKY: Stores the las message in the queue and automatically publish it to new subscribers.
 * FL_NONRECURSIVE: Only sends the message to the exact topic path, not to part of the path of the topic.
 * INT_TYP: Message of type integer
 * DBL_TYP: Message of type double
 * PTR_TYP: Message of type pointer
 * STR_TYP: Message of type string
 * BUF_TYP: Message of type buffer
 * BOOL_TYP: Message of type boolean
 * ERR_TYP: Message of type error
 * MSK_TYP: Mask used for getting the message type.
 *
 */
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

/**
 * @brief ps_init initializes the publish/subscribe internal context.
 *
 */
void ps_init(void);

/**
 * @brief ps_new_msg generic method to create a new message to be enqueued or dequeued
 *
 * @param topic string path where the message is send or received
 * @param flags for specifying the message type.
 * @param ... values
 * @return ps_msg_t*
 */
ps_msg_t *ps_new_msg(const char *topic, uint32_t flags, ...);
/**
 * @brief ps_ref_msg increments the message reference counter
 *
 * @param msg
 * @return ps_msg_t*
 */
ps_msg_t *ps_ref_msg(ps_msg_t *msg);
/**
 * @brief ps_unref_msg decrease the message reference counter
 *
 * @param msg
 */
void ps_unref_msg(ps_msg_t *msg);
/**
 * @brief ps_msg_set_rtopic sets a response topic for the message
 *
 * @param msg message to set the reply topic
 * @param rtopic string with the path to reply
 */
void ps_msg_set_rtopic(ps_msg_t *msg, const char *rtopic);

/**
 * @brief ps_new_subscriber create a new subscriber to a path.
 *
 * @param queue_size of elements to store
 * @param subs string paths to subscribe for messages
 * @return ps_subscriber_t*
 */
ps_subscriber_t *ps_new_subscriber(size_t queue_size, strlist_t subs);
/**
 * @brief ps_free_subscriber frees data from the subscriber and closes the queues.
 *
 * @param s subscriber to free.
 */
void ps_free_subscriber(ps_subscriber_t *s);

/**
 * @brief ps_get get message from subscribers
 *
 * @param su subscriber from where to check if new messages
 * @param timeout in miliseconds to wait.
 * @return ps_msg_t* message value or null if timeout reached.
 */
ps_msg_t *ps_get(ps_subscriber_t *su, int64_t timeout);
/**
 * @brief ps_subscribe adds topic to the subscriber instance
 *
 * @param su subscriber instance
 * @param topic string path of topic to subscribe
 * @return int status
 */
int ps_subscribe(ps_subscriber_t *su, const char *topic);

/**
 * @brief ps_subscribe_many adds several topics to the subscriber instance
 *
 * @param su subscriber instance
 * @param subs strings with the paths to subscribe
 * @return int status
 */
int ps_subscribe_many(ps_subscriber_t *su, strlist_t subs);
/**
 * @brief ps_unsubscribe removes one topic from the subscribe instance
 *
 * @param su subscriber instance
 * @param topic string path to remove
 * @return int state
 */
int ps_unsubscribe(ps_subscriber_t *su, const char *topic);
/**
 * @brief ps_unsubscribe_many removes several topic from the subscribe instance
 *
 * @param su subscriber instance
 * @param subs strings with the paths to subscribe
 * @return int number of unsubscribed topics
 */
int ps_unsubscribe_many(ps_subscriber_t *su, strlist_t subs);
/**
 * @brief ps_unsubscribe_all removes all topic from the subscribe instance
 *
 * @param su subscriber instance
 * @return size_t number of unsubscribed topics
 */
size_t ps_unsubscribe_all(ps_subscriber_t *su);
/**
 * @brief ps_flush clears all messages pending in the queue
 *
 * @param su subscribe instance
 * @return size_t number of freed messages
 */
size_t ps_flush(ps_subscriber_t *su);
/**
 * @brief ps_num_subs gives the number of topics subscribed to
 *
 * @param su subscribe instance
 * @return size_t number of subscribed topics
 */
size_t ps_num_subs(ps_subscriber_t *su);
/**
 * @brief ps_waiting gives the number of messages pending to read from subscriber
 *
 * @param su subscribe instance
 * @return size_t number of subscribed topics
 */
size_t ps_waiting(ps_subscriber_t *su);
/**
 * @brief ps_overflow gives the number of messages that could not store in the queue because overflow of the queue
 *
 * @param su subscribe instance
 * @return size_t number of messages overflowed
 */
size_t ps_overflow(ps_subscriber_t *su);

/**
 * @brief ps_publish publishes a message
 *
 * @param msg message instance
 * @return size_t number of topics the message was sent to
 */
size_t ps_publish(ps_msg_t *msg);
/**
 * @brief ps_call create publishes a message, generate a rtopic and waits for a response.
 *
 * @param msg message instance
 * @param timeout timeout in miliseconds to wait for response
 * @return ps_msg_t* message response or null if timeout expired
 */
ps_msg_t *ps_call(ps_msg_t *msg, int64_t timeout);
/**
 * @brief ps_wait_one waits one message without creating the subscriber instace
 *
 * @param topic string path of topic to subscribe
 * @param timeout timeout in miliseconds to wait for response
 * @return ps_msg_t* message response or null if timeout expired
 */
ps_msg_t *ps_wait_one(const char *topic, int64_t timeout);

size_t ps_stats_live_msg(void);
size_t ps_stats_live_subscribers(void);
void ps_clean_sticky(void);

/**
 * @brief PUB_INT_FL PUB_DBL_FL PUB_PTR_FL PUB_STR_FL PUB_BOOL_FL PUB_BUF_FL PUB_ERR_FL are macros for simplifying the
 * publish method of messages with flags
 *
 */
#define PUB_INT_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | INT_TYP, (int64_t)(val)))
#define PUB_DBL_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | DBL_TYP, (double) (val)))
#define PUB_PTR_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | PTR_TYP, (void *) (val)))
#define PUB_STR_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | STR_TYP, (char *) (val)))
#define PUB_BOOL_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | BOOL_TYP, (int) (val)))
#define PUB_BUF_FL(topic, ptr, sz, dtor, fl)                                                                           \
	ps_publish(ps_new_msg(topic, (fl) | BUF_TYP, (void *) (ptr), (size_t)(sz), (ps_dtor_t)(dtor)));
#define PUB_ERR_FL(topic, id, desc, fl) ps_publish(ps_new_msg(topic, (fl) | ERR_TYP, (int) (id), (char *) (desc)))

/**
 * @brief PUB_INT PUB_DBL PUB_PTR PUB_STR PUB_BOOL PUB_BUF PUB_ERR are macros for simplifying the publish method of
 * messages without flags
 *
 */
#define PUB_INT(topic, val) PUB_INT_FL(topic, val, 0)
#define PUB_DBL(topic, val) PUB_DBL_FL(topic, val, 0)
#define PUB_PTR(topic, val) PUB_PTR_FL(topic, val, 0)
#define PUB_STR(topic, val) PUB_STR_FL(topic, val, 0)
#define PUB_BOOL(topic, val) PUB_BOOL_FL(topic, val, 0)
#define PUB_BUF(topic, ptr, sz, dtor) PUB_BUF_FL(topic, ptr, sz, dtor, 0)
#define PUB_ERR(topic, id, desc) PUB_ERR_FL(topic, id, desc, 0)

/**
 * @brief CALL_INT CALL_DBL CALL_PTR CALL_STR CALL_BOOL CALL_BUF are macros for simplifying the call method of messages
 *
 */
#define CALL_INT(topic, val, timeout) ps_call(ps_new_msg(topic, INT_TYP, (int64_t)(val)), (timeout))
#define CALL_DBL(topic, val, timeout) ps_call(ps_new_msg(topic, DBL_TYP, (double) (val)), (timeout))
#define CALL_PTR(topic, val, timeout) ps_call(ps_new_msg(topic, PTR_TYP, (void *) (val)), (timeout))
#define CALL_STR(topic, val, timeout) ps_call(ps_new_msg(topic, STR_TYP, (char *) (val)), (timeout))
#define CALL_BOOL(topic, val, timeout) ps_call(ps_new_msg(topic, BOOL_TYP, (int) (val)), (timeout))
#define CALL_BUF(topic, ptr, sz, dtor, timeout)                                                                        \
	ps_call(ps_new_msg(topic, BUF_TYP, (void *) (ptr), (size_t)(sz), (ps_dtor_t)(dtor)), (timeout))

/**
 * @brief IS_INT IS_DBL IS_PTR IS_STR IS_BOOL IS_BUF IS_ERR are macros for simplifying the way of checking the type of
 * message received
 *
 */
#define IS_INT(m) (((m)->flags & MSK_TYP) == INT_TYP)
#define IS_DBL(m) (((m)->flags & MSK_TYP) == DBL_TYP)
#define IS_PTR(m) (((m)->flags & MSK_TYP) == PTR_TYP)
#define IS_STR(m) (((m)->flags & MSK_TYP) == STR_TYP)
#define IS_BOOL(m) (((m)->flags & MSK_TYP) == BOOL_TYP)
#define IS_BUF(m) (((m)->flags & MSK_TYP) == BUF_TYP)
#define IS_ERR(m) (((m)->flags & MSK_TYP) == ERR_TYP)