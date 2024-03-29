// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <stdlib.h>
#include "tin/runtime/util.h"


namespace tin {
namespace runtime {

struct Sudog {
  G* gp;
  uint32_t * selectdone;
  Sudog* next;
  Sudog* prev;
  void* elem;  // data element
  int32_t nrelease;
  Sudog* waitlink;
  uint32_t* address;
  uint32_t  wakedup;

  Sudog() {
    wakedup = 0;
    gp = NULL;
    selectdone = NULL;
    next = NULL;
    prev = NULL;
    elem = NULL;
    nrelease = 0;
    waitlink = NULL;
    address = NULL;
  }
};

bool SemAcquire(uint32_t* addr);

void SemRelease(uint32_t* addr);

class SyncSema {
 public:
  SyncSema()
    : head_(NULL)
    , tail_(NULL) {
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

}  // namespace runtime
}  // namespace tin
