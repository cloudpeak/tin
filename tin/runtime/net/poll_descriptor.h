// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cliff/memory/ref_counted.h>

#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/timer/timer_queue.h"
#include <atomic>


namespace tin::runtime {


 struct PollDescriptor : public cliff::RefCountedThreadSafe {
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





