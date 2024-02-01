// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <stdlib.h>

#include <cstdint>

#include "tin/runtime/semaphore.h"
#include "tin/sync/mutex.h"

namespace tin {

class Cond {
 public:
  explicit Cond(Mutex* lock)
    : lock_(lock)
    , waiters_(0) {
  }
  void Wait();
  void Signal();
  void Broascast();

 private:
  void SignalImpl(bool all);

 private:
  Mutex* lock_;
  runtime::SyncSema sem_;
  uint32_t waiters_;
  DISALLOW_COPY_AND_ASSIGN(Cond);
};

}  // namespace tin
