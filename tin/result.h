// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Result<T>: a value-plus-status union, inspired by Rust's Result<T, E> and
// std::expected. Used together with tin::Status to replace the global errno
// model. A Result<T> either holds a successfully computed T or a non-OK
// Status; the two are mutually exclusive.

#ifndef TIN_RESULT_H_
#define TIN_RESULT_H_

#include <utility>
#include "tin/status.h"

namespace tin {

template <typename T>
class Result {
 public:
  // Success constructor: implicitly creates Status::OK().
  Result(T value) : status_(), value_(std::move(value)) {}

  // Failure constructor: holds a non-OK status; value_ is default-initialised.
  Result(Status status) : status_(std::move(status)) {}

  bool ok() const { return status_.ok(); }
  const Status& status() const { return status_; }

  // Access the contained value. Behaviour is undefined if !ok().
  T& value() { return value_; }
  const T& value() const { return value_; }

 private:
  Status status_;
  T value_;
};

}  // namespace tin

#endif  // TIN_RESULT_H_
