// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>


#include "tin/runtime/runtime.h"
#include "tin/runtime/posix_util.h"
#include "tin/runtime/net/NetPoll.h"

namespace {
int kq = -1;
}

namespace tin {
namespace runtime {

struct PollDescriptor;

void NetPollInit() {
  kq = kqueue();
  DCHECK_NE(kq, -1);
  if (kq >= 0) {
    DCHECK_EQ(tin::Cloexec(kq, true), 0);
    return;
  }
  LOG(FATAL) << "kqueue failed";
}

void NetPollShutdown() {
}

void NetPollPreDeinit() {
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
  int n = kevent(kq, &ev[0], 2, NULL, 0, NULL);
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

G* NetPoll(bool block) {
  if (kq == -1) {
    return NULL;
  }
  struct timespec* tp = NULL;
  struct timespec ts;
  memset(&ts, 0, sizeof(ts));
  if (!block) {
    tp = &ts;
  }
  struct kevent events[64];
  while (true) {
    int n =
      HANDLE_EINTR(kevent(kq, NULL, 0, &events[0], arraysize(events), tp));
    if (n == -1) {
      LOG(FATAL) << "kevent failed";
    }

    G* gp = NULL;
    for (int i = 0; i < n; ++i) {
      struct kevent& ev = events[i];
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
    if (!block || gp != NULL) {
      return gp;
    }
  }
  return NULL;
}

}  // namespace runtime
}  // namespace tin
