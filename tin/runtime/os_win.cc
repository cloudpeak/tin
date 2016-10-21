// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include "base/basictypes.h"

namespace tin {

const uintptr_t kInterruptTime = 0x7ffe0008;
const uintptr_t kSystemTime = 0x7ffe0014;

struct KSYSTEMIME {
  uint32 LowPart;
  int32 High1Time;
  int32 High2Time;
};

int64 SysTime(uintptr_t addr) {
  const KSYSTEMIME* time_addr = reinterpret_cast<const KSYSTEMIME*>(addr);
  KSYSTEMIME t;
  int64 time;
  while (true) {
    t.High1Time = time_addr->High1Time;
    t.LowPart = time_addr->LowPart;
    t.High2Time = time_addr->High2Time;
    if (t.High1Time == t.High2Time) {
      time = static_cast<int64>(t.High1Time) << 32 |
             static_cast<int64>(t.LowPart);
      break;
    }
  }
  return time;
}


int64 Now() {
  return (SysTime(kSystemTime) - 116444736000000000) * 100;
}

int64 MonoNow() {
  return SysTime(kInterruptTime) * 100;
}

int32 NowSeconds() {
  int64 millisecond = Now() / 1000000000;
  return static_cast<uint32>(millisecond);
}

}  // namespace tin
