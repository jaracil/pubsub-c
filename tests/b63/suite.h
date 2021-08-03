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

#ifndef _B63_CONFIG_H_
#define _B63_CONFIG_H_

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "benchmark.h"

/* Configuration for printing the results. */
typedef struct b63_printer_config {
  uint8_t plaintext;
  char delimiter;
} b63_printer_config;

/* This is execution state for whole benchmark suite. */
typedef struct b63_suite {
  int64_t epochs, timelimit_s;
  b63_benchmark *baseline; /* NULL if no baseline in the suite */
  b63_printer_config printer_config;

  /* full list of counters to run */
  b63_counter_list counter_list;

  /* global seed for whole suite */
  int64_t seed;
} b63_suite;

/*
 * Reads and updates suite from CLI arguments.
 * Might allocate counter, the responsibility to free is on caller
 *
 * Examples of what should be parsed:
 *
 * bm_sqlite -t 30 -e 5 -c disk_read
 * bm_sqlite -t 30 -e 5 -c disk_write
 * bm_hashmap -t 5 -e 10 -c lpe:L1-dcache-loads
 * bm_hashmap -t 5 -e 10 -c time
 * bm_hashmap
 * bm_custom -c calls
 * bm_decision_tree -t 10 -e 5 -i -c lpe:branch-misses
 */

static void b63_suite_init(b63_suite *suite, int argc, char **argv) {
  int c;

  /* default values */
  suite->timelimit_s = 1;
  suite->epochs = 30;
  suite->counter_list.size = 0;
  suite->counter_list.data = NULL;
  suite->baseline = NULL;
  suite->printer_config.plaintext = 1;
  suite->printer_config.delimiter = ',';

  while ((c = getopt(argc, argv, "it:e:c:d:s:")) != -1) {
    switch (c) {
    case 'i':
      suite->printer_config.plaintext = 0;
      break;
    case 'd':
      /* TODO: rename to delimiter */
      suite->printer_config.delimiter = optarg[0];
      break;
    case 's':
      suite->seed = strtoll(optarg, NULL, 10);
      break;
    case 't':
      suite->timelimit_s = atoi(optarg);
      break;
    case 'e':
      suite->epochs = atoi(optarg);
      if (!(suite->epochs > 0)) {
        fprintf(stderr, "epochs count much be > 0\n");
        exit(EXIT_FAILURE);
      }
      break;
    case 'c':
      b63_counter_list_init(&suite->counter_list, optarg);
      if (suite->counter_list.size == 0) {
        fprintf(stderr, "counter_list unable to init: %s\n", optarg);
        exit(EXIT_FAILURE);
      }
      break; /* from switch */
    }
  }
}

#endif
