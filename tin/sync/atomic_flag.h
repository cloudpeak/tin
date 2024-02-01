// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <stdlib.h>

#include "base/basictypes.h"

#include "tin/sync/atomic.h"

namespace tin {

class AtomicFlag {
 public:
  explicit AtomicFlag(bool flag = false)
    : flag_(flag ? 1 : 0) {
  }
  ~AtomicFlag() {
  }

  operator bool() const {
    return atomic::acquire_load32(&flag_) == 1;
  }

  AtomicFlag& operator=(bool flag) {
    atomic::release_store32(&flag_, flag ? 1 : 0);
    return *this;
  }

 private:
  uint32_t flag_;
  DISALLOW_COPY_AND_ASSIGN(AtomicFlag);
};

}  // namespace tin
