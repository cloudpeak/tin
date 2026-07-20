// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_STACK_STACK_H_
#define TIN_RUNTIME_STACK_STACK_H_
namespace tin::runtime {

class Stack {
 public:
  Stack() {}
  virtual ~Stack() {}

  virtual void* Pointer() = 0;
  virtual void* Allocate(size_t size) = 0;

 private:
  Stack(const Stack&) = delete;
  Stack& operator=(const Stack&) = delete;

};

enum StackType {
  kFixedStack = 0,
  kProtectedFixedStack = 1,
};

Stack* NewStack(int type, int size);

}  // namespace tin::runtime
#endif  // TIN_RUNTIME_STACK_STACK_H_
