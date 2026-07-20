// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_CONFIG_DEFAULT_H_
#define TIN_CONFIG_DEFAULT_H_

namespace tin {

const int kDefaultStackSize = 64 * 1024;

const int kStackAlignment = 64;

const int kDefaultOSThreadStackSize = 640 * 1024;

constexpr int kCacheLineSize = 64;

}  // namespace tin

#endif  // TIN_CONFIG_DEFAULT_H_
