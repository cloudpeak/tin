// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_RAW_MUTEX_H_
#define TIN_RUNTIME_RAW_MUTEX_H_
#include <cstdlib>
#include <cstdint>

namespace tin::runtime {

class M;
// ---------------------------------------------------------------------------
// RawMutex —runtime-internal spinlock + semaphore mutex.
//
// P1-5: Self-made, MUST be preserved. Uses acquire CAS on `key_` for the
// fast path and parks the M (OS thread) via SemaSleep when contended.
// The `owner_` field is set in Lock() and cleared in Unlock() for
// debug-mode deadlock detection (DCHECK on recursive locking).
// ---------------------------------------------------------------------------
class RawMutex {
 public:
  RawMutex();
  RawMutex(const RawMutex&) = delete;
  RawMutex& operator=(const RawMutex&) = delete;
  ~RawMutex();
  void Lock();
  bool TryLock();  // P1-5: non-blocking attempt
  void Unlock();

 private:
  uintptr_t key_;     // 0 = unlocked, kLocked = held, or M* | kLocked = held + waiters
  M* owner_;         // P1-5: set in Lock, cleared in Unlock; DCHECK'd for recursive lock
};

class  RawMutexGuard {
 public:
  inline explicit RawMutexGuard(RawMutex* lock)
    : lock_(lock) {
    lock->Lock();
  }
  RawMutexGuard(const RawMutexGuard&) = delete;
  RawMutexGuard& operator=(const RawMutexGuard&) = delete;
  inline ~RawMutexGuard() {
    lock_->Unlock();
  }

 private:
  RawMutex* lock_;
};

// ---------------------------------------------------------------------------
// Note —single-wakeup synchronization primitive (from Go runtime).
//
// A Note can be in one of three states:
//   - 0 (clear):        no wakeup pending
//   - kLocked:           wakeup has been issued, sleeper (if any) has been woken
//   - M* (pointer):      an M is sleeping, waiting to be woken
//
// Wakeup() transitions clear→kLocked (no-op) or M*→kLocked (wakes the M).
// Sleep()/TimedSleep() transition clear→M* (register sleeper) then block.
//
// TimedSleep  —only callable on g0 (the per-M system goroutine). Uses
//               the M's semaphore directly (no coroutine state machine).
// TimedSleepG —callable on any user coroutine. Wraps the sleep in
//               EnterSyscallBlock/ExitSyscall so the scheduler knows the
//               coroutine is blocked and can run others on the same M.
// ---------------------------------------------------------------------------
class Note {
 public:
  Note();
  void Wakeup();
  void Sleep();
  void Clear();
  bool TimedSleep(int64_t ns);   // g0-only: sleep on M semaphore
  bool TimedSleepG(int64_t ns);  // any coroutine: sleep with syscall block/unblock

 private:
  bool SleepInternal(int64_t ns);
 private:
  uintptr_t key_;
};

} // namespace tin::runtime
#endif  // TIN_RUNTIME_RAW_MUTEX_H_
