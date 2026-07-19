// Copyright (c) 2026 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Shim that re-exports base::subtle atomic operations into cliff::, and
// provides the small set of helpers tin's tin/sync/atomic.h expects:
//   - cliff::MemoryBarrier()
//   - Acquire_Store / Release_Load (not provided by base::subtle for
//     Atomic64; emulated via std::atomic_thread_fence + NoBarrier_{Store,Load})
//   - NoBarrier_Store for Atomic64 (also missing from base::subtle's
//     portable implementation)
// The remaining operations (Acquire_CompareAndSwap, Release_CompareAndSwap,
// NoBarrier_AtomicExchange, NoBarrier_AtomicIncrement, NoBarrier_Load,
// Acquire_Load, Release_Store, ...) are re-exported via using-declarations.

#ifndef CLIFF_SHIM_BASE_ATOMICOPS_H_
#define CLIFF_SHIM_BASE_ATOMICOPS_H_

#include <atomic>

#include "base/atomicops.h"

namespace cliff {

// Full memory barrier.
inline void MemoryBarrier() {
  std::atomic_thread_fence(std::memory_order_seq_cst);
}

using ::base::subtle::NoBarrier_CompareAndSwap;
using ::base::subtle::NoBarrier_AtomicExchange;
using ::base::subtle::NoBarrier_AtomicIncrement;
using ::base::subtle::Barrier_AtomicIncrement;
using ::base::subtle::Acquire_CompareAndSwap;
using ::base::subtle::Release_CompareAndSwap;
using ::base::subtle::Release_Store;
using ::base::subtle::NoBarrier_Load;
using ::base::subtle::Acquire_Load;

// NoBarrier_Store for Atomic32 exists in base::subtle; re-export it.
using ::base::subtle::NoBarrier_Store;

// Acquire_Store and Release_Load are not exposed by base::subtle. Emulate
// them as MemoryBarrier + NoBarrier_{Store,Load}, matching the semantics
// of the legacy Chromium atomicops interface.
inline void Acquire_Store(volatile ::base::subtle::Atomic32* ptr,
                          ::base::subtle::Atomic32 value) {
  ::base::subtle::NoBarrier_Store(ptr, value);
  MemoryBarrier();
}
inline ::base::subtle::Atomic32 Release_Load(
    volatile const ::base::subtle::Atomic32* ptr) {
  MemoryBarrier();
  return ::base::subtle::NoBarrier_Load(ptr);
}

#if defined(ARCH_CPU_64_BITS)
// base::subtle's portable implementation omits NoBarrier_Store for
// Atomic64. Provide it directly via std::atomic with relaxed ordering.
inline void NoBarrier_Store(volatile ::base::subtle::Atomic64* ptr,
                            ::base::subtle::Atomic64 value) {
  reinterpret_cast<volatile std::atomic<::base::subtle::Atomic64>*>(ptr)->store(
      value, std::memory_order_relaxed);
}

inline void Acquire_Store(volatile ::base::subtle::Atomic64* ptr,
                          ::base::subtle::Atomic64 value) {
  NoBarrier_Store(ptr, value);
  MemoryBarrier();
}
inline ::base::subtle::Atomic64 Release_Load(
    volatile const ::base::subtle::Atomic64* ptr) {
  MemoryBarrier();
  return ::base::subtle::NoBarrier_Load(ptr);
}
#endif  // defined(ARCH_CPU_64_BITS)

}  // namespace cliff

#endif  // CLIFF_SHIM_BASE_ATOMICOPS_H_
