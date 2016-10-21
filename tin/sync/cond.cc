// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "tin/sync/atomic.h"
#include "tin/runtime/raw_mutex.h"

#include "tin/sync/cond.h"

namespace tin {

void Cond::Wait() {
  atomic::Inc32(&waiters_, 1);
  lock_->Unlock();
  sem_.Acquire();
  lock_->Lock();
}

void Cond::Signal() {
  SignalImpl(false);
}

void Cond::Broascast() {
  SignalImpl(true);
}

void Cond::SignalImpl(bool all) {
  while (true) {
    uint32 old_waiters = atomic::load32(&waiters_);
    if (old_waiters == 0) {
      return;
    }
    uint32 new_waiters = old_waiters - 1;
    if (all) {
      new_waiters = 0;
    }
    if (atomic::cas32(&waiters_, old_waiters, new_waiters)) {
      sem_.Release(old_waiters - new_waiters);
    }
  }
}

}  // namespace tin
