// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <cstdint>
#include <cstdlib>

#include "context/zcontext.h"
#include "tin/sync/atomic.h"
#include "tin/runtime/coroutine.h"
#include "tin/runtime/p.h"
#include "tin/runtime/m.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/net/netpoll.h"
#include "tin/runtime/timer/timer_queue.h"

#include "tin/runtime/scheduler.h"

namespace tin::runtime {
const int kTinProcsLimit = 256;

bool ExitSyscallUnlockFunc(void* arg1, void* arg2);

// Only steal timers from a P that isn't actively running (reduces lock
// contention on its timersLock). tin currently has no preempt flag, so
// we use the P status as a proxy.
namespace {
bool ShouldStealTimers(P* p2) {
  return p2->GetStatus() != kPrunning;
}
}  // namespace

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
  last_poll_ = static_cast<uint32_t>(MonoNow() / tin::kMillisecond);
  if (last_poll_ == 0)
    last_poll_ = 1;
  allp_ = new P*[kTinProcsLimit];
  for (int i = 0; i < kTinProcsLimit; i++) {
    allp_[i] = nullptr;
  }
}

// call it after world is stopped.
P* Scheduler::ResizeProc(int nprocs) {
  int old = rtm_conf->MaxProcs();
  if (old < 0 || old > kTinProcsLimit || nprocs <= 0 ||
      nprocs > kTinProcsLimit) {
    LOG(FATAL) << "ResizeProc: invalid arg";
  }

  for (int i = 0; i < nprocs; ++i) {
    P* pp = allp_[i];
    if (pp == nullptr) {
      // placement new, cache-line alignment.
#if defined(OS_WIN)
      void* ptr = _aligned_malloc(sizeof(P), 64);
#else
      // aligned_alloc requires size to be a multiple of alignment.
      size_t aligned_size = (sizeof(P) + 63) & ~size_t(63);
      void* ptr = aligned_alloc(64, aligned_size);
#endif
      pp = new(ptr) P(i);
      atomic::store(reinterpret_cast<uintptr_t*>(&allp_[i]),
                    reinterpret_cast<uintptr_t>(pp));
    }
  }

  for (int i = nprocs; i < old; i++) {
    P* p = Allp()[i];
    while (1) {
      G* gp = p->RunqGet();
      if (gp == nullptr)
        break;
      GlobalRunqPutHead(gp);
    }

    // === Per-P timer migration ===
    // Move all timers from the dying P to the current P's heap so they
    // keep firing. STW is in effect (caller holds sched->lock_ via the
    // ResizeProc path), so concurrent timer access is impossible.
    if (!p->Timers().empty()) {
      P* plocal = GetP();
      plocal->TimersLock().Lock();
      p->TimersLock().Lock();
      MoveTimers(plocal, p->Timers());
      p->SetTimer0When(0);
      // Reset counters on the dying P.
      p->IncNumTimers(-static_cast<int32_t>(p->NumTimers()));
      p->IncDeletedTimers(-static_cast<int32_t>(p->DeletedTimers()));
      p->IncAdjustTimers(-static_cast<int32_t>(p->AdjustTimers()));
      p->TimersLock().Unlock();
      plocal->TimersLock().Unlock();
    }

    p->SetStatus(kPdead);
  }

  if (GetP() != nullptr && GetP()->Id() < nprocs) {
    GetP()->SetStatus(kPrunning);
  } else {
    if (GetP() != nullptr) {
      GetP()->SetM(nullptr);
    }
    P* p = Allp()[0];
    p->SetM(nullptr);
    p->SetStatus(kPidle);
    AcquireP(p);
  }

  P* runnable_ps = nullptr;
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
  gp->SetSchedLink(nullptr);
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
  gtail->SetSchedLink(nullptr);
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
G* Scheduler::GlobalRunqGet(P* p, int32_t maximum) {
  if (runq_size_ == 0) {
    return nullptr;
  }

  int32_t n = runq_size_ / rtm_conf->MaxProcs() + 1;
  if (n > runq_size_) {
    n = runq_size_;
  }
  if (maximum > 0 && n > maximum) {
    n = maximum;
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
  if (glist == nullptr) {
    return;
  }
  int n = 0;
  {
    RawMutexGuard guard(&lock_);
    for (n = 0; glist != nullptr; n++) {
      G* gp = glist;
      glist = GpCastBack(gp->SchedLink());
      gp->SetState(CoroutineState::kRunnable);
      GlobalRunqPut(gp);
    }
  }

  for ( ; n != 0; n--) {
    StartM(nullptr, false);
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
  atomic::relaxed_inc32(&nr_idlep_, 1);
}

P* Scheduler::PIdleGet() {
  P* p = idlep_;
  if (p != nullptr) {
    idlep_ = p->Link();
    atomic::relaxed_inc32(&nr_idlep_, -1);
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

  // Shutdown takes precedence over timer processing: once ExitFlag is set,
  // PollDescriptors may be torn down (and their embedded Timers freed) while
  // still physically present in some P's heap. Running CheckTimers in that
  // window would dereference freed memory. Bail out before touching timers.
  if (rtm_env->ExitFlag()) {
    return nullptr;
  }

  // === Per-P timer check (Go 1.15 model) ===
  // Run any expired timers on the current P first. A timer callback may
  // Ready() a G which then shows up in curp's runq.
  // poll_until is kept in function scope so the blocking NetPoll below
  // can use it as a timeout (Go 1.15 proc.go:2928-2930).
  int64_t now = 0;
  int64_t poll_until = 0;
  {
    bool ran_timer = false;
    CheckTimers(curp, 0, &now, &poll_until, &ran_timer);
    if (ran_timer) {
      G* gp = curp->RunqGet(inherit_time);
      if (gp != nullptr) {
        return gp;
      }
    }
  }

  G* gp = curp->RunqGet(inherit_time);
  if (gp != nullptr) {
    return gp;
  }

  if (atomic::relaxed_load32(&runq_size_) != 0) {
    {
      RawMutexGuard guard(&lock_);
      gp = GlobalRunqGet(curp, 0);
    }
    if (gp != nullptr) {
      *inherit_time = false;
      return gp;
    }
  }

  // Go 1.15 proc.go:2888-2897 — non-blocking NetPoll, but only if there
  // are netpoll waiters and the scheduler hasn't polled recently.
  if (NetPollInited() && last_poll_ != 0 && NetPollWaiters() > 0) {
    gp = NetPoll(0);
    if (gp != 0) {
      InjectGList(GpCastBack(gp->SchedLink()));
      gp->SetState(CoroutineState::kRunnable);
      *inherit_time = false;
      return gp;
    }
  }

  // If number of spinning M's >= number of busy P's, block.
  // This is necessary to prevent excessive CPU consumption
  // when GOMAXPROCS>>1 but the program parallelism is low.

  if (!curm->GetSpinning() && (2 * atomic::relaxed_load32(&nr_spinning_)) >=
      (static_cast<uint32_t>(rtm_conf->MaxProcs()) -
       atomic::relaxed_load32(&nr_idlep_))) {
    goto stop;
  }

  if (!curm->GetSpinning()) {
    curm->SetSpinning(true);
    atomic::inc32(&nr_spinning_, 1);
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
      // After a failed steal, if p2 isn't running, opportunistically
      // check its timers (Go's work-stealing checkTimers).
      if (gp == nullptr && ShouldStealTimers(p)) {
        int64_t tnow = 0;
        int64_t w = 0;
        bool ran = false;
        CheckTimers(p, 0, &tnow, &w, &ran);
        if (ran) {
          gp = curp->RunqGet(inherit_time);
        }
      }
    }
    if (gp != nullptr) {
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
    if (atomic::inc32(&nr_spinning_, -1) < 0) {
      LOG(FATAL) << "FindRunnable: negative nmspinning";
    }
  }

  for (int i = 0; i < rtm_conf->MaxProcs(); i++) {
    P* p = Allp()[i];
    if (p != nullptr && !p->RunqEmpty()) {
      {
        RawMutexGuard guard(&lock_);
        p = PIdleGet();
      }
      if (p != nullptr) {
        AcquireP(p);
        if (was_spinning) {
          curm->SetSpinning(true);
          atomic::inc32(&nr_spinning_, 1);
        }
        goto top;
      }
    }
  }

  // Go 1.15 proc.go:2928-2942 — blocking NetPoll with timeout = poll_until - now.
  // If poll_until is 0 (no timers pending), block indefinitely.
  if (NetPollInited() && atomic::exchange32(&last_poll_, 0) != 0) {
    int64_t delta = -1;  // block indefinitely by default
    if (poll_until != 0) {
      int64_t now_ns = MonoNow();
      delta = poll_until - now_ns;
      if (delta < 0) {
        delta = 0;  // timer already expired, poll non-blocking
      }
    }
    gp = NetPoll(delta);
    uint32_t now = static_cast<uint32_t>(MonoNow() / tin::kMillisecond);
    if (now == 0)
      now = 1;
    atomic::relaxed_store32(&last_poll_, now);
    if (gp != nullptr) {
      P* p = nullptr;
      {
        RawMutexGuard guard(&lock_);
        p = PIdleGet();
      }
      if (p != nullptr) {
        AcquireP(p);
        InjectGList(GpCastBack(gp->SchedLink()));
        gp->SetState(CoroutineState::kRunnable);
        *inherit_time = false;
        return gp;
      }
      InjectGList(gp);
    }
  }

  M::Stop();
  if (rtm_env->ExitFlag()) {
    return nullptr;
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
  if (idlem_ != nullptr) {
    idlem_ = mp->GetSchedLink();
    nr_idlem_--;
  }
  return mp;
}

void Scheduler::MGetForP(P* curp, bool spinning, P** newp, M** newm) {
  if (curp == nullptr) {
    curp = PIdleGet();
    if (curp == nullptr) {
      if (spinning) {
        if (atomic::inc32(&nr_spinning_, -1) < 0) {
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

  if (m->LockedG() != nullptr) {
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
    G* nextg = nullptr;

    // from global queue.
    if (p->SchedTick() % 61 == 0 && sched->runq_size_ > 0) {
      // Check the global runnable queue once in a while to ensure fairness.
      // Otherwise two goroutines can completely occupy the local runqueue
      // by constantly respawning each other.
      SchedulerLocker guard;
      nextg = sched->GlobalRunqGet(p, 1);
    }
    // from local queue.
    if (nextg == nullptr) {
      nextg = p->RunqGet(&inherit_time);
      if (nextg != nullptr && curg->M()->GetSpinning()) {
        LOG(FATAL) << "schedule: spinning with local work";
      }
    }
    // FindRunnable.
    if (nextg == nullptr) {
      nextg = sched->FindRunnable(&inherit_time);
    }

    // still null?
    if (nextg == nullptr) {
      // tin os power off
      return false;
    }

    if (m->GetSpinning()) {
      sched->ResetSpinning();
    }

    if (nextg->LockedM() != nullptr) {
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
      if (lockedg == nullptr)
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
  ResizeProc(rtm_conf->MaxProcs());
  return 0;
}

void Scheduler::MakeReady(G* gp) {
  if (gp->GetState() != CoroutineState::kWaiting) {
    LOG(FATAL) << "bad g->status in ready";
  }
  gp->SetState(CoroutineState::kRunnable);

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
  StartM(nullptr, true);
}

void Scheduler::HandoffP(P* p) {
  // Go 1.15 proc.go:1987-2043 — hand off P to another M.
  //
  // When P is in kPsyscall (from EnterSyscallBlock), we try to CAS it
  // to kPidle before starting a new M so that AcquireP (which expects
  // kPidle) works. If the CAS fails, ExitSyscallFast reacquired P and
  // we have nothing to do.
  //
  // If there's no work to do, we leave P in kPsyscall (don't put it in
  // the idle list) so ExitSyscallFast can quickly reacquire it. retake
  // will idle it if the syscall runs too long (>10ms).

  // If P has local work, start a new M straight away.
  if (!p->RunqEmpty() || sched->GlobalRunqSize() != 0) {
    if (p->GetStatus() == kPsyscall &&
        !p->CasStatus(kPsyscall, kPidle)) {
      return;  // ExitSyscallFast reacquired P
    }
    StartM(p, false);
    return;
  }

  // No local work, check that there are no spinning/idle M's,
  // otherwise our help is not required.
  if (atomic::load32(&nr_spinning_) + atomic::load32(&nr_idlep_) == 0 &&
      atomic::cas32(&nr_spinning_, 0, 1)) {
    if (p->GetStatus() == kPsyscall &&
        !p->CasStatus(kPsyscall, kPidle)) {
      atomic::inc32(&nr_spinning_, -1);  // undo spinning count
      return;  // ExitSyscallFast reacquired P
    }
    StartM(p, true);
    return;
  }

  lock_.Lock();
  if (runq_size_ != 0) {
    lock_.Unlock();
    if (p->GetStatus() == kPsyscall &&
        !p->CasStatus(kPsyscall, kPidle)) {
      return;  // ExitSyscallFast reacquired P
    }
    StartM(p, false);
    return;
  }

  if ((nr_idlep_ == static_cast<uint32_t>(rtm_conf->MaxProcs() - 1))
      && atomic::load32(&last_poll_) != 0) {
    lock_.Unlock();
    if (p->GetStatus() == kPsyscall &&
        !p->CasStatus(kPsyscall, kPidle)) {
      return;  // ExitSyscallFast reacquired P
    }
    StartM(p, false);
    return;
  }

  // Go 1.15: If P is in kPsyscall (from EnterSyscallBlock), leave it
  // there instead of putting it in the idle list. ExitSyscallFast can
  // quickly reacquire it; retake will idle it if the syscall runs
  // too long (>10ms).
  if (p->GetStatus() == kPsyscall) {
    lock_.Unlock();
    return;
  }

  PIdlePut(p);
  lock_.Unlock();
}

// should not called from g0.
void Scheduler::ExitSyscall0(G* gp) {
  M* curm = gp->M();
  // Clear oldp. If P was still in kPsyscall (HandoffP left it there
  // because there was no work), retake will eventually take it. If P
  // was already transitioned (by HandoffP or retake), there's nothing
  // to clean up.
  curm->SetOldP(nullptr);

  gp->SetState(CoroutineState::kRunnable);
  P* p = nullptr;
  {
    RawMutexGuard guard(&lock_);
    p = PIdleGet();
  }

  if (p != nullptr) {
    AcquireP(p);
    return;
  }

  // no P available, jump to g0;
  G* g0 = curm->G0();
  if (curm->LockedG() != nullptr) {
    M::StopLocked();
  } else {
    curm->SetUnlockInfo(ExitSyscallUnlockFunc, gp, nullptr, nullptr);
    SwitchG(gp, g0, GpCast(g0));
  }
}

bool Scheduler::ExitSyscallFast() {
  G* gp = GetG();
  M* curm = gp->M();
  P* oldp = curm->OldP();
  // Go 1.15 proc.go:3035-3090 — try to re-acquire the old P if it's
  // still in kPsyscall. This gives syscall affinity: the G tends to
  // return to its original P, improving cache locality.
  if (oldp != nullptr && oldp->CasStatus(kPsyscall, kPrunning)) {
    curm->SetP(oldp);
    oldp->SetM(curm);
    curm->SetOldP(nullptr);
    return true;
  }

  // Try to get any other idle P.
  curm->SetP(nullptr);
  if (idlep_ != nullptr) {
    if (ExitSyscallPIdle()) {
      curm->SetOldP(nullptr);
      return true;
    }
  }
  return false;
}

bool Scheduler::ExitSyscallPIdle() {
  P* p = nullptr;
  {
    RawMutexGuard guard(&lock_);
    p = PIdleGet();
  }
  if (p != nullptr) {
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
  uint32_t nr_spinning = atomic::inc32(&nr_spinning_, -1);
  if (static_cast<int32_t>(nr_spinning) < 0) {
    LOG(FATAL) << "ResetSpinning: negative nr_spinning";
  }
  if (nr_spinning == 0 && atomic::load32(&nr_idlep_) > 0) {
    WakeupP();
  }
}

uint32_t Scheduler::LastPollTime() {
  return atomic::acquire_load32(&last_poll_);
}

// Go 1.15 proc.go:4746-4813 — sysmon calls this to take back Ps stuck
// in kPsyscall for too long. Returns the number of Ps retaken.
//
// Algorithm:
// - For each P in kPsyscall, check if syscalltick changed since last pass.
// - If changed: this is a new syscall. Record sysmontick and skip (give
//   the syscall a chance to complete).
// - If unchanged: the syscall has been running for at least one sysmon
//   cycle (up to 10ms). CAS kPsyscall → kPidle and hand off to a new M.
//
// tin does not implement the _Prunning preemptone branch (no async
// preemption).
uint32_t Scheduler::Retake(int64_t now) {
  uint32_t n = 0;
  int nprocs = rtm_conf->MaxProcs();
  for (int i = 0; i < nprocs; i++) {
    P* p = allp_[i];
    if (p == nullptr) continue;
    uint32_t s = p->GetStatus();
    if (s == kPsyscall) {
      uint32_t t = p->SyscallTick();
      if (p->SysmonTick() != t) {
        // First observation of this syscall. Record sysmontick
        // and give it one cycle before retaking.
        p->SetSysmonTick(t);
        continue;
      }
      // Same syscalltick as last observation → syscall has been
      // running for at least one sysmon cycle. Take the P back.
      if (p->CasStatus(kPsyscall, kPidle)) {
        n++;
        HandoffP(p);
      }
    }
    // Skip kPrunning (tin has no async preemption / preemptone)
  }
  return n;
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
  curg->M()->SetP(nullptr);
  p->SetStatus(kPidle);
  p->SetM(nullptr);
  return p;
}

void AcquireP(P* p) {
  G* curg = GetG();
  if (curg->M()->P() != nullptr) {
    LOG(FATAL) << "AcquireP: already in go";
  }
  if (p->M() != nullptr || p->GetStatus() != kPidle) {
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
  if (m->LockedG() == nullptr) {
    m->CurG()->SetM(nullptr);
    m->SetCurG(nullptr);
  }
}

void ParkUnlock(RawMutex* lock) {
  Park(ParkUnlockF, lock, nullptr);
}

void Park(UnlockFunc unlockf, void* arg1, void* arg2) {
  G* gp = GetG();
  M* mp = gp->M();
  if (gp->GetState() != CoroutineState::kRunning) {
    LOG(FATAL) << "gopark: bad g status";
  }
  mp->GetUnlockInfo()->Set(unlockf, arg1, arg2, gp);
  gp->SetState(CoroutineState::kWaiting);
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
  M* curm = gp->M();
  P* p = curm->P();
  // Go 1.9+ proc.go:3035 — save oldp for ExitSyscallFast affinity.
  curm->SetOldP(p);
  gp->SetState(CoroutineState::kSyscall);
  // Go 1.15 runtime2.go:571 — increment syscalltick so sysmon retake
  // can detect long-running syscalls.
  p->IncSyscallTick();
  // Set P to kPsyscall (not kPidle) so:
  // 1. ExitSyscallFast can quickly reacquire it (syscall affinity)
  // 2. retake can take it back if the syscall runs too long (>10ms)
  curm->SetP(nullptr);
  p->SetStatus(kPsyscall);
  p->SetM(nullptr);
  // HandoffP may start a new M (if there's work) or leave P in
  // kPsyscall (if there's no work, for fast ExitSyscall reacquisition).
  sched->HandoffP(p);
}

void ExitSyscall() {
  G* gp = GetG();
  if (sched->ExitSyscallFast()) {
    gp->SetState(CoroutineState::kRunning);
    return;
  }
  sched->ExitSyscall0(gp);
}

void WakePIfNecessary() {
  sched->WakePIfNecessary();
}

void SwitchG(Coroutine* from, Coroutine* to, intptr_t args) {
  from->M()->SetCurG(to);
  to->SetM(from->M());
  SetG(to);
  to->SetState(CoroutineState::kRunning);
  jump_zcontext(from->MutableContext(), *to->MutableContext(), args);
}

// -------------------------------------------------------------


}  // namespace tin::runtime
