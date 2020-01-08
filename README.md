# PUBSUB

## Publish and Subscribe Overview

[![Licence](https://img.shields.io/github/license/jaracil/pubsub-c)](https://github.com/jaracil/pubsub-c/blob/master/LICENSE)
[![Actions Status](https://github.com/jaracil/pubsub-c/workflows/build/badge.svg?branch=master)](https://github.com/jaracil/pubsub-c/actions)

Pubsub utilizes a Publish/Subscribe paradigm model for real-time data streaming between several decoupled components in a application by using string topics separated by dots E.g. "system.status.ready".

For example, a instances can publish it's temperature value in `<instance_name>.evt.temp` topic. This is useful when several other instances uses another instance data to process (for example one can make push the data to a server, another one can push it using Bluetooth or store it and calculate the median temperature). The instances subscribe to that topic and waits for the data to be received.

Other useful way modules use the pubsub system is when they offer an API to other modules or to other software. For example, the temperature module offers the topic `<instance_name>.cmd.set.scale` on the pubsub to send "ºC" or "ºF".

In addition, some topics in the pubsub expect the publisher to provide a pubsub response topic to publish a response. For example, a component can publish to topic to the temperature component using  `<instance_name>.cmd.get.cfg`, witch packs the configuration and publishes the result in the response topic the caller provided. The response can be flagged as an error too if something failed.

## Data types

Data types that can be published and received through the pubsub are:

* integer
* double
* bool
* pointer
* string
* byte array
* error
* nil


## Usage examples

As an example we have the main program and a thread and they use the pubsub library to communicate one with another.

The main initializes the pubsub library using `ps_init()` and creates the thread:
```c
...
int main(void) {
    ps_init();

    pthread_t thread;
    pthread_create(&thread, NULL, subscriber_thread, NULL);
}
...
```

Then the main thread subscribes to the `"thread.ready"` path to listen if the thread has started or not and is ready to process data and the thread will send a bool message once it is ready. The difference is that the message is of type `FL_STICKY`. This means that all the subscribers that connect after the message was sent will still receive the last message sent in that path.  

```c
...
int main(void) {
...
	pthread_create(&thread, NULL, subscriber_thread, NULL);
	sleep(1); // add some delay to give time for the thread to start and send the status

	ps_subscriber_t *sub = ps_new_subscriber(10, STRLIST("thread.ready"));
	ps_msg_t *msg = ps_get(sub, 5000);
	if (msg == NULL || !IS_BOOL(msg) || msg->bool_val != true) {
		pthread_cancel(thread);
		return 0;
	}
	ps_unref_msg(msg);

...
}
```

After receiving the status of the thread and confirming that is started, we start to do some process by sending to the path `"main.evt"` some integers. And once finished we send a in the path `"main.stopping"` a bool indicating if the thread to finish it's execution. 

```c
int main(void) {
	...
	int counter = 0;
	while (counter < 4) {
		printf("Sending: %d\n", counter);
        // Send a message and wait for a response. 
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
```
The thread task subscribes to the `"main.evt"` and `"main.stopping"` and sends two messages. One with `"thread.ready"` to `false` and another with `"true"` both with message type `FL_STICKY`. Since the main thread is waiting 2 seconds before subscribing to the `"thread.ready"`, the last `FL_STICKY` is `true`.

```c
static void *subscriber_thread(void *v) {
	ps_subscriber_t *s = ps_new_subscriber(10, STRLIST("main.evt", "main.stopping"));
	sleep(1);
	PUB_BOOL_FL("thread.ready", true, FL_STICKY);
	printf("Thread: sent ready\n");

	...
}
```

The thread waits for messages from the main thread and replies to the main thread if the message is of type int and has a `rtopic`. If it receives a bool value then it stops if or keeps working depending on it's value. 

```c
static void *subscriber_thread(void *v) {
	...
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
```

The full code is available in the `test/example.c`.
