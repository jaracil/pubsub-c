#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "pubsub.h"

static void *subscriber_thread(void *v) {
	ps_subscriber_t *s = ps_new_subscriber(10, STRLIST("main.evt", "main.stopping"));
	sleep(1);
	PUB_BOOL_FL("thread.ready", true, FL_STICKY);
	printf("Thread: sent ready\n");

	bool run = true;
	ps_msg_t *msg = NULL;
	while (run) {
		msg = ps_get(s, 5000);
		if (msg != NULL) {
			if (IS_INT(msg)) {
				if (msg->rtopic != NULL) {
					PUB_INT(msg->rtopic, msg->int_val + 1);
					printf("Thread: recv int: %ld, sending: %ld\n", msg->int_val, msg->int_val + 1);
				} else {
					printf("Thread: recv int: %ld\n", msg->int_val);
				}
			} else {
				if (IS_BOOL(msg)) {
					run = msg->bool_val;
					printf("Thread: recv bool: %d\n", run);
				} else {
					printf("Thread: MSG not int or bool\n");
				}
			}
			ps_unref_msg(msg);
		}
	}
	ps_free_subscriber(s);
	return NULL;
}

int main(void) {
	ps_init();

	pthread_t thread;
	pthread_create(&thread, NULL, subscriber_thread, NULL);

	ps_subscriber_t *sub = ps_new_subscriber(10, STRLIST("thread.ready"));
	ps_msg_t *msg = ps_get(sub, 5000);
	if (msg == NULL || !IS_BOOL(msg) || msg->bool_val != true) {
		pthread_cancel(thread);
		return 0;
	}
	ps_unref_msg(msg);

	int counter = 0;
	while (counter < 4) {
		printf("Sending: %d\n", counter);

		msg = CALL_INT("main.evt", counter, 1000);
		if (msg != NULL && IS_INT(msg)) {
			printf("Recv: %ld\n", msg->int_val);
		}
		ps_unref_msg(msg);
		counter++;
	}
	PUB_BOOL("main.stopping", false);

	pthread_join(thread, NULL);
	return 0;
}
