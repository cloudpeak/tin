// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "tin/sync/atomic.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/net/netpoll.h"
#include "tin/runtime/net/pollops.h"

namespace tin {
namespace runtime {
namespace pollops {

void ServerInit() {
  NetPollInit();
  NetPollPostInit();
}

void ServerNotifyShutdown() {
  NetPollShutdown();
}

void ServerDeinit() {
  NetPollDeinit();
}

PollDescriptor* Open(uintptr_t fd, int* error_no) {
  PollDescriptor* pd = NewPollDescriptor();
  pd->lock.Lock();
  if (pd->wg != 0 && pd->wg != kPdReady) {
    LOG(FATAL) << "netpollOpen: blocked write on free descriptor";
  }
  if (pd->rg != 0 && pd->rg != kPdReady) {
    LOG(FATAL) << "netpollOpen: blocked read on free descriptor";
  }
  pd->fd = fd;
  pd->closing = false;
  pd->seq++;
  pd->rg = 0;
  pd->rd = 0;
  pd->wg = 0;
  pd->wd = 0;
  pd->lock.Unlock();

  *error_no = NetPollOpen(fd, pd);
  return pd;
}

void Close(PollDescriptor* pd) {
  if (!pd->closing) {
    LOG(FATAL) << "netpollClose: close w/o unblock";
  }

  if (pd->wg != 0 && pd->wg != kPdReady) {
    LOG(FATAL) << "netpollOpen: blocked write on closing descriptor";
  }
  if (pd->rg != 0 && pd->rg != kPdReady) {
    LOG(FATAL) << "netpollOpen: blocked read on closing descriptor";
  }
  NetPollClose(pd->fd);
  pd->Release();
}

int Reset(PollDescriptor* pd, int mode) {
  int err = NetPollCheckErr(pd, mode);
  if (err != 0) {
    return err;
  }
  if (mode == 'r') {
    pd->rg = 0;
  } else if (mode == 'w') {
    pd->wg = 0;
  }
  return 0;
}

int Wait(PollDescriptor* pd, int mode) {
  int err = NetPollCheckErr(pd, mode);
  if (err != 0) {
    return err;
  }

  while (!NetPollBlock(pd, mode, false)) {
    err = NetPollCheckErr(pd, mode);
    if (err != 0) {
      return err;
    }
    // Can happen if timeout has fired and unblocked us,
    // but before we had a chance to run, timeout has been reset.
    // Pretend it has not happened and retry.
  }
  return 0;
}

void WaitCanceled(PollDescriptor* pd, int mode) {
  while (!NetPollBlock(pd, mode, true)) {
  }
}

void AddTimerRefCounted(PollDescriptor* pd, Timer* t) {
  pd->AddRef();
  timer_q->AddTimer(t);
}

void DelTimerRefCounted(PollDescriptor* pd, Timer* t) {
  if (timer_q->DelTimer(t))
    pd->Release();
}

void SetDeadline(PollDescriptor* pd, int64 d, int mode) {
  pd->lock.Lock();
  if (pd->closing) {
    pd->lock.Unlock();
    return;
  }
  pd->seq++;  // invalidate current timers
  // Reset current timers.
  if (pd->rt.f != NULL) {
    DelTimerRefCounted(pd, &pd->rt);
    pd->rt.f = NULL;
  }

  if (pd->wt.f != NULL) {
    DelTimerRefCounted(pd, &pd->wt);
    pd->wt.f = NULL;
  }

  if (d != 0 && d <= MonoNow()) {
    d = -1;
  }
  if (mode == 'r' || mode == 'r' + 'w') {
    pd->rd = d;
  }
  if (mode == 'w' || mode == 'r' + 'w') {
    pd->wd = d;
  }

  if (pd->rd > 0 && pd->rd == pd->wd) {
    pd->rt.f = NetpollDeadline;
    pd->rt.when = pd->rd;
    // Copy current seq into the timer arg.
    // Timer func will check the seq against current descriptor seq,
    // if they differ the descriptor was reused or timers were reset.
    pd->rt.arg = pd;
    pd->rt.seq = pd->seq;
    AddTimerRefCounted(pd, &pd->rt);
  } else {
    if (pd->rd > 0) {
      pd->rt.f = NetpollReadDeadline;
      pd->rt.when = pd->rd;
      pd->rt.arg = pd;
      pd->rt.seq = pd->seq;
      AddTimerRefCounted(pd, &pd->rt);
    }

    if (pd->wd > 0) {
      pd->wt.f = NetPollWriteDeadline;
      pd->wt.when = pd->wd;
      pd->wt.arg = pd;
      pd->wt.seq = pd->seq;
      AddTimerRefCounted(pd, &pd->wt);
    }
  }

  G* rg = NULL;
  G* wg = NULL;
  // full memory barrier between stores to rd/wd
  // and load of rg/wg in NetPollUnblock
  base::subtle::MemoryBarrier();
  if (pd->rd < 0) {
    rg = NetPollUnblock(pd, 'r', false);
  }
  if (pd->wd < 0) {
    wg = NetPollUnblock(pd, 'w', false);
  }
  pd->lock.Unlock();

  if (rg != NULL) {
    Ready(rg);
  }
  if (wg != NULL) {
    Ready(wg);
  }
}

void Unblock(PollDescriptor* pd) {
  pd->lock.Lock();
  if (pd->closing) {
    LOG(FATAL) << "netpollUnblock: already closing";
  }
  pd->closing = true;
  pd->seq++;

  G* rg = NULL;
  G* wg = NULL;
  // full memory barrier between stores to rd/wd
  // and load of rg/wg in NetPollUnblock
  base::subtle::MemoryBarrier();

  rg = NetPollUnblock(pd, 'r', false);
  wg = NetPollUnblock(pd, 'w', false);
  if (pd->rt.f != NULL) {
    DelTimerRefCounted(pd, &pd->rt);
    pd->rt.f = NULL;
  }

  if (pd->wt.f != NULL) {
    DelTimerRefCounted(pd, &pd->wt);
    pd->wt.f = NULL;
  }
  pd->lock.Unlock();

  if (rg != NULL) {
    Ready(rg);
  }
  if (wg != NULL) {
    Ready(wg);
  }
}

}  // namespace pollops
}  // namespace runtime
}  // namespace tin
