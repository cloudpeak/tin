// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/synchronization/once.h"
#include "tin/error/error.h"
#include "tin/net/net.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/net/pollops.h"
#include "tin/net/poll_desc.h"

namespace tin {
namespace net {

namespace {
int ConvertErr(int res) {
  switch (res) {
  case 0:
    return 0;
  case 1:
    return TIN_ECLOSE_INTR;
  case 2:
    return TIN_ETIMEOUT_INTR;
  }
  LOG(FATAL) << "unreachable error.";
  // unreachable.
  return 0;
}
}  // namespace

base::OnceType server_init = ONCE_INIT;

int PollDesc::Init(uintptr_t sysfd) {
  base::CallOnce(&server_init, tin::runtime::pollops::ServerInit);
  int error_no = 0;
  runtime::PollDescriptor* ctx = runtime::pollops::Open(sysfd, &error_no);
  if (error_no == 0) {
    runtime_ctx_ = reinterpret_cast<uintptr_t>(ctx);
  }
  return error_no;
}

void PollDesc::Close() {
  if (runtime_ctx_ == 0) {
    return;
  }
  runtime::pollops::Close(Desc());
  runtime_ctx_ = 0;
}

void PollDesc::Evict() {
  if (runtime_ctx_ == 0) {
    return;
  }
  runtime::pollops::Unblock(Desc());
}

int PollDesc::Prepare(int mode) {
  int res = runtime::pollops::Reset(Desc(), mode);
  return ConvertErr(res);
}

int PollDesc::PrepareRead() {
  return Prepare('r');
}

int PollDesc::PrepareWrite() {
  return Prepare('w');
}

int PollDesc::Wait(int mode) {
  int res = runtime::pollops::Wait(Desc(), mode);
  return ConvertErr(res);
}

int PollDesc::WaitRead() {
  return Wait('r');
}

int PollDesc::WaitWrite() {
  return Wait('w');
}

void PollDesc::WaitCanceled(int mode) {
  runtime::pollops::WaitCanceled(Desc(), mode);
}

void PollDesc::WaitCanceledRead() {
  WaitCanceled('r');
}

void PollDesc::WaitCanceledWrite() {
  WaitCanceled('w');
}

}  // namespace net
}  // namespace tin
