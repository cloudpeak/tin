// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_NET_NETFD_H_
#define TIN_NET_NETFD_H_
#include <string>
#include "build/build_config.h"

#ifdef OS_WIN
#include "tin/net/netfd_windows.h"
#else
#include "tin/net/netfd_posix.h"
#endif

namespace tin::net {

}  // namespace tin::net
#endif  // TIN_NET_NETFD_H_
