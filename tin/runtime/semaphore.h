// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <stdlib.h>
#include "base/basictypes.h"
#include "tin/runtime/util.h"


namespace tin {
namespace runtime {

struct Sudog {
  G* gp;
  uint32* selectdone;
  Sudog* next;
  Sudog* prev;
  void* elem;  // data element
  int32 nrelease;
  Sudog* waitlink;
  uint32* address;
  uint32  wakedup;

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

bool SemAcquire(uint32* addr);

void SemRelease(uint32* addr);

class SyncSema {
 public:
  SyncSema()
    : head_(NULL)
    , tail_(NULL) {
  }

  void Acquire();
  void Release(uint32 n);

 private:
  RawMutex lock_;
  Sudog* head_;
  Sudog* tail_;
  DISALLOW_COPY_AND_ASSIGN(SyncSema);
};

}  // namespace runtime
}  // namespace tin
