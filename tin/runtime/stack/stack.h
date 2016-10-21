// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

namespace tin {
namespace runtime {

class Stack {
 public:
  Stack() {}
  virtual ~Stack() {}

  virtual void* Pointer() = 0;
  virtual void* Allocate(size_t size) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Stack);
};

enum StackType {
  kFixedStack = 0,
  kProtectedFixedStack = 1,
};

Stack* NewStack(int type, int size);

}  // namespace runtime
}  // namespace tin













