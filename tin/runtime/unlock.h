// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <cstdlib>

#include "tin/runtime/util.h"

namespace tin {
namespace runtime {

// return false if resumed.
typedef bool(*UnlockFunc)(void* arg1, void* arg2);

class UnLockInfo {
 public:
  UnLockInfo()
    : f_(NULL)
    , arg1_(NULL)
    , arg2_(NULL)
    , owner_(NULL) {
  }

  UnLockInfo(const UnLockInfo&) = delete;
  UnLockInfo& operator=(const UnLockInfo&) = delete;

  UnlockFunc F() const {
    return f_;
  }

  void* Arg1() const {
    return arg1_;
  }

  void* Arg2() const {
    return arg2_;
  }

  G* Owner() const {
    return owner_;
  }

  bool Empty() const {
    return f_ == NULL;
  }

  // set
  void Set(UnlockFunc unlockf, void* arg1, void* arg2, G* owner) {
    f_ = unlockf;
    arg1_ = arg1;
    arg2_ = arg2;
    owner_ = owner;
  }

  void SetF(UnlockFunc unlockf) {
    f_ = unlockf;
  }

  void SetArg1(void* arg) {
    arg1_ = arg;
  }

  void SetArg2(void* arg) {
    arg2_ = arg;
  }

  void SetOwner(G* owner) {
    owner_ = owner;
  }

  void Clear() {
    f_ = NULL;
    arg1_ =  NULL;
    arg2_ = NULL;
    owner_ = NULL;
  }

  void Run() {
    if (f_ != NULL)
      RunInternal();
  }

 private:
  void RunInternal();

 private:
  UnlockFunc f_;
  void* arg1_;
  void* arg2_;
  G* owner_;
};

}  // namespace runtime
}   // namespace tin














