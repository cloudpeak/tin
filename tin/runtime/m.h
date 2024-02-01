// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <list>
#include <absl/log/log.h>
#include <absl/log/check.h>

#include "base/threading/platform_thread.h"
#include "base/synchronization/waitable_event.h"
#include "context/zcontext.h"

#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/unlock.h"

namespace tin {
namespace runtime {

class P;

typedef class M AliasM;

class M : public base::PlatformThread::Delegate {
 public:
  virtual ~M();

  bool GetSpinning() const {
    return spinning_;
  }

  void SetSpinning(bool spinning) {
    spinning_ = spinning;
  }

  void EnsureSemaphoreExists();

  void SetNextWaitM(uintptr_t m) {
    next_waitm_ = m;
  }

  uintptr_t NextWaitM() {
    return next_waitm_;
  }

  base::WaitableEvent* WaitSemaphore() {
    return wait_sema_.get();
  }

  tin::runtime::P* P() {
    return p_;
  }

  void SetP(tin::runtime::P* p) {
    p_ = p;
  }

  void SetSchedLink(M* m) {
    schedlink_ = m;
  }

  M* GetSchedLink() const {
    return schedlink_;
  }

  G* CurG() const {
    return curg_;
  }

  G* G0() const {
    return g0_;
  }

  G* LockedG() const {
    return locked_g_;
  }

  void SetLockedG(G* gp) {
    locked_g_ = gp;
  }

  void SetCurG(G* gp) {
    curg_ = gp;
  }


  void SetUnlockInfo(UnlockFunc f, void* arg1, void* arg2, G* owner) {
    unlock_info_->SetF(f);
    unlock_info_->SetArg1(arg1);
    unlock_info_->SetArg2(arg2);
    unlock_info_->SetOwner(owner);
  }

  UnLockInfo* GetUnlockInfo() const {
    return unlock_info_.get();
  }

  static M* New(std::function<void()> fn, tin::runtime::P* p);

  static void Start(tin::runtime::P* p, bool spinning);

  static void StartLocked(G* gp);

  static void Stop();

  static void StopLocked();

  void Join();

  void SetM0Flag() {
    is_m0_ = true;
  }

  bool IsM0() const {
    return is_m0_;
  }

  void ClearDeadQueue();

  void AddToDeadQueue(G* gp) {
    dead_queue_.push_back(gp);
  }

  uint32* MutableLocked() {
    return &locked_;
  }

  char* Cache() {
    if (!cache_) {
      cache_.reset(new char[64 * 1024]);
    }
    return cache_.get();
  }

 private:
  // private constructor
  M();

  void OnSysThreadStart();
  virtual void ThreadMain();
  void OnSysThreadStop();

  static void* G0StaticProc(intptr_t args);
  void* G0Proc();

  static M* Allocate(tin::runtime::P* p);
  void DoUnlock();

 private:
  uintptr_t next_waitm_;
  scoped_ptr<char[]> cache_;
  scoped_ptr<base::WaitableEvent> wait_sema_;
  Note park_;
  tin::runtime::P* p_;
  bool spinning_;
  tin::runtime::M* schedlink_;
  tin::runtime::P* nextp_;
  G* curg_;
  G* g0_;
  G* locked_g_;
  std::function<void()> mstart_fn_;
  zcontext_t sys_context_;
  base::PlatformThreadHandle sys_thread_handle_;
  scoped_ptr<UnLockInfo> unlock_info_;
  bool is_m0_;
  std::list<G*> dead_queue_;
  uint32_t locked_;
  DISALLOW_COPY_AND_ASSIGN(M);
};

}  // namespace runtime
}   // namespace tin








