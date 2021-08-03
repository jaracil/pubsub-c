/*
 * Copyright 2019 Oleksandr Kuvshynov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _B63_COUNTERS_PERF_EVENTS_H_
#define _B63_COUNTERS_PERF_EVENTS_H_

#ifdef __linux__

#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "../counter.h"
#include "perf_events_map.h"

/*
 * Linux perf_events-based counters.
 * 'lpe' is used as an acronym for linux perf_events.
 */

/* 'state' of a counter */
typedef struct b63_counter_lpe {
  int32_t fd;
} b63_counter_lpe;

/*
 * open file descriptor to read counter. See
 * http://man7.org/linux/man-pages/man2/perf_event_open.2.html for the list
 */
static int32_t b63_counter_lpe_open(uint32_t type, uint64_t config) {
  struct perf_event_attr pe;

  memset(&pe, 0, sizeof(struct perf_event_attr));
  pe.type = type;
  pe.size = sizeof(struct perf_event_attr);
  pe.config = config;
  pe.disabled = 1;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;

  /* Counting for current process, any cpu: pid == 0, cpu == -1 */
  int32_t fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
  if (fd == -1) {
    fprintf(stderr, "perf event open error\n");
    return -1;
  }
  /* no need to reset really, as we care about difference, not value */
  ioctl(fd, PERF_EVENT_IOC_RESET, 0);
  ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
  return fd;
}

/*
 * looking up type/config combination by event_name.
 * names match those of perf tool (`perf list`), for consistency.
 * returns -1 on failure.
 */
static int32_t b63_counter_lpe_init(const char *event_name) {
  /* first, check list of predefined events */
  for (size_t i = 0; i < sizeof(b63_counter_events_flat_map) /
                             sizeof(struct b63_counter_event_map);
       ++i) {
    if (strcmp(b63_counter_events_flat_map[i].event_name, event_name) == 0) {
      return b63_counter_lpe_open(b63_counter_events_flat_map[i].type,
                                  b63_counter_events_flat_map[i].config);
    }
  }
  /* now, trying raw event in format r<mask><event>, for example r01a1 */
  if (strlen(event_name) > 1) {
    if (event_name[0] == 'r') {
      uint64_t conf = strtoull(event_name + 1, NULL, 16);
      return b63_counter_lpe_open(PERF_TYPE_RAW, conf);
    }
  }
  fprintf(stderr, "linux perf_events: unable to find event %s\n", event_name);
  return -1;
}

/*
 * conf comes in format 'lpe:cycles'
 * Thus, we pick the suffix and try to find needed options.
 * Return:
 *  0 if failure,
 *  1 if success
 */
static int8_t b63_counter_lpe_create(const char *conf, void **impl) {
  b63_counter_lpe *lpe = (b63_counter_lpe *)malloc(sizeof(b63_counter_lpe));
  if (lpe == NULL) {
    fprintf(stderr, "memory allocation failed for lpe counter\n");
    return 0;
  }

  const char *event_name = conf + strlen("lpe:");
  lpe->fd = b63_counter_lpe_init(event_name);
  if (lpe->fd == -1) {
    /* failure: need to free resources */
    free(lpe);
    return 0;
  }
  *impl = lpe;
  return 1;
}

/* Close file descriptor */
static void b63_counter_lpe_cleanup(void *impl) {
  if (impl != NULL) {
    b63_counter_lpe *lpe_impl = (b63_counter_lpe *)impl;
    close(lpe_impl->fd);
  }
}

/* impl is 'passed' implicitly */
B63_COUNTER(lpe, b63_counter_lpe_create, b63_counter_lpe_cleanup) {
  b63_counter_lpe *lpe_impl = (b63_counter_lpe *)impl;
  int64_t res;
  if (read(lpe_impl->fd, &res, sizeof(int64_t)) != sizeof(int64_t)) {
    fprintf(stderr, "read from perf_events fd failed");
    return 0;
  }
  return res;
}

#endif /* __linux__ */
#endif
