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

#ifndef _B63_UTILS_STATS_H_
#define _B63_UTILS_STATS_H_

#include <math.h>
#include <stdint.h>

#include "ttable.h"

/*
 * This struct represents data needed to perform incremental
 * computation of difference between two measurements, its mean, variance,
 * paired t-test.
 */
typedef struct b63_stats {
  int64_t n;
  double sum_base, sum_test;
  double sum;
  double sum_squared;
} b63_stats;

/*
 * Initialize with 'empty' values.
 */
void b63_stats_init(b63_stats *stats) {
  stats->n = 0LL;
  stats->sum = 0.0;
  stats->sum_base = 0.0;
  stats->sum_test = 0.0;
  stats->sum_squared = 0.0;
}

/*
 * Add a new value.
 */
void b63_stats_add(double test, double base, b63_stats *stats) {
  stats->sum_base += base;
  stats->sum_test += test;
  double diff = test - base;
  stats->sum += diff;
  stats->sum_squared += diff * diff;
  stats->n++;
}

/*
 * Average difference.
 */
double b63_stats_diff(b63_stats *stats) { return stats->sum / stats->n; }

/*
 * Average difference in %
 */
double b63_stats_percentage_diff(b63_stats *stats) {
  return 100.0 * stats->sum_test / stats->sum_base - 100.0;
}

/*compute 95% confidence interval */
double b63_stats_99_interval(b63_stats *stats) {
  double val = b63_ttable_0005[stats->n < b63_ttable_0005_size
                                   ? stats->n
                                   : b63_ttable_0005_size - 1];
  double n = stats->n;
  double s = stats->sum;
  double ssq = stats->sum_squared;
  return sqrt((ssq - s * s / n) / (n * (n - 1))) * val;
}

/*
 * compute paired t-test values. Unused at the moment.
 */
double b63_stats_ttest(b63_stats *stats) {
  if (stats->n == 0) {
    return 0.0;
  }
  double n = stats->n;
  double s = stats->sum;
  double ssq = stats->sum_squared;

  return sqrtl((n * s * s - s * s) / (n * ssq - s * s));
}

#endif
