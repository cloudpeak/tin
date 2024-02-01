// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

namespace tin {
namespace runtime {
struct PollDescriptor;

namespace pollops {

void ServerInit();
void ServerNotifyShutdown();
void ServerDeinit();
PollDescriptor* Open(uintptr_t fd, int* error_no);
int Wait(PollDescriptor* pd, int mode);
void Close(PollDescriptor* pd);
int Reset(PollDescriptor* pd, int mode);
void Unblock(PollDescriptor* pd);
void WaitCanceled(PollDescriptor* pd, int mode);
void SetDeadline(PollDescriptor* pd, int64_t d, int mode);

}  // namespace pollops
}  // namespace runtime
}  // namespace tin


