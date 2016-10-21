// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"
#include "tin/sync/atomic.h"
#include "tin/time/time.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/net/netpoll.h"

#include "tin/runtime/sysmon.h"

namespace tin {
namespace runtime {

void SysMon() {
  while (!rtm_env->ExitFlag()) {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(8));
    uint32 last_poll = sched->LastPollTime();
    uint32 now = static_cast<uint32>(MonoNow() / tin::kMillisecond);
    if (now == 0)
      now = 1;
    // no worry about uint32 wrapping, it's well defined in C++ standard.
    if (NetPollInited() && last_poll != 0 && (last_poll + 10 < now)) {
      atomic::cas32(sched->MutableLastPollTime(), last_poll, now);
      G* gp = NetPoll(false);
      if (gp != NULL) {
        sched->InjectGList(gp);
      }
    }
  }
}

void SysMonJoin() {
}

}  // namespace runtime
}  // namespace tin
