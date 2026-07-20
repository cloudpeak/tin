// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_THREADPOLL_H_
#define TIN_RUNTIME_THREADPOLL_H_
#include <cstdlib>
#include <vector>
#include <deque>

#include <absl/synchronization/notification.h>
#include <absl/synchronization/mutex.h>
#include <absl/base/call_once.h>


#include "tin/runtime/env.h"

namespace tin::runtime {

class M;
// consider replace with shared_ptr<Work>
class  Work {
 public:
  Work() { }
  Work(const Work&) = delete;
  Work& operator=(const Work&) = delete;
  virtual ~Work() { }
  virtual void Run() = 0;
};

class GletWork : public Work {
 public:
  GletWork();
  virtual ~GletWork() {}
  virtual void Run() = 0;
  int LastError() const {return last_error_;}
  void SaveLastError(int err) {last_error_ = err;}

 protected:
  void Resume();
  void Finalize();

 private:
  int last_error_;
  G* gp_;
};

void SubmitGletWork(GletWork* work);
void SubmitGetAddrInfoGletWork(GletWork* work);


class ThreadPool {
 public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    static ThreadPool* GetInstance();

  void Start();
  void JoinAll();
  void AddWork(Work* work);
  void Run();

private:
    ThreadPool();
 private:
  int num_threads_;
  std::vector<M*> threads_;
  std::deque<Work*> tasks_;
  absl::Mutex lock_;

  absl::Notification dry_;
};

}  // namespace tin::runtime
#endif  // TIN_RUNTIME_THREADPOLL_H_
