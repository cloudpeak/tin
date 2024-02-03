// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <cstdlib>
#include <cstdint>

namespace tin::runtime {

class M;
class RawMutex {
 public:
  RawMutex();
  RawMutex(const RawMutex&) = delete;
  RawMutex& operator=(const RawMutex&) = delete;
  ~RawMutex();
  void Lock();
  void Unlock();

 private:
  uintptr_t key;
  M* owner_;
};

class  RawMutexGuard {
 public:
  inline explicit RawMutexGuard(RawMutex* lock)
    : lock_(lock) {
    lock->Lock();
  }
  RawMutexGuard(const RawMutexGuard&) = delete;
  RawMutexGuard& operator=(const RawMutexGuard&) = delete;
  inline ~RawMutexGuard() {
    lock_->Unlock();
  }

 private:
  RawMutex* lock_;
};

class Note {
 public:
  Note();
  void Wakeup();
  void Sleep();
  void Clear();
  bool TimedSleep(int64_t ns);
  bool TimedSleepG(int64_t ns);

 private:
  bool SleepInternal(int64_t ns);
 private:
  uintptr_t key;
};

} // namespace tin::runtime

