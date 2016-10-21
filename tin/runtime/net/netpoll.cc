// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include "tin/sync/atomic.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/greenlet.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/net/netpoll.h"

namespace tin {
namespace runtime {

namespace {
uint32 net_poll_Inited = 0;
}

bool NetPollInited() {
  return atomic::acquire_load32(&net_poll_Inited) != 0;
}

void NetPollPostInit() {
  atomic::store32(&net_poll_Inited, 1);
}

void NetPollDeinit() {
  NetPollPreDeinit();
  atomic::store32(&net_poll_Inited, 0);
}

void NetPollReady(G** gpp, PollDescriptor* pd, int32 mode) {
  G* rg = 0;
  G* wg = 0;

  if (mode == 'r' || mode == 'r' + 'w') {
    rg = NetPollUnblock(pd, 'r', true);
  }
  if (mode == 'w' || mode == 'r' + 'w') {
    wg = NetPollUnblock(pd, 'w', true);
  }

  if (rg != NULL) {
    rg->SetSchedLink(*gpp);
    *gpp = rg;
  }
  if (wg != NULL) {
    wg->SetSchedLink(*gpp);
    *gpp = wg;
  }
}

int NetPollCheckErr(PollDescriptor* pd, int32 mode) {
  if (pd->closing) {
    return 1;  // errClosing
  }
  if ((mode == 'r' && pd->rd < 0) || (mode == 'w' && pd->wd < 0)) {
    return 2;  // errTimeout
  }
  return 0;
}

bool NetPollBlockCommit(void* arg1, void* arg2) {
  uintptr_t gp = reinterpret_cast<uintptr_t>(arg1);
  uintptr_t* gpp = reinterpret_cast<uintptr_t*>(arg2);
  return atomic::release_cas(gpp, kPdWait, gp);
}

bool NetPollBlock(PollDescriptor* pd, int32 mode, bool waitio) {
  uintptr_t* gpp = &pd->rg;
  if (mode == 'w') {
    gpp = &pd->wg;
  }

  while (true) {
    uintptr_t old = *gpp;
    if (old == kPdReady) {
      *gpp = 0;
      return true;
    }
    if (old != 0) {
      LOG(FATAL) << "NetPollBlock: double wait";
    }
    if (atomic::release_cas(gpp, 0, kPdWait)) {
      break;
    }
  }

  if (waitio || NetPollCheckErr(pd, mode) == 0) {
    G* gp = GetG();
    Park(NetPollBlockCommit, gp, gpp);
  }

  uintptr_t old = atomic::exchange(gpp, 0);
  if (old > kPdWait) {
    LOG(FATAL) << "NetPollBlock: corrupted state";
  }
  return old == kPdReady;
}

G* NetPollUnblock(PollDescriptor* pd, int32 mode, bool ioready) {
  uintptr_t* gpp = &pd->rg;
  if (mode == 'w') {
    gpp = &pd->wg;
  }

  while (true) {
    uintptr_t old = *gpp;
    if (old == kPdReady) {
      return NULL;
    }
    if (old == 0 && !ioready) {
      // Only set READY for ioready. runtime_pollWait
      // will check for timeout/cancel before waiting.
      return NULL;
    }
    uintptr_t new_value = 0;
    if (ioready) {
      new_value = kPdReady;
    }

    if (atomic::release_cas(gpp, old, new_value)) {
      if (old == kPdReady || old == kPdWait) {
        old = 0;
      }
      return GpCastBack(old);
    }
  }
}

void NetPollDeadlineImpl(PollDescriptor* pd, uintptr_t seq, bool read,
                         bool write) {
  pd->lock.Lock();
  if (seq != pd->seq) {
    pd->lock.Unlock();
    pd->Release();
    return;
  }
  G* rg = NULL;
  if (read) {
    if (pd->rd <= 0 || pd->rt.f == NULL) {
      LOG(FATAL) << "NetPollDeadlineImpl: inconsistent read deadline";
    }
    pd->rd = -1;
    // full memory barrier between store to rd and load of rg in NetPollUnblock
    atomic::store(reinterpret_cast<uintptr_t*>(&pd->rt.f), 0);
    rg = NetPollUnblock(pd, 'r', false);
  }

  G* wg = NULL;
  if (write) {
    if ((pd->wd <= 0 || pd->wt.f == NULL) && !read) {
      LOG(FATAL) << "NetPollDeadlineImpl: inconsistent write deadline";
    }
    pd->wd = -1;
    // full memory barrier between store to rd and load of wg in NetPollUnblock
    atomic::store(reinterpret_cast<uintptr_t*>(&pd->wt.f), 0);
    wg = NetPollUnblock(pd, 'w', false);
  }
  pd->lock.Unlock();
  if (rg != NULL) {
    Ready(rg);
  }
  if (wg != NULL) {
    Ready(wg);
  }
  pd->Release();
}

void NetpollDeadline(void* arg, uintptr_t seq) {
  NetPollDeadlineImpl(static_cast<PollDescriptor*>(arg), seq, true, true);
}

void NetpollReadDeadline(void* arg, uintptr_t seq) {
  NetPollDeadlineImpl(static_cast<PollDescriptor*>(arg), seq, true, false);
}

void NetPollWriteDeadline(void* arg, uintptr_t seq) {
  NetPollDeadlineImpl(static_cast<PollDescriptor*>(arg), seq, false, true);
}

}  // namespace runtime
}  // namespace tin
