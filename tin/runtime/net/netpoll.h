// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_NET_NETPOLL_H_
#define TIN_RUNTIME_NET_NETPOLL_H_
#include <string>

#include "tin/runtime/net/poll_descriptor.h"

namespace tin::runtime {

const uintptr_t kPdReady = 1;
const uintptr_t kPdWait = 2;

void NetPollInit();

bool NetPollInited();

void NetPollPostInit();

// Go 1.15 runtime/netpoll.go:18 — returns the count of goroutines that
// have ever committed to blocking on network I/O. Used by FindRunnable
// as a heuristic to decide whether to do a non-blocking NetPoll.
uint32_t NetPollWaiters();

void NetPollShutdown();

void NetPollPreDeinit();

void NetPollDeinit();

int32_t NetPollOpen(uintptr_t fd, PollDescriptor* pd);

int32_t NetPollClose(uintptr_t fd);

void NetPollArm(PollDescriptor* pd, int mode);

// Go 1.15 netpoll_epoll.go:106 — poll for ready network Gs.
//   delay_ns < 0  → block indefinitely
//   delay_ns == 0  → non-blocking poll
//   delay_ns > 0   → block at most delay_ns nanoseconds
G* NetPoll(int64_t delay_ns);

// Wake up a blocked NetPoll call. Used by WakeNetPoller (timer add)
// and NetPollShutdown to interrupt epoll_wait / kevent / IOCP.
void NetPollBreak();

int NetPollCheckErr(PollDescriptor* pd, int32_t mode);

bool NetPollBlock(PollDescriptor* pd, int32_t mode, bool waitio);

G* NetPollUnblock(PollDescriptor* pd, int32_t mode, bool ioready);

void NetPollReady(G** gpp, PollDescriptor* pd, int32_t mode);

void NetpollDeadline(void* arg, uintptr_t seq);

void NetpollReadDeadline(void* arg, uintptr_t seq);

void NetPollWriteDeadline(void* arg, uintptr_t seq);

}  // namespace tin::runtime
#endif  // TIN_RUNTIME_NET_NETPOLL_H_
