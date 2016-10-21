// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <stdlib.h>
#include "base/basictypes.h"

namespace tin {
namespace runtime {

class M;
class RawMutex {
 public:
  RawMutex();
  ~RawMutex();
  void Lock();
  void Unlock();

 private:
  uintptr_t key;
  M* owner_;
  DISALLOW_COPY_AND_ASSIGN(RawMutex);
};

class  RawMutexGuard {
 public:
  inline explicit RawMutexGuard(RawMutex* lock)
    : lock_(lock) {
    lock->Lock();
  }
  inline ~RawMutexGuard() {
    lock_->Unlock();
  }

 private:
  RawMutex* lock_;
  DISALLOW_COPY_AND_ASSIGN(RawMutexGuard);
};

class Note {
 public:
  Note();
  void Wakeup();
  void Sleep();
  void Clear();
  bool TimedSleep(int64 ns);
  bool TimedSleepG(int64 ns);

 private:
  bool SleepInternal(int64 ns);
 private:
  uintptr_t key;
  DISALLOW_COPY_AND_ASSIGN(Note);
};

}  // namespace runtime
}  // namespace tin
