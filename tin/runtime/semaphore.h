// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_SEMAPHORE_H_
#define TIN_RUNTIME_SEMAPHORE_H_
#include <cstdlib>
#include "tin/runtime/util.h"


namespace tin::runtime {

struct Sudog {
  G* gp = nullptr;
  uint32_t* selectdone = nullptr;
  Sudog* next = nullptr;
  Sudog* prev = nullptr;
  void* elem = nullptr;  // data element
  int32_t nrelease = 0;
  Sudog* waitlink = nullptr;
  uint32_t* address = nullptr;
  uint32_t wakedup = 0;
};

bool SemAcquire(uint32_t* addr);

void SemRelease(uint32_t* addr);

class SyncSema {
 public:
  SyncSema()
    : head_(nullptr)
    , tail_(nullptr) {
  }
  SyncSema(const SyncSema&) = delete;
  SyncSema& operator=(const SyncSema&) = delete;
  void Acquire();
  void Release(uint32_t n);

 private:
  RawMutex lock_;
  Sudog* head_;
  Sudog* tail_;
};

}  // namespace tin::runtime
#endif  // TIN_RUNTIME_SEMAPHORE_H_
