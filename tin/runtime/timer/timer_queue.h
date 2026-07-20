// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_TIMER_TIMER_QUEUE_H_
#define TIN_RUNTIME_TIMER_TIMER_QUEUE_H_
#include <atomic>
#include <cstdint>
#include <vector>

#include "tin/time/time.h"
#include "tin/runtime/util.h"
#include "tin/runtime/raw_mutex.h"

namespace tin {
namespace runtime {

class P;

void InternalNanoSleep(int64_t ns);

using TimerCallback = void (*)(void* arg, uintptr_t seq);

int64_t NanoFromNow(int64_t deadline);

// Per-P timer status machine (mirrors Go 1.15 src/runtime/time.go).
enum TimerStatus : uint32_t {
  kTimerNoStatus = 0,        // 0: new timer, not yet in any heap
  kTimerWaiting,             // 1: in heap, waiting to fire
  kTimerRunning,             // 2: callback currently executing (transient)
  kTimerDeleted,             // 3: marked deleted, still in heap
  kTimerRemoving,            // 4: being physically removed (transient)
  kTimerRemoved,             // 5: physically removed from heap
  kTimerModifying,           // 6: being modified (transient)
  kTimerModifiedEarlier,     // 7: modified to earlier when, nextwhen holds new value
  kTimerModifiedLater,       // 8: modified to later when, nextwhen holds new value
  kTimerMoving,              // 9: being moved in heap (transient)
};

struct Timer {
  Timer() {
    pp = nullptr;
    when = period = nextwhen = 0;
    seq = 0;
    f = nullptr;
    arg = nullptr;
    status.store(kTimerNoStatus, std::memory_order_relaxed);
  }

  P* pp;                          // P whose heap owns this timer
  int64_t when;
  int64_t period;
  int64_t nextwhen;               // modtimer: new when value
  uintptr_t seq;
  TimerCallback f;
  void* arg;
  std::atomic<uint32_t> status;   // lock-free state machine
};

// ---- Per-P Timer public API ----
// All of these operate on the per-P timer heap (Go 1.15 model).

// AddTimer adds t to the current P's heap. Sets t->status to kTimerWaiting.
void AddTimer(Timer* t);

// DelTimer marks t as deleted via CAS (lock-free). Returns true if the
// timer was waiting/modified and is now marked deleted; false if it had
// already run or was never added.
bool DelTimer(Timer* t);

// ModTimer modifies an existing timer (or re-adds a removed one) to fire
// at the new `when` with the given callback/arg/seq. Returns true if the
// timer was previously waiting/modified, false if it had already fired
// or was never added (in which case it is re-added).
bool ModTimer(Timer* t, int64_t when, int64_t period,
              TimerCallback f, void* arg, uintptr_t seq);

// ResetTimer resets t to fire at the new `when`. Equivalent to ModTimer
// keeping the existing callback/arg/seq. Returns true on success.
bool ResetTimer(Timer* t, int64_t when);

// ---- Scheduler-loop entry points (called from FindRunnable) ----

// CheckTimers is the entry point from the scheduler loop. It runs any
// expired timers on pp's heap and reports the next pending deadline.
//   now        - 0 to read the clock here, or a cached MonoNow()
//   rnow       - out: the effective "now"
//   poll_until - out: next deadline (0 if none)
//   ran        - out: true if at least one timer fired
void CheckTimers(P* pp, int64_t now,
                 int64_t* rnow, int64_t* poll_until, bool* ran);

// MoveTimers moves all timers from src into dst's heap. Caller must hold
// dst->TimersLock() and have src already detached from its origin P.
// Used during ResizeProc when a P is being destroyed.
void MoveTimers(P* dst, std::vector<Timer*>& src);

// TimeSleepUntil scans all P heaps and returns the earliest pending
// timer deadline. *out_pp (if non-null) receives the owning P.
// Used by SysMon for adaptive sleep.
int64_t TimeSleepUntil(P** out_pp);

// WakeNetPoller wakes up an idle P (if any) so the scheduler loop can
// pick up newly-added/modified timers. Called from AddTimer/ModTimer.
void WakeNetPoller(int64_t when);

}  // namespace runtime
}  // namespace tin
#endif  // TIN_RUNTIME_TIMER_TIMER_QUEUE_H_
