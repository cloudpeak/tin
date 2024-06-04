// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <cstdlib>
#include <cstdint>
#include <atomic>
#include "tin/sync/mutex.h"

namespace tin {

class RWMutex {
 public:
  RWMutex();
  RWMutex(const RWMutex&) = delete;
  RWMutex& operator=(const RWMutex&) = delete;
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
};

class MutexReaderGuard {
 public:
  inline explicit MutexReaderGuard(RWMutex* lock)
    : lock_(lock) {
    lock->RLock();
  }
  MutexReaderGuard(const MutexReaderGuard&) = delete;
  MutexReaderGuard& operator=(const MutexReaderGuard&) = delete;
  inline ~MutexReaderGuard() {
    lock_->RUnlock();
  }

 private:
  RWMutex* lock_;
};

class MutexWriterGuard {
 public:
  inline explicit MutexWriterGuard(RWMutex* lock)
    : lock_(lock) {
    lock->Lock();
  }
  MutexWriterGuard(const MutexWriterGuard&) = delete;
  MutexWriterGuard& operator=(const MutexWriterGuard&) = delete;
  inline ~MutexWriterGuard() {
    lock_->Unlock();
  }

 private:
  RWMutex* lock_;
};

}  // namespace tin
