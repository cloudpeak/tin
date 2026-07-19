// Copyright (c) 2026 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Shim that exposes base's byte-order helpers (NetToHost16, HostToNet16,
// ...) under the cliff:: namespace consumed by tin sources.

#ifndef CLIFF_SHIM_BASE_SYS_BYTEORDER_H_
#define CLIFF_SHIM_BASE_SYS_BYTEORDER_H_

#include "base/sys_byteorder.h"

namespace cliff {

using ::base::ByteSwap;
using ::base::ByteSwapUintPtrT;
using ::base::ByteSwapToLE16;
using ::base::ByteSwapToLE32;
using ::base::ByteSwapToLE64;
using ::base::NetToHost16;
using ::base::NetToHost32;
using ::base::NetToHost64;
using ::base::HostToNet16;
using ::base::HostToNet32;
using ::base::HostToNet64;

}  // namespace cliff

#endif  // CLIFF_SHIM_BASE_SYS_BYTEORDER_H_
