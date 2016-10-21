// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>
#include "base/basictypes.h"
#include "base/time/time.h"

namespace tin {

/* Available from 2.6.32 onwards. */
#ifndef CLOCK_MONOTONIC_COARSE
# define CLOCK_MONOTONIC_COARSE 6
#endif

int64 Now() {
  int64 t = base::Time::Now().ToTimeT();
  // to nano seconds.
  return t * 1000000000LL;
}
#if !defined(OS_MACOSX)
int64 MonoNow() {
  static clock_t fast_clock_id = -1;
  struct timespec t;
  // ignore data race currently, it's harmless.
  if (fast_clock_id == -1) {
    if (clock_getres(CLOCK_MONOTONIC_COARSE, &t) == 0 &&
        t.tv_nsec <= 1 * 1000 * 1000) {
      fast_clock_id = CLOCK_MONOTONIC_COARSE;
    } else {
      fast_clock_id = CLOCK_MONOTONIC;
    }
  }
  clock_t clock_id = fast_clock_id;
  if (clock_gettime(clock_id, &t))
    return 0;  /* Not really possible. */

  return t.tv_sec * 1000000000LL + t.tv_nsec;
}
#else
int64 MonoNow() {
  int64 t = base::TimeTicks::Now().ToInternalValue() * 1000;
  // to nano seconds.
  return t;
}
#endif

int32 NowSeconds() {
  int64 millisecond = Now() / 1000000000LL;
  return static_cast<uint32>(millisecond);
}

}   // namespace tin
