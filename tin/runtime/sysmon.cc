// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdint>
#include <limits>

#include <absl/time/clock.h>
#include <absl/log/log.h>

#include "tin/sync/atomic.h"
#include "tin/time/time.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/net/netpoll.h"
#include "tin/runtime/timer/timer_queue.h"

#include "tin/runtime/sysmon.h"

namespace tin::runtime {

// SysMon is the runtime watchdog. It runs on its own OS thread (no P
// attached) and is responsible for:
//   - polling the network when the scheduler hasn't done so recently
//   - waking up idle Ps when timers expire (per-P timer model)
//
// The sleep interval is adaptive: it starts at 20us and grows up to a
// 10ms cap when nothing happens. If a timer is pending, idle stays at 0
// so sysmon stays responsive; if a timer has already expired but no P
// picked it up (all Ps parked), sysmon calls WakeupP() to start one.
void SysMon() {
  uint32_t idle = 0;
  uint32_t delay = 20 * 1000;  // 20ms initial (in microseconds)
  const uint32_t kMaxDelayUs = 10 * 1000;  // 10ms cap

  while (!rtm_env->ExitFlag()) {
    // Adaptive sleep.
    if (idle == 0) {
      delay = 20 * 1000;  // 20ms when busy
    } else if (idle > 50) {
      delay = std::min(delay * 2, kMaxDelayUs);
    }
    if (delay > kMaxDelayUs) {
      delay = kMaxDelayUs;
    }
    absl::SleepFor(absl::Microseconds(delay));

    int64_t now = MonoNow();

    // --- Net poll: if the scheduler hasn't polled in the last 10ms, do
    // it ourselves and inject any ready Gs.
    uint32_t last_poll = sched->LastPollTime();
    uint32_t now_ms = static_cast<uint32_t>(now / tin::kMillisecond);
    if (now_ms == 0) {
      now_ms = 1;
    }
    if (NetPollInited() && last_poll != 0 && (last_poll + 10 < now_ms)) {
      if (atomic::cas32(sched->MutableLastPollTime(), last_poll, now_ms)) {
        G* gp = NetPoll(false);
        if (gp != nullptr) {
          sched->InjectGList(gp);
        }
      }
    }

    // --- Per-P timer check: ask the heap for the earliest pending
    // deadline. If it has already expired and no P is awake to run it,
    // wake an idle P. If it's pending soon, keep idle=0 so sysmon
    // doesn't deepen its sleep.
    P* timer_pp = nullptr;
    int64_t next = TimeSleepUntil(&timer_pp);
    int64_t max_when = std::numeric_limits<int64_t>::max();
    if (next != max_when) {
      if (next > now) {
        // Timer pending. Stay responsive so we can wake a P in time.
        idle = 0;
      } else if (timer_pp != nullptr) {
        // Timer expired but apparently no P has run CheckTimers yet.
        // Wake one up (if any idle) so FindRunnable -> CheckTimers fires.
        sched->WakeupP();
        idle = 0;
      }
    } else {
      idle++;
    }

    // --- Retake: take back Ps stuck in kPsyscall for too long
    // (Go 1.15 proc.go:4746-4813). EnterSyscallBlock sets P to
    // kPsyscall; if the syscall runs longer than one sysmon cycle
    // (~10ms), retake CASes it to kPidle and hands it off to a new M.
    uint32_t retaken = sched->Retake(now);
    if (retaken > 0) {
      idle = 0;  // reset idle count — we did work
    }
  }
}

void SysMonJoin() {
}

}  // namespace tin::runtime
