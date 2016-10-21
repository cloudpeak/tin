// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <stdlib.h>

#include "build/build_config.h"

#include "base/sys_info.h"
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
  , argv_(NULL)
  , main_signal_(false, false) {
}

void Env::PreInit() {
  num_processors_ = base::SysInfo::NumberOfProcessors();
}

int Env::Initialize(EntryFn fn, int argc, char** argv, tin::Config* new_conf) {
  PreInit();
  fn_ = fn;
  conf_ = new_conf;
  rtm_conf = conf_;
  SignalInit();
  sched = new Scheduler;
  timer_q = new TimerQueue;
  glet_tls = new base::ThreadLocalPointer<Greenlet>;
  ThreadPoll::GetInstance()->Start();
  M::New(base::Bind(&SysInit), NULL);
  return 0;
}

void Env::Deinitialize() {
  // don't delete rtm_config.
  delete timer_q;
}

int Env::WaitMainExit() {
  main_signal_.Wait();
  return 0;
}

void Env::SysInit() {
  GetM()->SetM0Flag();
  sched->Init();
  SpawnSimple(&MainGlet, NULL, "main");
  M::New(base::Bind(&SysMon), NULL);
}

void* Env::MainGlet(intptr_t) {
  rtm_env->fn_(rtm_env->argc_, rtm_env->argv_);
  rtm_env->OnMainExit();
  return NULL;
}

void Env::OnMainExit() {
  // current workaround, exit directly.
  _exit(0);
  // TODO(author) wait for all exit, thread pool, net poller etc.
  exit_flag_ = true;
  ThreadPoll::GetInstance()->JoinAll();
  timer_q->Join();
  rtm_env->main_signal_.Signal();
}

void Env::SignalInit() {
  // signal(SIGPIPE, SIG_IGN).
#if defined(OS_POSIX)
  if (rtm_conf->IgnoreSigpipe()) {
    struct sigaction sigpipe_action;
    memset(&sigpipe_action, 0, sizeof(sigpipe_action));
    sigpipe_action.sa_handler = SIG_IGN;
    sigemptyset(&sigpipe_action.sa_mask);
    bool success = (sigaction(SIGPIPE, &sigpipe_action, NULL) == 0);
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
}

Env* rtm_env = NULL;
Scheduler* sched = NULL;
TimerQueue* timer_q = NULL;
base::ThreadLocalPointer<Greenlet>* glet_tls = NULL;
tin::Config* rtm_conf = NULL;

}  // namespace runtime
}  // namespace tin
