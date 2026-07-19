// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Public aggregate header for tin's runtime-control API: Spawn (from
// tin/runtime/spawn.h) and the scheduling/exception helpers Sched/
// LockOSThread/UnlockOSThread/Throw/Panic (declared in
// tin/runtime/runtime.h).
//
// Spawn is a first-class public API (the moral equivalent of Go's `go`
// statement), so it is exposed via this top-level header rather than
// requiring users to reach into tin/runtime/spawn.h. The internal headers
// remain available for backwards compatibility.

#ifndef TIN_RUNTIME_H_
#define TIN_RUNTIME_H_

#include "tin/runtime/spawn.h"

namespace tin {

// Scheduling and exception helpers. Definitions live in
// tin/runtime/runtime.cc; re-declared here so users do not have to include
// the runtime internals header. Duplicate declarations are benign in C++.
void Sched();
void LockOSThread();
void UnlockOSThread();
void Throw(const char* str);
void Panic(const char* str = 0);

}  // namespace tin

#endif  // TIN_RUNTIME_H_
