// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/log/check.h>
#include <absl/log/log.h>

#include "tin/sync/atomic.h"
#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/spin.h"
#include "tin/runtime/semaphore.h"
#include "tin/runtime/runtime.h"

#include "tin/sync/mutex.h"

namespace tin {

namespace {
// Go 1.15 sync/mutex.go constants.
const int32_t kMutexLocked = 1;        // bit 0
const int32_t kMutexWoken = 2;         // bit 1
const int32_t kMutexStarving = 4;      // bit 2 (Go 1.9+)
const int32_t kMutexWaiterShift = 3;   // waiter count starts at bit 3

// Starvation threshold: if a waiter blocks for longer than this, the mutex
// enters starvation mode (Go 1.15: 1ms = 1,000,000 ns).
const int64_t kStarvationThresholdNs = 1000000;
}  // namespace

Mutex::Mutex()
  : state_(0)
  , sema_(0) {
}

Mutex::~Mutex() {
}

void Mutex::Lock() {
  // Fast path: acquire lock via CAS (acquire ordering). If state_ is 0
  // (unlocked), atomically set kMutexLocked.
  if (atomic::cas32(&state_, 0, kMutexLocked)) {
    return;
  }
  // Slow path with normal/starving dual mode (Go 1.15 sync/mutex.go:lockSlow).
  int64_t wait_start = 0;
  bool starved = false;
  bool awoke = false;
  int32_t iter = 0;
  int32_t old = atomic::load32(&state_);
  while (true) {
    // Don't spin in starvation mode — ownership is handed off to waiters
    // so we won't get the lock through preemption.
    if ((old & (kMutexLocked | kMutexStarving)) == kMutexLocked &&
        tin::runtime::CanSpin(iter)) {
      // Active spinning makes sense. Try to set mutexWoken flag to inform
      // Unlock to not wake other blocked goroutines.
      if (!awoke && (old & kMutexWoken) == 0 &&
          (old >> kMutexWaiterShift) != 0 &&
          atomic::cas32(&state_, old, old | kMutexWoken)) {
        awoke = true;
      }
      tin::runtime::DoSpin();
      iter++;
      old = atomic::load32(&state_);
      continue;
    }

    int32_t new_state = old;
    // In normal mode, try to acquire the lock.
    if ((old & kMutexStarving) == 0) {
      new_state |= kMutexLocked;
    }
    // If the mutex is locked or in starvation mode, add ourselves as a waiter.
    if ((old & (kMutexLocked | kMutexStarving)) != 0) {
      new_state += (1 << kMutexWaiterShift);
    }
    // If we're starved and the mutex is locked, enter starvation mode.
    if (starved && (old & kMutexLocked) != 0) {
      new_state |= kMutexStarving;
    }
    if (awoke) {
      if ((new_state & kMutexWoken) == 0) {
        LOG(FATAL) << "sync: inconsistent mutex state";
      }
      new_state &= ~kMutexWoken;
    }

    if (atomic::cas32(&state_, old, new_state)) {
      // CAS succeeded.
      if ((old & (kMutexLocked | kMutexStarving)) == 0) {
        // We acquired the lock in normal mode.
        break;
      }

      // We are a waiter. Record wait start time and sleep.
      if (wait_start == 0) {
        wait_start = MonoNow();
      }

      // SemAcquire cooperates with the scheduler to park this coroutine.
      // With handoff (starvation mode), the woken goroutine gets a ticket=1
      // and the semaphore token is pre-consumed by SemRelease.
      bool handoff = tin::runtime::SemAcquire(&sema_);

      // Check if we should enter starvation mode.
      if (!starved && MonoNow() - wait_start > kStarvationThresholdNs) {
        starved = true;
      }

      if (handoff) {
        // Go 1.15 sync/mutex.go: "If this goroutine was woken and mutex is
        // in starvation mode, ownership was handed off to us but mutex is
        // in somewhat inconsistent state: mutexLocked is not set and we are
        // still accounted as waiter. Fix that."
        //
        // The lock was handed to us. Set locked, remove ourselves as waiter,
        // and exit starvation mode if we are the last waiter.
        while (true) {
          int32_t s = atomic::load32(&state_);
          int32_t ns = s | kMutexLocked;
          ns -= (1 << kMutexWaiterShift);  // remove ourselves as waiter
          if ((s >> kMutexWaiterShift) == 1) {
            // Last waiter — exit starvation mode.
            ns &= ~kMutexStarving;
          }
          if (atomic::cas32(&state_, s, ns)) {
            break;
          }
        }
        break;  // We have the lock!
      }

      // Normal wake: loop and try again.
      awoke = true;
      iter = 0;
      old = atomic::load32(&state_);
    } else {
      old = atomic::load32(&state_);
    }
  }
}

bool Mutex::TryLock() {
  // Non-blocking attempt. Uses acquire CAS to match Lock's fast path.
  return atomic::cas32(&state_, 0, kMutexLocked);
}

void Mutex::Unlock() {
  // Fast path: drop lock bit. inc32 uses seq_cst ordering, which is a
  // superset of release — this publishes all critical-section writes
  // before the lock bit is cleared.
  int32_t new_state = atomic::inc32(&state_, -kMutexLocked);
  if (((new_state + kMutexLocked) & kMutexLocked) == 0) {
    LOG(FATAL) << "sync: unlock of unlocked mutex";
  }

  // Go 1.15 sync/mutex.go:Unlock — starvation mode handoff.
  if ((new_state & kMutexStarving) != 0) {
    // Starving mode: hand off the mutex to the next waiter directly.
    // SemRelease(handoff=true) pre-consumes the semaphore token and sets
    // the waiter's ticket=1, so the waiter acquires the lock immediately
    // without going through the CAS loop.
    tin::runtime::SemRelease(&sema_, true);
    return;
  }

  // Normal mode: wake one waiter.
  int32_t old_state = new_state;
  while (true) {
    // If there are no waiters or a goroutine has already
    // been woken or grabbed the lock, no need to wake anyone.
    if ((old_state >> kMutexWaiterShift) == 0 ||
        (old_state & (kMutexLocked | kMutexWoken | kMutexStarving)) != 0) {
      return;
    }
    // Grab the right to wake someone.
    new_state = (old_state - (1 << kMutexWaiterShift)) | kMutexWoken;
    if (atomic::cas32(&state_, old_state, new_state)) {
      tin::runtime::SemRelease(&sema_, false);
      return;
    }
    old_state = atomic::load32(&state_);
  }
}

}  // namespace tin
