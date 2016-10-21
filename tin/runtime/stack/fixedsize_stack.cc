// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/basictypes.h"
#include "tin/config/config.h"
#include "tin/runtime/util.h"
#include "tin/runtime/stack/fixedsize_stack.h"

namespace tin {
namespace runtime {

FixedSizeStack::FixedSizeStack()
  : vaddr_(NULL) {
}

FixedSizeStack::~FixedSizeStack() {
  std::free(vaddr_);
}

// Pointer to the beginning of the stack (depending of the architecture
// the stack grows downwards or upwards).
void* FixedSizeStack::Allocate(size_t size) {
  vaddr_ = std::malloc(size);
  if (!vaddr_)
    throw std::bad_alloc();

  // initialize to zero for debug build.
#if !defined(NDEBUG)
  memset(vaddr_, 0, size);
#endif
  vsize_ = size;
  // top.
  sp_ = static_cast<char*>(vaddr_) + size;
  return sp_;
}

}  // namespace runtime
}   // namespace tin
