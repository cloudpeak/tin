// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Public API: error codes and per-greenlet error accessors.
// Does not include any runtime/ internals.

#ifndef TIN_ERROR_H_
#define TIN_ERROR_H_

#include "tin/error/error.h"

namespace tin {

void SetErrorCode(int error_code);
int GetErrorCode();
bool ErrorOccured();
const char* GetErrorStr();

}  // namespace tin

#endif  // TIN_ERROR_H_
