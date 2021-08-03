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

#ifndef _B63_COUNTERS_TIME_H_
#define _B63_COUNTERS_TIME_H_
#ifdef NO_GET_TIME_SUPPORTED
#include <sys/time.h>
#else
#include <time.h>
#endif
#include "../counter.h"

/* default counter returning time in nanoseconds */
B63_COUNTER(time) {
#ifdef NO_GET_TIME_SUPPORTED
  struct timeval tv;

  if (gettimeofday (&tv, NULL) == 0)
    return (uint64_t) (tv.tv_sec * 1000000 + tv.tv_usec) * 1000;
  else
    return 0;
#else
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  int64_t res = 1000000000LL * (int64_t)t.tv_sec + t.tv_nsec;
  return res;
#endif
}

#endif
