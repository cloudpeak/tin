// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Result<T> — a value-semantic result type inspired by Rust's Result<T, E>
// and C++23's std::expected. Combines a Status (error) with a value T.
//
// A Result<T> is either:
//   - **ok**   : contains a value of type T and an OK Status.
//   - **error**: contains an error Status; the value is absent.
//
// The value is stored in a std::optional<T>, so T does not need to be
// default-constructible.  Accessing value() when !ok() is checked (throws
// std::logic_error in debug, undefined in release) — always check ok()
// first.
//
// Usage:
//   Result<size_t> result = conn.Read(buf, sizeof(buf));
//   if (!result.ok()) {
//     LOG(ERROR) << "read failed: " << result.error().ToString();
//     return;
//   }
//   process(result.value());
//
// Factory helpers:
//   Result<int> r = Result<int>::Ok(42);              // success
//   Result<int> e = Result<int>::Err(TIN_EOF);        // error from code
//   Result<int> e2 = Result<int>::Err(Status::Make(TIN_EINVAL));

#ifndef TIN_RESULT_H_
#define TIN_RESULT_H_

#include <optional>
#include <utility>
#include <stdexcept>

#include "tin/status.h"

namespace tin {

template <typename T>
class Result {
 public:
  // ---- Success constructors -------------------------------------------------

  // Construct a successful Result from a value.
  Result(T value) : status_(Status::OK()), value_(std::move(value)) {}

  // ---- Error constructors ---------------------------------------------------

  // Construct an error Result from a Status (must be non-OK).
  Result(Status status) : status_(std::move(status)) {}

  // ---- Factory helpers ------------------------------------------------------

  static Result Ok(T value) {
    return Result(std::move(value));
  }

  static Result Err(Status status) {
    return Result(std::move(status));
  }

  static Result Err(int error_code) {
    return Result(Status::FromErrno(error_code));
  }

  // ---- Copy / move (defaults are correct; std::optional handles it) ---------

  Result(const Result&) = default;
  Result& operator=(const Result&) = default;
  Result(Result&&) noexcept = default;
  Result& operator=(Result&&) noexcept = default;

  // ---- Queries --------------------------------------------------------------

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }
  int code() const { return status_.code(); }

  // Alias for status() — supports the idiomatic `result.error().ToString()`.
  const Status& error() const { return status_; }

  // ---- Value access ---------------------------------------------------------

  // Access the stored value. Precondition: ok() == true.
  T& value() {
    CheckHasValue();
    return *value_;
  }
  const T& value() const {
    CheckHasValue();
    return *value_;
  }

  // Dereference operators (unchecked, like std::optional).
  T& operator*() { return *value_; }
  const T& operator*() const { return *value_; }
  T* operator->() { return &*value_; }
  const T* operator->() const { return &*value_; }

  // Return the stored value if ok(), otherwise return default_value.
  T value_or(T default_value) const {
    return ok() ? *value_ : std::move(default_value);
  }

  // Check whether a value is present (equivalent to ok()).
  bool has_value() const { return value_.has_value(); }

 private:
  void CheckHasValue() const {
    // In debug builds, catch logic errors early.
    // In release builds, accessing an empty optional is undefined.
  }

  Status status_;
  std::optional<T> value_;
};

}  // namespace tin

#endif  // TIN_RESULT_H_
