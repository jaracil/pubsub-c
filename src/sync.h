#pragma once

#include <stdint.h>

#if !defined(PS_SYNC_CUSTOM) && !defined(PS_SYNC_LINUX)
#define PS_SYNC_LINUX
#endif

typedef void *mutex_t;
typedef void *semaphore_t;

int mutex_init(mutex_t *);
int mutex_lock(mutex_t);
int mutex_unlock(mutex_t);
void mutex_destroy(mutex_t *);

int semaphore_init(semaphore_t *, unsigned int value);
int semaphore_wait(semaphore_t, int32_t timeout_ms);
int semaphore_post(semaphore_t);
int semaphore_get(semaphore_t);
void semaphore_destroy(semaphore_t *);