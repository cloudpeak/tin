// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/timer/timer_queue.h"


namespace tin {
namespace runtime {

struct PollDescriptor : public base::RefCountedThreadSafe<PollDescriptor> {
  PollDescriptor();

  ~PollDescriptor() {
    // LOG(INFO) << "PollDescriptor destructor";
  }
  RawMutex lock;
  uintptr_t fd;
  bool closing;
  uintptr_t seq;

  uintptr_t rg;
  Timer rt;
  int64 rd;

  uintptr_t wg;
  Timer wt;
  int64 wd;

  uint32 user;

 private:
  DISALLOW_COPY_AND_ASSIGN(PollDescriptor);
};

inline PollDescriptor* NewPollDescriptor() {
  PollDescriptor* descriptor = new PollDescriptor;
  descriptor->AddRef();
  return descriptor;
}

}  // namespace runtime
}  // namespace tin




