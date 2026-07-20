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
  uint32_t ticket = 0;  // Go 1.15 sema.go: handoff ticket (1 = handoff)
};

// SemAcquire parks the current coroutine on the semaphore at *addr.
// Returns true if woken via handoff (ticket=1, lock ownership transferred),
// false otherwise. (Go 1.15 sema.go:semacquire)
bool SemAcquire(uint32_t* addr);

// SemRelease wakes one waiter on the semaphore at *addr.
// If handoff is true, the semaphore token is pre-consumed and the waiter's
// ticket is set to 1, so the waiter acquires the semaphore immediately
// without going through CanSemAcquire. (Go 1.15 sema.go:semrelease1)
void SemRelease(uint32_t* addr, bool handoff = false);

// AcquireSudog returns a Sudog from the per-P sudogcache, or allocates
// a new one if the cache is empty. (Go 1.15 proc.go:acquireSudog)
Sudog* AcquireSudog();

// ReleaseSudog returns a Sudog to the per-P sudogcache, or frees it if
// the cache is full. (Go 1.15 proc.go:releaseSudog)
void ReleaseSudog(Sudog* s);

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
