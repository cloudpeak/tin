// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Status: an immutable, value-semantic error type inspired by leveldb::Status.
// Designed to eventually replace tin's global errno-per-greenlet error model
// (see docs/code-review-2026.md §2). A Status is small (one int), trivially
// copyable, and can be safely ignored — but unlike errno it is explicit and
// local to the call that produced it.

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

  static Status OK() { return Status(); }

  // Build a Status from a TIN_* error code. code == 0 means OK.
  static Status FromErrno(int code) { return Status(code); }

  bool ok() const { return code_ == 0; }
  int code() const { return code_; }

  // Convenience predicates for common error categories.
  bool IsEOF() const { return code_ == TIN_EOF; }
  bool IsTimeout() const { return code_ == TIN_ETIMEOUT_INTR; }
  bool IsClosed() const { return code_ == TIN_OBJECT_CLOSED; }

  // Human-readable representation, e.g. "OK" or "EOF: end of file".
  std::string ToString() const;

 private:
  explicit Status(int code) : code_(code) {}
  int code_ = 0;
};

}  // namespace tin

#endif  // TIN_STATUS_H_
