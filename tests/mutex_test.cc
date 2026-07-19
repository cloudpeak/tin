// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for tin::Mutex (self-made, not std::mutex).
// These tests exercise the fast-path (uncontended) Lock/Unlock/TryLock
// and the MutexGuard RAII wrapper. Contended tests require runtime
// initialization and are left for integration tests.

#include "test.h"
#include "tin/sync/mutex.h"

#include <absl/log/check.h>

TEST(Mutex, LockUnlock) {
  tin::Mutex mu;
  mu.Lock();
  mu.Unlock();
}

TEST(Mutex, TryLockUnlocked) {
  tin::Mutex mu;
  CHECK(mu.TryLock());
  mu.Unlock();
}

TEST(Mutex, TryLockLocked) {
  tin::Mutex mu;
  mu.Lock();
  // TryLock should fail — mutex is already held.
  CHECK(!mu.TryLock());
  mu.Unlock();
  // After unlock, TryLock should succeed again.
  CHECK(mu.TryLock());
  mu.Unlock();
}

TEST(Mutex, MutexGuardRAII) {
  tin::Mutex mu;
  {
    tin::MutexGuard guard(&mu);
    // Mutex is held inside this scope.
    CHECK(!mu.TryLock());
  }
  // After scope exit, mutex is released.
  CHECK(mu.TryLock());
  mu.Unlock();
}

TEST(Mutex, RepeatedLockUnlock) {
  tin::Mutex mu;
  for (int i = 0; i < 100; ++i) {
    mu.Lock();
    mu.Unlock();
  }
  // Verify the mutex is still usable.
  CHECK(mu.TryLock());
  mu.Unlock();
}
