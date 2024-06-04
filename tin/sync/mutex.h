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
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;
  ~Mutex();
  void Lock();
  void Unlock();

 private:
  int32_t state_;
  uint32_t sema_;
};

class  MutexGuard {
 public:
  inline explicit MutexGuard(Mutex* lock)
    : lock_(lock) {
    lock->Lock();
  }
  MutexGuard(const MutexGuard&) = delete;
  MutexGuard& operator=(const MutexGuard&) = delete;
  inline ~MutexGuard() {
    lock_->Unlock();
  }

 private:
  Mutex* lock_;
};


}  // namespace tin
