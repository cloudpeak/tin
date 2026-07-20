// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <csignal>
#include <memory>
#include <thread>

#include "build/build_config.h"

#include <absl/functional/bind_front.h>

#include "tin/runtime/util.h"
#include "tin/runtime/timer/timer_queue.h"
#include "tin/runtime/greenlet.h"
#include "tin/runtime/m.h"
#include "tin/runtime/threadpoll.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/sysmon.h"

#include "tin/runtime/env.h"

namespace tin::runtime {

Env::Env()
  : argc_(0)
  , argv_(nullptr)
  , main_signal_(false) {
}

void Env::PreInit() {
  num_processors_ = static_cast<int>(std::thread::hardware_concurrency());
}

int Env::Initialize(EntryFn fn, int argc, char** argv, tin::Config* new_conf) {
  PreInit();
  fn_ = fn;
  conf_ = new_conf;
  rtm_conf = conf_;
  SignalInit();
  // P1-1: core objects owned by Env via unique_ptr. The global non-owning
  // pointers are set here so existing call sites (sched->..., timer_q->...)
  // work without modification.
  sched_ = std::make_unique<Scheduler>();
  timer_q_ = std::make_unique<TimerQueue>();
  sched = sched_.get();
  timer_q = timer_q_.get();
  // glet_tls is thread_local and cannot be a member of Env; it remains a
  // raw pointer managed manually (deleted in Deinitialize).
  glet_tls = new Greenlet;
  ThreadPool::GetInstance()->Start();
  M::New(absl::bind_front(&SysInit), nullptr);

  return 0;
}

void Env::Deinitialize() {
  // If OnMainExit() hasn't run yet (Stop() was called before the entry
  // function returned), do the cleanup now from the main thread.
  // JoinAll() sends nullptr to all thread pool workers and joins them;
  // timer_q->Join() signals the timer goroutine to exit and waits for it.
  if (!main_exited_) {
    ThreadPool::GetInstance()->JoinAll();
    if (timer_q_)
      timer_q_->Join();
  }
  delete glet_tls;
  glet_tls = nullptr;
  // Clear non-owning global pointers before unique_ptr members are reset.
  sched = nullptr;
  timer_q = nullptr;
  // sched_ and timer_q_ will be destroyed when Env is destroyed (via
  // rtm_env.reset() in DeInitializeEnv).
}

int Env::WaitMainExit() {
  main_signal_.WaitForNotification();
  return exit_code_;
}

void Env::SysInit() {
  GetM()->SetM0Flag();
  sched->Init();
  SpawnSimple(&MainGlet, nullptr, "main");
  M::New(absl::bind_front(&SysMon), nullptr);
}

void* Env::MainGlet(intptr_t) {
  int code = rtm_env->fn_(rtm_env->argc_, rtm_env->argv_);
  rtm_env->exit_code_ = code;
  rtm_env->OnMainExit();
  return nullptr;
}

void Env::OnMainExit() {
  // Graceful shutdown: stop the scheduler, join the worker thread pool and
  // the timer queue, then notify WaitForPowerOff() so that the main thread
  // can return and run Deinitialize()/RAII destructors.
  //
  // Do NOT call _exit() here -- that would skip RAII destructors, atexit
  // hooks, and the Deinitialize() call in the user's main(), defeating the
  // purpose of the lifecycle API.
  exit_flag_ = true;
  main_exited_ = true;  // signal to Deinitialize() that cleanup is done
  ThreadPool::GetInstance()->JoinAll();
  timer_q->Join();
  rtm_env->main_signal_.Notify();
}

void Env::RequestStop(int exit_code) {
  exit_code_ = exit_code;
  exit_flag_ = true;
  // Notify main_signal_ so that WaitForPowerOff() returns.
  // The actual cleanup (JoinAll + timer_q->Join) will be done by
  // Deinitialize() since main_exited_ remains false.
  main_signal_.Notify();
}

void Env::SignalInit() {
  // signal(SIGPIPE, SIG_IGN).
#if defined(OS_POSIX)
  if (rtm_conf->IgnoreSigpipe()) {
    struct sigaction sigpipe_action{};  // value-initialized to zero
    sigpipe_action.sa_handler = SIG_IGN;
    sigemptyset(&sigpipe_action.sa_mask);
    bool success = (sigaction(SIGPIPE, &sigpipe_action, nullptr) == 0);
    DCHECK(success);
  }
#endif
}

int InitializeEnv(EntryFn fn, int argc, char** argv, tin::Config* new_conf) {
  // P1-1: rtm_env owned by unique_ptr; eliminates raw new/delete.
  rtm_env = std::make_unique<Env>();
  rtm_env->Initialize(fn, argc, argv, new_conf);
  return 0;
}

void DeInitializeEnv() {
  rtm_env->Deinitialize();
  rtm_env.reset();  // releases Env and its unique_ptr members
}

void RequestStop(int exit_code) {
  if (rtm_env) {
    rtm_env->RequestStop(exit_code);
  }
}

bool StopRequested() {
  return rtm_env && rtm_env->ExitFlag();
}

std::unique_ptr<Env> rtm_env;
Scheduler* sched = nullptr;
TimerQueue* timer_q = nullptr;
thread_local Greenlet* glet_tls = nullptr;

tin::Config* rtm_conf = nullptr;

}  // namespace tin::runtime
