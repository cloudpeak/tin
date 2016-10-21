// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "build/build_config.h"
#include "base/compiler_specific.h"
#include "tin/runtime/util.h"

namespace tin {
namespace runtime {

// note, GUintptr is POD type, memset on GUintptr should be OK.
class ALIGNAS(SIZE_OF_POINTER) GUintptr {
 public:
  GUintptr()
    : integer_(0) {
  }

  GUintptr(G* pointer)
    : integer_(reinterpret_cast<uintptr_t>(pointer)) {
  }

  GUintptr(void* pointer)
    : integer_(reinterpret_cast<uintptr_t>(pointer)) {
  }

  GUintptr(uintptr_t ingeter)
    : integer_(ingeter) {
  }

  GUintptr& operator=(GUintptr rhs) {
    integer_ = rhs.integer_;
    return *this;
  }

  GUintptr& operator=(uintptr_t rhs) {
    integer_ = rhs;
    return *this;
  }

  GUintptr& operator=(G* pointer) {
    integer_ = reinterpret_cast<uintptr_t>(pointer);
    return *this;
  }

  GUintptr& operator=(void* pointer) {
    integer_ = reinterpret_cast<uintptr_t>(pointer);
    return *this;
  }

  uintptr_t Integer() const {
    return integer_;
  }

  G* Pointer() const {
    return reinterpret_cast<G*>(integer_);
  }

  uintptr_t* Address() {
    return &integer_;
  }

  bool IsNull() {
    return integer_ == 0;
  }

 private:
  uintptr_t integer_;
};


}  // namespace runtime
}  // namespace tin














