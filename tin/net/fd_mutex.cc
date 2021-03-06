// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "tin/net/fd_mutex.h"
#include "tin/runtime/runtime.h"

namespace tin {

void RuntimeSemacquire(uint32* addr);
void RuntimeSemrelease(uint32* addr);

namespace net {

const uint64 kMutexClosed  = (uint64)1LL << 0;  // NOLINT
const uint64 kMutexRLock   = (uint64)1LL << 1;  // NOLINT
const uint64 kMutexWLock   = (uint64)1LL << 2;  // NOLINT
const uint64 kMutexRef     = (uint64)1LL << 3;  // NOLINT
const uint64 kMutexRefMask = (uint64)((1LL << 20) - 1) << 3;  // NOLINT
const uint64 kMutexRWait   = (uint64)1LL << 23;  // NOLINT
const uint64 kMutexRMask   = (uint64)((1LL << 20) - 1) << 23;  // NOLINT
const uint64 kMutexWWait   = (uint64)1LL << 43;  // NOLINT
const uint64 kMutexWMask   = (uint64)((1LL << 20) - 1) << 43;  // NOLINT

bool FdMutex::Incref() {
  while (true) {
    uint64 old_value = state_.load(quark::memory_order_acquire);
    if ((old_value & kMutexClosed) != 0) {
      // already closed, return false.
      return false;
    }
    uint64 new_value = old_value + kMutexRef;
    if ((new_value & kMutexRefMask) == 0) {
      LOG(FATAL) << "net: inconsistent fdMutex";
    }

    if (state_.compare_exchange_strong(old_value, new_value)) {
      return true;
    }
  }
}

bool FdMutex::IncrefAndClose() {
  while (true) {
    uint64 old_value = state_.load(quark::memory_order_acquire);
    if ((old_value & kMutexClosed) != 0) {
      return false;
    }
    // Mark as closed and acquire a reference.
    uint64 new_value = (old_value | kMutexClosed) + kMutexRef;
    if ((new_value & kMutexRefMask) == 0) {
      LOG(FATAL) << "net: inconsistent fdMutex";
    }

    // Remove all read and write waiters.
    new_value &= ~(kMutexRMask | kMutexWMask);
    if (state_.compare_exchange_strong(old_value, new_value)) {
      while ((old_value & kMutexRMask) != 0) {
        old_value -= kMutexRWait;
        RuntimeSemrelease(&rsema_);
      }
      while ((old_value & kMutexWMask) != 0) {
        old_value -= kMutexWWait;
        RuntimeSemrelease(&wsema_);
      }
      return true;
    }
  }
}

bool FdMutex::Deref() {
  while (true) {
    uint64 old_value = state_.load(quark::memory_order_acquire);
    if ((old_value & kMutexRefMask) == 0) {
      LOG(FATAL) << "net: inconsistent fdMutex";
    }
    uint64 new_value  = old_value - kMutexRef;
    if (state_.compare_exchange_strong(old_value, new_value)) {
      return (new_value & (kMutexClosed | kMutexRefMask)) == kMutexClosed;
    }
  }
}

bool FdMutex::RWLock(bool read) {
  uint64 mutex_bit, mutex_wait, mutex_mask;
  uint32* mutex_sema;
  if (read) {
    mutex_bit = kMutexRLock;
    mutex_wait = kMutexRWait;
    mutex_mask = kMutexRMask;
    mutex_sema = &rsema_;
  } else {
    mutex_bit = kMutexWLock;
    mutex_wait = kMutexWWait;
    mutex_mask = kMutexWMask;
    mutex_sema = &wsema_;
  }

  while (true) {
    uint64 old_value = state_.load(quark::memory_order_acquire);
    if ((old_value & kMutexClosed) != 0) {
      return false;
    }
    uint64 new_value;
    if ((old_value & mutex_bit) == 0) {
      // Lock is free, acquire it.
      new_value = (old_value | mutex_bit) + kMutexRef;
      if ((new_value & kMutexRefMask) == 0) {
        LOG(FATAL) << "net: inconsistent fdMutex";
      }
    } else {
      // Wait for lock.
      new_value = old_value + mutex_wait;
      if ((new_value & mutex_mask) == 0) {
        LOG(FATAL) << "net: inconsistent fdMutex";
      }
    }
    if (state_.compare_exchange_strong(old_value, new_value)) {
      if ((old_value & mutex_bit) == 0) {
        return true;
      }
      RuntimeSemacquire(mutex_sema);
    }
  }
}

bool FdMutex::RWUnlock(bool read) {
  uint64 mutex_bit, mutex_wait, mutex_mask;
  uint32* mutex_sema;
  if (read) {
    mutex_bit = kMutexRLock;
    mutex_wait = kMutexRWait;
    mutex_mask = kMutexRMask;
    mutex_sema = &rsema_;
  } else {
    mutex_bit = kMutexWLock;
    mutex_wait = kMutexWWait;
    mutex_mask = kMutexWMask;
    mutex_sema = &wsema_;
  }

  while (true) {
    uint64 old_value = state_.load(quark::memory_order_acquire);
    if (((old_value & mutex_bit) == 0) || ((old_value & kMutexRefMask) == 0)) {
      LOG(FATAL) << "net: inconsistent fdMutex";
    }
    // Drop lock, drop reference and wake read waiter if present.
    uint64 new_value = (old_value & ~mutex_bit) - kMutexRef;
    if ((old_value & mutex_mask) != 0) {
      new_value -= mutex_wait;
    }

    if (state_.compare_exchange_strong(old_value, new_value)) {
      if ((old_value & mutex_mask) != 0) {
        RuntimeSemrelease(mutex_sema);
        return true;
      }
      return (new_value & (kMutexClosed | kMutexRefMask)) == kMutexClosed;
    }
  }
}

}  // namespace net
}  // namespace tin
