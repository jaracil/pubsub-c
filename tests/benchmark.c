#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "pubsub.h"

uint64_t timespec_to_ns(struct timespec t) {
	return t.tv_sec * 1000000000 + t.tv_nsec;
}

#define BENCH(S, N, X)                                                                                                 \
	do {                                                                                                               \
		struct timespec t0, t1;                                                                                        \
		clock_gettime(CLOCK_MONOTONIC, &t0);                                                                           \
		for (int i = 0; i < N; i++) {                                                                                  \
			X;                                                                                                         \
		}                                                                                                              \
		clock_gettime(CLOCK_MONOTONIC, &t1);                                                                           \
		uint64_t elapsed = timespec_to_ns(t1) - timespec_to_ns(t0);                                                    \
		printf("%s/%s\t%ld ns/op\n", __FUNCTION__, S, elapsed / N);                                                    \
	} while (0);

#define ITERATIONS 1000000

void test1(void) {
	ps_subscriber_t *su = NULL;

	su = ps_new_subscriber(ITERATIONS, PS_STRLIST("topic.a"));
	BENCH("publish without sub", ITERATIONS, { PS_PUB_INT("topic.b", 5); });
	BENCH("publish without overflow", ITERATIONS, { PS_PUB_INT("topic.a", 5); });
	BENCH("publish with overflow", ITERATIONS, { PS_PUB_INT("topic.a", 5); });
	BENCH("ps_get and ps_unref_msg", ITERATIONS, { ps_unref_msg(ps_get(su, 1000)); });
	// BENCH("ps_get and wait 1s", 1, { ps_unref_msg(ps_get(su, 1000)); });

	ps_free_subscriber(su);
}

void test2(void) {
	ps_subscriber_t *su = NULL;

	su = ps_new_subscriber(ITERATIONS, PS_STRLIST("topic.a"));
	char t[128] = {0};
	for (int i = 0; i < 1000; i++) {
		snprintf(t, 128, "t%d", i);
		ps_subscribe(su, t);
	}
	BENCH("publish nonsubbed topic (1 sub 1000 topics)", ITERATIONS, { PS_PUB_INT("topic.b", 5); });

	ps_free_subscriber(su);
}

void test3(size_t n) {
	ps_subscriber_t **su = calloc(n, sizeof(ps_subscriber_t *));

	for (size_t i = 0; i < n; i++) {
		su[i] = ps_new_subscriber(100, PS_STRLIST("topic.a"));
	}

	char t[128] = {0};
	snprintf(t, 128, "publish nonsubbed topic (%ld subs 1 topic) ", n);
	BENCH(t, 100, { PS_PUB_INT("topic.b", 5); });

	for (int i = 0; i < n; i++) {
		ps_free_subscriber(su[i]);
	}
}

void test4(size_t n) {
	ps_subscriber_t **su = calloc(n, sizeof(ps_subscriber_t *));

	for (size_t i = 0; i < n; i++) {
		su[i] = ps_new_subscriber(100, PS_STRLIST("topic.a"));
	}

	char t[128] = {0};
	snprintf(t, 128, "publish subbed topic (%ld subs 1 topic)", n);
	BENCH(t, 100, { PS_PUB_INT("topic.a", 5); });

	for (int i = 0; i < n; i++) {
		ps_free_subscriber(su[i]);
	}

	free(su);
}

int main(int argc, char **argv) {
	ps_init();
	test1();
	test2();
	for (size_t i = 0; i < 5; i++)
		test3(pow(10, i));
	for (size_t i = 0; i < 5; i++)
		test4(pow(10, i));
	ps_deinit();
	return 0;
}