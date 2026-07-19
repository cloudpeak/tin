// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for tin::net::IPAddress.

#include "test.h"
#include "tin/net/ip_address.h"

#include <absl/log/check.h>

TEST(IPAddress, ParseIPv4) {
  tin::net::IPAddress addr;
  CHECK(addr.AssignFromIPLiteral("127.0.0.1"));
  CHECK(addr.IsIPv4());
  CHECK(!addr.IsIPv6());
}

TEST(IPAddress, ParseIPv6) {
  tin::net::IPAddress addr;
  CHECK(addr.AssignFromIPLiteral("::1"));
  CHECK(addr.IsIPv6());
  CHECK(!addr.IsIPv4());
}

TEST(IPAddress, ParseInvalid) {
  tin::net::IPAddress addr;
  CHECK(!addr.AssignFromIPLiteral("not.an.ip.address"));
}

TEST(IPAddress, ParseAnyIPv4) {
  tin::net::IPAddress addr;
  CHECK(addr.AssignFromIPLiteral("0.0.0.0"));
  CHECK(addr.IsIPv4());
}

TEST(IPAddress, ParseAnyIPv6) {
  tin::net::IPAddress addr;
  CHECK(addr.AssignFromIPLiteral("::"));
  CHECK(addr.IsIPv6());
}
