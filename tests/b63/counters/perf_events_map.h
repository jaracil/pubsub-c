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

#ifndef _B63_COUNTERS_PERF_EVENTS_MAP_H_
#define _B63_COUNTERS_PERF_EVENTS_MAP_H_

#ifdef __linux__

#include <linux/perf_event.h>
#include <stdint.h>

/*
 * Map from user-friendly strings to constants perf_events can understand.
 */
struct b63_counter_event_map {
  const char *event_name;
  uint32_t type;
  uint64_t config;
} b63_counter_events_flat_map[] = {
    /* HW events */
    {"branches", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS},
    {"branch-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES},
    {"bus-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES},
    {"cache-misses", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES},
    {"cache-references", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES},
    {"cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES},
    {"instructions", PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS},
    {"ref-cycles", PERF_TYPE_HARDWARE, PERF_COUNT_HW_REF_CPU_CYCLES},

    /* SW events */
    {"context-switches", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES},
    {"cs", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES},
    {"major-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ},
    {"minor-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN},
    {"page-faults", PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS},

    /* Cache events */
    {"L1-dcache-load-misses", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
    {"L1-dcache-loads", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
    {"L1-dcache-prefetches", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_PREFETCH << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
    {"L1-dcache-store-misses", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
    {"L1-dcache-stores", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_L1D | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
    {"L1-icache-load-misses", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
    {"L1-icache-loads", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_L1I | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},

    {"LLC-load-misses", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
    {"LLC-loads", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
    {"LLC-store-misses", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
    {"LLC-stores", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_LL | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},

    {"branch-load-misses", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_BPU | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
    {"branch-loads", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_BPU | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},

    {"iTLB-load-misses", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
    {"iTLB-loads", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_ITLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},

    {"dTLB-load-misses", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
    {"dTLB-loads", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
    {"dTLB-store-misses", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
    {"dTLB-stores", PERF_TYPE_HW_CACHE,
     (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
      (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))}};

#endif /* __linux__ */

#endif
