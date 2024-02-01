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

/*
template <typename Functor>
void Spawn(Functor functor) {
  DoSpawn(base::Bind(functor));
}

template <typename Functor, typename P1>
void Spawn(Functor functor, const P1& p1) {
  DoSpawn(base::Bind(functor, p1));
}

template <typename Functor, typename P1, typename P2>
void Spawn(Functor functor, const P1& p1, const P2& p2) {
  DoSpawn(base::Bind(functor, p1, p2));
}

template <typename Functor, typename P1, typename P2, typename P3>
void Spawn(Functor functor, const P1& p1, const P2& p2, const P3& p3) {
  DoSpawn(base::Bind(functor, p1, p2, p3));
}

template <typename Functor, typename P1, typename P2, typename P3, typename P4>
void Spawn(Functor functor, const P1& p1, const P2& p2, const P3& p3,
           const P4& p4) {
  DoSpawn(base::Bind(functor, p1, p2, p3, p4));
}

template <typename Functor, typename P1, typename P2, typename P3, typename P4,
          typename P5>
void Spawn(Functor functor, const P1& p1, const P2& p2, const P3& p3,
           const P4& p4, const P5& p5) {
  DoSpawn(base::Bind(functor, p1, p2, p3, p4, p5));
}

template <typename Functor, typename P1, typename P2, typename P3, typename P4,
          typename P5, typename P6>
void Spawn(Functor functor, const P1& p1, const P2& p2, const P3& p3,
           const P4& p4, const P5& p5, const P6& p6) {
  DoSpawn(base::Bind(functor, p1, p2, p3, p4, p5, p6));
}

template <typename Functor, typename P1, typename P2, typename P3, typename P4,
          typename P5, typename P6, typename P7>
void Spawn(Functor functor, const P1& p1, const P2& p2, const P3& p3,
           const P4& p4, const P5& p5, const P6& p6, const P7& p7) {
  DoSpawn(base::Bind(functor, p1, p2, p3, p4, p5, p6, p7));
}
*/

}  // namespace tin
