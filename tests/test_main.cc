// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Minimal test runner: no GTest dependency. The test registry and TEST()
// macro live in test.h; this file provides main() and iterates the registry.

#include <cstdio>
#include <cstdlib>

#include "test.h"

int main() {
  int passed = 0;
  int failed = 0;

  for (const auto& entry : TestRegistry()) {
    printf("[ RUN      ] %s\n", entry.name.c_str());
    try {
      entry.fn();
      printf("[       OK ] %s\n", entry.name.c_str());
      ++passed;
    } catch (const std::exception& e) {
      printf("[  FAILED  ] %s: %s\n", entry.name.c_str(), e.what());
      ++failed;
    } catch (...) {
      printf("[  FAILED  ] %s: unknown exception\n", entry.name.c_str());
      ++failed;
    }
  }

  printf("\n");
  printf("Passed: %d\n", passed);
  printf("Failed: %d\n", failed);
  printf("Total:  %d\n", passed + failed);
  printf("\n");
  return failed == 0 ? 0 : 1;
}
