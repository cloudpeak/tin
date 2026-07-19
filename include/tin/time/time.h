// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <cstdint>

namespace tin {

const int64_t kNanosecond = 1;
const int64_t kMicrosecond = 1000 * kNanosecond;
const int64_t kMillisecond = 1000 * kMicrosecond;
const int64_t kSecond = 1000 * kMillisecond;
const int64_t kMinute = 60 * kSecond;
const int64_t kHour = 60 * kMinute;

}  // namespace tin
