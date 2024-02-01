// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


//#include "base/memory/aligned_memory.h"

#include <thread>
#include <cstdlib>

#include "context/zcontext.h"
#include "tin/sync/atomic.h"
#include "tin/runtime/greenlet.h"
#include "tin/runtime/p.h"
#include "tin/runtime/m.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/net/netpoll.h"

#include "tin/runtime/scheduler.h"

namespace tin {
namespace runtime {
const int kTinProcsLimit = 256;

bool ExitSyscallUnlockFunc(void* arg1, void* arg2);

Scheduler::Scheduler()
  : runq_size_(0)
  , idlep_(0)
  , nr_idlep_(0)
  , nr_spinning_(0)
  , idlem_(0)
  , nr_idlem_(0)
  , nr_idlem_locked_(0)
  , mcount_(0)
  , max_mcount_(10000)
  , last_poll_(0) {
  last_poll_ = static_cast<uint32>(MonoNow() / tin::kMillisecond);
  if (last_poll_ == 0)
    last_poll_ = 1;
  allp_ = new P*[kTinProcsLimit];
  for (int i = 0; i < kTinProcsLimit; i++) {
    allp_[i] = NULL;
  }
}
#include <cstdio>
#include <cstdlib>
// call it after world is stopped.
P* Scheduler::ResizeProc(int nprocs) {
  int old = rtm_conf->MaxProcs();
  if (old < 0 || old > kTinProcsLimit || nprocs <= 0 ||
      nprocs > kTinProcsLimit) {
    LOG(FATAL) << "ResizeProc: invalid arg";
  }

  for (int i = 0; i < nprocs; ++i) {
    P* pp = allp_[i];
    if (pp == NULL) {
      // placement new, cache-line alignment.
      void* ptr = _aligned_malloc(sizeof(P), 64); // TODO
      pp = new(ptr) P(i);
      atomic::store(reinterpret_cast<uintptr_t*>(&allp_[i]),
                    reinterpret_cast<uintptr_t>(pp));
    }
  }

  for (int i = nprocs; i < old; i++) {
    P* p = Allp()[i];
    while (1) {
      G* gp = p->RunqGet();
      if (gp == NULL)
        break;
      GlobalRunqPutHead(gp);
    }

    p->SetStatus(kPdead);
  }

  if (GetP() != NULL && GetP()->Id() < nprocs) {
    GetP()->SetStatus(kPrunning);
  } else {
    if (GetP() != NULL) {
      GetP()->SetM(NULL);
    }
    P* p = Allp()[0];
    p->SetM(NULL);
    p->SetStatus(kPidle);
    AcquireP(p);
  }

  P* runnable_ps = NULL;
  for (int i = nprocs - 1; i >= 0; i--) {
    P* p = Allp()[i];
    if (GetP() == p) {
      continue;
    }
    p->SetStatus(kPidle);
    if (p->RunqEmpty()) {
      PIdlePut(p);
    } else {
      p->SetM(MGet());
      p->SetLink(runnable_ps);
      runnable_ps = p;
    }
  }
  rtm_conf->SetMaxProcs(nprocs);
  return runnable_ps;
}



// Put gp on the global runnable queue.
// Sched must be locked.
void Scheduler::GlobalRunqPut(G* gp) {
  gp->SetSchedLink(NULL);
  if (!runq_tail_.IsNull()) {
    runq_tail_.Pointer()->SetSchedLink(gp);
  } else {
    runq_head_ = gp;
  }
  runq_tail_ = gp;
  runq_size_++;
}

// Put gp at the head of the global runnable queue.
// Sched must be locked.

void Scheduler::GlobalRunqPutHead(G* gp) {
  gp->SetSchedLink(runq_head_.Pointer());
  runq_head_ = gp;
  if (runq_tail_.IsNull()) {
    runq_tail_ = gp;
  }
  runq_size_++;
}

// Put a batch of runnable goroutines on the global runnable queue.
// Sched must be locked.

void Scheduler::GlobalRunqBatch(G* ghead, G* gtail, int32_t n) {
  gtail->SetSchedLink(NULL);
  if (!runq_tail_.IsNull()) {
    runq_tail_.Pointer()->SetSchedLink(ghead);
  } else {
    runq_head_ = ghead;
  }
  runq_tail_ = gtail;
  runq_size_ += n;
}

// Try get a batch of G's from the global runnable queue.
// Sched must be locked.
G* Scheduler::GlobalRunqGet(P* p, int32_t maximium) {
  if (runq_size_ == 0) {
    return NULL;
  }

  int32_t n = runq_size_ / rtm_conf->MaxProcs() + 1;
  if (n > runq_size_) {
    n = runq_size_;
  }
  if (maximium > 0 && n > maximium) {
    n = maximium;
  }
  if (n > p->RunqCapacity() / 2) {
    n = p->RunqCapacity() / 2;
  }

  runq_size_ -= n;
  if (runq_size_ == 0) {
    runq_tail_ = static_cast<void*>(0);
  }

  G* gp = runq_head_.Pointer();
  runq_head_ = gp->SchedLink();
  n--;
  for (; n > 0; n--) {
    G* gp1 = runq_head_.Pointer();
    runq_head_ = gp1->SchedLink();
    p->RunqPut(gp1, false);
  }

  return gp;
}

void Scheduler::InjectGList(G* glist) {
  if (glist == NULL) {
    return;
  }
  int n = 0;
  {
    RawMutexGuard guard(&lock_);
    for (n = 0; glist != NULL; n++) {
      G* gp = glist;
      glist = GpCastBack(gp->SchedLink());
      gp->SetState(GLET_RUNNABLE);
      GlobalRunqPut(gp);
    }
  }

  for ( ; n != 0; n--) {
    StartM(NULL, false);
  }
}

// Put p to on _Pidle list.
// Sched must be locked.
void Scheduler::PIdlePut(P* p) {
  if (!p->RunqEmpty()) {
    LOG(FATAL) << "pidleput: P has non-empty run queue";
  }
  p->SetLink(idlep_);
  idlep_ = p;
  atomic::relaxed_Inc32(&nr_idlep_, 1);
}

P* Scheduler::PIdleGet() {
  P* p = idlep_;
  if (p != NULL) {
    idlep_ = p->Link();
    atomic::relaxed_Inc32(&nr_idlep_, -1);
    if (!p->RunqEmpty()) {
      LOG(FATAL) << "pidleput: P has non-empty run queue";
    }
  }
  return p;
}

G* Scheduler::FindRunnable(bool* inherit_time) {
  G* curg = GetG();
  M* curm = curg->M();

top:
  P* curp = curm->P();
  G* gp = curp->RunqGet(inherit_time);
  if (gp != NULL) {
    return gp;
  }

  if (atomic::relaxed_load32(&runq_size_) != 0) {
    {
      RawMutexGuard guard(&lock_);
      gp = GlobalRunqGet(curp, 0);
    }
    if (gp != NULL) {
      *inherit_time = false;
      return gp;
    }
  }

  if (rtm_env->ExitFlag()) {
    return NULL;
  }

  if (NetPollInited() && last_poll_ != 0) {
    gp = NetPoll(false);
    if (gp != 0) {
      InjectGList(GpCastBack(gp->SchedLink()));
      gp->SetState(GLET_RUNNABLE);
      *inherit_time = false;
      return gp;
    }
  }

  // If number of spinning M's >= number of busy P's, block.
  // This is necessary to prevent excessive CPU consumption
  // when GOMAXPROCS>>1 but the program parallelism is low.

  if (!curm->GetSpinning() && (2 * atomic::relaxed_load32(&nr_spinning_)) >=
      (static_cast<uint32>(rtm_conf->MaxProcs()) -
       atomic::relaxed_load32(&nr_idlep_))) {
    goto stop;
  }

  if (!curm->GetSpinning()) {
    curm->SetSpinning(true);
    atomic::Inc32(&nr_spinning_, 1);
  }

  for (int i = 0;
       i < static_cast<int>(4 * rtm_conf->MaxProcs());
       i++) {
    P* p = Allp()[rand() % rtm_conf->MaxProcs()];
    if (p == curp) {
      gp = p->RunqGet();
    } else {
      bool steal_run_next = i > 2 * static_cast<int>(rtm_conf->MaxProcs());
      gp = curp->RunqSteal(p, steal_run_next);
    }
    if (gp != NULL) {
      *inherit_time = false;
      return gp;
    }
  }

stop: {
    RawMutexGuard guard(&lock_);
    if (atomic::relaxed_load32(&runq_size_) != 0) {
      gp = GlobalRunqGet(curp, 0);
      *inherit_time = false;
      return gp;
    }

    P* p = ReleaseP();
    PIdlePut(p);
  }

  bool was_spinning = curm->GetSpinning();
  if (curm->GetSpinning()) {
    curm->SetSpinning(false);
    if (atomic::Inc32(&nr_spinning_, -1) < 0) {
      LOG(FATAL) << "FindRunnable: negative nmspinning";
    }
  }

  for (int i = 0; i < rtm_conf->MaxProcs(); i++) {
    P* p = Allp()[i];
    if (p != NULL && !p->RunqEmpty()) {
      {
        RawMutexGuard guard(&lock_);
        p = PIdleGet();
      }
      if (p != NULL) {
        AcquireP(p);
        if (was_spinning) {
          curm->SetSpinning(true);
          atomic::Inc32(&nr_spinning_, 1);
        }
        goto top;
      }
    }
  }

  if (NetPollInited() && atomic::exchange32(&last_poll_, 0) != 0) {
    gp = NetPoll(true);
    uint32_t now = static_cast<uint32>(MonoNow() / tin::kMillisecond);
    if (now == 0)
      now = 1;
    atomic::relaxed_store32(&last_poll_, now);
    if (gp != NULL) {
      P* p = NULL;
      {
        RawMutexGuard guard(&lock_);
        p = PIdleGet();
      }
      if (p != NULL) {
        AcquireP(p);
        InjectGList(GpCastBack(gp->SchedLink()));
        gp->SetState(GLET_RUNNABLE);
        *inherit_time = false;
        return gp;
      }
      InjectGList(gp);
    }
  }

  M::Stop();
  if (rtm_env->ExitFlag()) {
    return NULL;
  }
  goto top;
}

void Scheduler::MPut(M* m) {
  m->SetSchedLink(idlem_);
  idlem_ = m;
  nr_idlem_++;
}

M* Scheduler::MGet() {
  M* mp = idlem_;
  if (idlem_ != NULL) {
    idlem_ = mp->GetSchedLink();
    nr_idlem_--;
  }
  return mp;
}

void Scheduler::MGetForP(P* curp, bool spinning, P** newp, M** newm) {
  if (curp == NULL) {
    curp = PIdleGet();
    if (curp == NULL) {
      if (spinning) {
        if (atomic::Inc32(&nr_spinning_, -1) < 0) {
          LOG(FATAL) << "negative nmspinning";
        }
      }
      return;
    }
  }
  *newp = curp;
  *newm = MGet();
}

void Scheduler::Reschedule() {
  G* curg = GetG();
  M* m = curg->M();

  if (m->LockedG() != NULL) {
    SwitchG(curg, m->G0(), GpCast(m->G0()));
    return;
  }

  G* nextg = m->G0();
  SwitchG(curg, nextg, GpCast(nextg));
  // switch back.
  OnSwitch(GetG());
}

bool Scheduler::OneRoundSched(G* curg) {
  while (true) {
    M* m = curg->M();
    P* p = m->P();
    bool inherit_time = false;
    G* nextg = NULL;

    // from global queue.
    if (p->SchedTick() % 61 == 0 && sched->runq_size_ > 0) {
      // Check the global runnable queue once in a while to ensure fairness.
      // Otherwise two goroutines can completely occupy the local runqueue
      // by constantly respawning each other.
      SchedulerLocker guard;
      nextg = sched->GlobalRunqGet(p, 1);
    }
    // from local queue.
    if (nextg == NULL) {
      nextg = p->RunqGet(&inherit_time);
      if (nextg != NULL && curg->M()->GetSpinning()) {
        LOG(FATAL) << "schedule: spinning with local work";
      }
    }
    // FindRunnable.
    if (nextg == NULL) {
      nextg = sched->FindRunnable(&inherit_time);
    }

    // still null?
    if (nextg == NULL) {
      // tin os power off
      return false;
    }

    if (m->GetSpinning()) {
      sched->ResetSpinning();
    }

    if (nextg->LockedM() != NULL) {
      M::StartLocked(nextg);
      continue;
    }

    if (!inherit_time) {
      p->IncSchedTick();
    }

    SwitchG(curg, nextg, GpCast(nextg));
    // switch back.

    while (true) {
      OnSwitch(curg);
      G* lockedg = m->LockedG();
      if (lockedg == NULL)
        break;
      M::StopLocked();
      SwitchG(curg, lockedg, GpCast(lockedg));
    }
    break;
  }
  return true;
}

void Scheduler::G0Loop() {
  M* m = GetG()->M();
  // note that curg always points to g0 in this function.
  G* curg = m->G0();

  // one round scheduler.
  while (true) {
    if (!OneRoundSched(curg))
      break;
  }
}

void Scheduler::OnSwitch(G* curg) {
  curg->M()->GetUnlockInfo()->Run();
  curg->M()->ClearDeadQueue();
}

int Scheduler::Init() {
  ResizeProc(std::thread::hardware_concurrency());
  return 0;
}

void Scheduler::MakeReady(G* gp) {
  if (gp->GetState() != GLET_WAITING) {
    LOG(FATAL) << "bad g->status in ready";
  }
  gp->SetState(GLET_RUNNABLE);

  GetP()->RunqPut(gp, true);

  if (atomic::load32(&nr_idlep_) != 0 && atomic::load32(&nr_spinning_) == 0) {
    WakeupP();
  }
}

void Scheduler::WakePIfNecessary() {
  if (atomic::load32(&nr_idlep_) != 0 && atomic::load32(&nr_spinning_) == 0) {
    WakeupP();
  }
}

void Scheduler::WakeupP() {
  if (!atomic::cas32(&nr_spinning_, 0, 1)) {
    return;
  }
  StartM(NULL, true);
}

void Scheduler::HandoffP(P* p) {
  // if it has local work, start it straight away
  if (!p->RunqEmpty() || sched->GlobalRunqSize() != 0) {
    StartM(p, false);
    return;
  }

  // no local work, check that there are no spinning/idle M's,
  // otherwise our help is not required
  if (atomic::load32(&nr_spinning_) + atomic::load32(&nr_idlep_) == 0 &&
      atomic::cas32(&nr_spinning_, 0, 1)) {
    StartM(p, true);
    return;
  }

  lock_.Lock();
  if (runq_size_ != 0) {
    lock_.Unlock();
    StartM(p, false);
    return;
  }

  if ((nr_idlep_ == static_cast<uint32>(rtm_conf->MaxProcs() - 1))
      && atomic::load32(&last_poll_) != 0) {
    lock_.Unlock();
    StartM(p, false);
    return;
  }
  PIdlePut(p);
  lock_.Unlock();
}

// should not called from g0.
void Scheduler::ExitSyscall0(G* gp) {
  M* curm = gp->M();
  gp->SetState(GLET_RUNNABLE);
  P* p = NULL;
  {
    RawMutexGuard guard(&lock_);
    p = PIdleGet();
  }

  if (p != NULL) {
    AcquireP(p);
    return;
  }

  // no P available, jump to g0;
  G* g0 = curm->G0();
  if (curm->LockedG() != NULL) {
    M::StopLocked();
  } else {
    curm->SetUnlockInfo(ExitSyscallUnlockFunc, gp, NULL, NULL);
    SwitchG(gp, g0, GpCast(g0));
  }
}

bool Scheduler::ExitSyscallFast() {
  G* gp = GetG();
  M* curm = gp->M();
  P* curp = curm->P();
  // Try to re-acquire the last P.
  if (curp != 0 && curp->GetStatus() != kPsyscall &&
      curp->CasStatus(kPsyscall, kPrunning)) {
    // There's a cpu for us, so we can run.
    curp->SetM(curm);
    return true;
  }

  // Try to get any other idle P.
  curm->SetP(NULL);
  if (idlep_ != NULL) {
    if (ExitSyscallPIdle()) {
      return true;
    }
  }
  return false;
}

bool Scheduler::ExitSyscallPIdle() {
  P* p = NULL;
  {
    RawMutexGuard guard(&lock_);
    p = PIdleGet();
  }
  if (p != NULL) {
    AcquireP(p);
    return true;
  }

  return false;
}

void Scheduler::ResetSpinning() {
  G* gp = GetG();
  if (!gp->M()->GetSpinning()) {
    LOG(FATAL) << "resetspinning: not a spinning m";
  }
  gp->M()->SetSpinning(false);
  uint32_t nr_spinning = atomic::Inc32(&nr_spinning_, -1);
  if (static_cast<int32>(nr_spinning) < 0) {
    LOG(FATAL) << "ResetSpinning: negative nr_spinning";
  }
  if (nr_spinning == 0 && atomic::load32(&nr_idlep_) > 0) {
    WakeupP();
  }
}

uint32_t Scheduler::LastPollTime() {
  return atomic::acquire_load32(&last_poll_);
}

void Scheduler::DoUnlock(UnLockInfo* info) {
  if (!info->F()(info->Arg1(), info->Arg2())) {
    GetP()->RunqPut(info->Owner(), false);
  }
  info->Clear();
}

// -------------------------------------------------------------

P* GetP() {
  return GetG()->M()->P();
}

M* GetM() {
  return GetG()->M();
}

P* ReleaseP() {
  G* curg = GetG();
  P* p = curg->M()->P();
  curg->M()->SetP(NULL);
  p->SetStatus(kPidle);
  p->SetM(NULL);
  return p;
}

void AcquireP(P* p) {
  G* curg = GetG();
  if (curg->M()->P() != NULL) {
    LOG(FATAL) << "AcquireP: already in go";
  }
  if (p->M() != NULL || p->GetStatus() != kPidle) {
    LOG(FATAL) << "AcquireP: invalid p state";
  }

  curg->M()->SetP(p);
  p->SetStatus(kPrunning);
  p->SetM(curg->M());
}

void StartM(P* p, bool spinning) {
  M::Start(p, spinning);
}

void DropG() {
  G* gp = GetG();
  M* m = gp->M();
  if (m->LockedG() == NULL) {
    m->CurG()->SetM(NULL);
    m->SetCurG(NULL);
  }
}

void ParkUnlock(RawMutex* lock) {
  Park(ParkUnlockF, lock, NULL);
}

void Park(UnlockFunc unlockf, void* arg1, void* arg2) {
  G* gp = GetG();
  M* mp = gp->M();
  if (gp->GetState() != GLET_RUNNING) {
    LOG(FATAL) << "gopark: bad g status";
  }
  mp->GetUnlockInfo()->Set(unlockf, arg1, arg2, gp);
  gp->SetState(GLET_WAITING);
  sched->Reschedule();
}

void Ready(G* gp) {
  sched->MakeReady(gp);
}

bool ParkUnlockF(void* arg1, void* arg2) {
  RawMutex* mutex = static_cast<RawMutex*>(arg1);
  mutex->Unlock();
  return true;
}

bool ExitSyscallUnlockFunc(void* arg1, void* arg2) {
  M* curm = GetG()->M();
  G* gp = static_cast<G*>(arg1);
  {
    SchedulerLocker guard;
    sched->GlobalRunqPut(gp);
  }
  curm->Stop();
  return true;
}

void EnterSyscallBlock() {
  G* gp = GetG();
  gp->SetState(GLET_SYSCALL);
  sched->HandoffP(ReleaseP());
}

void ExitSyscall() {
  G* gp = GetG();
  if (sched->ExitSyscallFast()) {
    gp->SetState(GLET_RUNNING);
    return;
  }
  sched->ExitSyscall0(gp);
}

void WakePIfNecessary() {
  sched->WakePIfNecessary();
}

void SwitchG(Greenlet* from, Greenlet* to, intptr_t args) {
  from->M()->SetCurG(to);
  to->SetM(from->M());
  SetG(to);
  to->SetState(GLET_RUNNING);
  jump_zcontext(from->MutableContext(), *to->MutableContext(), args);
}

// -------------------------------------------------------------


}  // namespace runtime
}  // namespace tin
