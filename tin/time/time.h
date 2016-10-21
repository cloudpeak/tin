// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "base/basictypes.h"

namespace tin {

const int64 kNanosecond = 1;
const int64 kMicrosecond = 1000 * kNanosecond;
const int64 kMillisecond = 1000 * kMicrosecond;
const int64 kSecond = 1000 * kMillisecond;
const int64 kMinute = 60 * kSecond;
const int64 kHour = 60 * kMinute;

}  // namespace tin
