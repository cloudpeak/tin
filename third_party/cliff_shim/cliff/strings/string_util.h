// Copyright (c) 2026 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Shim that exposes base::strlcpy as cliff::strlcpy. cliff is no longer
// available; base owns the same implementation.

#ifndef CLIFF_SHIM_STRINGS_STRING_UTIL_H_
#define CLIFF_SHIM_STRINGS_STRING_UTIL_H_

#include "base/strings/string_util.h"

namespace cliff {

using ::base::strlcpy;
using ::base::u16cstrlcpy;

}  // namespace cliff

#endif  // CLIFF_SHIM_STRINGS_STRING_UTIL_H_
