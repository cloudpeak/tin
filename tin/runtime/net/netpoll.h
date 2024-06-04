// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>

#include "tin/runtime/net/poll_descriptor.h"

namespace tin {
namespace runtime {

const uintptr_t kPdReady = 1;
const uintptr_t kPdWait = 2;

void NetPollInit();

bool NetPollInited();

void NetPollPostInit();

void NetPollShutdown();

void NetPollPreDeinit();

void NetPollDeinit();

int32_t NetPollOpen(uintptr_t fd, PollDescriptor* pd);

int32_t NetPollClose(uintptr_t fd);

void NetPollArm(PollDescriptor* pd, int mode);

G* NetPoll(bool block);

int NetPollCheckErr(PollDescriptor* pd, int32_t mode);

bool NetPollBlock(PollDescriptor* pd, int32_t mode, bool waitio);

G* NetPollUnblock(PollDescriptor* pd, int32_t mode, bool ioready);

void NetPollReady(G** gpp, PollDescriptor* pd, int32_t mode);

void NetpollDeadline(void* arg, uintptr_t seq);

void NetpollReadDeadline(void* arg, uintptr_t seq);

void NetPollWriteDeadline(void* arg, uintptr_t seq);

}  // namespace runtime
}  // namespace tin
