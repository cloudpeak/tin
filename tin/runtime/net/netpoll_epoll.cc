// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include <absl/base/macros.h>
#include <absl/log/log.h>
#include <absl/log/check.h>

#include "base/posix/eintr_wrapper.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/posix_util.h"
#include "tin/runtime/net/netpoll.h"

namespace {
int epfd = -1;

// Break pipe: writing one byte wakes up a blocked epoll_wait.
// rd is added to the epoll set; wr is written by NetPollBreak.
int g_break_rd = -1;
int g_break_wr = -1;

// Sentinel stored in epoll_event.data.ptr to identify break events.
// A valid PollDescriptor* is always aligned (at least 2), so 1 is safe.
constexpr uintptr_t kNetpollBreak = 1;
}  // namespace

namespace tin::runtime {

struct PollDescriptor;

void NetPollInit() {
  epfd = epoll_create(1024);
  DCHECK_NE(epfd, -1);
  if (epfd < 0) {
    LOG(FATAL) << "epoll_create failed";
  }
  DCHECK_EQ(tin::Cloexec(epfd, true), 0);

  // Create break pipe (Go 1.15 netpoll_epoll.go:42-58).
  int pipefd[2];
  if (pipe(pipefd) != 0) {
    LOG(FATAL) << "NetPollInit: pipe failed";
  }
  g_break_rd = pipefd[0];
  g_break_wr = pipefd[1];
  DCHECK_EQ(tin::Cloexec(g_break_rd, true), 0);
  DCHECK_EQ(tin::Cloexec(g_break_wr, true), 0);
  // Non-blocking read end so we can drain without deadlocking.
  int flags = fcntl(g_break_rd, F_GETFL, 0);
  fcntl(g_break_rd, F_SETFL, flags | O_NONBLOCK);

  // Register break rd with epoll.
  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.ptr = reinterpret_cast<void*>(kNetpollBreak);
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, g_break_rd, &ev) == -1) {
    LOG(FATAL) << "NetPollInit: epoll_ctl break fd failed: " << errno;
  }
}

void NetPollShutdown() {
  // Wake up any blocked NetPoll so shutdown can proceed.
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

#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif

int32_t NetPollOpen(uintptr_t fd, PollDescriptor* pd) {
  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
  ev.data.ptr = pd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, static_cast<int>(fd), &ev) == -1)
    return errno;
  return 0;
}

int32_t NetPollClose(uintptr_t fd) {
  struct epoll_event ev;
  if (epoll_ctl(epfd, EPOLL_CTL_DEL,  static_cast<int>(fd), &ev) == -1)
    return errno;
  return 0;
}

void NetPollArm(PollDescriptor* pd, int mode) {
  LOG(FATAL) << "unused";
}

// Go 1.15 netpoll_epoll.go:106-123
G* NetPoll(int64_t delay_ns) {
  if (epfd == -1)
    return nullptr;

  // Convert nanoseconds to milliseconds for epoll_wait.
  int waitms;
  if (delay_ns < 0) {
    waitms = -1;  // block indefinitely
  } else if (delay_ns == 0) {
    waitms = 0;   // non-blocking
  } else {
    waitms = static_cast<int>(delay_ns / 1000000);  // ns → ms
    if (waitms == 0) waitms = 1;  // at least 1ms for sub-ms delays
  }

  epoll_event events[128];  // 1536 bytes on stack.
  while (true) {
    int n =
      HANDLE_EINTR(epoll_wait(epfd, &events[0], ABSL_ARRAYSIZE(events), waitms));
    if (n < 0) {
      if (errno == EINTR) continue;
      LOG(FATAL) << "epoll_wait, fatal error, error code: " << errno;
    }

    // After the first iteration, switch to non-blocking (Go behavior:
    // only the first epoll_wait blocks; subsequent loops are draining).
    waitms = 0;

    G* gp = nullptr;
    for (int i = 0; i < n; ++i) {
      epoll_event& ev = events[i];
      if (ev.events == 0) {
        continue;
      }
      // Check for break event (netpoll_epoll.go:81-99).
      if (reinterpret_cast<uintptr_t>(ev.data.ptr) == kNetpollBreak) {
        char buf[16];
        // Drain the pipe — there may be multiple pending bytes.
        while (HANDLE_EINTR(read(g_break_rd, buf, sizeof(buf))) > 0) {
          // discard
        }
        continue;  // skip, not a ready G
      }
      int mode = 0;
      if ((ev.events & (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR)) != 0) {
        mode += 'r';
      }
      if ((ev.events & (EPOLLOUT | EPOLLHUP | EPOLLERR)) != 0) {
        mode += 'w';
      }
      if (mode != 0) {
        PollDescriptor* pd = static_cast<PollDescriptor*>(ev.data.ptr);
        NetPollReady(&gp, pd, mode);
      }
    }
    // Return if non-blocking, or if we got ready Gs, or if the first
    // (blocking) wait returned 0 events (timeout).
    if (gp != nullptr || delay_ns == 0 || n == 0) {
      return gp;
    }
    // Blocking mode with events but no ready Gs: loop to drain (non-blocking now).
  }
}

}  // namespace tin::runtime
