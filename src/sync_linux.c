#include "sync.h"

#ifdef PS_SYNC_LINUX

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>

static int deadline_ms(int64_t ms, struct timespec *tout) {
#ifdef PS_USE_GETTIMEOFDAY
	struct timeval tv;
	gettimeofday(&tv, NULL);
	tout->tv_sec = tv.tv_sec;
	tout->tv_nsec = tv.tv_usec * 1000;
#else
	clock_gettime(CLOCK_REALTIME, tout);
#endif
	tout->tv_sec += (ms / 1000);
	tout->tv_nsec += ((ms % 1000) * 1000000);
	if (tout->tv_nsec > 1000000000) {
		tout->tv_sec++;
		tout->tv_nsec -= 1000000000;
	}
	return 0;
}

int mutex_init(mutex_t *_m) {
	pthread_mutex_t **m = (pthread_mutex_t **) _m;
	*m = calloc(1, sizeof(pthread_mutex_t));
	return pthread_mutex_init(*m, NULL);
}

int mutex_lock(mutex_t _m) {
	pthread_mutex_t *m = (pthread_mutex_t *) _m;
	return pthread_mutex_lock(m);
}

int mutex_unlock(mutex_t _m) {
	pthread_mutex_t *m = (pthread_mutex_t *) _m;
	return pthread_mutex_unlock(m);
}

void mutex_destroy(mutex_t *_m) {
	pthread_mutex_t **m = (pthread_mutex_t **) _m;
	pthread_mutex_destroy(*m);
	free(*m);
	*m = NULL;
}

int semaphore_init(semaphore_t *_s, unsigned int value) {
	sem_t **s = (sem_t **) _s;
	*s = calloc(1, sizeof(sem_t));
	return sem_init(*s, 0, value);
}

int semaphore_wait(semaphore_t _s, int32_t timeout_ms) {
	sem_t *s = (sem_t *) _s;
	struct timespec tout = {0};

	if (timeout_ms < 0) {
		return sem_wait(s);
	} else {
		deadline_ms(timeout_ms, &tout);
		return sem_timedwait(s, &tout);
	}
}

int semaphore_post(semaphore_t _s) {
	sem_t *s = (sem_t *) _s;
	return sem_post(s);
}

int semaphore_get(semaphore_t _s) {
	sem_t *s = (sem_t *) _s;
	int val = 0;
	sem_getvalue(s, &val);
	return val;
}

void semaphore_destroy(semaphore_t *_s) {
	sem_t **s = (sem_t **) _s;
	sem_destroy(*s);
	free(*s);
	*s = NULL;
}

#endif