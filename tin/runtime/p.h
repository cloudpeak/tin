// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_P_H_
#define TIN_RUNTIME_P_H_
#include <atomic>
#include <vector>

#include "tin/runtime/util.h"
#include "tin/runtime/guintptr.h"
#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/timer/timer_queue.h"

namespace tin::runtime {
class M;
struct Sudog;

// P status
enum {
  kPidle = 0,
  kPrunning,  // Only this P is allowed to change from _Prunning.
  kPsyscall,
  kPdead
};

using AliasP = P;

class P {
 public:
  explicit P(int id);
  P(const P&) = delete;
  P& operator=(const P&) = delete;
  int Id() const {
    return id_;
  }

  // ---- Per-P Timer accessors ----
  RawMutex& TimersLock() { return timers_lock_; }
  std::vector<Timer*>& Timers() { return timers_; }
  void SetTimer0When(uint64_t when) {
    timer0_when_.store(when, std::memory_order_release);
  }
  uint64_t Timer0When() {
    return timer0_when_.load(std::memory_order_acquire);
  }
  void IncNumTimers(int32_t n) {
    num_timers_.fetch_add(n, std::memory_order_relaxed);
  }
  uint32_t NumTimers() {
    return num_timers_.load(std::memory_order_relaxed);
  }
  void IncDeletedTimers(int32_t n) {
    deleted_timers_.fetch_add(n, std::memory_order_relaxed);
  }
  uint32_t DeletedTimers() {
    return deleted_timers_.load(std::memory_order_relaxed);
  }
  uint32_t AdjustTimers() {
    return adjust_timers_.load(std::memory_order_relaxed);
  }
  void IncAdjustTimers(int32_t n) {
    adjust_timers_.fetch_add(n, std::memory_order_relaxed);
  }

  int32_t RunqCapacity() const {
    return kRunqCapacity;
  }

  bool RunqEmpty();

  void RunqPut(G* gp, bool next);

  G* RunqGet(bool* inherit_time = nullptr);

  G* RunqSteal(P* p2, bool steal_nextg);

  void MoveRunqToGlobal();

  void SetLink(P* p) {
    link_ = p;
  }

  P* Link() {
    return link_;
  }

  uintptr_t Address() {
    return reinterpret_cast<uintptr_t>(this);
  }

  tin::runtime::M* M() const {
    return m_;
  }

  void SetM(tin::runtime::M* m) {
    m_ = m;
  }

  void SetStatus(uint32_t status) {
    status_ = status;
  }

  uint32_t GetStatus() {
    return status_;
  }

  uint32_t SchedTick() const {
    return sched_tick_;
  }

  void SetSchedTick(uint32_t sched_tick) {
    sched_tick_ = sched_tick;
  }

  void IncSchedTick() {
    sched_tick_++;
  }

  // ---- Go 1.15 runtime2.go:571-572 syscall tick fields ----

  // Incremented each time the P enters a syscall (runtime2.go:571).
  // sysmon uses this to detect long-running syscalls.
  uint32_t SyscallTick() const { return syscalltick_; }
  void IncSyscallTick() { syscalltick_++; }

  // The syscalltick value observed by sysmon on its last pass
  // (runtime2.go:572). If it hasn't changed, the P is still in the
  // same syscall.
  uint32_t SysmonTick() const { return sysmontick_; }
  void SetSysmonTick(uint32_t t) { sysmontick_ = t; }

  bool CasStatus(uint32_t old_status, uint32_t new_status);

  // ---- Per-P sudog cache (Go 1.15 runtime2.go:606-607) ----
  static constexpr int kSudogCacheSize = 128;
  Sudog* AcquireSudogFromCache();
  void ReleaseSudogToCache(Sudog* s);

 private:
  bool RunqPutSlow(G* gp, uint32_t h, uint32_t t);
  uint32_t RunqGrab(GUintptr* batch, int batch_size, uint32_t batch_head,
                  bool steal_nextg);

 private:
  enum {
    kRunqCapacity = 256
  };
  uint32_t runq_head_;
  uint32_t runq_tail_;
  GUintptr runq_[kRunqCapacity];
  GUintptr run_next_;
  P* link_;
  int id_;
  uint32_t status_;
  uint32_t sched_tick_;
  uint32_t syscalltick_;   // Go 1.15 runtime2.go:571
  uint32_t sysmontick_;    // Go 1.15 runtime2.go:572
  tin::runtime::M* m_;

  // ---- Per-P Timer fields ----
  RawMutex timers_lock_;
  std::vector<Timer*> timers_;
  std::atomic<uint64_t> timer0_when_{0};
  std::atomic<uint32_t> num_timers_{0};
  std::atomic<uint32_t> adjust_timers_{0};
  std::atomic<uint32_t> deleted_timers_{0};

  // ---- Per-P sudog cache (Go 1.15 runtime2.go:606-607) ----
  Sudog* sudogcache_[kSudogCacheSize] = {};
  int sudogcache_len_ = 0;
};

}  // namespace tin::runtime
#endif  // TIN_RUNTIME_P_H_
