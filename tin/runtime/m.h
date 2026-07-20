// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_M_H_
#define TIN_RUNTIME_M_H_
#include <list>
#include <memory>
#include <semaphore>
#include <string>
#include <thread>
#include <absl/log/log.h>
#include <absl/log/check.h>

#include "context/zcontext.h"

#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/unlock.h"

namespace tin::runtime {

class P;

using AliasM = M;

class M  {
 public:
  M(const M&) = delete;
  M& operator=(const M&) = delete;
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

  std::counting_semaphore<1>* WaitSemaphore() {
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

  uint32_t* MutableLocked() {
    return &locked_;
  }

  // ---- Go 1.15 runtime2.go:490-535 fields ----

  //持锁计数（runtime2.go:505）。
  int32_t Locks() const { return locks_; }
  void IncLocks() { locks_++; }
  void DecLocks() { locks_--; }

  // 分配中标记（runtime2.go:502）。
  int32_t Mallocing() const { return mallocing_; }
  void SetMallocing(int32_t v) { mallocing_ = v; }

  // 禁止抢占字符串（runtime2.go:504），空字符串表示允许。
  const std::string& PreemptOff() const { return preemptoff_; }
  void SetPreemptOff(const std::string& s) { preemptoff_ = s; }

  // syscall 前的 P（runtime2.go:500）。
  tin::runtime::P* OldP() const { return oldp_; }
  void SetOldP(tin::runtime::P* p) { oldp_ = p; }

  // 快速随机数状态（runtime2.go:514）。
  uint32_t Fastrand();

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
  std::unique_ptr<char[]> cache_;
  std::unique_ptr<std::counting_semaphore<1>> wait_sema_;
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
  std::thread sys_thread_handle_;
  std::unique_ptr<UnLockInfo> unlock_info_;
  bool is_m0_;
  std::list<G*> dead_queue_;
  uint32_t locked_;

  // ---- Go 1.15 runtime2.go:490-535 fields ----
  int32_t locks_;          // runtime2.go:505
  int32_t mallocing_;      // runtime2.go:502
  std::string preemptoff_; // runtime2.go:504
  tin::runtime::P* oldp_;      // runtime2.go:500
  uint32_t fastrand_;      // runtime2.go:514
};

} // namespace tin::runtime
#endif  // TIN_RUNTIME_M_H_
