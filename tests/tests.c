#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "pubsub.h"

/* Helper functions */
static void check_leak(void) {
	ps_clean_sticky("");
	assert(ps_stats_live_msg() == 0);
	assert(ps_stats_live_subscribers() == 0);
}

static void *inc_thread(void *v) {
	ps_subscriber_t *s = ps_new_subscriber(10, STRLIST("fun.inc"));
	PUB_BOOL_FL("thread.ready", true, FL_STICKY);
	ps_msg_t *msg = ps_get(s, 5000);
	assert(msg != NULL);
	assert(IS_INT(msg));
	PUB_INT(msg->rtopic, msg->int_val + 1); // Response
	ps_unref_msg(msg);
	ps_free_subscriber(s);
	return NULL;
}

static int new_msg_cb_touch;
static ps_subscriber_t *new_msg_cb_subscriber;

static void new_msg_cb(ps_subscriber_t *su) {
	new_msg_cb_touch++;
	new_msg_cb_subscriber = su;
}

/* End helper functions*/

/* Test Functions */
void test_subscriptions(void) {
	printf("Test subscriptions\n");
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("foo.bar"));
	ps_subscriber_t *s2 = ps_new_subscriber(10, STRLIST("foo", "baz"));
	assert(ps_num_subs(s1) == 1);
	assert(ps_num_subs(s2) == 2);
	ps_unsubscribe(s2, "baz");
	assert(ps_num_subs(s1) == 1);
	assert(ps_num_subs(s2) == 1);
	ps_free_subscriber(s1);
	ps_free_subscriber(s2);
	check_leak();
}

void test_hidden_subscription(void) {
	printf("Test hidden subscription\n");
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("foo.bar"));
	ps_subscriber_t *s2 = ps_new_subscriber(10, STRLIST("foo.bar h"));
	assert(PUB_NIL("foo.bar") == 1);
	assert(ps_waiting(s1) == 1);
	assert(ps_waiting(s2) == 1);
	ps_free_subscriber(s1);
	ps_free_subscriber(s2);
	check_leak();
}

void test_subs_count(void) {
	printf("Test subs_count\n");
	assert(ps_subs_count(NULL) == 0);
	assert(ps_subs_count("") == 0);
	assert(ps_subs_count("foo") == 0);
	assert(ps_subs_count("foo.bar") == 0);
	assert(ps_subs_count("bar") == 0);
	assert(ps_subs_count("baz") == 0);
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("foo.bar"));
	ps_subscriber_t *s2 = ps_new_subscriber(10, STRLIST("foo", "baz"));
	assert(ps_subs_count("foo") == 1);
	assert(ps_subs_count("foo.bar") == 2);
	assert(ps_subs_count("bar") == 0);
	assert(ps_subs_count("baz") == 1);
	ps_free_subscriber(s1);
	ps_free_subscriber(s2);
	check_leak();
}

void test_subscribe_many(void) {
	printf("Test subscribe/unsubscribe many\n");
	ps_subscriber_t *s1 = ps_new_subscriber(10, NULL);
	assert(ps_subscribe_many(s1, STRLIST("foo", "bar", "baz")) == 3);
	assert(ps_num_subs(s1) == 3);
	assert(ps_unsubscribe_many(s1, STRLIST("foo", "bar", "baz")) == 3);
	assert(ps_num_subs(s1) == 0);
	ps_free_subscriber(s1);
	check_leak();
}

void test_publish(void) {
	printf("Test publish\n");
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("foo.bar"));
	ps_subscriber_t *s2 = ps_new_subscriber(10, STRLIST("foo", "baz"));
	PUB_BOOL("foo.bar", true);
	PUB_BOOL("foo", true);
	assert(ps_waiting(s1) == 1);
	assert(ps_waiting(s2) == 2);
	assert(ps_stats_live_msg() == 2);
	ps_flush(s1);
	assert(ps_stats_live_msg() == 2);
	ps_flush(s2);
	assert(ps_stats_live_msg() == 0);
	assert(ps_waiting(s1) == 0);
	assert(ps_waiting(s2) == 0);
	ps_free_subscriber(s1);
	ps_free_subscriber(s2);
	check_leak();
}

void test_sticky(void) {
	printf("Test sticky\n");
	PUB_INT_FL("foo", 1, FL_STICKY);
	PUB_INT_FL("foo", 2, FL_STICKY); // Last sticky override previous
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("foo"));
	assert(ps_waiting(s1) == 1);
	ps_msg_t *msg = ps_get(s1, -1);
	assert(msg->int_val == 2);
	ps_unref_msg(msg);
	ps_free_subscriber(s1);
	assert(ps_stats_live_msg() == 1); // The sticky message
	PUB_INT("foo", 3);                // Publish normal message on the same path, unsticks previous sitcked message
	assert(ps_stats_live_msg() == 0);
	check_leak();
}

void test_clean_sticky(void) {
	printf("Test clean sticky\n");
	PUB_INT_FL("foo.bar.baz", 1, FL_STICKY);
	PUB_INT_FL("foo.fiz.fuz", 1, FL_STICKY);
	assert(ps_stats_live_msg() == 2); // The sticky messages;
	ps_clean_sticky("foo.bar");       // Remove sticky messages from "foo.bar" prefix
	assert(ps_stats_live_msg() == 1); // The sticky messages;
	ps_clean_sticky("foo");           // Remove sticky messages from "foo" prefix
	assert(ps_stats_live_msg() == 0); // The sticky messages;
	check_leak();
}

void test_clean_all_children_sticky(void) {
	printf("Test clean all children sticky\n");
	PUB_INT_FL("foo.bar.baz", 1, FL_STICKY);
	PUB_INT_FL("foo.fiz.fuz", 1, FL_STICKY);
	assert(ps_stats_live_msg() == 2); // The sticky messages;
	ps_clean_sticky("foo");           // Remove sticky messages from "foo" prefix
	assert(ps_stats_live_msg() == 0); // The sticky messages;
	check_leak();
}

void test_no_sticky_flag(void) {
	printf("Test no sticky flag\n");
	PUB_INT_FL("foo", 1, FL_STICKY);
	ps_subscriber_t *s1 =
	ps_new_subscriber(10, STRLIST("foo s")); // Ignore sticky mesages published before subscription
	assert(ps_waiting(s1) == 0);
	PUB_INT_FL("foo", 2, FL_STICKY); // This is a new message (published after subscription)
	assert(ps_waiting(s1) == 1);
	ps_free_subscriber(s1);
	check_leak();
}

void test_child_sticky_flag(void) {
	printf("Test child sticky flag\n");
	PUB_NIL_FL("foo.bar.baz", FL_STICKY);
	PUB_NIL_FL("foo.bar", FL_STICKY);
	PUB_NIL_FL("foo", FL_STICKY);

	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("foo S")); // Get all child sticky messages
	assert(ps_waiting(s1) == 3);
	ps_free_subscriber(s1);

	s1 = ps_new_subscriber(10, STRLIST("foo.bar S")); // Get all child sticky messages
	assert(ps_waiting(s1) == 2);
	ps_free_subscriber(s1);

	s1 = ps_new_subscriber(10, STRLIST("foo.bar.baz S")); // Get all child sticky messages
	assert(ps_waiting(s1) == 1);
	ps_free_subscriber(s1);

	check_leak();
}

void test_no_recursive(void) {
	printf("Test no recursive\n");
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("foo.bar"));
	ps_subscriber_t *s2 = ps_new_subscriber(10, STRLIST("foo"));
	PUB_INT_FL("foo.bar", 1, FL_NONRECURSIVE);
	assert(ps_waiting(s1) == 1);
	assert(ps_waiting(s2) == 0);
	ps_free_subscriber(s1);
	ps_free_subscriber(s2);
	check_leak();
}

void test_on_empty(void) {
	ps_msg_t *msg;
	printf("Test on empty\n");
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("foo e"));
	PUB_NIL("foo.bar");
	assert(ps_waiting(s1) == 1);
	PUB_NIL("foo.bar");
	assert(ps_waiting(s1) == 1);
	msg = ps_get(s1, 10);
	assert(IS_NIL(msg));
	ps_unref_msg(msg);
	assert(ps_waiting(s1) == 0);
	PUB_NIL("foo.bar");
	assert(ps_waiting(s1) == 1);
	PUB_NIL("foo.bar");
	assert(ps_waiting(s1) == 1);
	ps_free_subscriber(s1);
	check_leak();
}

void test_pub_get(void) {
	printf("Test pub->get\n");
	ps_msg_t *msg;
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("foo.bar"));
	PUB_INT("foo.bar", 1);
	PUB_DBL("foo.bar", 1.25);
	PUB_STR("foo.bar", "Hello");
	PUB_ERR("foo.bar", -1, "Bad result");
	PUB_BUF("foo.bar", malloc(10), 10, free);

	msg = ps_get(s1, 10);
	assert(IS_INT(msg) && msg->int_val == 1);
	ps_unref_msg(msg);
	msg = ps_get(s1, 10);
	assert(IS_DBL(msg) && msg->dbl_val == 1.25);
	ps_unref_msg(msg);
	msg = ps_get(s1, 10);
	assert(IS_STR(msg) && strcmp(msg->str_val, "Hello") == 0);
	ps_unref_msg(msg);
	msg = ps_get(s1, 10);
	assert(IS_ERR(msg) && (msg->err_val.id == -1) && (strcmp(msg->err_val.desc, "Bad result") == 0));
	ps_unref_msg(msg);
	msg = ps_get(s1, 10);
	assert(IS_BUF(msg) && (msg->buf_val.sz == 10));
	ps_unref_msg(msg);

	msg = ps_get(s1, 1);
	assert(msg == NULL);

	assert(ps_waiting(s1) == 0);
	ps_free_subscriber(s1);
	check_leak();
}

void test_overflow(void) {
	printf("Test overflow\n");
	ps_msg_t *msg;
	ps_subscriber_t *s1 = ps_new_subscriber(2, STRLIST("foo.bar"));
	PUB_INT("foo.bar", 1);
	PUB_INT("foo.bar", 2);
	PUB_INT("foo.bar", 3);
	assert(ps_overflow(s1) == 1);
	assert(ps_overflow(s1) == 0);
	msg = ps_get(s1, 10);
	assert(IS_INT(msg) && msg->int_val == 1);
	ps_unref_msg(msg);
	msg = ps_get(s1, 10);
	assert(IS_INT(msg) && msg->int_val == 2);
	ps_unref_msg(msg);

	assert(ps_waiting(s1) == 0);
	ps_free_subscriber(s1);
	check_leak();
}

void test_new_msg_cb(void) {
	printf("Test new msg callback\n");

	new_msg_cb_touch = 0;
	new_msg_cb_subscriber = NULL;

	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("foo.bar"));
	PUB_INT("foo.bar", 1);
	ps_set_new_msg_cb(s1, new_msg_cb);
	assert(new_msg_cb_touch == 1);
	PUB_INT("foo.bar", 1);
	assert(new_msg_cb_touch == 2);
	assert(ps_waiting(s1) == 2);
	ps_free_subscriber(s1);
	check_leak();
}

void test_call(void) {
	printf("Test call\n");
	ps_msg_t *msg = NULL;
	pthread_t thread;
	pthread_create(&thread, NULL, inc_thread, NULL);
	msg = ps_wait_one("thread.ready", 5000);
	assert(msg != NULL && msg->bool_val == true);
	ps_unref_msg(msg);
	msg = CALL_INT("fun.inc", 25, 1000);
	assert(msg != NULL && msg->int_val == 26);
	ps_unref_msg(msg);

	msg = CALL_INT("fun.other", 0, 1000000); // If message is not delivered timeout is triggered
	assert(msg == NULL);
	ps_unref_msg(msg); // unref_msg suppots NULL messages

	pthread_join(thread, NULL);
	check_leak();
}

void test_no_return_path(void) {
	printf("Test no return path\n");
	ps_msg_t *msg = NULL;
	pthread_t thread;
	pthread_create(&thread, NULL, inc_thread, NULL);
	msg = ps_wait_one("thread.ready", 5000);
	assert(msg != NULL && msg->bool_val == true);
	ps_unref_msg(msg);
	PUB_INT("fun.inc", 25);
	pthread_join(thread, NULL);
	check_leak();
}

void test_topic_prefix_suffix(void) {
	printf("Test has_topic, has_topic_prefix, has_topic_suffix\n");
	ps_msg_t *msg;
	ps_subscriber_t *s1 = ps_new_subscriber(2, STRLIST("foo.bar"));
	PUB_NIL("foo.bar");
	msg = ps_get(s1, 10);
	assert(ps_has_topic(msg, "foo.bar"));
	assert(ps_has_topic_prefix(msg, "foo."));
	assert(ps_has_topic_suffix(msg, ".bar"));

	assert(!ps_has_topic(msg, "foo.baz"));
	assert(!ps_has_topic_prefix(msg, "baz."));
	assert(!ps_has_topic_suffix(msg, ".baz"));

	ps_unref_msg(msg);
	ps_free_subscriber(s1);
	check_leak();
}

void run_all(void) {
	test_subscriptions();
	test_hidden_subscription();
	test_subscribe_many();
	test_subs_count();
	test_publish();
	test_sticky();
	test_clean_sticky();
	test_clean_all_children_sticky();
	test_no_sticky_flag();
	test_child_sticky_flag();
	test_no_recursive();
	test_on_empty();
	test_pub_get();
	test_overflow();
	test_new_msg_cb();
	test_call();
	test_no_return_path();
	test_topic_prefix_suffix();
	printf("All tests passed!\n");
}

int main(void) {
	ps_init();
	run_all();
	return 0;
}
