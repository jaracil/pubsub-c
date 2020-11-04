#pragma once

/**
 * @file pubsub.h
 * @author José Luis Aracil (pepe.aracil.gomez@gmail.com)
 * @brief
 * @version 0.1
 * @date 2019-04-23
 *
 * @copyright Copyright (c) 2019
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//#define PS_USE_GETTIMEOFDAY // Use gettimeofday instead of monotonic clock_gettime

/**
 * @brief Flags associated to the message:
 * FL_STICKY: Stores the las message sent to the topic and automatically publish it to new subscribers to that topic.
 * FL_NONRECURSIVE: Only sends the message to the exact topic path, not to the parent topics.
 * INT_TYP: Message of type integer
 * DBL_TYP: Message of type double
 * PTR_TYP: Message of type pointer
 * STR_TYP: Message of type string
 * BUF_TYP: Message of type buffer
 * BOOL_TYP: Message of type boolean
 * ERR_TYP: Message of type error
 * NIL_TYP: Message of type nil/null
 * MSK_TYP: Mask used for getting the message type.
 */
enum msg_flags {
	FL_STICKY = 1 << 0,
	FL_NONRECURSIVE = 1 << 1,
	FL_EXTERNAL = 1 << 2,
	FL_UNTRUSTED = 1 << 3,
	MSK_FL = 0x000000FFu,
	INT_TYP = 0x00000100u,
	DBL_TYP = 0x00000200u,
	PTR_TYP = 0x00000300u,
	STR_TYP = 0x00000400u,
	BUF_TYP = 0x00000500u,
	BOOL_TYP = 0x00000600u,
	ERR_TYP = 0x00000700u,
	NIL_TYP = 0x00000800u,
	MSK_TYP = 0x00000F00u,
	RAW_ENC = 0x00000000u,
	MSGPACK_ENC = 0x00010000u,
	JSON_ENC = 0x00020000u,
	BSON_ENC = 0x00030000u,
	YAML_ENC = 0x00040000u,
	PROTOBUF_ENC = 0x00050000u,
	XML_ENC = 0x00060000u,
	MSK_ENC = 0x000F0000u,
	MSK_VALUE = MSK_TYP | MSK_ENC
};

typedef const char *const strlist_t[];

/**
 * @brief Helper macro to make a NULL terminated string array
 */
#define STRLIST(...)                                                                                                   \
	(const strlist_t) {                                                                                                \
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

typedef void (*new_msg_cb_t)(ps_subscriber_t *);

/**
 * @brief ps_init initializes the publish/subscribe internal context.
 */
void ps_init(void);

/**
 * @brief ps_new_msg generic method to create a new message to be published
 *
 * @param topic string path where the message is published
 * @param flags for specifying the message type.
 * @param ... values (Depends on flags)
 * @return ps_msg_t
 */
ps_msg_t *ps_new_msg(const char *topic, uint32_t flags, ...);

/**
 * @brief ps_dup_msg duplicates message
 *
 * @param msg_orig message to duplicate
 * @return ps_msg_t
 */
ps_msg_t *ps_dup_msg(ps_msg_t const *msg_orig);

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
 * @brief ps_msg_set_topic sets message topic
 *
 * @param msg message to set the topic
 * @param topic string with the topic
 */
void ps_msg_set_topic(ps_msg_t *msg, const char *topic);

/**
 * @brief ps_msg_set_rtopic sets a response topic for the message
 *
 * @param msg message to set the reply topic
 * @param rtopic string with the path to reply
 */
void ps_msg_set_rtopic(ps_msg_t *msg, const char *rtopic);

/**
 * @brief ps_msg_set_value stores a new value in a message, freeing
 * the previous stored value.
 *
 * @param msg message to set the reply topic
 * @param rtopic string with the path to reply
 */
void ps_msg_set_value(ps_msg_t *msg, uint32_t flags, ...);

/**
 * @brief ps_msg_set_value_int shorthand for storing int values.
 *
 * @param msg message to set the reply topic
 * @param value value
 */
static inline void ps_msg_set_value_int(ps_msg_t *msg, int64_t value) {
	ps_msg_set_value(msg, INT_TYP, value);
}

/**
 * @brief ps_msg_set_value_double shorthand for storing double values.
 *
 * @param msg message to set the reply topic
 * @param value value
 */
static inline void ps_msg_set_value_double(ps_msg_t *msg, double value) {
	ps_msg_set_value(msg, DBL_TYP, value);
}

/**
 * @brief ps_msg_set_value_string shorthand for storing string values.
 *
 * @param msg message to set the reply topic
 * @param value value
 */
static inline void ps_msg_set_value_string(ps_msg_t *msg, const char *value) {
	ps_msg_set_value(msg, STR_TYP, value);
}

/**
 * @brief ps_msg_set_value_boolean shorthand for storing bool values.
 *
 * @param msg message to set the reply topic
 * @param value value
 */
static inline void ps_msg_set_value_boolean(ps_msg_t *msg, bool value) {
	ps_msg_set_value(msg, BOOL_TYP, value);
}

/**
 * @brief ps_msg_set_value_nil shorthand for storing nil values.
 *
 * @param msg message to set the reply topic
 */
static inline void ps_msg_set_value_nil(ps_msg_t *msg) {
	ps_msg_set_value(msg, NIL_TYP);
}

/**
 * @brief ps_msg_set_value_buffer shorthand for storing buffer values.
 *
 * @param msg message to set the reply topic
 * @param buf buffer
 * @param sz buffer size
 * @param dtor buffer destructor
 * @param encoding buffer encoding, masked with MSK_ENC
 */
static inline void ps_msg_set_value_buffer(ps_msg_t *msg, const void *buf, size_t sz, ps_dtor_t dtor,
                                           uint32_t encoding) {
	ps_msg_set_value(msg, BUF_TYP | (encoding & MSK_ENC), buf, sz, dtor);
}

/**
 * @brief ps_msg_value_int coerces the value of the msg to an integer.
 * Performs the conversion from double and boolean to int.
 * Succeeds if IS_NUMBER(msg)
 *
 * @param msg message to set the reply topic
 */
int64_t ps_msg_value_int(const ps_msg_t *msg);
/**
 * @brief ps_msg_value_double coerces the value of the msg to a double.
 * Performs the conversion from int and boolean to double.
 * Succeeds if IS_NUMBER(msg)
 *
 * @param msg message to set the reply topic
 */
double ps_msg_value_double(const ps_msg_t *msg);
/**
 * @brief ps_msg_value_bool coerces the value of the msg to a boolean.
 * Performs the conversion from int and double to boolean.
 * Succeeds if IS_NUMBER(msg)
 *
 * @param msg message to set the reply topic
 */
bool ps_msg_value_bool(const ps_msg_t *msg);

/**
 * @brief ps_new_subscriber create a new subscriber to a path.
 *
 * @param queue_size of messages to store
 * @param subs string paths to subscribe for messages (see STRLIST macro)
 * @return ps_subscriber_t*
 */
ps_subscriber_t *ps_new_subscriber(size_t queue_size, const strlist_t subs);

/**
 * @brief ps_free_subscriber frees all data from the subscriber, including message queue.
 *
 * @param s subscriber to free.
 */
void ps_free_subscriber(ps_subscriber_t *s);

void ps_subscriber_user_data_set(ps_subscriber_t *s, void *userData);
void *ps_subscriber_user_data(ps_subscriber_t *s);

/**
 * @brief ps_get get message from subscribers
 *
 * @param su subscriber from where to check if new messages
 * @param timeout in miliseconds to wait (-1 = waits forever, 0 = don't block).
 * @return ps_msg_t* message value or null if timeout reached.
 */
ps_msg_t *ps_get(ps_subscriber_t *su, int64_t timeout);

/**
 * @brief ps_subscribe adds topic to the subscriber instance
 *
 * @param su subscriber instance
 * @param topic string path of topic to subscribe
 * @return status (-1 = Error, 0 = Ok)
 */
int ps_subscribe(ps_subscriber_t *su, const char *topic);

/**
 * @brief ps_subscribe_many adds several topics to the subscriber instance
 *
 * @param su subscriber instance
 * @param subs strings with the paths to subscribe
 * @return the number of unsubscribed topics.
 */
int ps_subscribe_many(ps_subscriber_t *su, const strlist_t subs);

/**
 * @brief ps_unsubscribe removes one topic from the subscribe instance
 *
 * @param su subscriber instance
 * @param topic string path to remove
 * @return state (-1 = Error, 0 = Ok)
 */
int ps_unsubscribe(ps_subscriber_t *su, const char *topic);

/**
 * @brief ps_unsubscribe_many removes several topic from the subscribe instance
 *
 * @param su subscriber instance
 * @param subs strings with the paths to subscribe (see STRLIST macro)
 * @return the number of unsubscribed topics
 */
int ps_unsubscribe_many(ps_subscriber_t *su, const strlist_t subs);

/**
 * @brief ps_unsubscribe_all removes all topic from the subscribe instance
 *
 * @param su subscriber instance
 * @return the number of unsubscribed topics
 */
int ps_unsubscribe_all(ps_subscriber_t *su);

/**
 * @brief ps_set_new_msg_cb set up a callback which is called when there are new messages
 *
 * @param su subscriber instance
 * @param cb callback function pointer
 */
void ps_set_new_msg_cb(ps_subscriber_t *su, new_msg_cb_t cb);

/**
 * @brief ps_flush clears all messages pending in the queue
 *
 * @param su subscribe instance
 * @return the number of freed messages
 */
int ps_flush(ps_subscriber_t *su);

/**
 * @brief ps_num_subs gives the number of topics subscribed to
 *
 * @param su subscribe instance
 * @return the number of subscribed topics
 */
int ps_num_subs(ps_subscriber_t *su);

/**
 * @brief ps_subs_count gives the number of subscriptors of a topic
 *
 * @param topic string path to count subcriptors
 * @return the number of subcriptors
 */
int ps_subs_count(char *topic);

/**
 * @brief ps_waiting gives the number of messages pending to read from subscriber
 *
 * @param su subscribe instance
 * @return size_t number of subscribed topics
 */
int ps_waiting(ps_subscriber_t *su);

/**
 * @brief ps_overflow gives the number of messages that could not be stored in the queue because it was full.
 *        Calling this function resets the overflow counter.
 * @param su subscribe instance
 * @return the number of messages overflowed
 */
int ps_overflow(ps_subscriber_t *su);

/**
 * @brief ps_publish publishes a message
 *
 * @param msg message instance
 * @return the number of subscribers the message was delivered to
 */
int ps_publish(ps_msg_t *msg);

/**
 * @brief ps_call create publishes a message, generate a rtopic and waits for a response.
 *
 * @param msg message instance
 * @param timeout timeout in miliseconds to wait for response (-1 = waits forever)
 * @return ps_msg_t* message response or null if timeout expired
 */
ps_msg_t *ps_call(ps_msg_t *msg, int64_t timeout);

/**
 * @brief ps_wait_one waits one message without creating the subscriber instace
 *
 * @param topic string path of topic to subscribe
 * @param timeout timeout in miliseconds to wait for response (-1 = waits forever)
 * @return ps_msg_t* message response or null if timeout expired
 */
ps_msg_t *ps_wait_one(const char *topic, int64_t timeout);

/**
 * @brief ps_has_topic_prefix returns true if topic of msg starts with prefix
 *
 * @param msg message instance
 * @param pre topic prefix to check
 * @return bool true if message topic starts with prefix
 */
bool ps_has_topic_prefix(ps_msg_t *msg, const char *pre);

/**
 * @brief ps_has_topic_suffix returns true if topic of msg ends with suffix
 *
 * @param msg is the message instance
 * @param suf is the topic suffix to check
 * @return bool true if message topic ends with suffix
 */
bool ps_has_topic_suffix(ps_msg_t *msg, const char *suf);

/**
 * @brief ps_has_topic returns true if topic of msg and topic are equal
 *
 * @param msg is the message instance
 * @param topic is the topic to check
 * @return bool true if message topic and topic are equal
 */
bool ps_has_topic(ps_msg_t *msg, const char *topic);

int ps_stats_live_msg(void);
int ps_stats_live_subscribers(void);
void ps_clean_sticky(const char *prefix);

/**
 * @brief PUB_INT_FL PUB_DBL_FL PUB_PTR_FL PUB_STR_FL PUB_BOOL_FL PUB_BUF_FL PUB_ERR_FL are macros for simplifying the
 * publish method of messages with flags
 */
#define PUB_INT_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | INT_TYP, (int64_t)(val)))
#define PUB_DBL_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | DBL_TYP, (double) (val)))
#define PUB_PTR_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | PTR_TYP, (void *) (val)))
#define PUB_STR_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | STR_TYP, (char *) (val)))
#define PUB_BOOL_FL(topic, val, fl) ps_publish(ps_new_msg(topic, (fl) | BOOL_TYP, (int) (val)))
#define PUB_BUF_FL(topic, ptr, sz, dtor, fl)                                                                           \
	ps_publish(ps_new_msg(topic, (fl) | BUF_TYP, (void *) (ptr), (size_t)(sz), (ps_dtor_t)(dtor)));
#define PUB_ERR_FL(topic, id, desc, fl) ps_publish(ps_new_msg(topic, (fl) | ERR_TYP, (int) (id), (char *) (desc)))
#define PUB_NIL_FL(topic, fl) ps_publish(ps_new_msg(topic, (fl) | NIL_TYP))

/**
 * @brief PUB_INT PUB_DBL PUB_PTR PUB_STR PUB_BOOL PUB_BUF PUB_ERR are macros for simplifying the publish method of
 * messages without flags
 */
#define PUB_INT(topic, val) PUB_INT_FL(topic, val, 0)
#define PUB_DBL(topic, val) PUB_DBL_FL(topic, val, 0)
#define PUB_PTR(topic, val) PUB_PTR_FL(topic, val, 0)
#define PUB_STR(topic, val) PUB_STR_FL(topic, val, 0)
#define PUB_BOOL(topic, val) PUB_BOOL_FL(topic, val, 0)
#define PUB_BUF(topic, ptr, sz, dtor) PUB_BUF_FL(topic, ptr, sz, dtor, 0)
#define PUB_ERR(topic, id, desc) PUB_ERR_FL(topic, id, desc, 0)
#define PUB_NIL(topic) PUB_NIL_FL(topic, 0)

/**
 * @brief CALL_INT CALL_DBL CALL_PTR CALL_STR CALL_BOOL CALL_BUF are macros for simplifying the call method of messages
 */
#define CALL_INT(topic, val, timeout) ps_call(ps_new_msg(topic, INT_TYP, (int64_t)(val)), (timeout))
#define CALL_DBL(topic, val, timeout) ps_call(ps_new_msg(topic, DBL_TYP, (double) (val)), (timeout))
#define CALL_PTR(topic, val, timeout) ps_call(ps_new_msg(topic, PTR_TYP, (void *) (val)), (timeout))
#define CALL_STR(topic, val, timeout) ps_call(ps_new_msg(topic, STR_TYP, (char *) (val)), (timeout))
#define CALL_BOOL(topic, val, timeout) ps_call(ps_new_msg(topic, BOOL_TYP, (int) (val)), (timeout))
#define CALL_NIL(topic, timeout) ps_call(ps_new_msg(topic, NIL_TYP), (timeout))
#define CALL_BUF(topic, ptr, sz, dtor, timeout)                                                                        \
	ps_call(ps_new_msg(topic, BUF_TYP, (void *) (ptr), (size_t)(sz), (ps_dtor_t)(dtor)), (timeout))

/**
 * @brief IS_INT IS_DBL IS_PTR IS_STR IS_BOOL IS_BUF IS_ERR are macros for simplifying the way of checking the type of
 * message received
 */
#define IS_INT(m) ((m) != NULL && ((m)->flags & MSK_TYP) == INT_TYP)
#define IS_DBL(m) ((m) != NULL && ((m)->flags & MSK_TYP) == DBL_TYP)
#define IS_BOOL(m) ((m) != NULL && ((m)->flags & MSK_TYP) == BOOL_TYP)
#define IS_NUMBER(m) (IS_INT(m) || IS_DBL(m) || IS_BOOL(m))
#define IS_PTR(m) ((m) != NULL && ((m)->flags & MSK_TYP) == PTR_TYP)
#define IS_STR(m) ((m) != NULL && ((m)->flags & MSK_TYP) == STR_TYP)
#define IS_BUF(m) ((m) != NULL && ((m)->flags & MSK_TYP) == BUF_TYP)
#define IS_ERR(m) ((m) != NULL && ((m)->flags & MSK_TYP) == ERR_TYP)
#define IS_NIL(m) ((m) != NULL && ((m)->flags & MSK_TYP) == NIL_TYP)

/**
 * @brief IS_EXTERNAL IS_UNTRUSTED are macros for simplifying the way of checking the source of
 * message received
 */
#define IS_EXTERNAL(m) ((m) != NULL && ((m)->flags & FL_EXTERNAL))
#define IS_UNTRUSTED(m) ((m) != NULL && ((m)->flags & FL_UNTRUSTED))
