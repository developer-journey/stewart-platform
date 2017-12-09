/*
 * Copyright (c) 2015-2017, Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 */
#include <time.h>

void delay(long us) {
  struct timespec tv = {
    .tv_sec = 0,
    .tv_nsec = us * 1000
  };
  while (nanosleep(&tv, &tv));
}
