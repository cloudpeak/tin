// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// P2-1 PIMPL: Cond's implementation (which uses runtime::SyncSema) is
// hidden behind a forward-declared Impl class. Users no longer see
// tin/runtime/semaphore.h.

#ifndef TIN_SYNC_COND_H_
#define TIN_SYNC_COND_H_

#include "tin/sync/mutex.h"

namespace tin {

class Cond {
 public:
  explicit Cond(Mutex* lock);
  Cond(const Cond&) = delete;
  Cond& operator=(const Cond&) = delete;
  ~Cond();

  void Wait();
  void Signal();
  void Broadcast();

 private:
  void SignalImpl(bool all);
  Mutex* lock_;
  // PIMPL: hides runtime::SyncSema from the public header.
  struct Impl;
  std::unique_ptr<Impl> impl_;
  uint32_t waiters_;
};

}  // namespace tin

#endif  // TIN_SYNC_COND_H_
