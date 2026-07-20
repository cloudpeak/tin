// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_SYNC_MUTEX_H_
#define TIN_SYNC_MUTEX_H_
#include <cstdlib>
#include <cstdint>

namespace tin {

// ---------------------------------------------------------------------------
// Mutex —futex-style lock translated from Go runtime sync.Mutex.
//
// P1-5: This is a self-made mutex that MUST be preserved (not replaced with
// std::mutex or absl::Mutex). tin's mutex cooperates with the coroutine
// scheduler via SemAcquire/SemRelease, which park/unpark coroutines rather
// than blocking OS threads. Standard library locks would break M:N scheduling.
//
// Memory ordering: all atomic operations on `state_` use acquire/release
// semantics via tin::atomic wrappers (which delegate to std::atomic). The
// fast-path CAS (0 �?kMutexLocked) is acquire, ensuring subsequent reads
// see the critical section's prior writes. Unlock uses release semantics
// to publish critical-section writes before the lock bit is cleared.
// ---------------------------------------------------------------------------
class Mutex {
 public:
  Mutex();
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;
  ~Mutex();
  void Lock();
  bool TryLock();  // P1-5: non-blocking attempt; returns false if already held
  void Unlock();

 private:
  int32_t state_;   // bitfield: kMutexLocked | kMutexWoken | waiter count
  uint32_t sema_;   // self-made semaphore (parks coroutine, not OS thread)
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
#endif  // TIN_SYNC_MUTEX_H_
