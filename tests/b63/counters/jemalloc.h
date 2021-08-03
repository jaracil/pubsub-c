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

#ifndef _B63_COUNTERS_JEMALLOC_H_
#define _B63_COUNTERS_JEMALLOC_H_

#include "../counter.h"
#include <stdio.h>
#ifdef __FreeBSD__
#include <malloc_np.h>
#else
#include <jemalloc/jemalloc.h>
#endif

/*
 * Defines a counter for memory allocated. Only allocations in
 * local thread is being counted for this counter.
 */
B63_COUNTER(jemalloc_thread_allocated) {
  uint64_t res;
  size_t len = sizeof(uint64_t);
  if (0 == mallctl("thread.allocated", &res, &len, NULL, 0)) {
    return res;
  }
  fprintf(stderr, "Unable to get stats from jemalloc");
  return 0;
}

#endif
