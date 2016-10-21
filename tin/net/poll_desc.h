// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <string>

namespace tin {

// forward declaration.
namespace runtime {
struct PollDescriptor;
}

namespace net {

class PollDesc {
 public:
  PollDesc()
    : runtime_ctx_(0) {
  }

  ~PollDesc() {
  }

  int Init(uintptr_t sysfd);
  void Close();
  void Evict();
  int Prepare(int mode);
  int PrepareRead();
  int PrepareWrite();
  int Wait(int mode);
  int WaitRead();
  int WaitWrite();
  void WaitCanceled(int mode);
  void WaitCanceledRead();
  void WaitCanceledWrite();
  inline tin::runtime::PollDescriptor* Desc() {
    return reinterpret_cast<tin::runtime::PollDescriptor*>(runtime_ctx_);
  }
  uintptr_t DescAsUintptr() {
    return runtime_ctx_;
  }

 private:
  uintptr_t runtime_ctx_;
};

}  // namespace net
}  // namespace tin






