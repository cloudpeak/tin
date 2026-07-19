// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for tin::atomic (thin wrappers over std::atomic).
// These tests verify that the wrapper functions delegate to std::atomic
// with the correct memory ordering. They run without runtime init since
// the atomic operations are pure C++ standard library calls.

#include "test.h"
#include "tin/sync/atomic.h"

#include <cstdint>
#include <thread>

#include <absl/log/check.h>

TEST(Atomic, Cas32Success) {
  int32_t val = 0;
  CHECK(tin::atomic::cas32(&val, 0, 42));
  CHECK_EQ(val, 42);
}

TEST(Atomic, Cas32Failure) {
  int32_t val = 10;
  CHECK(!tin::atomic::cas32(&val, 0, 42));  // old_value mismatch
  CHECK_EQ(val, 10);  // unchanged
}

TEST(Atomic, AcquireCas32Success) {
  int32_t val = 0;
  CHECK(tin::atomic::acquire_cas32(&val, 0, 100));
  CHECK_EQ(val, 100);
}

TEST(Atomic, ReleaseCas32Success) {
  int32_t val = 0;
  CHECK(tin::atomic::release_cas32(&val, 0, 200));
  CHECK_EQ(val, 200);
}

TEST(Atomic, StoreLoad32) {
  int32_t val = 0;
  tin::atomic::store32(&val, 12345);
  CHECK_EQ(tin::atomic::load32(&val), 12345);
}

TEST(Atomic, RelaxedStoreLoad32) {
  int32_t val = 0;
  tin::atomic::relaxed_store32(&val, 999);
  CHECK_EQ(tin::atomic::relaxed_load32(&val), 999);
}

TEST(Atomic, AcquireLoad32) {
  int32_t val = 42;
  CHECK_EQ(tin::atomic::acquire_load32(&val), 42);
}

TEST(Atomic, ReleaseStore32) {
  int32_t val = 0;
  tin::atomic::release_store32(&val, 777);
  CHECK_EQ(tin::atomic::acquire_load32(&val), 777);
}

TEST(Atomic, Inc32) {
  int32_t val = 0;
  CHECK_EQ(tin::atomic::Inc32(&val, 1), 1);
  CHECK_EQ(tin::atomic::Inc32(&val, 1), 2);
  CHECK_EQ(tin::atomic::Inc32(&val, -1), 1);
  CHECK_EQ(val, 1);
}

TEST(Atomic, RelaxedInc32) {
  int32_t val = 10;
  CHECK_EQ(tin::atomic::relaxed_Inc32(&val, 5), 15);
  CHECK_EQ(val, 15);
}

TEST(Atomic, Exchange32) {
  int32_t val = 100;
  CHECK_EQ(tin::atomic::exchange32(&val, 200), 100);
  CHECK_EQ(val, 200);
}

TEST(Atomic, CasPointerSized) {
  intptr_t val = 0;
  CHECK(tin::atomic::cas(&val, static_cast<intptr_t>(0),
                         static_cast<intptr_t>(0x12345678)));
  CHECK_EQ(val, 0x12345678);
}

TEST(Atomic, AcquireExchangePtr) {
  intptr_t val = 42;
  intptr_t old = tin::atomic::acquire_exchange(&val, static_cast<intptr_t>(99));
  CHECK_EQ(old, 42);
  CHECK_EQ(val, 99);
}

TEST(Atomic, MemoryBarrier) {
  // Just verify it compiles and doesn't crash.
  tin::atomic::memory_barrier();
}

TEST(Atomic, ConcurrentInc32) {
  int32_t val = 0;
  const int nthreads = 4;
  const int per_thread = 10000;
  std::thread threads[nthreads];
  for (int i = 0; i < nthreads; ++i) {
    threads[i] = std::thread([&]() {
      for (int j = 0; j < per_thread; ++j) {
        tin::atomic::Inc32(&val, 1);
      }
    });
  }
  for (int i = 0; i < nthreads; ++i) {
    threads[i].join();
  }
  CHECK_EQ(val, nthreads * per_thread);
}
