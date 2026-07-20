// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_RUNTIME_H_
#define TIN_RUNTIME_RUNTIME_H_
#include <cstdint>
#include <stdexcept>
#include <string>

namespace tin {

// PanicException is thrown by Panic() for unrecoverable runtime errors.
// It derives from std::runtime_error so standard catch (...) handlers work.
class PanicException : public std::runtime_error {
 public:
  explicit PanicException(const std::string& msg)
    : std::runtime_error(msg) {}
};

void Throw(const char* str);

void Panic(const char* str = 0);

void LockOSThread();

void UnlockOSThread();

// Per-coroutine errno model (deprecated).
// New code should use Status/Result<T> instead. See include/tin/status.h.
[[deprecated("Use Status/Result<T> instead")]]
void SetErrorCode(int error_code);

[[deprecated("Use Status/Result<T> instead")]]
int GetErrorCode();

[[deprecated("Use Status/Result<T> instead")]]
bool ErrorOccurred();

[[deprecated("Use Status/Result<T> instead")]]
const char* GetErrorStr();

// Yield conflicts with Windows macro Yield.
void Sched();

void NanoSleep(int64_t ns);

// sleep for ms milliseconds.
void Sleep(int64_t ms);

// unix time, posix time, in nano seconds.
int64_t Now();

// monotonic time, system up time, in nano seconds.
int64_t MonoNow();

// unix time, posix time, in seconds.
int32_t NowSeconds();

}  // namespace tin
#endif  // TIN_RUNTIME_RUNTIME_H_
