// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "tin/runtime/util.h"
#include "tin/runtime/guintptr.h"

namespace tin {
namespace runtime {
class M;

// P status
enum {
  kPidle = 0,
  kPrunning,  // Only this P is allowed to change from _Prunning.
  kPsyscall,
  kPdead
};

typedef class P AliasP;

class P {
 public:
  explicit P(int id);
  int Id() const {
    return id_;
  }

  int32_t RunqCapacity() const {
    return kRunqCapacity;
  }

  bool RunqEmpty();

  void RunqPut(G* gp, bool next);

  G* RunqGet(bool* inherit_time = NULL);

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

  bool CasStatus(uint32_t old_status, uint32_t new_status);

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
  tin::runtime::M* m_;
  DISALLOW_COPY_AND_ASSIGN(P);
};

}  // namespace runtime
}  // namespace tin




