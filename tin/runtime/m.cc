// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include <absl/functional/bind_front.h>

#include "context/zcontext.h"
#include "tin/sync/atomic.h"
#include "tin/config/config.h"
#include "tin/runtime/util.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/greenlet.h"
#include "tin/runtime/p.h"
#include "tin/runtime/scheduler.h"

#include "tin/runtime/m.h"

namespace tin {
namespace runtime {

M::M()
  : next_waitm_(0)
  , cache_()
  , wait_sema_()
  , park_()
  , p_(0)
  , spinning_(false)
  , schedlink_(0)
  , nextp_(NULL)
  , curg_(NULL)
  , g0_(0)
  , locked_g_(NULL)
  , mstart_fn_()
  , sys_context_(NULL)
  , unlock_info_(new UnLockInfo)
  , is_m0_(0)
  , dead_queue_()
  , locked_(0) {
}

M::~M() {
  delete g0_;
}

void M::EnsureSemaphoreExists() {
  if (!wait_sema_) {
    wait_sema_.reset(new absl::Notification(false));
  }
}

void* M::G0StaticProc(intptr_t args) {
  M* m = reinterpret_cast<M*>(args);
  return m->G0Proc();
}

void* M::G0Proc() {
  if (mstart_fn_)
    mstart_fn_();
  if (nextp_ != NULL && !IsM0()) {
    AcquireP(nextp_);
    nextp_ = NULL;
    sched->G0Loop();
  }
  jump_zcontext(g0_->MutableContext(), sys_context_, 0);
  return NULL;
}

M* M::Allocate(tin::runtime::P* p) {
  M* m = new M;
  if (reinterpret_cast<uintptr_t>(m) % 2 != 0) {
    LOG(FATAL) << "m's address is not power of 2";
  }
  return m;
}

void M::OnSysThreadStart() {
}

void M::OnSysThreadStop() {
  srand(static_cast<unsigned>(time(NULL)));
}

void M::ThreadMain() {
  OnSysThreadStart();

  g0_ = Greenlet::Create(G0StaticProc,
                         NULL,
                         true,
                         reinterpret_cast<intptr_t>(this),
                         false,
                         rtm_conf->StackSize(),
                         "sysg0");
  g0_->SetM(this);
  glet_tls = g0_;
  // switch to g0
  jump_zcontext(&sys_context_,
                *g0_->MutableContext(),
                reinterpret_cast<intptr_t>(g0_));
  // switch back from g0.
  OnSysThreadStop();
}

void mspinning() {
  // startm's caller incremented nmspinning. Set the new M's spinning.
  GetM()->SetSpinning(true);
}

M* M::New(std::function<void()> fn, tin::runtime::P* p) {
  M* m = Allocate(p);
  m->nextp_ = p;
  std::swap(m->mstart_fn_, fn);
  std::thread t(&M::ThreadMain, m);
  m->sys_thread_handle_ = std::move(t);
  return m;
}

void M::Join() {
  if (sys_thread_handle_.joinable()) {
    sys_thread_handle_.join();
  }
}

void M::Start(tin::runtime::P* p, bool spinning) {
  M* m = NULL;
  {
    SchedulerLocker guard;
    sched->MGetForP(p, spinning, &p, &m);
  }

  if (p == NULL)
    return;

  if (m == NULL) {
    std::function<void()> closure;
    if (spinning) {
      // closure = base::Bind(&mspinning);
      closure = absl::bind_front(&mspinning);
    }
    M::New(closure, p);
    return;
  }
  if (m->GetSpinning()) {
    LOG(FATAL) << "StartM: m is spinning";
  }
  if (m->nextp_ != NULL) {
    LOG(FATAL) << "startm: m has p";
  }
  if (spinning && !p->RunqEmpty()) {
    LOG(FATAL) << "startm: p has runnable gs";
  }

  m->SetSpinning(spinning);
  m->nextp_ = p;
  m->park_.Wakeup();
}

void M::StartLocked(G* gp) {
  G* curg = GetG();
  M* mlocked = gp->LockedM();
  if (mlocked == curg->M()) {
    LOG(FATAL) << "M::StartLocked: locked to me";
  }
  if (mlocked->nextp_ != 0) {
    LOG(FATAL) << "M::StartLocked: m has p";
  }
  AliasP* p = ReleaseP();
  mlocked->nextp_ = p;
  mlocked->park_.Wakeup();

  M::Stop();
}

void M::Stop() {
  G* gp = GetG();
  M* m = gp->M();

  if (m->P() != NULL) {
    LOG(FATAL) << "Stop m holding p";
  }
  if (m->GetSpinning()) {
    LOG(FATAL) << "Stop m  spinning";
  }
  {
    SchedulerLocker guard;
    if (rtm_env->ExitFlag()) {
      return;
    }
    sched->MPut(m);
  }
  m->park_.Sleep();
  m->park_.Clear();
  AcquireP(m->nextp_);
  m->nextp_ = NULL;
}

void M::StopLocked() {
  G* curg = GetG();
  M* curm = curg->M();
  if (curm->LockedG() == NULL || curm->LockedG()->M() != curm) {
    LOG(FATAL) << "M::StopLocked: inconsistent locking";
  }

  if (curm->P() != NULL) {
    sched->HandoffP(ReleaseP());
  }

  curm->park_.Sleep();
  curm->park_.Clear();
  AcquireP(curm->nextp_);
  curm->nextp_ = NULL;
}

void M::ClearDeadQueue() {
  if (!dead_queue_.empty()) {
    G* gp = dead_queue_.front();
    delete gp;
    dead_queue_.pop_front();
  }
}

// -----------------------------------------------------


}  // namespace runtime
}  // namespace tin
