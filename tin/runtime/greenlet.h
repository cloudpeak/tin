// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdlib>

#include <functional>
#include "context/zcontext.h"
#include "tin/config/config.h"
#include "tin/runtime/util.h"
#include "tin/runtime/guintptr.h"
#include "tin/runtime/stack/stack.h"

namespace tin {
namespace runtime {

class M;
struct Timer;

enum GletState {
  GLET_RUNNING = 0,
  GLET_RUNNABLE = 1,
  GLET_WAITING = 2,
  GLET_SYSCALL = 3,
  GLET_EXITED = 4
};

enum GreenletFlag {
  kGletFlagG0 = 1,
};

typedef void* (*GreenletFunc)(intptr_t);

class Greenlet {
 public:
  Greenlet();
  Greenlet(const Greenlet&) = delete;
  Greenlet& operator=(const Greenlet&) = delete;

  ~Greenlet();

  void SetSchedLink(G* gp) {
    schedlink_ = gp;
  }
  uintptr_t SchedLink() const {
    return schedlink_.Integer();
  }

  tin::runtime::M* M() const {
    return m_;
  }

  void SetM(tin::runtime::M* m) {
    m_ = m;
  }

  tin::runtime::M* LockedM() const {
    return lockedm_;
  }

  void SetLockedM(tin::runtime::M* m) {
    lockedm_ = m;
  }

  int GetState() const {
    return state_;
  }

  void SetState(int state) {
    state_ = state;
  }

  void SetName(const char* name);

  const char* GetName() {
    return name_;
  }

  zcontext_t* MutableContext() {
    return &context_;
  }

  int GetErrorCode() {
    return error_code_;
  }

  void SetErrorCode(int error_code) {
    error_code_ = error_code;
  }

  void SetG0Flag() {
    flags_ |= kGletFlagG0;
  }

  bool IsG0() {
    return (flags_ & kGletFlagG0) != 0;
  }

  Timer* GetTimer();

  static Greenlet* Create(GreenletFunc entry,
                          std::function<void()>*  closure,
                          bool sysg0 = false,
                          intptr_t args = 0,
                          bool joinable = false,
                          int stack_size = kDefaultStackSize,
                          const char* name = "greenlet");

 private:
  static void StaticProc(intptr_t args);
  void Proc();

 private:
  GUintptr schedlink_;
  tin::runtime::M* m_;
  tin::runtime::M* lockedm_;
  std::function<void()>  cb_;
  GreenletFunc entry_;
  std::function<void()> closure_;
  intptr_t args_;
  void* retval_;
  char name_[32];
  std::unique_ptr<Stack> stack_;
  zcontext_t context_;
  int state_;
  int32_t flags_;
  int error_code_;
  Timer* timer_;
};

void SpawnSimple(GreenletFunc entry, void* args = NULL,
                 const char* name = NULL);
void SpawnSimple(std::function<void()> closure,  const char* name = NULL);

}  // namespace runtime

void RuntimeSpawn(std::function<void()>* closure);

}  // namespace tin










