// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/timer/timer_queue.h"
#include <atomic>


namespace tin::runtime {

// Non-template RefCountedThreadSafe base, replacing the former
// cliff::RefCountedThreadSafe shim. Delete-on-zero via virtual dtor.
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

 struct PollDescriptor : public RefCountedThreadSafe {
  PollDescriptor();
  PollDescriptor(const PollDescriptor&) = delete;
  PollDescriptor& operator=(const PollDescriptor&) = delete;

  ~PollDescriptor() {
    // LOG(INFO) << "PollDescriptor destructor";
  }
  RawMutex lock;
  uintptr_t fd;
  bool closing;
  uintptr_t seq;

  uintptr_t rg;
  Timer rt;
  int64_t rd;

  uintptr_t wg;
  Timer wt;
  int64_t wd;

  uint32_t user;

};

inline PollDescriptor* NewPollDescriptor() {
  PollDescriptor* descriptor = new PollDescriptor;
  descriptor->AddRef();
  return descriptor;
}

} // namespace tin::runtime





