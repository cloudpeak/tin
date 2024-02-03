// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <stdlib.h>
#include <stdint.h>

namespace tin {

// Mutex is a Futex implementation.
class Mutex {
 public:
  Mutex();
  ~Mutex();
  void Lock();
  void Unlock();

 private:
  int32_t state_;
  uint32_t sema_;
//  DISALLOW_COPY_AND_ASSIGN(Mutex);
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
 // DISALLOW_COPY_AND_ASSIGN(MutexGuard);
};


}  // namespace tin
