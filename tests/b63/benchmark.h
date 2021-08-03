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

#ifndef _B63_DATA_H_
#define _B63_DATA_H_

#include "counter_list.h"

struct b63_epoch;
struct b63_benchmark;
struct b63_sink;
struct b63_suite;
struct b63_counter;

/*
 * Epoch is a unit of benchmark execution which might consist
 * of multiple iterations. For each benchmark,counter pair there
 * could be several epochs configured to run.
 * Result of each epoch will be used as individual measurement
 * in confidence interval computation.
 */
typedef struct b63_epoch {
  struct b63_benchmark *benchmark;
  struct b63_counter *counter;
  int64_t iterations;
  int64_t events;
  int8_t suspension_done;
  int8_t fail;
} b63_epoch;

/*
 * benchmarked function template, it needs to support 'run n iterations' and
 * seed for any generation.
 */
typedef void (*b63_target_fn)(struct b63_epoch *, uint64_t, int64_t);

/*
 * This struct represents individual benchmark.
 */
typedef struct b63_benchmark {
  /* benchmark name */
  const char *name;
  /* function to benchmark */
  const b63_target_fn run;
  /* is this benchmark a baseline? */
  const int8_t is_baseline;
  /* [weak] pointer to suite config */
  struct b63_suite *suite;
  /* [weak] pointer to current results. Used ONLY for baseline */
  b63_epoch *results;
  /* if any run has failed; */
  int8_t failed;
} b63_benchmark;

#endif
