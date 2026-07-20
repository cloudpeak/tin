// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for tin::Status and tin::Result<T>.

#include "test.h"
#include "tin/status.h"
#include "tin/result.h"
#include "tin/error/error.h"

#include <string>
#include <utility>

#include <absl/log/check.h>

// ---------------------------------------------------------------------------
// Status tests
// ---------------------------------------------------------------------------

TEST(Status, DefaultIsOk) {
  tin::Status s;
  CHECK(s.ok());
  CHECK_EQ(s.code(), 0);
}

TEST(Status, OKFactory) {
  tin::Status s = tin::Status::OK();
  CHECK(s.ok());
  CHECK_EQ(s.code(), 0);
}

TEST(Status, FromErrnoEOF) {
  tin::Status s = tin::Status::FromErrno(TIN_EOF);
  CHECK(!s.ok());
  CHECK(s.IsEOF());
  CHECK_EQ(s.code(), TIN_EOF);
}

TEST(Status, FromErrnoTimeout) {
  tin::Status s = tin::Status::FromErrno(TIN_ETIMEOUT_INTR);
  CHECK(!s.ok());
  CHECK(s.IsTimeout());
}

TEST(Status, FromErrnoClosed) {
  tin::Status s = tin::Status::FromErrno(TIN_OBJECT_CLOSED);
  CHECK(!s.ok());
  CHECK(s.IsClosed());
}

TEST(Status, FromErrnoUnexpectedEOF) {
  tin::Status s = tin::Status::FromErrno(TIN_UNEXPECTED_EOF);
  CHECK(!s.ok());
  CHECK(s.IsUnexpectedEOF());
}

TEST(Status, FromErrnoConnectionReset) {
  tin::Status s = tin::Status::FromErrno(TIN_ECONNRESET);
  CHECK(!s.ok());
  CHECK(s.IsConnectionReset());
}

TEST(Status, FromErrnoBrokenPipe) {
  tin::Status s = tin::Status::FromErrno(TIN_EPIPE);
  CHECK(!s.ok());
  CHECK(s.IsBrokenPipe());
}

TEST(Status, FromErrnoInvalid) {
  tin::Status s = tin::Status::FromErrno(TIN_EINVAL);
  CHECK(!s.ok());
  CHECK(s.IsInvalid());
}

TEST(Status, FromErrnoZeroIsOk) {
  tin::Status s = tin::Status::FromErrno(0);
  CHECK(s.ok());
}

TEST(Status, MakeAlias) {
  tin::Status s = tin::Status::Make(TIN_EOF);
  CHECK(!s.ok());
  CHECK(s.IsEOF());
  CHECK(s == tin::Status::FromErrno(TIN_EOF));
}

TEST(Status, ToStringOk) {
  tin::Status s;
  CHECK_EQ(s.ToString(), std::string("OK"));
}

TEST(Status, ToStringError) {
  tin::Status s = tin::Status::FromErrno(TIN_EOF);
  std::string str = s.ToString();
  CHECK(str != "OK");
  CHECK(str.find("EOF") != std::string::npos);
}

TEST(Status, ErrorNameOk) {
  tin::Status s;
  CHECK_EQ(s.ErrorName(), std::string("OK"));
}

TEST(Status, ErrorNameError) {
  tin::Status s = tin::Status::FromErrno(TIN_EOF);
  CHECK_EQ(s.ErrorName(), std::string("EOF"));
}

TEST(Status, ErrorSelfRef) {
  // Status::error() returns *this, enabling result.error().ToString().
  tin::Status s = tin::Status::FromErrno(TIN_EOF);
  CHECK_EQ(s.error().ToString(), s.ToString());
  CHECK(s.error().IsEOF());
}

TEST(Status, Comparison) {
  tin::Status ok1 = tin::Status::OK();
  tin::Status ok2;
  tin::Status err1 = tin::Status::FromErrno(TIN_EOF);
  tin::Status err2 = tin::Status::FromErrno(TIN_EOF);
  tin::Status err3 = tin::Status::FromErrno(TIN_EINVAL);

  CHECK(ok1 == ok2);
  CHECK(err1 == err2);
  CHECK(err1 != err3);
  CHECK(ok1 != err1);
}

TEST(Status, Copyable) {
  tin::Status s1 = tin::Status::FromErrno(TIN_EOF);
  tin::Status s2 = s1;
  CHECK(s1 == s2);
  CHECK(s2.IsEOF());
}

TEST(Status, Movable) {
  tin::Status s1 = tin::Status::FromErrno(TIN_EOF);
  tin::Status s2 = std::move(s1);
  CHECK(s2.IsEOF());
}

// ---------------------------------------------------------------------------
// Result<T> tests
// ---------------------------------------------------------------------------

TEST(Result, Success) {
  tin::Result<int> r(42);
  CHECK(r.ok());
  CHECK_EQ(r.value(), 42);
}

TEST(Result, Failure) {
  tin::Result<int> r(tin::Status::FromErrno(TIN_EOF));
  CHECK(!r.ok());
  CHECK(r.status().IsEOF());
}

TEST(Result, Copyable) {
  tin::Result<int> r1(10);
  tin::Result<int> r2 = r1;
  CHECK(r2.ok());
  CHECK_EQ(r2.value(), 10);
}

TEST(Result, Movable) {
  tin::Result<int> r1(10);
  tin::Result<int> r2 = std::move(r1);
  CHECK(r2.ok());
  CHECK_EQ(r2.value(), 10);
}

TEST(Result, OkFactory) {
  auto r = tin::Result<int>::Ok(42);
  CHECK(r.ok());
  CHECK_EQ(r.value(), 42);
}

TEST(Result, ErrFactoryFromCode) {
  auto r = tin::Result<int>::Err(TIN_EOF);
  CHECK(!r.ok());
  CHECK(r.error().IsEOF());
  CHECK_EQ(r.code(), TIN_EOF);
}

TEST(Result, ErrFactoryFromStatus) {
  auto r = tin::Result<int>::Err(tin::Status::FromErrno(TIN_EINVAL));
  CHECK(!r.ok());
  CHECK(r.error().IsInvalid());
}

TEST(Result, ValueOrOnSuccess) {
  tin::Result<int> r(42);
  CHECK_EQ(r.value_or(0), 42);
}

TEST(Result, ValueOrOnFailure) {
  tin::Result<int> r = tin::Result<int>::Err(TIN_EOF);
  CHECK_EQ(r.value_or(99), 99);
}

TEST(Result, HasValue) {
  tin::Result<int> ok_r(42);
  CHECK(ok_r.has_value());

  tin::Result<int> err_r = tin::Result<int>::Err(TIN_EOF);
  CHECK(!err_r.has_value());
}

TEST(Result, DereferenceOperators) {
  tin::Result<int> r(42);
  CHECK_EQ(*r, 42);
  // operator-> is mainly for pointer-like T, but int* works:
  int x = 10;
  tin::Result<int*> rp(&x);
  CHECK_EQ(*rp, &x);
  CHECK_EQ(**rp, 10);
}

TEST(Result, ErrorAlias) {
  // result.error() should return the same Status as result.status().
  auto r = tin::Result<int>::Err(TIN_EOF);
  CHECK(r.error() == r.status());
  CHECK(r.error().IsEOF());
  CHECK_EQ(r.error().ToString(), r.status().ToString());
}

TEST(Result, StringValue) {
  tin::Result<std::string> r(std::string("hello"));
  CHECK(r.ok());
  CHECK_EQ(r.value(), std::string("hello"));
}

TEST(Result, StringValueOr) {
  auto r = tin::Result<std::string>::Err(TIN_EOF);
  CHECK_EQ(r.value_or(std::string("default")), std::string("default"));
}

TEST(Result, SizeTypeValue) {
  tin::Result<size_t> r(static_cast<size_t>(1024));
  CHECK(r.ok());
  CHECK_EQ(r.value(), static_cast<size_t>(1024));
}

TEST(Result, ChainedErrorCheck) {
  // Simulate the pattern used in the echo example:
  //   auto result = conn.Read(buf, n);
  //   if (!result.ok()) { ... result.error().IsEOF() ... }
  auto r = tin::Result<size_t>::Err(TIN_EOF);
  if (!r.ok()) {
    CHECK(r.error().IsEOF());
    CHECK(!r.error().IsTimeout());
  }
}

TEST(Result, OkWithZero) {
  // Zero is a valid value, not an error.
  tin::Result<size_t> r(static_cast<size_t>(0));
  CHECK(r.ok());
  CHECK_EQ(r.value(), static_cast<size_t>(0));
}
