// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>


#include "tin/error/error.h"
#include "tin/runtime/util.h"
#include "tin/runtime/greenlet.h"
#include "tin/runtime/m.h"
#include "tin/runtime/p.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/timer/timer_queue.h"

#include "tin/runtime/runtime.h"

namespace tin {

namespace runtime {

void InternalLockOSThread() {
  G* curg = GetG();
  curg->SetLockedM(curg->M());
  curg->M()->SetLockedG(curg);
}

void InternalUnlockOSThread() {
  G* curg = GetG();
  if (curg->M()->MutableLocked() != 0) {
    return;
  }
  curg->M()->SetLockedG(NULL);
  curg->SetLockedM(NULL);
}

bool YieldUnlockFn(void* arg1, void* arg2) {
  G* g = static_cast<G*>(arg1);

  // global queue.
  SchedulerLocker guard;
  sched->GlobalRunqPut(g);

  return true;
}

void InternalYield() {
  G* me = GetG();
  Park(YieldUnlockFn, me, NULL);
}

}  // namespace runtime

void Throw(const char* str) {
  std::cout << str << std::endl;
  // base::debug::BreakDebugger(); // TODO
  throw(str);
}

void Panic(const char* str) {
  throw(str);
}

void LockOSThread() {
  runtime::InternalLockOSThread();
}

void UnlockOSThread() {
  runtime::InternalUnlockOSThread();
}

void SetErrorCode(int error_code) {
  runtime::GetG()->SetErrorCode(error_code);
}

int GetErrorCode() {
  return runtime::GetG()->GetErrorCode();
}

bool ErrorOccured() {
  return runtime::GetG()->GetErrorCode() != 0;
}

const char* GetErrorStr() {
  return TinErrorName(GetErrorCode());
}

void Sched() {
  tin::runtime::InternalYield();
}

void NanoSleep(int64_t ns) {
  return tin::runtime::InternalNanoSleep(ns);
}

void Sleep(int64_t ms) {
  return NanoSleep(ms * 1000 * 1000);
}

}  // namespace tin
