#include "sync.h"

#ifdef PS_SYNC_FREERTOS

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

int mutex_init(mutex_t *_m) {
	SemaphoreHandle_t *m = (SemaphoreHandle_t *) _m;
	*m = xSemaphoreCreateMutex();
	return 0;
}

int mutex_lock(mutex_t m) {
	return xSemaphoreTake((SemaphoreHandle_t) m, portMAX_DELAY) ? 0 : -1;
}

int mutex_unlock(mutex_t m) {
	return xSemaphoreGive((SemaphoreHandle_t) m) ? 0 : -1;
}

void mutex_destroy(mutex_t *_m) {
	SemaphoreHandle_t *m = (SemaphoreHandle_t *) _m;
	vSemaphoreDelete(*m);
	*m = NULL;
}

int semaphore_init(semaphore_t *_s, unsigned int value) {
	SemaphoreHandle_t *s = (SemaphoreHandle_t *) _s;
	*s = xSemaphoreCreateCounting(INT32_MAX, value);
	return 0;
}

int semaphore_wait(semaphore_t s, int32_t timeout_ms) {
	if (timeout_ms < 0) {
		return xSemaphoreTake((SemaphoreHandle_t) s, portMAX_DELAY) ? 0 : -1;
	}
	return xSemaphoreTake((SemaphoreHandle_t) s, pdMS_TO_TICKS(timeout_ms)) ? 0 : -1;
}

int semaphore_post(semaphore_t s) {
	return xSemaphoreGive((SemaphoreHandle_t) s) ? 0 : -1;
}

int semaphore_get(semaphore_t s) {
	return uxSemaphoreGetCount((SemaphoreHandle_t) s);
}

void semaphore_destroy(semaphore_t *_s) {
	SemaphoreHandle_t *s = (SemaphoreHandle_t *) _s;
	vSemaphoreDelete(*s);
	*s = NULL;
}

#endif