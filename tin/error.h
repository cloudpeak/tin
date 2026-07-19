// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Public aggregate header for tin's error API: the TIN_* error code
// enumeration (defined in tin/error/error.h) together with the per-greenlet
// error accessor functions GetErrorCode/SetErrorCode/ErrorOccured/
// GetErrorStr (declared in tin/runtime/runtime.h).
//
// This header replaces the need to include both tin/error/error.h and
// tin/runtime/runtime.h just to read the current error code.

#ifndef TIN_ERROR_H_
#define TIN_ERROR_H_

#include "tin/error/error.h"

namespace tin {

// Per-greenlet error accessors. Definitions live in tin/runtime/runtime.cc;
// re-declared here so users do not have to include the runtime internals
// header. Duplicate declarations are benign in C++.
void SetErrorCode(int error_code);
int GetErrorCode();
bool ErrorOccured();
const char* GetErrorStr();

}  // namespace tin

#endif  // TIN_ERROR_H_
