// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"
#include "base/atomicops.h"
#include "tin/sync/mutex.h"
#include "tin/util/unique_id.h"
#include "quark/atomic.hpp"

namespace tin {

class UniqueIdGenerator {
 public:
  static UniqueIdGenerator* GetInstance() {
    return Singleton<UniqueIdGenerator>::get();
  }

  uint64_t Next() {
    uint64_t uid = uid_.fetch_add(1) + 1;
    return uid;
  }

 private:
  UniqueIdGenerator()
    : uid_(0) {
  }
 private:
  mutable tin::Mutex mutex_;
  quark::atomic_uint64_t uid_;
  friend struct DefaultSingletonTraits<UniqueIdGenerator>;
};

uint64_t GetUniqueId() {
  return UniqueIdGenerator::GetInstance()->Next();
}

}  // namespace tin.
