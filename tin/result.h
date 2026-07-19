// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Result<T> — a value-semantic result type inspired by Rust's Result<T, E>
// and C++23's std::expected. Combines a Status (error) with a value T.
//
// Usage:
//   Result<size_t> ReadResult(void* buf, int nbytes);
//
//   auto result = conn->ReadResult(buf, sizeof(buf));
//   if (!result.ok()) {
//     LOG(ERROR) << "read failed: " << result.status().ToString();
//     return;
//   }
//   process(result.value());

#ifndef TIN_RESULT_H_
#define TIN_RESULT_H_

#include <utility>
#include "tin/status.h"

namespace tin {

template <typename T>
class Result {
 public:
  // Success: construct with a value.
  Result(T value) : status_(Status::OK()), value_(std::move(value)) {}

  // Failure: construct with an error Status.
  Result(Status status) : status_(std::move(status)) {}

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }
  int code() const { return status_.code(); }

  // Access the stored value. Calling value() when !ok() is undefined behavior
  // (the value_ member is default-initialized in the error case).
  T& value() { return value_; }
  const T& value() const { return value_; }

 private:
  Status status_;
  T value_;
};

}  // namespace tin

#endif  // TIN_RESULT_H_
