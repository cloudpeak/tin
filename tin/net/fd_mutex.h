// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_NET_FD_MUTEX_H_
#define TIN_NET_FD_MUTEX_H_
#include "cstdint"
#include <atomic>

namespace tin::net {

class FdMutex {
 public:
  FdMutex()
    : state_(0)
    , rsema_(0)
    , wsema_(0)
  {};
  bool Incref();
  bool IncrefAndClose();
  bool Deref();
  bool RWLock(bool read);
  bool RWUnlock(bool read);

 private:
  std::atomic<uint64_t> state_;
  uint32_t rsema_;
  uint32_t wsema_;
};

}  // namespace tin::net
#endif  // TIN_NET_FD_MUTEX_H_
