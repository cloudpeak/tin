// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>

#include "tin/runtime/env.h"
#include "tin/runtime/greenlet.h"
#include "tin/runtime/m.h"
#include "tin/runtime/p.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/spin.h"

namespace tin {
namespace runtime {

bool CanSpin(int i) {
  int32_t max_proc = rtm_conf->MaxProcs();
  if (i >= spin::kActiveSpin ||
      rtm_env->NumberOfProcessors() <= 1 ||
      max_proc <=
      static_cast<int32>(sched->NrIdleP() + sched->NrSpinning() + 1)) {
    return false;
  }
  if (!GetG()->M()->P()->RunqEmpty()) {
    return false;
  }
  return true;
}

void DoSpin() {
  YieldLogicProcessor(spin::kActiveSpinCount);
  return;
}

}  // namespace runtime
}  // namespace tin
