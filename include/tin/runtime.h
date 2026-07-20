// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Public API: Spawn (coroutine creation) and scheduling helpers.
// Does not include any runtime/ internals.

#ifndef TIN_RUNTIME_H_
#define TIN_RUNTIME_H_

#include <functional>
#include <utility>

namespace tin {

// Spawn options for fine-grained control over coroutine creation.
struct SpawnOptions {
  int stack_size = 0;           // 0 = use global config default
  const char* name = "coroutine";
};

// Spawn a new coroutine. Accepts any callable and forwards arguments.
template <typename Functor, typename... Args>
void Spawn(Functor&& functor, Args&&... args) {
  SpawnClosure([fn = std::forward<Functor>(functor),
                ...args = std::forward<Args>(args)]() mutable {
    std::invoke(fn, args...);
  });
}

// Type-erased internal entry point (called by the Spawn template above).
void SpawnClosure(std::function<void()> closure,
                  const SpawnOptions& opts = {});

// Scheduling and exception helpers.
void Sched();
void LockOSThread();
void UnlockOSThread();
void Throw(const char* str);
void Panic(const char* str = nullptr);

}  // namespace tin

#endif  // TIN_RUNTIME_H_
