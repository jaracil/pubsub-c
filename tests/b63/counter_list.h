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

#ifndef _B63_COUNTER_LIST_H_
#define _B63_COUNTER_LIST_H_

#include "counter.h"
#include "utils/string.h"

/*
 * b63_counter_list represents list of counters.
 * The only way it should be constructed
 * is directly from comma-separated configuration string.
 * Operations it supports:
 *  - initialize from a config string
 *  - iterate
 *  - cleanup
 */

typedef struct b63_counter_list {
  b63_counter *data;
  size_t size;
  char *conf;
} b63_counter_list;

/*
 * Initialize counter list from config in the format
 * counter1[,counter2,counter3,...]
 * In case one of the counters is unable to initialize, it is skipped
 * and warning is printed out.
 */
static void b63_counter_list_init(b63_counter_list *counters,
                                  const char *conf) {
  const char sep = ',';
  /* empty config is not a valid one, so at least one should be there */
  counters->size = 1 + b63_str_count(conf, sep);

  /* allocate counters */
  counters->data = (b63_counter *)malloc(counters->size * sizeof(b63_counter));
  if (counters->data == NULL) {
    /* allocation failed; unable to proceed */
    fprintf(stderr, "memory allocation failed for counter list.\n");
    exit(EXIT_FAILURE);
  }
  for (size_t i = 0; i < counters->size; i++) {
    counters->data[i].impl = NULL;
    counters->data[i].name = NULL;
  }

  const char *b = conf, *e;
  for (size_t i = 0; i < counters->size; i++) {
    e = strchr(b, sep);
    if (e == NULL) { /* will happen on last iteration */
      e = b + strlen(b);
    }

    if (!b63_counter_init(&counters->data[i], b, e)) {
      fprintf(stderr, "counter not initialized: %.*s\n", (int)(e - b), b);
      i--;
      counters->size--;
    }
    /*
     * Extra 1 is for separator.
     * b might point to an invalid location on last iteration,
     * but it won't be dereferenced.
     */
    b = e + 1;
  }
}

/* does counter-specific cleanup + destroys the list */
static void b63_counter_list_cleanup(b63_counter_list *counters) {
  for (size_t i = 0; i < counters->size; i++) {
    b63_counter_cleanup(&counters->data[i]);
  }
  free(counters->data);
  counters->data = NULL;
}

/* iterates over counters in the list */
#define B63_FOR_EACH_COUNTER(list, pc)                                         \
  for (b63_counter *pc = list.data; pc < list.data + list.size; pc++)

#endif
