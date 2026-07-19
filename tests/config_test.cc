// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for tin::Config.

#include "test.h"
#include "tin/config/config.h"
#include "tin/config/default.h"

#include <absl/log/check.h>

TEST(Config, DefaultConfig) {
  tin::Config c;
  c.SetMaxProcs(4);
  CHECK_EQ(c.MaxProcs(), 4);
}

TEST(Config, StackSize) {
  tin::Config c;
  c.SetStackSize(128 * 1024);
  CHECK_EQ(c.StackSize(), 128 * 1024);
}

TEST(Config, StackProtection) {
  tin::Config c;
  c.EnableStackProtection(true);
  CHECK(c.IsStackProtectionEnabled());
  c.EnableStackProtection(false);
  CHECK(!c.IsStackProtectionEnabled());
}

TEST(Config, IgnoreSigpipe) {
  tin::Config c;
  c.SetIgnoreSigpipe(false);
  CHECK(!c.IgnoreSigpipe());
  c.SetIgnoreSigpipe(true);
  CHECK(c.IgnoreSigpipe());
}

TEST(Config, MaxMachines) {
  tin::Config c;
  c.SetMaxMachines(8);
  CHECK_EQ(c.MaxOSMachines(), 8);
}

TEST(ConfigDefaults, DefaultStackSize) {
  CHECK_EQ(tin::kDefaultStackSize, 64 * 1024);
}
