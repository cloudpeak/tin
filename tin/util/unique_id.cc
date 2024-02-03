// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>

#include "tin/sync/mutex.h"
#include "tin/util/unique_id.h"
#include "quark/atomic.hpp"

namespace tin {

class UniqueIdGenerator {
 public:

  UniqueIdGenerator()
    : uid_(0) {
  }

  uint64_t Next() {
    uint64_t uid = uid_.fetch_add(1) + 1;
    return uid;
    }
 private:
  mutable tin::Mutex mutex_;
  std::atomic<uint64_t> uid_;
};

UniqueIdGenerator UniqueIdGeneratorInst;

uint64_t GetUniqueId() {
  return UniqueIdGeneratorInst.Next();
}

}  // namespace tin.
