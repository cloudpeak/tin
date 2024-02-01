// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "absl/functional/bind_front.h"
#include "tin/runtime/util.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/greenlet.h"

#include "tin/runtime/timer/timer_queue.h"


namespace tin {
namespace runtime {

void WakeupSleeperFn(void* arg, uintptr_t seq) {
  G* gp = static_cast<G*>(arg);
  Ready(gp);
}

void InternalNanoSleep(int64_t ns) {
  G* gp = GetG();
  Timer* t = gp->GetTimer();
  t->when = MonoNow() + ns;
  t->f = WakeupSleeperFn;
  t->arg = gp;

  timer_q->Lock();
  timer_q->AddTimerLocked(t);
  Park(TimerQueue::UnlockQueue, timer_q, 0);
}

int64_t NanoFromNow(int64_t deadline) {
  int64_t now = MonoNow();
  int64_t when = now + deadline;
  // if infinite or int64_t overflow
  if (deadline == -1 || (now > kint64max - deadline)) {
    when = kint64max;
  }
  return when;
}

TimerQueue::TimerQueue()
  : gp_(NULL)
  , created_(false)
  , rescheduling_(false)
  , sleeping_(false)
  , exit_flag_(false) {
}

TimerQueue::~TimerQueue() {
}

void TimerQueue::AddTimer(Timer* t) {
  LOG_IF(FATAL, t->f == NULL) << "timer fn must not be NULL";
  RawMutexGuard guard(&mutex_);
  AddTimerLocked(t);
}

void TimerQueue::AddTimerLocked(Timer* t) {
  if (t->when < 0) {
    t->when = kint64max;
  }

  t->i = Length();
  timers_.push_back(t);
  SiftUp(t->i);
  if (t->i == 0) {
    if (sleeping_) {
      sleeping_ = false;
      wait_note_.Wakeup();
    }
    if (rescheduling_) {
      rescheduling_ = false;
      Ready(gp_);
    }
    if (!created_) {
      created_ = true;

      SpawnSimple(absl::bind_front(&TimerQueue::Proc, this),
                  "timer_queue");
      wait_group_.Add(1);
    }
  }
}

bool TimerQueue::DelTimer(Timer* t) {
  RawMutexGuard guard(&mutex_);
  int i = t->i;
  int last = Length() - 1;
  if (i < 0 || i > last || timers_[i] != t) {
    return false;
  }
  if (i != last) {
    timers_[i] = timers_[last];
    timers_[i]->i = i;
  }
  timers_.pop_back();
  if (i != last) {
    SiftUp(i);
    SiftDown(i);
  }
  return true;
}

void TimerQueue::Lock() {
  mutex_.Lock();
}

bool TimerQueue::UnlockQueue(void* arg1, void* arg2) {
  TimerQueue* self = static_cast<TimerQueue*>(arg1);
  self->mutex_.Unlock();
  return true;
}

void TimerQueue::SiftUp(int i) {
  int64_t when = timers_[i]->when;
  Timer* tmp = timers_[i];
  while (i > 0) {
    int p = (i - 1) / 4;  // parent.
    if (when >= timers_[p]->when) {
      break;
    }
    timers_[i] = timers_[p];
    timers_[i]->i = i;
    timers_[p] = tmp;
    timers_[p]->i = p;
    i = p;
  }
}

void TimerQueue::SiftDown(int i) {
  int n = Length();
  int64_t when = timers_[i]->when;
  Timer* tmp = timers_[i];
  while (true) {
    int c = i * 4 + 1;  // left child
    int c3 = c + 2;  // mid child
    if (c >= n) {
      break;
    }
    int64_t w = timers_[c]->when;
    if (c + 1 < n && timers_[c + 1]->when < w) {
      w = timers_[c + 1]->when;
      c++;
    }
    if (c3 < n) {
      int64_t w3 = timers_[c3]->when;
      if (c3 + 1 < n && timers_[c3 + 1]->when < w3) {
        w3 = timers_[c3 + 1]->when;
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
    timers_[i] = timers_[c];
    timers_[i]->i = i;
    timers_[c] = tmp;
    timers_[c]->i = c;
    i = c;
  }
}

void TimerQueue::Proc() {
  gp_ = GetG();
  while (true) {
    mutex_.Lock();
    sleeping_ = false;
    int64_t now = MonoNow();
    int64_t delta = -1;
    bool oneshot;
    (void)oneshot;
    while (!exit_flag_) {
      if (timers_.empty()) {
        delta = -1;
        break;
      }

      Timer* t = timers_[0];
      delta = t->when - now;
      if (delta > 0)
        break;
      if (t->period > 0) {
        t->when += t->period * (1 + -delta / t->period);
        SiftDown(0);
        oneshot = false;
      } else {
        // oneshot timer.
        int last = Length() - 1;
        if (last > 0) {
          timers_[0] = timers_[last];
          timers_[0]->i = 0;
        }
        timers_.pop_back();
        if (last > 0) {
          SiftDown(0);
        }
        t->i = -1;
        oneshot = true;
      }
      // save fields before unlock.
      TimerCallback fired_f = t->f;
      void* fired_arg = t->arg;
      uintptr_t fired_seq = t->seq;

      mutex_.Unlock();
      fired_f(fired_arg, fired_seq);
      mutex_.Lock();
    }

    if (delta < 0) {
      // No timers left - put goroutine to sleep.
      rescheduling_ = true;
      ParkUnlock(&mutex_);
      continue;
    }
    sleeping_ = true;
    wait_note_.Clear();
    mutex_.Unlock();
    wait_note_.TimedSleepG(delta);
  }
  wait_group_.Done();
}

void TimerQueue::Join() {
  {
    RawMutexGuard guard(&mutex_);
    exit_flag_ = true;
    if (sleeping_) {
      sleeping_ = false;
      wait_note_.Wakeup();
    }
    if (rescheduling_) {
      rescheduling_ = false;
      Ready(gp_);
    }
  }
  wait_group_.Wait();
}

}  // namespace runtime
}   // namespace tin

