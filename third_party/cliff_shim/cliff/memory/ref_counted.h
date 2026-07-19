// Copyright (c) 2026 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Shim that exposes a non-template cliff::RefCountedThreadSafe backed by
// base::subtle::RefCountedThreadSafeBase. tin source code uses
// `cliff::RefCountedThreadSafe` (no template argument) as a CRTP-free base
// class; base only ships the templated base::RefCountedThreadSafe<T>, so we
// provide a thin non-template wrapper here. Delete-on-zero is implemented
// via a virtual destructor (safe because the wrapper is the only base).

#ifndef CLIFF_SHIM_MEMORY_REF_COUNTED_H_
#define CLIFF_SHIM_MEMORY_REF_COUNTED_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

namespace cliff {

class RefCountedThreadSafe
    : public ::base::subtle::RefCountedThreadSafeBase {
 public:
  RefCountedThreadSafe()
      : ::base::subtle::RefCountedThreadSafeBase(
            ::base::subtle::kStartRefCountFromZeroTag) {}

  RefCountedThreadSafe(const RefCountedThreadSafe&) = delete;
  RefCountedThreadSafe& operator=(const RefCountedThreadSafe&) = delete;

  void AddRef() const { ::base::subtle::RefCountedThreadSafeBase::AddRef(); }

  void Release() const {
    if (::base::subtle::RefCountedThreadSafeBase::Release()) {
      delete this;
    }
  }

 protected:
  virtual ~RefCountedThreadSafe() = default;
};

}  // namespace cliff

#endif  // CLIFF_SHIM_MEMORY_REF_COUNTED_H_
