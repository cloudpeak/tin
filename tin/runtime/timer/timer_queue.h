// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <vector>

#include "base/synchronization/waitable_event.h"

#include "tin/time/time.h"
#include "tin/sync/wait_group.h"
#include "tin/runtime/util.h"
#include "tin/runtime/raw_mutex.h"

namespace tin {
namespace runtime {

void InternalNanoSleep(int64 ns);

typedef void (*TimerCallback)(void* arg, uintptr_t seq);

int64 NanoFromNow(int64 deadline);

struct Timer {
  Timer() {
    i = 0;
    when = period = 0;
    seq = 0;
    f = NULL;
    arg = 0;
  }

  int i;
  int64 when;
  int64 period;
  uintptr_t seq;
  TimerCallback f;
  void* arg;
};

class TimerQueue {
 public:
  TimerQueue();
  ~TimerQueue();

  void AddTimerLocked(Timer* t);
  void AddTimer(Timer* t);
  bool DelTimer(Timer* t);
  void Lock();
  void Join();
  static bool UnlockQueue(void* arg1, void* arg2);

 private:
  void Proc();
  void SiftUp(int i);
  void SiftDown(int i);

  int Length() {
    return static_cast<int>(timers_.size());
  }

 private:
  G* gp_;
  bool created_;
  bool rescheduling_;
  bool sleeping_;
  RawMutex mutex_;
  Note wait_note_;
  std::vector<Timer*> timers_;
  bool exit_flag_;
  tin::WaitGroup wait_group_;
  DISALLOW_COPY_AND_ASSIGN(TimerQueue);
};


}  // namespace runtime
}   // namespace tin









