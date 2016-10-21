// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "tin/runtime/stack/stack.h"

namespace tin {
namespace runtime {

class ProtectedFixedSizeStack : public Stack {
 public:
  ProtectedFixedSizeStack();

  virtual ~ProtectedFixedSizeStack();

  virtual void* Pointer() {
    return sp_;
  }

  virtual void* Allocate(size_t size);

 private:
  size_t size_;
  void* sp_;
};

}  // namespace runtime
}   // namespace tin

