// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/log/check.h>
#include <absl/log/log.h>

#include "tin/sync/atomic.h"
#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/semaphore.h"

#include "tin/sync/rwmutex.h"

namespace tin {

namespace {
const int32_t kRWMutexMaxReaders = 1 << 30;
}

RWMutex::RWMutex()
  : writer_sem_(0)
  , reader_sem(0)
  , reader_count_(0)
  , reader_wait_(0) {
}

RWMutex::~RWMutex() {
}

void RWMutex::RLock() {
  if (atomic::Inc32(&reader_count_, 1) < 0) {
    runtime::SemAcquire(&reader_sem);
  }
}

void RWMutex::RUnlock() {
  int32_t r = atomic::Inc32(&reader_count_, -1);
  if (r < 0) {
    if (r + 1 == 0 || r + 1 == -kRWMutexMaxReaders) {
      LOG(FATAL) << "sync: RUnlock of unlocked RWMutex";
    }
    if (atomic::Inc32(&reader_wait_, -1) == 0) {
      runtime::SemRelease(&writer_sem_);
    }
  }
}

void RWMutex::Lock() {
  w_.Lock();
  int32_t r =
    atomic::Inc32(&reader_count_, -kRWMutexMaxReaders) + kRWMutexMaxReaders;
  if (r != 0 && atomic::Inc32(&reader_wait_, r) != 0) {
    runtime::SemAcquire(&writer_sem_);
  }
}

void RWMutex::Unlock() {
  int32_t r = atomic::Inc32(&reader_count_, kRWMutexMaxReaders);
  if (r >= kRWMutexMaxReaders) {
    LOG(FATAL) << "sync: Unlock of unlocked RWMutex";
  }

  for (int32_t i = 0; i < r; i++) {
    runtime::SemRelease(&reader_sem);
  }
  w_.Unlock();
}

}  // namespace tin
