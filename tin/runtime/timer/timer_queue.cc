// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>
#include <thread>

#include <absl/log/log.h>
#include <absl/log/check.h>

#include "tin/sync/atomic.h"
#include "tin/runtime/util.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/coroutine.h"
#include "tin/runtime/p.h"
#include "tin/runtime/net/netpoll.h"

#include "tin/runtime/timer/timer_queue.h"


namespace tin {
namespace runtime {

// ---------------------------------------------------------------------------
// Internal helpers (called with pp->TimersLock() held)
// ---------------------------------------------------------------------------

namespace {

constexpr int64_t kMaxWhen = std::numeric_limits<int64_t>::max();

inline int64_t MaxWhen() {
  return kMaxWhen;
}

// 4-ary heap: parent(i) = (i-1)/4, children of i are i*4+1..i*4+4.
void SiftUpTimer(std::vector<Timer*>& t, int i) {
  int64_t when = t[i]->when;
  Timer* tmp = t[i];
  while (i > 0) {
    int p = (i - 1) / 4;
    if (when >= t[p]->when) {
      break;
    }
    t[i] = t[p];
    i = p;
  }
  t[i] = tmp;
}

void SiftDownTimer(std::vector<Timer*>& t, int i) {
  int n = static_cast<int>(t.size());
  int64_t when = t[i]->when;
  Timer* tmp = t[i];
  while (true) {
    int c = i * 4 + 1;  // left-most child
    int c3 = c + 2;     // third child
    if (c >= n) {
      break;
    }
    int64_t w = t[c]->when;
    if (c + 1 < n && t[c + 1]->when < w) {
      w = t[c + 1]->when;
      c++;
    }
    if (c3 < n) {
      int64_t w3 = t[c3]->when;
      if (c3 + 1 < n && t[c3 + 1]->when < w3) {
        w3 = t[c3 + 1]->when;
        c3++;
      }
      if (w3 < w) {
        w = w3;
        c = c3;
      }
    }
    if (w >= when) {
      break;
    }
    t[i] = t[c];
    i = c;
  }
  t[i] = tmp;
}

void UpdateTimer0When(P* pp) {
  if (pp->Timers().empty()) {
    pp->SetTimer0When(0);
  } else {
    pp->SetTimer0When(static_cast<uint64_t>(pp->Timers()[0]->when));
  }
}

// DoAddTimer physically adds t to pp's heap (caller holds pp->TimersLock()).
void DoAddTimer(P* pp, Timer* t) {
  if (t->pp != nullptr) {
    LOG(FATAL) << "DoAddTimer: timer already attached to a P";
  }
  t->pp = pp;
  int i = static_cast<int>(pp->Timers().size());
  pp->Timers().push_back(t);
  SiftUpTimer(pp->Timers(), i);
  if (t == pp->Timers()[0]) {
    pp->SetTimer0When(static_cast<uint64_t>(t->when));
  }
  pp->IncNumTimers(1);
}

// DoDelTimer0 physically removes pp->Timers()[0] (caller holds lock).
void DoDelTimer0(P* pp) {
  std::vector<Timer*>& t = pp->Timers();
  int last = static_cast<int>(t.size()) - 1;
  if (last > 0) {
    t[0] = t[last];
  }
  t.pop_back();
  if (last > 0) {
    SiftDownTimer(t, 0);
  }
  if (t.empty()) {
    pp->SetTimer0When(0);
  } else if (t[0]->when != static_cast<int64_t>(pp->Timer0When())) {
    pp->SetTimer0When(static_cast<uint64_t>(t[0]->when));
  }
}

// RunOneTimer executes the callback for t (caller holds pp->TimersLock()).
// The lock is temporarily released while the callback runs.
void RunOneTimer(P* pp, Timer* t, int64_t now) {
  TimerCallback fired_f = t->f;
  void* fired_arg = t->arg;
  uintptr_t fired_seq = t->seq;

  if (t->period > 0) {
    // Periodic timer: advance when, stay in heap.
    int64_t delta = t->when - now;
    // next when = when + period * (1 + (-delta)/period)
    int64_t periods = 1 + (-delta) / t->period;
    t->when += t->period * periods;
    if (t->when < 0) {
      t->when = MaxWhen();
    }
    SiftDownTimer(pp->Timers(), 0);
    t->status.store(kTimerWaiting, std::memory_order_release);
    UpdateTimer0When(pp);
  } else {
    // One-shot timer: remove from heap.
    DoDelTimer0(pp);
    t->pp = nullptr;
    t->status.store(kTimerNoStatus, std::memory_order_release);
  }

  // Temporarily release the lock so callbacks can re-enter timer APIs.
  pp->TimersLock().Unlock();
  fired_f(fired_arg, fired_seq);
  pp->TimersLock().Lock();
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void WakeupSleeperFn(void* arg, uintptr_t seq) {
  (void)seq;
  G* gp = static_cast<G*>(arg);
  Ready(gp);
}

void WakeNetPoller(int64_t when) {
  (void)when;
  // Go 1.15 time.go:resettimer — wake up any M blocked in NetPoll so
  // its epoll_wait/kevent/IOCP returns early and the scheduler loop
  // can pick up the newly-added timer.
  if (NetPollInited() && NetPollWaiters() > 0) {
    NetPollBreak();
  }
  sched->WakePIfNecessary();
}

void AddTimer(Timer* t) {
  if (t->f == nullptr) {
    LOG(FATAL) << "AddTimer: timer fn must not be nullptr";
  }
  if (t->when < 0) {
    t->when = MaxWhen();
  }

  // If the timer was previously used (DelTimer'd but maybe still in a
  // heap, or fully removed), delegate to ModTimer which handles the
  // state machine correctly. This supports the del+add pattern used by
  // pollops.cc SetDeadline.
  uint32_t s = t->status.load(std::memory_order_acquire);
  if (s != kTimerNoStatus) {
    ModTimer(t, t->when, t->period, t->f, t->arg, t->seq);
    return;
  }

  t->status.store(kTimerWaiting, std::memory_order_release);

  P* pp = GetP();
  pp->TimersLock().Lock();
  DoAddTimer(pp, t);
  pp->TimersLock().Unlock();

  WakeNetPoller(t->when);
}

bool DelTimer(Timer* t) {
  for (;;) {
    uint32_t s = t->status.load(std::memory_order_acquire);
    switch (s) {
      case kTimerWaiting:
      case kTimerModifiedLater: {
        if (t->status.compare_exchange_strong(
                s, kTimerModifying, std::memory_order_acquire,
                std::memory_order_relaxed)) {
          P* tpp = t->pp;
          t->status.store(kTimerDeleted, std::memory_order_release);
          if (tpp != nullptr) tpp->IncDeletedTimers(1);
          return true;
        }
        break;
      }
      case kTimerModifiedEarlier: {
        if (t->status.compare_exchange_strong(
                s, kTimerModifying, std::memory_order_acquire,
                std::memory_order_relaxed)) {
          P* tpp = t->pp;
          if (tpp != nullptr) tpp->IncAdjustTimers(-1);
          t->status.store(kTimerDeleted, std::memory_order_release);
          if (tpp != nullptr) tpp->IncDeletedTimers(1);
          return true;
        }
        break;
      }
      case kTimerDeleted:
      case kTimerRemoving:
      case kTimerRemoved:
        return false;
      case kTimerNoStatus:
        return false;
      case kTimerRunning:
      case kTimerMoving:
      case kTimerModifying:
        std::this_thread::yield();
        break;
      default:
        LOG(FATAL) << "DelTimer: invalid timer status " << s;
    }
  }
}

bool ModTimer(Timer* t, int64_t when, int64_t period,
              TimerCallback f, void* arg, uintptr_t seq) {
  if (when < 0) {
    when = MaxWhen();
  }

  for (;;) {
    uint32_t s = t->status.load(std::memory_order_acquire);
    switch (s) {
      case kTimerWaiting:
      case kTimerModifiedEarlier:
      case kTimerModifiedLater: {
        // Pending in some P's heap. Mark as being modified.
        if (!t->status.compare_exchange_strong(
                s, kTimerModifying, std::memory_order_acquire,
                std::memory_order_relaxed)) {
          break;
        }
        bool was_earlier = (s == kTimerModifiedEarlier);
        bool now_earlier = (when < t->when);
        t->nextwhen = when;
        t->period = period;
        t->f = f;
        t->arg = arg;
        t->seq = seq;

        TimerStatus next;
        if (now_earlier) {
          next = kTimerModifiedEarlier;
        } else {
          next = kTimerModifiedLater;
        }
        t->status.store(next, std::memory_order_release);

        P* tpp = t->pp;
        if (tpp != nullptr) {
          if (!was_earlier && now_earlier) {
            tpp->IncAdjustTimers(1);
            WakeNetPoller(when);
          }
        }
        return true;
      }
      case kTimerNoStatus:
      case kTimerRemoved: {
        // Timer was removed (or never added). Re-add to current P.
        if (!t->status.compare_exchange_strong(
                s, kTimerModifying, std::memory_order_acquire,
                std::memory_order_relaxed)) {
          break;
        }
        t->pp = nullptr;
        t->when = when;
        t->period = period;
        t->nextwhen = 0;
        t->f = f;
        t->arg = arg;
        t->seq = seq;
        t->status.store(kTimerWaiting, std::memory_order_release);

        P* pp = GetP();
        pp->TimersLock().Lock();
        DoAddTimer(pp, t);
        pp->TimersLock().Unlock();
        WakeNetPoller(when);
        return true;
      }
      case kTimerDeleted: {
        // Already deleted but still in heap. Treat like a modification.
        if (!t->status.compare_exchange_strong(
                s, kTimerModifying, std::memory_order_acquire,
                std::memory_order_relaxed)) {
          break;
        }
        t->nextwhen = when;
        t->period = period;
        t->f = f;
        t->arg = arg;
        t->seq = seq;
        // Was deleted (counted in deletedTimers). Re-mark as modified.
        P* tpp = t->pp;
        bool now_earlier = (when < t->when);
        TimerStatus next = now_earlier ? kTimerModifiedEarlier
                                       : kTimerModifiedLater;
        t->status.store(next, std::memory_order_release);
        if (tpp != nullptr) {
          tpp->IncDeletedTimers(-1);
          if (now_earlier) {
            tpp->IncAdjustTimers(1);
            WakeNetPoller(when);
          }
        }
        return true;
      }
      case kTimerRunning:
      case kTimerMoving:
      case kTimerModifying:
        std::this_thread::yield();
        break;
      default:
        LOG(FATAL) << "ModTimer: invalid timer status " << s;
    }
  }
}

bool ResetTimer(Timer* t, int64_t when) {
  return ModTimer(t, when, t->period, t->f, t->arg, t->seq);
}

// ---------------------------------------------------------------------------
// Scheduler-loop entry points (called with pp->TimersLock() held where
// noted). These mirror Go 1.15's cleantimers / adjusttimers / runtimer.
// ---------------------------------------------------------------------------

namespace {

// AdjustTimers scans the entire heap: removes all Deleted timers and
// re-heapifies all Modified timers. Called when pp->AdjustTimers() > 0.
void AdjustTimers(P* pp) {
  if (pp->AdjustTimers() == 0) {
    return;
  }
  std::vector<Timer*>& t = pp->Timers();
  int n = static_cast<int>(t.size());
  for (int i = 0; i < n; /* incremented conditionally */) {
    Timer* timer = t[i];
    uint32_t s = timer->status.load(std::memory_order_acquire);
    if (s == kTimerDeleted) {
      if (!timer->status.compare_exchange_strong(
              s, kTimerRemoving, std::memory_order_acquire,
              std::memory_order_relaxed)) {
        continue;
      }
      // Remove t[i] by swapping with last and sifting.
      int last = n - 1;
      if (i != last) {
        t[i] = t[last];
      }
      t.pop_back();
      n--;
      timer->pp = nullptr;
      timer->status.store(kTimerRemoved, std::memory_order_release);
      pp->IncDeletedTimers(-1);
      if (i < n) {
        SiftUpTimer(t, i);
        SiftDownTimer(t, i);
      }
      // Don't increment i; re-examine new t[i].
    } else if (s == kTimerModifiedEarlier ||
               s == kTimerModifiedLater) {
      if (!timer->status.compare_exchange_strong(
              s, kTimerMoving, std::memory_order_acquire,
              std::memory_order_relaxed)) {
        continue;
      }
      timer->when = timer->nextwhen;
      // Remove from position i.
      int last = n - 1;
      if (i != last) {
        t[i] = t[last];
      }
      t.pop_back();
      n--;
      // Re-insert at the end and sift up.
      timer->pp = nullptr;
      timer->status.store(kTimerWaiting, std::memory_order_release);
      t.push_back(timer);
      timer->pp = pp;
      n++;
      SiftUpTimer(t, n - 1);
      if (s == kTimerModifiedEarlier) {
        pp->IncAdjustTimers(-1);
      }
      i++;  // move on; the new t[i] is a different timer.
    } else {
      // kTimerWaiting or transient.
      i++;
    }
  }
  UpdateTimer0When(pp);
}

// RunTimer runs the heap-top timer if it's due. Returns:
//   0  = a timer was fired
//  -1  = heap is empty
//  >0  = next pending deadline (heap top not due yet)
int64_t RunTimer(P* pp, int64_t now) {
  for (;;) {
    if (pp->Timers().empty()) {
      return -1;
    }
    Timer* t = pp->Timers()[0];
    uint32_t s = t->status.load(std::memory_order_acquire);
    switch (s) {
      case kTimerWaiting:
        if (t->when > now) {
          return t->when;
        }
        if (!t->status.compare_exchange_strong(
                s, kTimerRunning, std::memory_order_acquire,
                std::memory_order_relaxed)) {
          continue;
        }
        RunOneTimer(pp, t, now);
        return 0;
      case kTimerDeleted: {
        if (!t->status.compare_exchange_strong(
                s, kTimerRemoving, std::memory_order_acquire,
                std::memory_order_relaxed)) {
          continue;
        }
        DoDelTimer0(pp);
        t->pp = nullptr;
        t->status.store(kTimerRemoved, std::memory_order_release);
        pp->IncDeletedTimers(-1);
        if (pp->Timers().empty()) {
          pp->SetTimer0When(0);
          return -1;
        }
        break;  // re-examine new heap top
      }
      case kTimerModifiedEarlier:
      case kTimerModifiedLater: {
        if (!t->status.compare_exchange_strong(
                s, kTimerMoving, std::memory_order_acquire,
                std::memory_order_relaxed)) {
          continue;
        }
        t->when = t->nextwhen;
        DoDelTimer0(pp);
        t->pp = nullptr;
        DoAddTimer(pp, t);
        if (s == kTimerModifiedEarlier) {
          pp->IncAdjustTimers(-1);
        }
        t->status.store(kTimerWaiting, std::memory_order_release);
        break;  // re-examine new heap top
      }
      case kTimerModifying:
        std::this_thread::yield();
        break;
      case kTimerRunning:
      case kTimerMoving:
      case kTimerRemoving:
        // Another P is mid-operation; yield and retry.
        std::this_thread::yield();
        break;
      default:
        LOG(FATAL) << "RunTimer: invalid timer status " << s;
    }
  }
}

// ClearDeletedTimers does a full sweep removing all Deleted timers from
// the heap. Called when deletedTimers > numTimers/4 to keep the heap
// from accumulating garbage.
void ClearDeletedTimers(P* pp) {
  std::vector<Timer*>& t = pp->Timers();
  int i = 0;
  while (i < static_cast<int>(t.size())) {
    Timer* timer = t[i];
    uint32_t s = timer->status.load(std::memory_order_acquire);
    if (s == kTimerDeleted) {
      if (!timer->status.compare_exchange_strong(
              s, kTimerRemoving, std::memory_order_acquire,
              std::memory_order_relaxed)) {
        continue;
      }
      int last = static_cast<int>(t.size()) - 1;
      if (i != last) {
        t[i] = t[last];
      }
      t.pop_back();
      timer->pp = nullptr;
      timer->status.store(kTimerRemoved, std::memory_order_release);
      pp->IncDeletedTimers(-1);
      if (i < static_cast<int>(t.size())) {
        SiftUpTimer(t, i);
        SiftDownTimer(t, i);
      }
    } else {
      i++;
    }
  }
  UpdateTimer0When(pp);
}

}  // namespace

void CheckTimers(P* pp, int64_t now,
                 int64_t* rnow, int64_t* poll_until, bool* ran) {
  *ran = false;
  *poll_until = 0;

  // Fast path: no adjustTimers and heap top not due.
  if (pp->AdjustTimers() == 0) {
    uint64_t next = pp->Timer0When();
    if (next == 0) {
      *rnow = now;
      return;
    }
    if (now == 0) {
      now = MonoNow();
    }
    if (now < static_cast<int64_t>(next)) {
      // Not due yet. But if we own pp and the heap has too many deleted
      // timers, we still need to clean up.
      if (pp != GetP() ||
          static_cast<int32_t>(pp->DeletedTimers()) <=
          static_cast<int32_t>(pp->NumTimers() / 4)) {
        *rnow = now;
        *poll_until = static_cast<int64_t>(next);
        return;
      }
    }
  }

  // Slow path: take the lock and process.
  pp->TimersLock().Lock();
  AdjustTimers(pp);

  *rnow = now;
  if (!pp->Timers().empty()) {
    if (*rnow == 0) {
      *rnow = MonoNow();
    }
    while (!pp->Timers().empty()) {
      int64_t tw = RunTimer(pp, *rnow);
      if (tw != 0) {
        if (tw > 0) {
          *poll_until = tw;
        }
        break;
      }
      *ran = true;
    }
  }

  // If we own pp and deletedTimers is a large fraction, do a full sweep.
  if (pp == GetP() &&
      static_cast<int32_t>(pp->DeletedTimers()) >
      static_cast<int32_t>(pp->Timers().size() / 4)) {
    ClearDeletedTimers(pp);
  }

  pp->TimersLock().Unlock();
}

void MoveTimers(P* dst, std::vector<Timer*>& src) {
  // Caller holds dst->TimersLock() and has already detached src from its
  // origin P. We re-attach each timer to dst.
  for (Timer* t : src) {
    // Force the timer into kTimerWaiting so it gets inserted cleanly.
    // The status may be kTimerRemoved (if it was previously removed)
    // or kTimerDeleted (still in old heap, now garbage).
    uint32_t s = t->status.load(std::memory_order_acquire);
    if (s == kTimerDeleted) {
      // Skip deleted timers (they're garbage).
      t->pp = nullptr;
      t->status.store(kTimerRemoved, std::memory_order_release);
      continue;
    }
    // For Modified timers, take nextwhen as the effective when.
    if (s == kTimerModifiedEarlier || s == kTimerModifiedLater) {
      t->when = t->nextwhen;
    }
    t->pp = nullptr;
    t->status.store(kTimerWaiting, std::memory_order_release);
    DoAddTimer(dst, t);
  }
  src.clear();
}

int64_t TimeSleepUntil(P** out_pp) {
  int64_t next = MaxWhen();
  if (out_pp != nullptr) *out_pp = nullptr;

  // Hold sched->lock_ so allp_ can't change underneath us.
  SchedulerLocker guard;

  int nprocs = rtm_conf->MaxProcs();
  for (int i = 0; i < nprocs; i++) {
    P* pp = sched->AllpPublic()[i];
    if (pp == nullptr) continue;

    uint32_t c = pp->AdjustTimers();
    if (c == 0) {
      // Fast path: just sample timer0When atomically.
      uint64_t w = pp->Timer0When();
      if (w != 0 && static_cast<int64_t>(w) < next) {
        next = static_cast<int64_t>(w);
        if (out_pp != nullptr) *out_pp = pp;
      }
      continue;
    }

    // Slow path: some timers are modified, scan under lock.
    pp->TimersLock().Lock();
    for (Timer* t : pp->Timers()) {
      uint32_t s = t->status.load(std::memory_order_acquire);
      switch (s) {
        case kTimerWaiting:
          if (t->when < next) {
            next = t->when;
            if (out_pp != nullptr) *out_pp = pp;
          }
          break;
        case kTimerModifiedEarlier:
        case kTimerModifiedLater:
          if (t->nextwhen < next) {
            next = t->nextwhen;
            if (out_pp != nullptr) *out_pp = pp;
          }
          if (s == kTimerModifiedEarlier) {
            if (c > 0) c--;
          }
          break;
        default:
          break;
      }
      if (static_cast<int32_t>(c) <= 0) {
        break;
      }
    }
    pp->TimersLock().Unlock();
  }
  return next;
}

void InternalNanoSleep(int64_t ns) {
  G* gp = GetG();
  Timer* t = gp->GetTimer();
  t->when = MonoNow() + ns;
  t->f = WakeupSleeperFn;
  t->arg = gp;
  AddTimer(t);
  // Park with no unlock callback: AddTimer already returned, nothing to
  // release. The WakeupSleeperFn callback will Ready() us when due.
  Park();
}

int64_t NanoFromNow(int64_t deadline) {
  int64_t now = MonoNow();
  int64_t when = now + deadline;
  // if infinite or int64_t overflow
  if (deadline == -1 || (now > std::numeric_limits<int64_t>::max() - deadline)) {
    when = std::numeric_limits<int64_t>::max();
  }
  return when;
}

}  // namespace runtime
}  // namespace tin
