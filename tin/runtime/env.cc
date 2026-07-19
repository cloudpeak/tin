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

namespace tin {
namespace runtime {

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
  sched = new Scheduler;
  timer_q = new TimerQueue;
  glet_tls = new Greenlet;
  ThreadPool::GetInstance()->Start();
  M::New(absl::bind_front(&SysInit), nullptr);

  return 0;
}

void Env::Deinitialize() {
  // Release order: timer_q first (already Joined in OnMainExit), then sched,
  // then glet_tls. Previously only timer_q was freed — sched and glet_tls
  // leaked on every shutdown.
  delete timer_q;
  delete sched;
  delete glet_tls;
  timer_q = nullptr;
  sched = nullptr;
  glet_tls = nullptr;
}

int Env::WaitMainExit() {
  main_signal_.WaitForNotification();
  return 0;
}

void Env::SysInit() {
  GetM()->SetM0Flag();
  sched->Init();
  SpawnSimple(&MainGlet, nullptr, "main");
  M::New(absl::bind_front(&SysMon), nullptr);
}

void* Env::MainGlet(intptr_t) {
  rtm_env->fn_(rtm_env->argc_, rtm_env->argv_);
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
  ThreadPool::GetInstance()->JoinAll();
  timer_q->Join();
  rtm_env->main_signal_.Notify();
}

void Env::SignalInit() {
  // signal(SIGPIPE, SIG_IGN).
#if defined(OS_POSIX)
  if (rtm_conf->IgnoreSigpipe()) {
    struct sigaction sigpipe_action;
    memset(&sigpipe_action, 0, sizeof(sigpipe_action));
    sigpipe_action.sa_handler = SIG_IGN;
    sigemptyset(&sigpipe_action.sa_mask);
    bool success = (sigaction(SIGPIPE, &sigpipe_action, nullptr) == 0);
    DCHECK(success);
  }
#endif
}

int InitializeEnv(EntryFn fn, int argc, char** argv, tin::Config* new_conf) {
  rtm_env = new Env;
  rtm_env->Initialize(fn, argc, argv, new_conf);
  return 0;
}

void DeInitializeEnv() {
  rtm_env->Deinitialize();
  delete rtm_env;
  rtm_env = nullptr;
}

Env* rtm_env = nullptr;
Scheduler* sched = nullptr;
TimerQueue* timer_q = nullptr;
thread_local Greenlet* glet_tls = nullptr;

tin::Config* rtm_conf = nullptr;

}  // namespace runtime
}  // namespace tin
