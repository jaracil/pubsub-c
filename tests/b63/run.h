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

#ifndef _B63_RUN_H_
#define _B63_RUN_H_

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "benchmark.h"
#include "printer.h"
#include "suite.h"
#include "utils/section_ptr_list.h"
#include "utils/stats.h"
#include "utils/timer.h"

#include "counters/time.h"

static void b63_epoch_run(b63_epoch *e, int64_t seed) {
  b63_benchmark *b = e->benchmark;
  const int64_t timelimit_ms =
      1000LL * b->suite->timelimit_s / b->suite->epochs;
  b63_counter *counter = e->counter;

  const int64_t max_iterations_per_epoch = (1LL << 31LL);
  e->events = 0LL;
  e->iterations = 0LL;
  e->suspension_done = 0;
  e->fail = 0;

  int64_t started, done;
  /*
   * For each epoch run as many iterations as fit within the time budget
   */
  int64_t started_ms = b63_now_ms();
  for (int64_t n = 1; e->iterations < max_iterations_per_epoch; n *= 2) {

    /* Here the 'measured' function is called */
    started = counter->type->read(counter->impl);
    b->run(e, n, seed);
    done = counter->type->read(counter->impl);

    e->events += (done - started);
    e->iterations += n;

    /* ran out of time */
    if ((b63_now_ms() - started_ms) > timelimit_ms) {
      break;
    }
  }
  b63_print_done(e);
}

/*
 * Runs benchmark b while measuring counter c
 */
static void b63_benchmark_run(b63_benchmark *b, b63_counter *c,
                              b63_epoch *results) {
  int64_t result_index = 0LL;
  b63_suite *suite = b->suite;
  b63_stats tt;
  b63_stats_init(&tt);

  int64_t next_seed = suite->seed;
  b63_epoch *baseline_result = NULL;
  if (suite->baseline != NULL && b->is_baseline == 0) {
    baseline_result = suite->baseline->results;
  }

  for (int64_t e = 0; e < suite->epochs; e++) {
    b63_epoch *r = &(results[result_index++]);
    r->benchmark = b;
    r->counter = c;
    b63_epoch_run(r, next_seed);
    if (r->fail) {
      b->failed = 1;
      break;
    }
    next_seed = 134775853LL * next_seed + 1;

    double baseline_rate = 0.0;
    if (baseline_result != NULL) {
      baseline_rate =
          1.0 * baseline_result->events / baseline_result->iterations;
      baseline_result++;
    }
    b63_stats_add(1.0 * r->events / r->iterations, baseline_rate, &tt);
  }
  if (baseline_result != NULL) {
    b63_print_comparison(b, c->name, &tt);
  } else {
    b63_print_individual(b, c->name, &tt);
  }
}

static void b63_suite_run(b63_suite *suite) {
  B63_LIST_FOR_EACH(b63_benchmark, b) {
    (*b)->suite = suite;
    /* set baseline */
    if ((*b)->is_baseline) {
      if (suite->baseline != NULL) {
        fprintf(stderr, "two or more baselines defined.\n");
        exit(EXIT_FAILURE);
      }
      suite->baseline = *b;
    }
  }

  b63_epoch *results = (b63_epoch *)malloc(suite->epochs * sizeof(b63_epoch));
  b63_epoch *baseline_results = NULL;

  B63_FOR_EACH_COUNTER(suite->counter_list, counter) {
    if (counter->type->activate != NULL) {
      counter->type->activate(counter->impl);
    }
    if (suite->baseline != NULL) {
      baseline_results = (b63_epoch *)malloc(suite->epochs * sizeof(b63_epoch));
      suite->baseline->results = baseline_results;
      b63_benchmark_run(suite->baseline, counter, baseline_results);
    }
    B63_LIST_FOR_EACH(b63_benchmark, b) {
      if ((*b)->is_baseline) {
        continue;
      }
      b63_benchmark_run(*b, counter, results);
    }
  }

  free(results);
  if (baseline_results != NULL) {
    free(baseline_results);
  }
}

/* Reads config, creates suite and executes it */
static void b63_go(int argc, char **argv, const char *default_counter) {
  b63_suite suite;

  b63_suite_init(&suite, argc, argv);

  if (suite.counter_list.size == 0) {
    b63_counter_list_init(&suite.counter_list, default_counter);
  }

  b63_suite_run(&suite);

  b63_counter_list_cleanup(&suite.counter_list);
}

/*
 * Runs all registered benchmarks with counter. If '-c' is provided, it
 * overrides the setting in the code.
 * default_counter is a string literal, to allow
 * configuration to be passed easily, for example "lpe:cycles".
 */
#define B63_RUN_WITH(default_counter, argc, argv)                              \
  b63_go(argc, argv, default_counter);

/* shortcut for 'default' timer-based counter. */
#define B63_RUN(argc, argv) B63_RUN_WITH("time", argc, argv)

/*
 * B63_KEEP implements a way to prevent compiler from optimizing out the
 * variable. Example:
 *
 * int res = 0;
 * ... some computations ...
 * B63_KEEP(res);
 */
#define B63_KEEP(v) __asm__ __volatile__("" ::"m"(v))

/*
 * B63_SUSPEND allows to 'exclude' the counted events (or time)
 * from part of the code. It is more convenient way to do set up, for example:
 *
 * B63_BENCHMARK(sort_benchmark, n) {
 *   std::vector<int> a;
 *   B63_SUSPEND { // this block will be excluded
 *     a.resize(n);
 *     std::generate(a.begin(), a.end(), gen); // some random generator
 *   }
 *   std::sort(a.begin(), a.end());
 * }
 */

typedef struct b63_suspension {
  int64_t start;
  b63_epoch *run;
} b63_suspension;

/*
 * This is a callback to execute when suspend context gets out of scope.
 * Total number of events during 'suspension loop' is subtracted from
 * event counter.
 */
static void b63_suspension_done(b63_suspension *s) {
  s->run->events -=
      (s->run->counter->type->read(s->run->counter->impl) - s->start);
}

/*
 * Use __attribute__((cleanup)) to trigger the measurement when sctx goes out of
 * scope. This allows to compute 'how many events happened during suspension'.
 */
#define B63_SUSPEND                                                            \
  b63run->suspension_done = 0;                                                 \
  for (b63_suspension b63s __attribute__((unused, cleanup(b63_suspension_done))) =     \
           {                                                                   \
               .start = b63run->counter->type->read(b63run->counter->impl),    \
              .run = b63run,                                                   \
           };                                                                  \
       b63run->suspension_done == 0; b63run->suspension_done = 1)

#endif

/*
 * Sanity check testing. This is not supposed to be a replacement for unit-testing,
 * but it's important to be able to make sure benchmark produced expected result
 * if applicable
 */
#define B63_ASSERT(c) if (!(c)) { b63run->fail = 1; } 

