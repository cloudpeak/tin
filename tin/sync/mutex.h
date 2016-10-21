// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <stdlib.h>
#include "base/basictypes.h"

namespace tin {

// Mutex is a Futex implementation.
class Mutex {
 public:
  Mutex();
  ~Mutex();
  void Lock();
  void Unlock();

 private:
  int32 state_;
  uint32 sema_;
  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

class  MutexGuard {
 public:
  inline explicit MutexGuard(Mutex* lock)
    : lock_(lock) {
    lock->Lock();
  }
  inline ~MutexGuard() {
    lock_->Unlock();
  }

 private:
  Mutex* lock_;
  DISALLOW_COPY_AND_ASSIGN(MutexGuard);
};


}  // namespace tin
