// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_TIME_TIME_H_
#define TIN_TIME_TIME_H_
#include <cstdint>

namespace tin {

const int64_t kNanosecond = 1;
const int64_t kMicrosecond = 1000 * kNanosecond;
const int64_t kMillisecond = 1000 * kMicrosecond;
const int64_t kSecond = 1000 * kMillisecond;
const int64_t kMinute = 60 * kSecond;
const int64_t kHour = 60 * kMinute;

}  // namespace tin
#endif  // TIN_TIME_TIME_H_
