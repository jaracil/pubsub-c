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

#ifndef _B63_REGISTER_H_
#define _B63_REGISTER_H_

#include <stdint.h>

#include "benchmark.h"
#include "utils/section_ptr_list.h"

/*
 * Registration consists of several parts:
 *  - declare the function which we will 'measure';
 *  - create a b63_benchmark struct to hold benchmark-specific data;
 *  - put a pointer to that struct in separate section, so we can iterate on
 *    all 'registered' benchmarks later;
 *  - start the definition of the function.
 */

B63_LIST_DECLARE(b63_benchmark)

#define B63_BENCHMARK_IMPL(bname, baseline, iters)                             \
  void b63_run_##bname(b63_epoch *, uint64_t, int64_t);                        \
  static b63_benchmark b63_b_##bname = {                                       \
      .name = #bname,                                                          \
      .run = b63_run_##bname,                                                  \
      .is_baseline = baseline,                                                 \
      .failed = 0,                                                             \
  };                                                                           \
  B63_LIST_ADD(b63_benchmark, bname, &b63_b_##bname);                          \
  void b63_run_##bname(b63_epoch *b63run, uint64_t iters, int64_t b63_seed)

/*
 * Marking benchmark as a 'baseline' would make other benchmarks to be compared
 * against it. There could be only one baseline.
 */
#define B63_BASELINE(name, iters) B63_BENCHMARK_IMPL(name, 1, iters)
#define B63_BENCHMARK(name, iters) B63_BENCHMARK_IMPL(name, 0, iters)

#endif
