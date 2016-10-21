// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <string>
#include "build/build_config.h"

#ifdef OS_WIN
#include "tin/net/netfd_windows.h"
#else
#include "tin/net/netfd_posix.h"
#endif

namespace tin {
namespace net {

}  // namespace net
}  // namespace tin



