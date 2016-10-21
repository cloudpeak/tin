// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "base/basictypes.h"

namespace tin {

void Throw(const char* str);

void Panic(const char* str = 0);

void LockOSThread();

void UnlockOSThread();

void SetErrorCode(int error_code);

int GetErrorCode();

bool ErrorOccured();

const char* GetErrorStr();

// Yield conflicts with Windows macro Yield.
void Sched();

void NanoSleep(int64 ns);

// sleep for ms milliseconds.
void Sleep(int64 ms);

// unix time, posix time, in nano seconds.
int64 Now();

// monotonic time, system up time, in nano seconds.
int64 MonoNow();

// unix time, posix time, in seconds.
int32 NowSeconds();

}  // namespace tin
