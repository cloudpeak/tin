// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include "tin/runtime/stack/fixedsize_stack.h"
#include "tin/runtime/stack/protected_fixedsize_stack.h"

#include "tin/runtime/stack/stack.h"

namespace tin {
namespace runtime {

Stack* NewStack(int type, int size) {
  Stack* stack = NULL;
  switch (type) {
  case kFixedStack:
    stack = new FixedSizeStack();
    break;
  case kProtectedFixedStack:
    stack = new ProtectedFixedSizeStack();
    break;
  default:
    LOG(FATAL) << "invalid stack type";
  }
  stack->Allocate(size);
  return stack;
}

}  // namespace runtime
}  // namespace tin
