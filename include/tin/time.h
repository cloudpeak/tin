// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Public API: time constants and clock/sleep helpers.
// Does not include any runtime/ internals.

#ifndef TIN_TIME_H_
#define TIN_TIME_H_

#include <cstdint>

#include "tin/time/time.h"

namespace tin {

int64_t Now();
int64_t MonoNow();
int32_t NowSeconds();
void NanoSleep(int64_t ns);
void Sleep(int64_t ms);

}  // namespace tin

#endif  // TIN_TIME_H_
