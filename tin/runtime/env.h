// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_ENV_H_
#define TIN_RUNTIME_ENV_H_
#include <memory>

#include "absl/synchronization/notification.h"

#include "tin/tin.h"
#include "tin/sync/atomic_flag.h"
#include "tin/config/config.h"

namespace tin::runtime {

class Scheduler;
class Coroutine;

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

  // Go 1.15 proc.go:4875+ — SCHEDTRACE / SCHEDDETAIL env vars.
  // schedtrace_ > 0: dump scheduler state every schedtrace_ ms.
  // scheddetail_: if true, include per-P details.
  int schedtrace_ms() const { return schedtrace_ms_; }
  bool scheddetail() const { return scheddetail_; }
  int WaitMainExit();
  bool ExitFlag() const {
    return exit_flag_ == true;
  }

  // Request the runtime to stop. Sets exit_flag_ and notifies
  // main_signal_ so WaitForPowerOff() returns. Can be called from
  // any coroutine or from a signal handler on the main thread.
  void RequestStop(int exit_code = 0);

  // Returns the exit code set by Stop() or by the entry function's
  // return value.
  int exit_code() const { return exit_code_; }

  // Returns true if OnMainExit() has already done cleanup (JoinAll).
  // Used by Deinitialize() to avoid double-cleanup when Stop() was
  // called before the entry function returned.
  bool main_exited() const { return main_exited_; }

 private:
  void SignalInit();
  void PreInit();
  static void SysInit();
  static void* MainCoro(intptr_t);
  void OnMainExit();

 private:
  EntryFn fn_;
  int argc_;
  char** argv_;
  tin::Config* conf_;
  int num_processors_;
  int schedtrace_ms_ = 0;   // TIN_SCHEDTRACE in milliseconds (0 = disabled)
  bool scheddetail_ = false;  // TIN_SCHEDDETAIL
  absl::Notification main_signal_;
  tin::AtomicFlag exit_flag_;
  bool main_exited_ = false;
  int exit_code_ = 0;
  // Owned by Env via unique_ptr (P1-1). The global non-owning pointer
  // `sched` below is set to point at this in Initialize() and cleared
  // in Deinitialize(), so existing call sites (sched->Init() etc.)
  // work without modification.
  std::unique_ptr<Scheduler> sched_;
};

// rtm_env is owned by a unique_ptr (P1-1). DeInitializeEnv() calls reset().
extern std::unique_ptr<Env> rtm_env;

// Non-owning pointers into rtm_env's members. Set in Initialize(),
// cleared in Deinitialize(). Retained so the hundreds of call sites
// that use `sched->...` do not need modification.
extern Scheduler* sched;
extern thread_local Coroutine* coro_tls;
extern tin::Config* rtm_conf;

int InitializeEnv(EntryFn fn, int argc, char** argv, Config* new_conf);
void DeInitializeEnv();

// Request the runtime to stop (sets exit_flag_ + notifies main_signal_).
void RequestStop(int exit_code = 0);

// Returns true if the runtime is shutting down.
bool StopRequested();

}  // namespace tin::runtime
#endif  // TIN_RUNTIME_ENV_H_
