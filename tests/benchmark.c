#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include "pubsub.h"
#include "b63/b63.h"
#include "b63/counters/perf_events.h"

B63_BENCHMARK(publish_with_a_subscriber, n) {
	ps_subscriber_t *su;

	B63_SUSPEND {
		su = ps_new_subscriber(1000000, STRLIST("topic.a"));
	}

	for (int i = 0; i < n; i++) {
		PUB_INT("topic.a", 5);
	}

	B63_SUSPEND {
		ps_free_subscriber(su);
	}
}

B63_BENCHMARK(publish_without_subscriber, n) {
	ps_subscriber_t *su;

	B63_SUSPEND {
		su = ps_new_subscriber(1000000, STRLIST("topic.a"));
	}

	for (int i = 0; i < n; i++) {
		PUB_INT("topic.b", 5);
	}

	B63_SUSPEND {
		ps_free_subscriber(su);
	}
}

B63_BENCHMARK(ps_get, n) {
	ps_subscriber_t *su;

	B63_SUSPEND {
		su = ps_new_subscriber(1000000, STRLIST("topic.a"));
	}

	for (int i = 0; i < n; i++) {
		B63_SUSPEND {
			PUB_INT("topic.a", 5);
		}

		ps_msg_t *msg = ps_get(su, 5000);

		B63_SUSPEND {
			if (msg != NULL)
				ps_unref_msg(msg);
		}
	}

	B63_SUSPEND {
		ps_free_subscriber(su);
	}
}

B63_BENCHMARK(ps_unref_msg, n) {
	ps_subscriber_t *su = NULL;
	ps_msg_t *msg = NULL;

	B63_SUSPEND {
		su = ps_new_subscriber(1000000, STRLIST("topic.a"));
	}

	for (int i = 0; i < n; i++) {
		B63_SUSPEND {
			PUB_INT("topic.a", 5);
			msg = ps_get(su, 5000);
		}

		if (msg != NULL)
			ps_unref_msg(msg);
	}

	B63_SUSPEND {
		ps_free_subscriber(su);
	}
}

int main(int argc, char **argv) {
	ps_init();
	B63_RUN(argc, argv);
	return 0;
}