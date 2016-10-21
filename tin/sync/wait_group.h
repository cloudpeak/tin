// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <stdlib.h>
#include "quark/atomic.hpp"
#include "base/basictypes.h"
#include "tin/sync/mutex.h"

namespace tin {

class WaitGroup {
 public:
  explicit WaitGroup(int delta = 0);
  ~WaitGroup();

  void Add(int32 delta);
  void Done();
  void Wait();

 private:
  quark::atomic_uint64_t state_;
  uint32 sem_;
  DISALLOW_COPY_AND_ASSIGN(WaitGroup);
};




}  // namespace tin
