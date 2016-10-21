// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tin/runtime/net/poll_descriptor.h"

namespace tin {
namespace runtime {

PollDescriptor::PollDescriptor() {
  fd = 0;
  closing = false;
  seq = 0;
  rg = 0;
  rd = 0;
  wg = 0;
  wd = 0;
  user = 0;
}

}  // namespace runtime
}  // namespace tin
