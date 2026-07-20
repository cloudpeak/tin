// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Minimal test framework header: no GTest dependency. Each test file
// includes this header and uses the TEST() macro to register tests.
// test_main.cc provides the main() runner.

#ifndef TIN_TESTS_TEST_H_
#define TIN_TESTS_TEST_H_

#include <functional>
#include <string>
#include <vector>

struct TestEntry {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<TestEntry>& TestRegistry() {
  static std::vector<TestEntry> registry;
  return registry;
}

struct TestRegistrar {
  TestRegistrar(const char* name, std::function<void()> fn) {
    TestRegistry().push_back({name, std::move(fn)});
  }
};

// Usage: TEST(Status, Ok) { ... }
#define TEST(suite, name)                                          \
  static void suite##_##name##_test();                             \
  static TestRegistrar registrar_##suite##_##name(                 \
      #suite "." #name, suite##_##name##_test);                    \
  static void suite##_##name##_test()

#endif  // TIN_TESTS_TEST_H_
