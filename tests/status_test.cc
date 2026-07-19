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

#include <absl/log/check.h>

TEST(Status, DefaultIsOk) {
  tin::Status s;
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

TEST(Status, FromErrnoZeroIsOk) {
  tin::Status s = tin::Status::FromErrno(0);
  CHECK(s.ok());
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
