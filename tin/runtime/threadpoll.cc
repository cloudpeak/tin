// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "tin/error/error.h"
#include "tin/runtime/m.h"
#include "tin/runtime/util.h"
#include "tin/runtime/runtime.h"
#include "tin/runtime/scheduler.h"

#include "tin/runtime/threadpoll.h"

namespace tin {
namespace runtime {

GletWork::GletWork() {
  gp_ = GetG();
}

void GletWork::Resume() {
  {
    SchedulerLocker guard;
    sched->GlobalRunqPut(gp_);
  }
  WakePIfNecessary();
}

void GletWork::Finalize() {
  SaveLastError(GetLastSystemErrorCode());
  Resume();
}

bool SubmitGletWorkUnlockF(void* arg1, void* arg2) {
  GletWork* work = static_cast<GletWork*>(arg1);
  ThreadPoll::GetInstance()->AddWork(work);
  return true;
}

void SubmitGletWork(GletWork* work) {
  Park(SubmitGletWorkUnlockF, work, NULL);
  SetErrorCode(TinTranslateSysError(work->LastError()));
}

void SubmitGetAddrInfoGletWork(GletWork* work) {
  Park(SubmitGletWorkUnlockF, work, NULL);
  SetErrorCode(TinGetaddrinfoTranslateError(work->LastError()));
}

// ThreadPoll implementation.
ThreadPoll::ThreadPoll()
  : num_threads_(64)
  , dry_(true, false) {
}

void ThreadPoll::Start() {
  for (int i = 0; i < num_threads_; ++i) {
    M* m = M::New(base::Bind(&ThreadPoll::Run, base::Unretained(this)), NULL);
    threads_.push_back(m);
  }
}

void ThreadPoll::JoinAll() {
  for (int i = 0; i < num_threads_; ++i) {
    AddWork(NULL);
  }

  // Join and destroy all the worker threads.
  for (int i = 0; i < num_threads_; ++i) {
    threads_[i]->Join();
    delete threads_[i];
  }
  threads_.clear();
}

void ThreadPoll::AddWork(Work* work) {
  base::AutoLock locked(lock_);
  tasks_.push_back(work);
  // If we were empty, signal that we have work now.
  if (!dry_.IsSignaled())
    dry_.Signal();
}

// consider replace with conditional variable.
void ThreadPoll::Run() {
  Work* work = NULL;

  while (true) {
    dry_.Wait();
    {
      base::AutoLock locked(lock_);
      if (!dry_.IsSignaled())
        continue;

      DCHECK(!tasks_.empty());
      work = tasks_.front();
      tasks_.pop_front();

      // Signal to any other threads that we're currently out of work.
      if (tasks_.empty())
        dry_.Reset();
    }

    // A NULL delegate pointer signals us to quit.
    if (!work)
      break;

    work->Run();
  }
}

}  // namespace runtime
}  // namespace tin
