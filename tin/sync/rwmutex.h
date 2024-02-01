// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <stdlib.h>
#include <stdint.h>
#include "tin/sync/mutex.h"

namespace tin {

class RWMutex {
 public:
  RWMutex();
  ~RWMutex();

  void RLock();
  void RUnlock();
  void Lock();
  void Unlock();

 private:
  Mutex  w_;             // held if there are pending writers
  uint32_t writer_sem_;    // semaphore for writers to wait for completing readers
  uint32_t reader_sem;     // semaphore for readers to wait for completing writers
  int32_t  reader_count_;  // number of pending readers
  int32_t  reader_wait_;   // number of departing readers
  DISALLOW_COPY_AND_ASSIGN(RWMutex);
};

class MutexReaderGuard {
 public:
  inline explicit MutexReaderGuard(RWMutex* lock)
    : lock_(lock) {
    lock->RLock();
  }
  inline ~MutexReaderGuard() {
    lock_->RUnlock();
  }

 private:
  RWMutex* lock_;
  DISALLOW_COPY_AND_ASSIGN(MutexReaderGuard);
};

class MutexWriterGuard {
 public:
  inline explicit MutexWriterGuard(RWMutex* lock)
    : lock_(lock) {
    lock->Lock();
  }
  inline ~MutexWriterGuard() {
    lock_->Unlock();
  }

 private:
  RWMutex* lock_;
  DISALLOW_COPY_AND_ASSIGN(MutexWriterGuard);
};

}  // namespace tin
