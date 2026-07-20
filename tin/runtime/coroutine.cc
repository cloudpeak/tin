// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <functional>
#include <string>

#include "base/strings/string_util.h"

#include "context/zcontext.h"
#include "tin/runtime.h"          // SpawnOptions full definition
#include "tin/runtime/m.h"
#include "tin/runtime/p.h"
#include "tin/runtime/scheduler.h"
#include "tin/runtime/timer/timer_queue.h"

#include "tin/runtime/coroutine.h"

namespace tin {
namespace runtime {

Coroutine::Coroutine()
  : lockedm_(nullptr)
  , error_code_(0)
  , timer_(nullptr) {
}

Coroutine::~Coroutine() {
  delete timer_;
}

void Coroutine::SetName(const char* name) {
  if (name != nullptr)
    name_ = name;
  else
    name_ = "coroutine";
}

Timer* Coroutine::GetTimer() {
  if (timer_ == nullptr) {
    timer_ = new Timer;
  }
  return timer_;
}

Coroutine* Coroutine::Create(std::function<void()> closure,
                           const SpawnOptions& opts) {
  int stack_size = opts.stack_size > 0 ? opts.stack_size : kDefaultStackSize;
  std::unique_ptr<Coroutine> coro(new Coroutine);
  coro->flags_ = 0;
  coro->args_ = 0;
  coro->entry_ = nullptr;
  coro->SetState(CoroutineState::kRunnable);
  coro->closure_ = std::move(closure);   // move, no swap hack
  coro->SetName(opts.name);
  if (rtm_conf->IsStackProtectionEnabled()) {
    coro->stack_.reset(NewStack(kProtectedFixedStack, stack_size));
  } else {
    coro->stack_.reset(NewStack(kFixedStack, stack_size));
  }
  // make_zcontext round address internally.
  coro->context_ =
    make_zcontext(coro->stack_->Pointer(), stack_size, StaticProc);
  // lock-free enqueue (unchanged).
  GetP()->RunqPut(coro.get(), true);
  sched->WakePIfNecessary();
  return coro.release();
}

Coroutine* Coroutine::CreateG0(ZContextEntry entry, intptr_t args,
                             int stack_size, const char* name) {
  std::unique_ptr<Coroutine> coro(new Coroutine);
  coro->flags_ = 0;
  coro->args_ = args;
  coro->entry_ = entry;
  coro->closure_ = nullptr;
  coro->SetState(CoroutineState::kRunnable);
  coro->SetName(name);
  coro->stack_.reset(NewStack(kFixedStack, stack_size));
  coro->context_ =
    make_zcontext(coro->stack_->Pointer(), stack_size, StaticProc);
  coro->SetG0Flag();
  coro_tls = coro.get();
  return coro.release();
}

void Coroutine::StaticProc(intptr_t args) {
  Coroutine* coro = reinterpret_cast<Coroutine*>(args);
  coro->Proc();
}

void Coroutine::Proc() {
  if (closure_) {
    closure_();
  } else {
    entry_return_ = entry_(args_);
  }

  // coro exit.
  // add to m local dead queue.
  M()->AddToDeadQueue(this);
  Park();
  // never return.
}

void SpawnInternal(std::function<void()> closure, const char* name) {
  SpawnOptions opts;
  opts.name = name ? name : "internal";
  Coroutine::Create(std::move(closure), opts);
}

}  // namespace runtime

void SpawnClosure(std::function<void()> closure, const SpawnOptions& opts) {
  runtime::Coroutine::Create(std::move(closure), opts);
}

}  // namespace tin
