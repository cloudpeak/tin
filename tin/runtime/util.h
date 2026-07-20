// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_UTIL_H_
#define TIN_RUNTIME_UTIL_H_
#include <cstdlib>

#include "tin/runtime/env.h"

namespace tin::runtime {

class Coroutine;
class P;
class M;
using G = Coroutine;

inline G* GetG() {
  return coro_tls;
}

inline void SetG(G* gp) {
  coro_tls = gp;
}

P* GetP();

M* GetM();

inline uintptr_t GpCast(G* gp) {
  return reinterpret_cast<uintptr_t>(gp);
}

inline G* GpCastBack(uintptr_t gp) {
  return reinterpret_cast<G*>(gp);
}

void YieldLogicProcessor();
void YieldLogicProcessor(int n);
int GetLastSystemErrorCode();

}  // namespace tin::runtime
#endif  // TIN_RUNTIME_UTIL_H_
