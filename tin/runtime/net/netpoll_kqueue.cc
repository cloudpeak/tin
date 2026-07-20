// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <errno.h>

#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctime>


#include "tin/runtime/runtime.h"
#include "tin/runtime/posix_util.h"
#include "tin/runtime/net/NetPoll.h"

namespace {
int kq = -1;

// Break pipe for kqueue (same pattern as epoll).
int g_break_rd = -1;
int g_break_wr = -1;
constexpr uintptr_t kNetpollBreak = 1;
}  // namespace

namespace tin::runtime {

struct PollDescriptor;

void NetPollInit() {
  kq = kqueue();
  DCHECK_NE(kq, -1);
  if (kq >= 0) {
    DCHECK_EQ(tin::Cloexec(kq, true), 0);

    // Create break pipe (Go 1.15 netpoll_kqueue.go equivalent).
    int pipefd[2];
    if (pipe(pipefd) != 0) {
      LOG(FATAL) << "NetPollInit: pipe failed";
    }
    g_break_rd = pipefd[0];
    g_break_wr = pipefd[1];
    DCHECK_EQ(tin::Cloexec(g_break_rd, true), 0);
    DCHECK_EQ(tin::Cloexec(g_break_wr, true), 0);
    int flags = fcntl(g_break_rd, F_GETFL, 0);
    fcntl(g_break_rd, F_SETFL, flags | O_NONBLOCK);

    // Register break rd with kqueue for read events.
    struct kevent ev;
    EV_SET(&ev, g_break_rd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0,
           reinterpret_cast<void*>(kNetpollBreak));
    kevent(kq, &ev, 1, nullptr, 0, nullptr);
    return;
  }
  LOG(FATAL) << "kqueue failed";
}

void NetPollShutdown() {
  NetPollBreak();
}

void NetPollPreDeinit() {
}

void NetPollBreak() {
  if (g_break_wr >= 0) {
    char c = 0;
    HANDLE_EINTR(write(g_break_wr, &c, 1));
  }
}

int32_t NetPollOpen(uintptr_t fd, PollDescriptor* pd) {
  struct kevent ev[2];
  ev[0].ident = fd;
  ev[0].filter = EVFILT_READ;
  ev[0].flags = EV_ADD | EV_CLEAR;
  ev[0].fflags = 0;
  ev[0].data = 0;
  ev[0].udata = pd;
  ev[1] = ev[0];
  ev[1].filter = EVFILT_WRITE;
  int n = kevent(kq, &ev[0], 2, nullptr, 0, nullptr);
  return n == -1 ? errno : 0;
}

int32_t NetPollClose(uintptr_t fd) {
  (void)fd;
  // Don't need to unregister because calling close()
  // on fd will remove any kevents that reference the descriptor.
  return 0;
}

void NetPollArm(PollDescriptor* pd, int mode) {
  LOG(FATAL) << "unused";
}

// Go 1.15 netpoll_kqueue.go equivalent.
G* NetPoll(int64_t delay_ns) {
  if (kq == -1) {
    return nullptr;
  }

  struct timespec ts;
  struct timespec* tp = nullptr;
  if (delay_ns < 0) {
    tp = nullptr;  // block indefinitely
  } else if (delay_ns == 0) {
    memset(&ts, 0, sizeof(ts));
    tp = &ts;  // non-blocking
  } else {
    ts.tv_sec = delay_ns / 1000000000LL;
    ts.tv_nsec = delay_ns % 1000000000LL;
    tp = &ts;
  }

  struct kevent events[64];
  while (true) {
    int n =
      HANDLE_EINTR(kevent(kq, nullptr, 0, &events[0], 64, tp));
    if (n == -1) {
      LOG(FATAL) << "kevent failed";
    }

    // After the first iteration, switch to non-blocking (draining).
    memset(&ts, 0, sizeof(ts));
    tp = &ts;

    G* gp = nullptr;
    for (int i = 0; i < n; ++i) {
      struct kevent& ev = events[i];
      // Check for break event.
      if (reinterpret_cast<uintptr_t>(ev.udata) == kNetpollBreak) {
        char buf[16];
        while (HANDLE_EINTR(read(g_break_rd, buf, sizeof(buf))) > 0) {
          // discard
        }
        continue;
      }
      int mode = 0;
      if (ev.filter == EVFILT_READ) {
        mode += 'r';
      }
      if (ev.filter == EVFILT_WRITE) {
        mode += 'w';
      }
      if (mode != 0) {
        PollDescriptor* pd = static_cast<PollDescriptor*>(ev.udata);
        NetPollReady(&gp, pd, mode);
      }
    }
    if (gp != nullptr || delay_ns == 0 || n == 0) {
      return gp;
    }
  }
  return nullptr;
}

}  // namespace tin::runtime
