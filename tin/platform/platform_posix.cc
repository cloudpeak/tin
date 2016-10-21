// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "tin/platform/platform.h"
#include "tin/platform/platform_posix.h"

namespace tin {

bool PlatformInit() {
  return true;
}

void PlatformDeinit() {
}

}  // namespace tin
