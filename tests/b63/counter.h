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

#ifndef _B63_COUNTER_H_
#define _B63_COUNTER_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/section_ptr_list.h"
#include "utils/string.h"

/*
 * This is an 'interface' for a counter.
 * Each counter needs to implement 'read' function, with optional state
 * passed. State will be created initially from string-based config.
 * If counter is stateless, default factory method for state could be used.
 *
 * Cleanup is an optional function with 'destructor-like' role. For trivial
 * state, like 'initial integer value' where no resource were acquired, it can
 * be NULL. In cases like linux perf_events, where file descriptor better be
 * closed, it should be set up to do that job.
 *
 * Example of 'stateless' counter: time. State is maintained externally,
 * nothing needs to be stored in the counter itself.
 *
 * Example of stateful counter: linux perf_events. During initialization
 * file descriptor is opened, and then used to read counter value. That
 * file descriptor will be a part of state.
 */

typedef int64_t (*b63_counter_read_fn)(void *impl);
typedef void (*b63_counter_cleanup_fn)(void *impl);
typedef void (*b63_counter_activate_fn)(void *impl);

/*
 * returns 0 if construction fails. Note that 'NULL' implementation
 * is valid for stateless counters, so, we have to do error-handling
 * differently
 */
typedef int8_t (*b63_counter_factory_fn)(const char *config, void **impl);
/* Default counter factory for stateless counters. */
int8_t b63_counter_factory_fn_default(const char *config, void **impl) {
  return 1;
}

/*
 * This represents 'type' or 'family' of a counter. For example,
 * 'lpe' will have a single type of counter, but might have multiple
 * instances for different events.
 */
typedef struct b63_ctype {
  b63_counter_read_fn read;
  b63_counter_factory_fn factory;
  b63_counter_cleanup_fn cleanup;
  b63_counter_activate_fn activate;
  const char *prefix;
} b63_ctype;

/*
 * This is an 'instance' of a counter, with specific configuration.
 * For example, 'lpe:cycles' and 'lpe:branch-misses' would be two
 * separate instances of same counter type/family.
 */
typedef struct b63_counter {
  b63_ctype *type;
  char *name;
  void *impl;
} b63_counter;

/* Pointers to registered counter types will be stored here */
B63_LIST_DECLARE(b63_ctype);

/* Counter registration */
#define B63_COUNTER_REG(name, f, c, a)                                         \
  static int64_t b63_counter_read_##name(void *);                              \
  static b63_ctype b63_ctype_##name = {                                        \
      .read = b63_counter_read_##name,                                         \
      .factory = f,                                                            \
      .cleanup = c,                                                            \
      .activate = a,                                                           \
      .prefix = #name,                                                         \
  };                                                                           \
  B63_LIST_ADD(b63_ctype, name, &b63_ctype_##name);                            \
  static int64_t b63_counter_read_##name(void *impl)

/* Default registration for stateless counter */
#define B63_COUNTER_REG_0(name)                                          \
  B63_COUNTER_REG(name, b63_counter_factory_fn_default, NULL, NULL)

/* Default registration for counter with trivial state. */
#define B63_COUNTER_REG_1(name, f) B63_COUNTER_REG(name, f, NULL, NULL)

#define B63_COUNTER_REG_2(name, f, c) B63_COUNTER_REG(name, f, c, NULL)

/* macro 'overloading', so we can just use B63_COUNTER */
#define B63_COUNTER_GET_REG(_1, _2, _3, _4, IMPL_NAME, ...) IMPL_NAME
#define B63_COUNTER(...)                                                       \
  B63_COUNTER_GET_REG(__VA_ARGS__, B63_COUNTER_REG, B63_COUNTER_REG_2, B63_COUNTER_REG_1,   \
                      B63_COUNTER_REG_0)                                 \
  (__VA_ARGS__)

/* deallocates name and impl if provided. */
void b63_counter_cleanup(b63_counter *c) {
  if (c->impl != NULL) {
    /* things like 'close file descriptor' happen here */
    if (c->type->cleanup != NULL) {
      c->type->cleanup(c->impl);
    }
    free(c->impl);
    c->impl = NULL;
  }
  free(c->name);
  c->name = NULL;
}

/*
 * initializes counter with config string passed as range from 'b' to 'e'.
 * returns 0 (= false) if no prefix matches.
 */
int8_t b63_counter_init(b63_counter *counter, const char *b, const char *e) {
  B63_LIST_FOR_EACH(b63_ctype, type) {
    if (b63_range_starts_with(b, e, (*type)->prefix)) {
      counter->type = *type;
      counter->name = (char *)malloc(e - b + 1);
      if (counter->name == NULL) {
        /* malloc failed */
        fprintf(stderr, "memory allocation failed\n");
        return 0;
      }
      memcpy(counter->name, b, e - b);
      if (!(*type)->factory(counter->name, &counter->impl)) {
        /* implementation construction fails */
        fprintf(stderr, "counter implementation construction fails for %s\n",
                counter->name);
        free(counter->name);
        return 0;
      }
      return 1;
    }
  }
  return 0;
}

#endif
