// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/log/check.h>
#include <absl/log/log.h>

#include "tin/sync/atomic.h"
#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/semaphore.h"

#include "tin/sync/wait_group.h"

namespace tin {

WaitGroup::WaitGroup(int delta /*= 0*/)
  :  state_(0)
  ,  sem_(0) {
  if (delta != 0) {
    Add(delta);
  }
}

WaitGroup::~WaitGroup() {
}

void WaitGroup::Add(int32_t delta) {
  uint64_t delta64 = delta;
  delta64 <<= 32;
  uint64_t state = state_.fetch_add(delta64) + delta64;
  int32_t v = static_cast<int32>(state >> 32);  // high 32 bits: counter.
  uint32_t w = static_cast<uint32>(state);  // lower 32 bits: waiter.
  if (v < 0) {
    LOG(FATAL) << "sync: negative WaitGroup counter";
  }
  if (w != 0 && delta > 0 && v == delta) {
    LOG(FATAL) << "sync: negative WaitGroup counter";
  }
  if (v > 0 || w == 0) {
    return;
  }
  if (state != state_) {
    LOG(FATAL) << "sync: WaitGroup misuse: Add called concurrently with Wait";
  }
  state_ = 0;
  for ( ; w != 0; w--) {
    runtime::SemRelease(&sem_);
  }
}

void WaitGroup::Done() {
  Add(-1);
}

void WaitGroup::Wait() {
  while (true) {
    uint64_t state = state_;
    int32_t v = static_cast<int32>(state >> 32);  // high 32 bits: counter.
    uint32_t w = static_cast<uint32>(state);  // lower 32 bits: waiter.
    (void)w;
    if (v == 0)
      return;
    if (state_.compare_exchange_strong(state, state + 1)) {
      runtime::SemAcquire(&sem_);
      if (state_ != 0) {
        LOG(FATAL)
            << "sync: WaitGroup is reused before previous Wait has returned";
      }
      return;
    }
  }
  return;
}

}  // namespace tin
