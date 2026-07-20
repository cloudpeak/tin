// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/log/log.h>
#include <absl/log/check.h>

#include "tin/sync/atomic.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/util.h"
#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/coroutine.h"
#include "tin/runtime/m.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/timer/timer_queue.h"
#include "tin/runtime/env.h"

#include "tin/runtime/semaphore.h"


namespace tin {
namespace runtime {

struct ALIGNAS(64) SemaRoot {
  RawMutex  lock;
  Sudog* head;
  Sudog* tail;
  uint32_t nwait;  // Number of waiters. Read w/o the lock.

  void queue(uint32_t* addr, Sudog* s);
  void dequeue(Sudog* s);
};

void SemaRoot::queue(uint32_t* addr, Sudog* s) {
  s->address = addr;
  s->gp = GetG();
  s->elem = addr;
  s->next = nullptr;
  s->prev = tail;

  if (tail != nullptr) {
    tail->next = s;
  } else {
    head = s;
  }
  tail = s;
}

void SemaRoot::dequeue(Sudog* s) {
  if (s->next != nullptr) {
    s->next->prev = s->prev;
  } else {
    tail = s->prev;
  }

  if (s->prev != nullptr) {
    s->prev->next = s->next;
  } else {
    head = s->next;
  }
  s->elem = nullptr;
  s->next = nullptr;
  s->prev = nullptr;
}

const int kSemTabSize = 251;

SemaRoot kSemTable[kSemTabSize];

SemaRoot* semroot(uint32_t* addr) {
  return &kSemTable[(uintptr_t(addr) >> 3) % kSemTabSize];
}

bool CanSemAcquire(uint32_t* addr) {
  while (true) {
    uint32_t v = atomic::load32(addr);
    if (v == 0) {
      return false;
    }
    if (atomic::cas32(addr, v, v - 1)) {
      return true;
    }
  }
}

namespace {
const uint32_t kWakedUpByReleaser = 1;
const uint32_t kWakedupByTimer = 2;
}

void OnSemDeadlineReached(void* arg, uintptr_t seq) {
  Sudog* s = static_cast<Sudog*>(arg);
  SemaRoot* root = semroot(s->address);
  G* gp = nullptr;
  {
    RawMutexGuard guard(&root->lock);
    if (s->wakedup == 0) {
      // remove from list.
      atomic::inc32(&root->nwait, -1);
      root->dequeue(s);
      gp = s->gp;
    }
    s->wakedup = kWakedupByTimer;
  }
  if (gp != nullptr)
    Ready(gp);
}

void SemSetDeadline(G* gp, Sudog* s, int64_t deadline) {
  Timer* timer = gp->GetTimer();
  timer->f = OnSemDeadlineReached;
  timer->when = NanoFromNow(deadline);
  timer->arg = s;
  AddTimer(timer);
}

bool SemAcquire(uint32_t* addr) {
  G* gp = GetG();
  if (gp != gp->M()->CurG()) {
    LOG(FATAL) << "SemAcquire not on the G stack";
  }

  // Easy case.
  if (CanSemAcquire(addr)) {
    return true;
  }
  // if interrupted by timer queue.
  bool interruptd = false;
  // timed is true if been added in timer queue.
  Sudog* s = new Sudog;
  SemaRoot* root = semroot(addr);
  while (true) {
    root->lock.Lock();
    if (s->wakedup == kWakedupByTimer) {
      // waked up by timer.
      interruptd = true;
      root->lock.Unlock();
      break;
    }
    // Add ourselves to nwait to disable "easy case" in semrelease.
    atomic::inc32(&root->nwait, 1);
    // Check CanSemAcquire to avoid missed wakeup.
    if (CanSemAcquire(addr)) {
      atomic::inc32(&root->nwait, -1);
      root->lock.Unlock();
      break;
    }

    root->queue(addr, s);
    s->wakedup = 0;

    ParkUnlock(&root->lock);
    if (CanSemAcquire(addr)) {
      break;
    }
  }
  delete s;
  return !interruptd;
}

void SemRelease(uint32_t* addr) {
  SemaRoot* root = semroot(addr);
  atomic::inc32(addr, 1);
  if (atomic::load32(&root->nwait) == 0) {
    return;
  }

  // Harder case: search for a waiter and wake it.
  root->lock.Lock();
  if (atomic::load32(&root->nwait) == 0) {
    // The count is already consumed by another goroutine,
    // so no need to wake up another goroutine
    root->lock.Unlock();
    return;
  }

  Sudog* s = root->head;
  for ( ; s != nullptr; s = s->next) {
    if (s->elem == addr) {
      atomic::inc32(&root->nwait, -1);
      root->dequeue(s);
      break;
    }
  }
  if (s != nullptr)
    s->wakedup = kWakedUpByReleaser;
  root->lock.Unlock();
  if (s != nullptr) {
    Ready(s->gp);
  }
}

void SyncSema::Acquire() {
  lock_.Lock();
  if (head_ != nullptr && head_->nrelease > 0) {
    Sudog* wake = nullptr;
    head_->nrelease--;
    if (head_->nrelease == 0) {
      wake = head_;
      head_ = wake->next;
      if (head_ == nullptr) {
        tail_ = nullptr;
      }
    }
    lock_.Unlock();
    if (wake != nullptr) {
      wake->next = nullptr;
      Ready(wake->gp);
    }
  } else {
    Sudog* w = new Sudog;
    w->gp = GetG();
    w->nrelease = -1;
    w->next = nullptr;

    if (tail_ == nullptr) {
      head_ = w;
    } else {
      tail_->next = w;
    }
    tail_ = w;
    ParkUnlock(&lock_);
    delete w;
  }
}

void SyncSema::Release(uint32_t n) {
  lock_.Lock();
  while (n > 0 && head_ != nullptr && head_->nrelease < 0) {
    // Have pending acquire, satisfy it.
    Sudog* wake = head_;
    head_ = wake->next;
    if (head_ == nullptr) {
      tail_ = nullptr;
    }
    wake->next = nullptr;
    Ready(wake->gp);
    n--;
  }
  if (n > 0) {
    Sudog* w = new Sudog;
    w->gp = GetG();
    w->nrelease = static_cast<int32_t>(n);
    w->next = nullptr;
    if (tail_ == nullptr) {
      head_ = w;
    } else {
      tail_->next = w;
    }
    tail_ = w;
    ParkUnlock(&lock_);
    delete w;
  } else {
    lock_.Unlock();
  }
}

}  // namespace runtime

void RuntimeSemacquire(uint32_t* addr) {
  runtime::SemAcquire(addr);
}

void RuntimeSemrelease(uint32_t* addr) {
  runtime::SemRelease(addr);
}
}  // namespace tin
