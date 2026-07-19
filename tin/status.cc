// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tin/status.h"

#include <string>

namespace tin {

std::string Status::ToString() const {
  if (ok()) {
    return "OK";
  }
  return TinErrorName(code_);
}

}  // namespace tin
