// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Public API: Spawn (coroutine creation) and scheduling helpers.
// Does not include any runtime/ internals.

#ifndef TIN_RUNTIME_H_
#define TIN_RUNTIME_H_

#include <functional>
#include <absl/functional/bind_front.h>

namespace tin {

// Spawn a new coroutine. Accepts any callable and forwards arguments.
void RuntimeSpawn(std::function<void()>* closure);

inline void DoSpawn(std::function<void()> closure) {
  RuntimeSpawn(&closure);
}

template <typename Functor, typename... Args>
void Spawn(Functor&& functor, Args&&... args) {
  auto closure = [functor = std::forward<Functor>(functor),
                  ...args = std::forward<Args>(args)]() mutable {
    std::invoke(functor, args...);
  };
  DoSpawn(closure);
}

// Scheduling and exception helpers.
void Sched();
void LockOSThread();
void UnlockOSThread();
void Throw(const char* str);
void Panic(const char* str = nullptr);

}  // namespace tin

#endif  // TIN_RUNTIME_H_
