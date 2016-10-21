// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "tin/config/default.h"

namespace tin {

class Config {
 public:
  int MaxProcs() const {
    return max_procs_;
  }

  void SetMaxProcs(int max_procs) {
    max_procs_ = max_procs;
    max_machine_ = max_procs_ * 4;
  }

  int StackSize() const {
    return stack_size_;
  }

  void SetStackSize(int stack_size) {
    stack_size_ = stack_size;
  }

  int OsThreadStackSize() const {
    return os_thread_stack_size_;
  }

  void SetOsThreadStackSize(int stack_size) {
    os_thread_stack_size_ = stack_size;
  }

  void SetIgnoreSigpipe(bool ignore) {
    ignore_sigpipe_ = ignore;
  }

  bool IgnoreSigpipe() const {
    return ignore_sigpipe_;
  }

  int MaxOSMachines() const {
    return max_machine_;
  }

  void SetMaxMachines(int max_machine) {
    max_machine_ = max_machine;
  }

  bool IsStackProtectionEnabled() const {
    return enable_stack_protection_;
  }

  void EnableStackPprotection(bool enable) {
    enable_stack_protection_ = enable;
  }

 private:
  int max_procs_;
  int max_machine_;
  int stack_size_;
  int os_thread_stack_size_;
  bool ignore_sigpipe_;
  bool enable_stack_protection_;
};

}  // namespace tin
