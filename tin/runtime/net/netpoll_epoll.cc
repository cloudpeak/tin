// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/epoll.h>
#include <fcntl.h>
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/posix_util.h"
#include "tin/runtime/net/netpoll.h"

namespace {
int epfd = -1;
}

namespace tin {
namespace runtime {

struct PollDescriptor;

void NetPollInit() {
  epfd = epoll_create(1024);
  DCHECK_NE(epfd, -1);
  if (epfd >= 0) {
    DCHECK_EQ(tin::Cloexec(epfd, true), 0);
    return;
  }
  LOG(FATAL) << "epoll_create failed";
}

void NetPollShutdown() {
}

void NetPollPreDeinit() {
}

#ifndef EPOLLRDHUP
#define EPOLLRDHUP 0x2000
#endif

int32 NetPollOpen(uintptr_t fd, PollDescriptor* pd) {
  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;
  ev.data.ptr = pd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, static_cast<int>(fd), &ev) == -1)
    return errno;
  return 0;
}

int32 NetPollClose(uintptr_t fd) {
  struct epoll_event ev;
  if (epoll_ctl(epfd, EPOLL_CTL_DEL,  static_cast<int>(fd), &ev) == -1)
    return errno;
  return 0;
}

void NetPollArm(PollDescriptor* pd, int mode) {
  LOG(FATAL) << "unused";
}

G* NetPoll(bool block) {
  if (epfd == -1)
    return NULL;
  int waitms = -1;
  epoll_event events[128];  // 1536 bytes on stack.
  while (true) {
    int n =
      HANDLE_EINTR(epoll_wait(epfd, &events[0], arraysize(events), waitms));
    if (n < 0) {
      LOG(FATAL) << "epoll_wait, fatal error, error code: " << errno;
    }
    G* gp = NULL;
    for (int i = 0; i < n; ++i) {
      epoll_event& ev = events[i];
      if (ev.events == 0) {
        continue;
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
    if (!block || gp != NULL) {
      return gp;
    }
  }
}

}  // namespace runtime
}  // namespace tin
