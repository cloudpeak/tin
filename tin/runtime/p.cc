// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <thread>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include "tin/sync/atomic.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/greenlet.h"
#include "tin/runtime/scheduler.h"

#include "tin/runtime/p.h"

namespace tin {
namespace runtime {

P::P(int id)
  : runq_head_(0)
  , runq_tail_(0)
  , link_(NULL)
  , id_(id)
  , status_(kPidle)
  , sched_tick_(0)
  , m_(NULL) {
  runq_head_ = runq_tail_ = 0;
}

bool P::RunqEmpty() {
  return runq_head_ == runq_tail_ && run_next_.Integer() == 0;
}

void P::RunqPut(G* gp, bool next) {
  if (next) {
    uintptr_t oldnext = atomic::relaxed_load(run_next_.Address());
    while (!atomic::cas(run_next_.Address(), oldnext, GUintptr(gp).Integer())) {
      oldnext = atomic::relaxed_load(run_next_.Address());
    }
    if (oldnext == 0) {
      return;
    }
    // Kick the old runnext out to the regular run queue.
    gp = GpCastBack(oldnext);
  }

  while (true) {
    uint32_t h = atomic::acquire_load32(&runq_head_);
    uint32_t t = atomic::acquire_load32(&runq_tail_);

    if (t - h < static_cast<uint32_t>(kRunqCapacity)) {
      runq_[t % static_cast<uint32_t>(kRunqCapacity)] = gp;
      // store-release, makes the item available for consumption
      atomic::release_store32(&runq_tail_, t + 1);
      return;
    }
    if (RunqPutSlow(gp, h, t)) {
      return;
    }
  }
}

G* P::RunqGet(bool* inherit_time) {
  // If there's a runnext, it's the next G to run.
  while (true) {
    uintptr_t next = atomic::relaxed_load(run_next_.Address());
    if (next == 0) {
      break;
    }
    if (atomic::cas(run_next_.Address(), next, 0)) {
      if (inherit_time != NULL)
        *inherit_time = true;
      return GpCastBack(next);
    }
  }

  while (true) {
    // load-acquire, synchronize with other consumers
    uint32_t h = atomic::acquire_load32(&runq_head_);
    uint32_t t = atomic::relaxed_load32(&runq_tail_);
    if (t == h) {
      if (inherit_time != NULL)
        *inherit_time = false;
      return NULL;
    }
    G* gp = runq_[h % static_cast<uint32_t>(kRunqCapacity)].Pointer();
    // cas-release, commits consume
    if (atomic::release_cas32(&runq_head_, h, h + 1)) {
      if (inherit_time != NULL)
        *inherit_time = false;
      return gp;
    }
  }
}

bool P::RunqPutSlow(G* gp, uint32_t h, uint32_t t) {
  G* batch[kRunqCapacity / 2 + 1];
  uint32_t n = t - h;
  n = n / 2;

  if (n != static_cast<uint32_t>(kRunqCapacity / 2)) {
    // unreachable code.
    LOG(FATAL) << "RunqPutSlow: queue is not full";
  }

  for (uint32_t i = 0; i < n; i++) {
    batch[i] = runq_[(h + i) % static_cast<uint32_t>(kRunqCapacity)].Pointer();
  }

  // cas-release, commits consume
  if (!atomic::release_cas32(&runq_head_, h, h + n)) {
    return false;
  }
  batch[n] = gp;

  for (uint32_t i = 0; i < n; i++) {
    batch[i]->SetSchedLink(batch[i + 1]);
  }

  SchedulerLocker guard;
  sched->GlobalRunqBatch(batch[0], batch[n], n + 1);
  return true;
}

uint32_t P::RunqGrab(GUintptr* batch, int batch_size, uint32_t batch_head,
                   bool steal_nextg) {
  while (true) {
    // load-acquire, synchronize with other consumers
    uint32_t h = atomic::acquire_load32(&runq_head_);
    // load-acquire, synchronize with the producer
    uint32_t t = atomic::acquire_load32(&runq_tail_);
    uint32_t n = t - h;
    n = n - n / 2;
    if (n == 0) {
      if (steal_nextg) {
        // Try to steal from _p_.runnext.
        uintptr_t next = atomic::relaxed_load(run_next_.Address());
        if (next != 0) {
          // Sleep to ensure that P isn't about to run the g we
          // are about to steal.
          // The important use case here is when the g running on P
          // ready()s another g and then almost immediately blocks.
          // Instead of stealing run_next_ in this window, back off
          // to give p a chance to schedule run_next_. This will avoid
          // thrashing gs between different Ps.
          std::this_thread::yield();
          if (!atomic::cas(run_next_.Address(), next, 0)) {
            continue;
          }
          batch[batch_head % static_cast<uint32_t>(batch_size)] = next;
          return 1;
        }
      }
      return 0;
    }

    // read inconsistent h and t
    if (n > static_cast<uint32_t>(kRunqCapacity) / 2) {
      continue;
    }

    for (uint32_t i  = 0; i < n; i++) {
      GUintptr g = runq_[(h + i) % static_cast<uint32_t>(kRunqCapacity)];
      batch[(batch_head + i) % static_cast<uint32_t>(batch_size)] = g;
    }
    // cas-release, commits consume
    if (atomic::release_cas32(&runq_head_, h, h + n)) {
      return n;
    }
  }
}

// Steal half of elements from local runnable queue of p2
G* P::RunqSteal(P* p2 , bool steal_nextg ) {
  uint32_t t = runq_tail_;
  uint32_t n = p2->RunqGrab(&runq_[0], kRunqCapacity, t, steal_nextg);
  if (n == 0) {
    return NULL;
  }
  n--;
  G* gp = runq_[(t + n) % static_cast<uint32_t>(kRunqCapacity)].Pointer();
  if (n == 0) {
    return gp;
  }
  // load-acquire, synchronize with consumers
  uint32_t h = atomic::acquire_load32(&runq_head_);
  if (t - h + n >= static_cast<uint32_t>(kRunqCapacity)) {
    LOG(FATAL) << "runqsteal: runq overflow";
  }
  // store-release, makes the item available for consumption
  atomic::release_store32(&runq_tail_, t + n);
  return gp;
}

void P::MoveRunqToGlobal() {
}

bool P::CasStatus(uint32_t old_status, uint32_t new_status) {
  return atomic::cas32(&status_, old_status, new_status);
}

}  // namespace runtime
}  // namespace tin
