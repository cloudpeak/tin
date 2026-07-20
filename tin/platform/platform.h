// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_PLATFORM_PLATFORM_H_
#define TIN_PLATFORM_PLATFORM_H_
namespace tin {
// each platform should implement the fellowing two functions.
bool PlatformInit();
void PlatformDeinit();

}
#endif  // TIN_PLATFORM_PLATFORM_H_
