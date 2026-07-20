// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



#include <chrono>
#include <thread>

#include "tin/sync/atomic.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/util.h"
#include "tin/runtime/coroutine.h"
#include "tin/runtime/m.h"
#include "tin/runtime/p.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/spin.h"

#include "tin/runtime/raw_mutex.h"

namespace {
const uintptr_t kLocked = 1;
}

namespace tin::runtime {

int32_t SemaSleep(int64_t ns);
void SemaWakeup(M* m);

RawMutex::RawMutex()
  : key_(0)
  , owner_(nullptr) {
}

RawMutex::~RawMutex() {
  // P1-5: In debug mode, assert the lock is not held at destruction.
  DCHECK(owner_ == nullptr) << "RawMutex destroyed while still locked";
}

bool RawMutex::TryLock() {
  // P1-5: Non-blocking attempt. acquire CAS on key_: 0 �?kLocked.
  // Returns false if already held or has waiters.
  if (atomic::acquire_cas(&key_, 0, kLocked)) {  // acquire
    owner_ = GetM();
    return true;
  }
  return false;
}

void RawMutex::Lock() {
  G* gp = GetG();
  M* m = gp->M();

  // P1-5: Debug-mode deadlock detection —if this M already holds the
  // lock, acquiring again would deadlock. This check is only in debug
  // builds (DCHECK is a no-op in release).
  DCHECK(owner_ != m) << "RawMutex: recursive locking detected (deadlock)";

  // Speculative grab for lock. acquire_cas ensures we see all writes
  // from the previous critical section.
  if (atomic::acquire_cas(&key_, 0, kLocked)) {  // acquire
    owner_ = GetM();
    return;
  }

  m->EnsureSemaphoreExists();

  int spin = 0;
  if (rtm_env->NumberOfProcessors() > 1) {
    spin = spin::kActiveSpin;
  }

  while (true) {
    for (int i = 0; ; i++) {
      uintptr_t v = atomic::acquire_load(&key_);  // acquire
      if ((v & kLocked) == 0) {
        // Unlocked. Try to lock.
        if (atomic::acquire_cas(&key_, v, v | kLocked)) {  // acquire
          owner_ = GetM();
          return;
        }
        i = 0;
      }
      if (i < spin) {
        YieldLogicProcessor(spin::kActiveSpinCount);
      } else if (i < spin + spin::kPassiveSpin) {
        std::this_thread::yield();
      } else {
        // Someone else has it.
        // l->waitm points to a linked list of M's waiting
        // for this lock, chained through m->nextwaitm.
        // Queue this M.
        bool try_again = false;
        while (true) {
          // ensure all Machine's address must be at least a power of 2;
          m->SetNextWaitM(v & ~kLocked);
          if (atomic::acquire_cas(&key_, v, uintptr_t(m) | kLocked)) {  // acquire
            break;
          }
          v = atomic::acquire_load(&key_);  // acquire
          if ((v & kLocked) == 0) {
            try_again = true;
            break;
          }
        }
        if (try_again)
          continue;
        if ((v & kLocked) != 0) {
          // Queued.  Wait.
          // SemaSleep parks the M (OS thread) on the M's semaphore.
          // This is NOT a coroutine park —RawMutex is runtime-internal
          // and operates at the M (thread) level.
          SemaSleep(-1);
          i = 0;
        }
      }
    }
  }
}

void RawMutex::Unlock() {
  // P1-5: Clear owner_ first (release ordering via the CAS below ensures
  // this write is visible to the next acquirer).
  owner_ = nullptr;
  M* mp = nullptr;
  while (true) {
    uintptr_t v = atomic::acquire_load(&key_);  // acquire
    if (v == kLocked) {
      // No waiters. Use full barrier CAS (cas = barrier + acquire_cas)
      // to ensure all critical-section writes are published before unlock.
      if (atomic::cas(&key_, kLocked, 0)) {  // seq_cst (release+)
        break;
      }
    } else {
      // Other M's are waiting for the lock.
      // Dequeue an M.
      mp = reinterpret_cast<M*>(v & ~kLocked);
      if (atomic::acquire_cas(&key_, v, mp->NextWaitM())) {  // acquire
        // Dequeued an M.  Wake it.
        SemaWakeup(mp);
        break;
      }
    }
  }
}

Note::Note()
  : key_(0) {
}

void Note::Wakeup() {
  uintptr_t v;
  while (true) {
    v = atomic::relaxed_load(&key_);
    if (atomic::cas(&key_, v, kLocked)) {
      break;
    }
  }

  if (v == 0) {
    // Nothing was waiting. Done.
  } else if (v == kLocked) {
    // Two Wakeups!  Not allowed.
    LOG(FATAL) << "Wakeup - double wakeup";
  } else {
    M* m = reinterpret_cast<M*>(v);
    SemaWakeup(m);
  }
}

// g0 sleep.
void Note::Sleep() {
  G* gp = GetG();
  if (!gp->IsG0()) {
    LOG(FATAL) << "Sleep not on g0";
  }
  M* m = gp->M();
  m->EnsureSemaphoreExists();
  if (!atomic::cas(&key_, 0, reinterpret_cast<uintptr_t>(m))) {
    if (key_ != kLocked) {
      LOG(FATAL) << "Note::Sleep - waitm out of sync";
    }
    return;
  }
  SemaSleep(-1);
}

void Note::Clear() {
  atomic::relaxed_store(&key_, 0);
}

// g0 timed sleep.
bool Note::TimedSleep(int64_t ns) {
  G* gp = GetG();
  if (!gp->IsG0()) {
    LOG(FATAL) << "TimedSleep on g0";
  }
  GetM()->EnsureSemaphoreExists();
  SleepInternal(ns);
  return true;
}

bool Note::TimedSleepG(int64_t ns) {
  G* gp = GetG();
  if (gp->IsG0()) {
    LOG(FATAL) << "TimedSleepG on g0";
  }
  GetM()->EnsureSemaphoreExists();
  EnterSyscallBlock();
  SleepInternal(ns);
  ExitSyscall();
  return true;
}

bool Note::SleepInternal(int64_t ns) {
  G* gp = GetG();
  if (!atomic::cas(&key_, 0, reinterpret_cast<uintptr_t>(gp->M()))) {
    // Must be locked (got wakeup).
    if (key_ != kLocked) {
      LOG(FATAL) << "Note::Sleep - waitm out of sync";
    }
    return true;
  }
  if (ns < 0) {
    SemaSleep(-1);
    return true;
  }

  int64_t deadline = MonoNow() + ns;
  while (true) {
    if (SemaSleep(ns) >= 0) {
      return true;
    }
    // Interrupted or timed out.  Still registered.  Semaphore not acquired.
    if (deadline  <= MonoNow()) {
      break;
    }
    // Deadline hasn't arrived.  Keep sleeping.
  }

  // Deadline arrived.  Still registered.  Semaphore not acquired.
  // Want to give up and return, but have to unregister first,
  // so that any Wakeup racing with the return does not
  // try to grant us the semaphore when we don't expect it.

  while (true) {
    uintptr_t v = atomic::acquire_load(&key_);
    if (v == reinterpret_cast<uintptr_t>(gp->M())) {
      if (atomic::cas(&key_, v, 0)) {
        return false;
      }
    } else if (v == kLocked) {
      // Wakeup happened so semaphore is available.
      // Grab it to avoid getting out of sync.
      if (SemaSleep(-1) < 0) {
        LOG(FATAL) << "runtime: unable to acquire - semaphore out of sync";
      }
      return true;
    } else {
      LOG(FATAL) << "runtime: unexpected waitm - semaphore out of sync";
    }
  }
}

int32_t SemaSleep(int64_t ns) {
  M* m = GetG()->M();
  int64_t us = ns;
  if (us > 0) {
    us = us / 1000;
    if (us == 0) {
      us = 1;
    }
  }
  if (us == -1) {
    m->WaitSemaphore()->acquire();
  } else {
    // us is in microseconds.
    m->WaitSemaphore()->try_acquire_for(std::chrono::microseconds(us));
  }
  return 0;
}

void SemaWakeup(M* m) {
  m->WaitSemaphore()->release();
}

}  // namespace tin::runtime
