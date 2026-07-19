// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <cstdlib>
#include <absl/functional/bind_front.h>

namespace tin {
void RuntimeSpawn(std::function<void()>* closure);

inline void  DoSpawn(std::function<void()> closure) {
  RuntimeSpawn(&closure);
}

template <typename Functor, typename... Args>
void Spawn(Functor functor, Args&&... args) {
  auto boundFunction = absl::bind_front(functor, std::forward<Args>(args)...);
  std::function<void()> closure = [boundFunction]() mutable { boundFunction(); };
  DoSpawn(closure);
}

}  // namespace tin
