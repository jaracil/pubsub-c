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

#ifndef _B63_UTILS_STRING_H_
#define _B63_UTILS_STRING_H_

#include <stdint.h>
#include <string.h>

/* returns 1 if string range [b, e) starts with prefix */
int8_t b63_range_starts_with(const char *b, const char *e, const char *prefix) {
  size_t lstr = e - b;
  size_t lprefix = strlen(prefix);
  return lstr < lprefix ? 0 : (memcmp(prefix, b, lprefix) == 0);
}

/* count how many times c is contained in s */
size_t b63_str_count(const char *s, char c) {
  if (s == NULL) {
    return 0;
  }
  size_t res = 0;
  for (; *s; s++) {
    if (*s == c) {
      res++;
    }
  }
  return res;
}

#endif
