// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Public aggregate header for tin's time API: the duration constants
// (kNanosecond ... kHour, defined in tin/time/time.h) together with the
// clock/sleep helpers Now/MonoNow/NowSeconds/NanoSleep/Sleep (declared in
// tin/runtime/runtime.h, defined in tin/runtime/os_posix.cc /
// tin/runtime/runtime.cc).
//
// This header replaces the need to include tin/runtime/runtime.h just to
// use Now() or tin::kSecond.

#ifndef TIN_TIME_H_
#define TIN_TIME_H_

#include <cstdint>

#include "tin/time/time.h"

namespace tin {

// Clock helpers. Definitions live in tin/runtime/os_posix.cc and
// tin/runtime/runtime.cc; re-declared here so users do not have to include
// the runtime internals header. Duplicate declarations are benign in C++.
int64_t Now();
int64_t MonoNow();
int32_t NowSeconds();
void NanoSleep(int64_t ns);
void Sleep(int64_t ms);

}  // namespace tin

#endif  // TIN_TIME_H_
