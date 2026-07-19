// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/functional/bind_front.h>

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
    ThreadPool::GetInstance()->AddWork(work);
  return true;
}

void SubmitGletWork(GletWork* work) {
  Park(SubmitGletWorkUnlockF, work, nullptr);
  SetErrorCode(TinTranslateSysError(work->LastError()));
}

void SubmitGetAddrInfoGletWork(GletWork* work) {
  Park(SubmitGletWorkUnlockF, work, nullptr);
  SetErrorCode(TinGetaddrinfoTranslateError(work->LastError()));
}

absl::once_flag thread_pool_once;

ThreadPool* ThreadPool::GetInstance() {
  static ThreadPool* instance = nullptr;
  absl::call_once(thread_pool_once, []() {
      instance = new ThreadPool();
  });
  return instance;
}

// ThreadPool implementation.
ThreadPool::ThreadPool()
  : num_threads_(64)
  , dry_(false) {
}

void ThreadPool::Start() {
  for (int i = 0; i < num_threads_; ++i) {
//    M* m = M::New(base::Bind(&ThreadPool::Run, base::Unretained(this)), nullptr);
    M* m = M::New(absl::bind_front(&ThreadPool::Run, this), nullptr);
    threads_.push_back(m);
  }
}

void ThreadPool::JoinAll() {
  for (int i = 0; i < num_threads_; ++i) {
    AddWork(nullptr);
  }

  // Join and destroy all the worker threads.
  for (int i = 0; i < num_threads_; ++i) {
    threads_[i]->Join();
    delete threads_[i];
  }
  threads_.clear();
}

void ThreadPool::AddWork(Work* work) {
  absl::MutexLock guard(&lock_);
  tasks_.push_back(work);
  // If we were empty, signal that we have work now.
  if (!dry_.HasBeenNotified())
    dry_.Notify();
}

// consider replace with conditional variable.
void ThreadPool::Run() {
  Work* work = nullptr;

  while (true) {
    dry_.WaitForNotification();
    {
      absl::MutexLock guard(&lock_);
      if (!dry_.HasBeenNotified())
        continue;

      DCHECK(!tasks_.empty());
      work = tasks_.front();
      tasks_.pop_front();

      // Signal to any other threads that we're currently out of work.
      if (tasks_.empty())
        dry_.Notify(); // TODO
    }

    // A nullptr delegate pointer signals us to quit.
    if (!work)
      break;

    work->Run();
  }
}

}  // namespace runtime
}  // namespace tin
