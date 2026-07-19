// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/log/check.h>
#include <absl/log/log.h>

#include "tin/sync/atomic.h"
#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/semaphore.h"  // internal: SyncSema
#include "tin/sync/cond.h"          // public: Cond (PIMPL)

namespace tin {

// P2-1 PIMPL: Cond::Impl wraps the runtime-internal SyncSema.
struct Cond::Impl {
  runtime::SyncSema sem_;
};

Cond::Cond(Mutex* lock)
  : lock_(lock)
  , impl_(new Impl())
  , waiters_(0) {
}

Cond::~Cond() = default;

void Cond::Wait() {
  atomic::Inc32(&waiters_, 1);
  lock_->Unlock();
  impl_->sem_.Acquire();
  lock_->Lock();
}

void Cond::Signal() {
  SignalImpl(false);
}

void Cond::Broadcast() {
  SignalImpl(true);
}

void Cond::SignalImpl(bool all) {
  while (true) {
    uint32_t old_waiters = atomic::load32(&waiters_);
    if (old_waiters == 0) {
      return;
    }
    uint32_t new_waiters = old_waiters - 1;
    if (all) {
      new_waiters = 0;
    }
    if (atomic::cas32(&waiters_, old_waiters, new_waiters)) {
      impl_->sem_.Release(old_waiters - new_waiters);
    }
  }
}

}  // namespace tin
