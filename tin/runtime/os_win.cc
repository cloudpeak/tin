// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <cstdint>

namespace tin {

const uintptr_t kInterruptTime = 0x7ffe0008;
const uintptr_t kSystemTime = 0x7ffe0014;

struct KSYSTEMIME {
  uint32_t LowPart;
  int32_t High1Time;
  int32_t High2Time;
};

int64_t SysTime(uintptr_t addr) {
  const KSYSTEMIME* time_addr = reinterpret_cast<const KSYSTEMIME*>(addr);
  KSYSTEMIME t;
  int64_t time;
  while (true) {
    t.High1Time = time_addr->High1Time;
    t.LowPart = time_addr->LowPart;
    t.High2Time = time_addr->High2Time;
    if (t.High1Time == t.High2Time) {
      time = static_cast<int64_t>(t.High1Time) << 32 |
             static_cast<int64_t>(t.LowPart);
      break;
    }
  }
  return time;
}


int64_t Now() {
  return (SysTime(kSystemTime) - 116444736000000000) * 100;
}

int64_t MonoNow() {
  return SysTime(kInterruptTime) * 100;
}

int32_t NowSeconds() {
  int64_t millisecond = Now() / 1000000000;
  return static_cast<uint32_t>(millisecond);
}

}  // namespace tin
