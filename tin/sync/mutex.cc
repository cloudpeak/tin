// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "tin/sync/atomic.h"
#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/spin.h"
#include "tin/runtime/semaphore.h"

#include "tin/sync/mutex.h"

namespace tin {

namespace {
const int32 kMutexLocked = 1;  // mutex is locked
const int32 kMutexWoken = 2;
const int32 kMutexWaiterShift = 2;
}

Mutex::Mutex()
  : state_(0)
  , sema_(0) {
}

Mutex::~Mutex() {
}

void Mutex::Lock() {
  if (atomic::cas32(&state_, 0, kMutexLocked)) {
    return;
  }
  bool awoke = false;
  int32 iter = 0;
  while (true) {
    int32 old_state = state_;
    int32 new_state = old_state | kMutexLocked;
    if ((old_state & kMutexLocked) != 0) {
      if (tin::runtime::CanSpin(iter)) {
        // Active spinning makes sense.
        // Try to set mutexWoken flag to inform Unlock
        // to not wake other blocked goroutines.
        if ((!awoke) &&
            ((old_state & kMutexWoken) == 0) &&
            ((old_state >> kMutexWaiterShift) != 0) &&
            atomic::cas32(&state_, old_state, old_state | kMutexWoken)) {
          awoke = true;
        }
        tin::runtime::DoSpin();
        iter++;
        continue;
      }
      new_state = old_state + (1 << kMutexWaiterShift);
    }
    if (awoke) {
      // The goroutine has been woken from sleep,
      // so we need to reset the flag in either case.
      if ((new_state & kMutexWoken) == 0) {
        LOG(FATAL) << "sync: inconsistent mutex state";
      }
      // clear mutexWoken bit.
      new_state &= ~kMutexWoken;
    }
    if (atomic::cas32(&state_, old_state, new_state)) {
      if ((old_state & kMutexLocked) == 0)
        break;
      tin::runtime::SemAcquire(&sema_);
      awoke = true;
      iter = 0;
    }
  }
}

void Mutex::Unlock() {
  // Fast path: drop lock bit
  int32 new_state = atomic::Inc32(&state_, -kMutexLocked);
  if (((new_state + kMutexLocked)&kMutexLocked) == 0) {
    LOG(FATAL) << "sync: unlock of unlocked mutex";
  }

  int32 old_state = new_state;
  while (true) {
    // If there are no waiters or a goroutine has already
    // been woken or grabbed the lock, no need to wake anyone.
    if ((old_state >> kMutexWaiterShift) == 0 ||
        (old_state & (kMutexLocked | kMutexWoken)) != 0) {
      return;
    }
    // Grab the right to wake someone.
    new_state = (old_state - (1 << kMutexWaiterShift)) | kMutexWoken;
    if (atomic::cas32(&state_, old_state, new_state)) {
      tin::runtime::SemRelease(&sema_);
    }
    old_state = state_;
  }
}

}  // namespace tin
