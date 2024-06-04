// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "absl/synchronization/notification.h"

#include "tin/tin.h"
#include "tin/sync/atomic_flag.h"
#include "tin/config/config.h"

namespace tin {
namespace runtime {

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
};

class Scheduler;
class Greenlet;
class TimerQueue;

extern Env* rtm_env;
extern Scheduler* sched;
extern TimerQueue* timer_q;
extern thread_local Greenlet* glet_tls;
extern tin::Config* rtm_conf;

int InitializeEnv(EntryFn fn, int argc, char** argv, Config* new_conf);
void DeInitializeEnv();


}  // namespace runtime
}  // namespace tin
