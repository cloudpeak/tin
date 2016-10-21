// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>

#include "tin/runtime/p.h"
#include "tin/runtime/greenlet.h"
#include "tin/runtime/unlock.h"

namespace tin {
namespace runtime {

void UnLockInfo::RunInternal() {
  if (!f_(arg1_, arg2_)) {
    owner_->SetState(GLET_RUNNABLE);
    GetP()->RunqPut(owner_, false);
  }
  Clear();
}

}  // namespace runtime
}   // namespace tin
