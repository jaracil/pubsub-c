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

#ifndef _B63_UTILS_B63_LIST_H_
#define _B63_UTILS_B63_LIST_H_

/*
 * This is a 'list of pointers, filled in at compile (link) time.
 * It is stored in separate named data section;
 * see register.h and counters/counter.h for usage example.
 * TODO: using __attribute__((constructor)) could be more portable.
 * We use gcc extensions anyway.
 */

#ifdef __APPLE__

#define B63_LIST_DECLARE(typ)                                                  \
  extern typ *__start_##typ __asm("section$start$__DATA$__" #typ);             \
  extern typ *__stop_##typ __asm("section$end$__DATA$__" #typ);

#define B63_LIST_ADD(typ, id, ptr)                                             \
  static typ *_##typ_##id __attribute__((used, section("__DATA,__" #typ))) =   \
      ptr;
#else

#define B63_LIST_DECLARE(typ)                                                  \
  extern typ *__start_##typ;                                                   \
  extern typ *__stop_##typ;

#define B63_LIST_ADD(typ, id, ptr)                                             \
  static typ *_##typ_##id __attribute__((used, section(#typ))) = ptr;
#endif

#define B63_LIST_FOR_EACH(typ, pp)                                             \
  for (typ **pp = &__start_##typ; pp < &__stop_##typ; ++pp)

#define B63_LIST_SIZE(typ) (&__stop_##typ - &__start_##typ)

#endif
