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

// WaitReason —reason a G is blocked (ref: Go 1.15 runtime2.go:979-1004).
// Only the wait reasons relevant to tin's existing blocking scenarios
// are enumerated; additional values can be added as new features land.
enum WaitReason {
  kWaitReasonZero = 0,
  kWaitReasonGCAssistWait,   // 1
  kWaitReasonIOWait,         // 2
  kWaitReasonChanReceive,    // 3
  kWaitReasonChanSend,       // 4
  kWaitReasonSelect,         // 5
  kWaitReasonMutex,          // 6
  kWaitReasonSleep,          // 7
  kWaitReasonTimer,          // 8
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

  // ---- Go 1.15 runtime2.go:425-431 fields ----

  // goroutine ID (runtime2.go:428). Assigned at construction from a
  // global monotonic counter; later Phase 6 will switch to per-P batched
  // allocation (goidcache).
  int64_t GoId() const { return goid_; }

  // Approximate time when the G became blocked (runtime2.go:430),
  // in nanoseconds since startup (MonoNow). 0 means not blocked.
  int64_t WaitSince() const { return waitsince_; }
  void SetWaitSince(int64_t t) { waitsince_ = t; }

  // Why the G is blocked (runtime2.go:431).
  int32_t WaitReason() const { return waitreason_; }
  void SetWaitReason(int32_t r) { waitreason_ = r; }

  // Parameter passed to the G when it is woken (runtime2.go:425).
  void* Param() const { return param_; }
  void SetParam(void* p) { param_ = p; }

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

  // ---- Go 1.15 runtime2.go:425-431 fields ----
  int64_t goid_;          // goroutine ID
  int64_t waitsince_;     // monotonic ns when blocking started
  int32_t waitreason_;    // WaitReason enum
  void* param_;           // wakeup parameter
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
