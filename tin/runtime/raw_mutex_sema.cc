// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



#include <thread>

#include "tin/sync/atomic.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/util.h"
#include "tin/runtime/greenlet.h"
#include "tin/runtime/m.h"
#include "tin/runtime/p.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/spin.h"

#include "tin/runtime/raw_mutex.h"

namespace {
const uintptr_t kLocked = 1;
}

namespace tin {
namespace runtime {

int32_t SemaSleep(int64_t ns);
void SemaWakeup(M* m);

RawMutex::RawMutex()
  : key(0)
  , owner_(NULL) {
}

RawMutex::~RawMutex() {
}

void RawMutex::Lock() {
  G* gp = GetG();
  M* m = gp->M();

  // Speculative grab for lock.
  if (atomic::acquire_cas(&key, 0, kLocked)) {
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
      uintptr_t v = atomic::acquire_load(&key);
      if ((v & kLocked) == 0) {
        // Unlocked. Try to lock.
        if (atomic::acquire_cas(&key, v, v | kLocked)) {
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
          if (atomic::acquire_cas(&key, v, uintptr_t(m) | kLocked)) {
            break;
          }
          v = atomic::acquire_load(&key);
          if ((v & kLocked) == 0) {
            try_again = true;
            break;
          }
        }
        if (try_again)
          continue;
        if ((v & kLocked) != 0) {
          // Queued.  Wait.
          SemaSleep(-1);
          i = 0;
        }
      }
    }
  }
}

void RawMutex::Unlock() {
  owner_ = NULL;
  M* mp = NULL;
  while (true) {
    uintptr_t v = atomic::acquire_load(&key);
    if (v == kLocked) {
      if (atomic::cas(&key, kLocked, 0)) {
        break;
      }
    } else {
      // Other M's are waiting for the lock.
      // Dequeue an M.
      mp = reinterpret_cast<M*>(v & ~kLocked);
      if (atomic::acquire_cas(&key, v, mp->NextWaitM())) {
        // Dequeued an M.  Wake it.
        SemaWakeup(mp);
        break;
      }
    }
  }
}

Note::Note()
  : key(0) {
}

void Note::Wakeup() {
  uintptr_t v;
  while (true) {
    v = atomic::relaxed_load(&key);
    if (atomic::cas(&key, v, kLocked)) {
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
  if (!atomic::cas(&key, 0, reinterpret_cast<uintptr_t>(m))) {
    if (key != kLocked) {
      LOG(FATAL) << "Note::Sleep - waitm out of sync";
    }
    return;
  }
  SemaSleep(-1);
}

void Note::Clear() {
  atomic::relaxed_store(&key, 0);
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
  if (!atomic::cas(&key, 0, reinterpret_cast<uintptr_t>(gp->M()))) {
    // Must be locked (got wakeup).
    if (key != kLocked) {
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
    uintptr_t v = atomic::acquire_load(&key);
    if (v == reinterpret_cast<uintptr_t>(gp->M())) {
      if (atomic::cas(&key, v, 0)) {
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
    m->WaitSemaphore()->WaitForNotification();
  } else {
    // us is in microseconds.
    absl::Duration duration = absl::Microseconds(us);
    m->WaitSemaphore()->WaitForNotificationWithTimeout(duration);
  }
  return 0;
}

void SemaWakeup(M* m) {
  m->WaitSemaphore()->Notify();
}

}  // namespace runtime
}  // namespace tin
