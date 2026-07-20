// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_GREENLET_H_
#define TIN_RUNTIME_GREENLET_H_
#include <cstdlib>
#include <memory>
#include <functional>
#include <string>

#include "context/zcontext.h"
#include "tin/config/config.h"
#include "tin/runtime/util.h"
#include "tin/runtime/guintptr.h"
#include "tin/runtime/stack/stack.h"

namespace tin {

// Forward declaration (full definition in include/tin/runtime.h).
// We do NOT include tin/runtime.h here to avoid clashing with the
// internal tin/runtime/runtime.h that most runtime TUs also include.
struct SpawnOptions;

namespace runtime {

class M;
struct Timer;

enum class CoroutineState {
  kRunning = 0,
  kRunnable = 1,
  kWaiting = 2,
  kSyscall = 3,
  kExited = 4
};

enum CoroutineFlag {
  kFlagG0 = 1,
};

// zcontext C ABI entry (signature fixed by zcontext assembly, do not change).
using ZContextEntry = void* (*)(intptr_t);

class Coroutine {
 public:
  Coroutine();
  Coroutine(const Coroutine&) = delete;
  Coroutine& operator=(const Coroutine&) = delete;
  ~Coroutine();

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

  CoroutineState GetState() const {
    return static_cast<CoroutineState>(state_);
  }
  void SetState(CoroutineState state) {
    state_ = static_cast<int>(state);
  }

  void SetName(const char* name);
  const char* GetName() {
    return name_.c_str();
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
    flags_ |= kFlagG0;
  }
  bool IsG0() {
    return (flags_ & kFlagG0) != 0;
  }

  Timer* GetTimer();

  // User coroutine factory: creates a coroutine with a closure and enqueues it.
  static Coroutine* Create(std::function<void()> closure,
                          const SpawnOptions& opts);

  // G0 factory: creates a system-stack coroutine (C ABI entry, no enqueue).
  static Coroutine* CreateG0(ZContextEntry entry, intptr_t args,
                            int stack_size, const char* name);

 private:
  static void StaticProc(intptr_t args);
  void Proc();

 private:
  GUintptr schedlink_;
  tin::runtime::M* m_;
  tin::runtime::M* lockedm_;
  ZContextEntry entry_;              // zcontext entry point (C ABI)
  std::function<void()> closure_;   // user closure executed by the coroutine
  intptr_t args_;                    // C ABI entry argument
  void* entry_return_;              // C ABI entry return value
  std::string name_;
  std::unique_ptr<Stack> stack_;
  zcontext_t context_;
  int state_;
  int32_t flags_;
  int error_code_;
  Timer* timer_;
};

// Internal spawn (replaces the old SpawnSimple overloads).
void SpawnInternal(std::function<void()> closure,
                   const char* name = nullptr);

}  // namespace runtime

// Type-erased spawn entry point (replaces the old RuntimeSpawn).
// Default argument is defined in include/tin/runtime.h (do not repeat here).
void SpawnClosure(std::function<void()> closure,
                  const SpawnOptions& opts);

}  // namespace tin
#endif  // TIN_RUNTIME_GREENLET_H_
