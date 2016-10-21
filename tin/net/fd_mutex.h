// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "base/basictypes.h"
#include "quark/atomic.hpp"

namespace tin {
namespace net {

class FdMutex {
 public:
  FdMutex()
    : state_(0)
    , rsema_(0)
    , wsema_(0)
  {};
  bool Incref();
  bool IncrefAndClose();
  bool Deref();
  bool RWLock(bool read);
  bool RWUnlock(bool read);

 private:
  quark::atomic_uint64_t state_;
  uint32 rsema_;
  uint32 wsema_;
};

}  // namespace net
}  // namespace tin






