// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>

#include "absl/synchronization/notification.h"

#include "tin/tin.h"
#include "tin/sync/atomic_flag.h"
#include "tin/config/config.h"

namespace tin {
namespace runtime {

class Scheduler;
class Greenlet;
class TimerQueue;

class Env {
 public:
  Env();
  Env(const Env&) = delete;
  Env& operator=(const Env&) = delete;
  int Initialize(EntryFn fn, int argc, char** argv, Config* new_conf);
  void Deinitialize();
  tin::Config* GetConfig() const {
    return conf_;
  }
  int NumberOfProcessors() {
    return num_processors_;
  }
  int WaitMainExit();
  bool ExitFlag() const {
    return exit_flag_ == true;
  }

 private:
  void SignalInit();
  void PreInit();
  static void SysInit();
  static void* MainGlet(intptr_t);
  void OnMainExit();

 private:
  EntryFn fn_;
  int argc_;
  char** argv_;
  tin::Config* conf_;
  int num_processors_;
  absl::Notification main_signal_;
  tin::AtomicFlag exit_flag_;
  // Owned by Env via unique_ptr (P1-1). The global non-owning pointers
  // `sched` and `timer_q` below are set to point at these in Initialize()
  // and cleared in Deinitialize(), so existing call sites (sched->Init()
  // etc.) work without modification.
  std::unique_ptr<Scheduler> sched_;
  std::unique_ptr<TimerQueue> timer_q_;
};

// rtm_env is owned by a unique_ptr (P1-1). DeInitializeEnv() calls reset().
extern std::unique_ptr<Env> rtm_env;

// Non-owning pointers into rtm_env's members. Set in Initialize(),
// cleared in Deinitialize(). Retained so the hundreds of call sites
// that use `sched->...` and `timer_q->...` do not need modification.
extern Scheduler* sched;
extern TimerQueue* timer_q;
extern thread_local Greenlet* glet_tls;
extern tin::Config* rtm_conf;

int InitializeEnv(EntryFn fn, int argc, char** argv, Config* new_conf);
void DeInitializeEnv();


}  // namespace runtime
}  // namespace tin
