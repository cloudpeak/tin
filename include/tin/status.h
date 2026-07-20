// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Status: an immutable, value-semantic error type inspired by leveldb::Status.
// Designed to replace tin's global errno-per-coroutine error model.
// A Status is small (one int), trivially copyable, and can be safely ignored
// — but unlike errno it is explicit and local to the call that produced it.

#ifndef TIN_STATUS_H_
#define TIN_STATUS_H_

#include <string>

#include "tin/error/error.h"  // TIN_* error codes

namespace tin {

class Status {
 public:
  Status() noexcept = default;  // implicit OK
  Status(const Status&) noexcept = default;
  Status& operator=(const Status&) noexcept = default;
  Status(Status&&) noexcept = default;
  Status& operator=(Status&&) noexcept = default;

  static Status OK() { return Status(); }

  // Build a Status from a TIN_* error code. code == 0 means OK.
  static Status FromErrno(int code) { return Status(code); }

  // Alias for FromErrno — more idiomatic factory name.
  static Status Make(int code) { return Status(code); }

  bool ok() const { return code_ == 0; }
  int code() const { return code_; }

  // Convenience predicates for common error categories.
  bool IsEOF() const { return code_ == TIN_EOF; }
  bool IsTimeout() const { return code_ == TIN_ETIMEOUT_INTR; }
  bool IsClosed() const { return code_ == TIN_OBJECT_CLOSED; }
  bool IsUnexpectedEOF() const { return code_ == TIN_UNEXPECTED_EOF; }
  bool IsConnectionReset() const { return code_ == TIN_ECONNRESET; }
  bool IsConnectionRefused() const { return code_ == TIN_ECONNREFUSED; }
  bool IsBrokenPipe() const { return code_ == TIN_EPIPE; }
  bool IsConnectionAborted() const { return code_ == TIN_ECONNABORTED; }
  bool IsInvalid() const { return code_ == TIN_EINVAL; }
  bool IsBufferFull() const { return code_ == TIN_EBUFFERFULL; }
  bool IsBadProtocol() const { return code_ == TIN_EBADPROTOCOL; }
  bool IsTooLarge() const { return code_ == TIN_ETOOLARGE; }

  // In the context of Result<T>::error(), this returns *this so that
  // callers can write: result.error().ToString().
  const Status& error() const { return *this; }

  // Human-readable representation, e.g. "OK" or "EOF: end of file".
  std::string ToString() const;

  // Human-readable error name (without the description).
  std::string ErrorName() const;

  // Comparison operators.
  friend bool operator==(const Status& a, const Status& b) {
    return a.code_ == b.code_;
  }
  friend bool operator!=(const Status& a, const Status& b) {
    return a.code_ != b.code_;
  }

 private:
  explicit Status(int code) : code_(code) {}
  int code_ = 0;
};

}  // namespace tin

#endif  // TIN_STATUS_H_
