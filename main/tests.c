#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "pubsub.h"

void check_leak(void) {
	assert(ps_stats_live_msg() == 0);
	assert(ps_stats_live_subscribers() == 0);
}

void test_subscriptions(void) {
	printf("Test subscriptions\n");
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("sota.caballo"));
	ps_subscriber_t *s2 = ps_new_subscriber(10, STRLIST("sota", "rey"));
	assert(ps_num_subs(s1) == 1);
	assert(ps_num_subs(s2) == 2);
	ps_unsubscribe(s2, "rey");
	assert(ps_num_subs(s1) == 1);
	assert(ps_num_subs(s2) == 1);
	ps_free_subscriber(s1);
	ps_free_subscriber(s2);
	check_leak();
}

void test_publish(void) {
	printf("Test publish\n");
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("sota.caballo"));
	ps_subscriber_t *s2 = ps_new_subscriber(10, STRLIST("sota", "rey"));
	PUB_BOOL("sota.caballo", true);
	PUB_BOOL("sota", true);
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
	PUB_INT_FL("sota", 1, FL_STICKY);
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("sota"));
	assert(ps_waiting(s1) == 1);
	ps_free_subscriber(s1);
	assert(ps_stats_live_msg() == 1); // The sticky message
	ps_clean_sticky();
	check_leak();
}

void test_no_recursive(void) {
	printf("Test no recursive\n");
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("sota.caballo"));
	ps_subscriber_t *s2 = ps_new_subscriber(10, STRLIST("sota"));
	PUB_INT_FL("sota.caballo", 1, FL_NONRECURSIVE);
	assert(ps_waiting(s1) == 1);
	assert(ps_waiting(s2) == 0);
	ps_free_subscriber(s1);
	ps_free_subscriber(s2);
	check_leak();
}

void test_pub_get(void) {
	printf("Test pub->get\n");
	ps_msg_t *msg;
	ps_subscriber_t *s1 = ps_new_subscriber(10, STRLIST("sota.caballo"));
	PUB_INT("sota.caballo", 1);
	PUB_DBL("sota.caballo", 1.25);
	PUB_STR("sota.caballo", "Hello");
	PUB_ERR("sota.caballo", -1, "Bad result");
	PUB_BUF("sota.caballo", malloc(10), 10, free);

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
	ps_subscriber_t *s1 = ps_new_subscriber(2, STRLIST("sota.caballo"));
	PUB_INT("sota.caballo", 1);
	PUB_INT("sota.caballo", 2);
	PUB_INT("sota.caballo", 3);
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

void app_main() {
	ps_init();
	test_subscriptions();
	test_publish();
	test_sticky();
	test_no_recursive();
	test_pub_get();
	test_overflow();

	printf("All tests passed!\n");
}

int main(void) {
	app_main();
	return 0;
}
