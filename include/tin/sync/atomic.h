// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_SYNC_ATOMIC_H_
#define TIN_SYNC_ATOMIC_H_
#include <cstdint>
#include <atomic>
#include <type_traits>

// ---------------------------------------------------------------------------
// tin::atomic —thin wrapper over std::atomic.
//
// History: this header previously forwarded to cliff:: (a Chromium //base
// shim) for atomic operations, pulling `using namespace cliff;` into the
// public API and permanently welding the cliff implementation detail to
// tin's public surface. The rewrite below uses std::atomic directly so that
// the cliff dependency is no longer leaked.
//
// The public function signatures are intentionally kept identical to the
// legacy API (volatile pointers, lowercase function names, `32` suffix for
// 32-bit variants) so that existing callers —atomic_flag.h, raw_mutex.h,
// m.h, netfd_common.h, etc. —compile without changes.
//
// The volatile qualifier on the pointer parameters is retained for source
// compatibility but is stripped internally via const_cast before
// reinterpret_cast to std::atomic<T>*. This is safe because std::atomic
// provides its own compiler fences; the volatile qualifier is redundant for
// atomic operations. The same pattern was used by the previous cliff shim.
// ---------------------------------------------------------------------------

namespace tin::atomic {

// Full memory barrier (sequential-consistency fence).
inline void memory_barrier() {
  std::atomic_thread_fence(std::memory_order_seq_cst);
}

// Internal helper: reinterpret a volatile T* as std::atomic<T>*.
// volatile is stripped (std::atomic provides its own ordering).
template <typename T>
inline std::atomic<T>* atomic_addr(volatile T* p) {
  return reinterpret_cast<std::atomic<T>*>(const_cast<T*>(p));
}

template <typename T>
inline const std::atomic<T>* atomic_addr(const volatile T* p) {
  return reinterpret_cast<const std::atomic<T>*>(const_cast<const T*>(p));
}

// ===========================================================================
// intptr_t operations
// ===========================================================================

// --- compare-and-swap ---

inline bool acquire_cas(volatile intptr_t* ptr,
                        intptr_t old_value,
                        intptr_t new_value) {
  return atomic_addr(ptr)->compare_exchange_strong(
      old_value, new_value,
      std::memory_order_acquire, std::memory_order_relaxed);
}

inline bool release_cas(volatile intptr_t* ptr,
                        intptr_t old_value,
                        intptr_t new_value) {
  return atomic_addr(ptr)->compare_exchange_strong(
      old_value, new_value,
      std::memory_order_release, std::memory_order_relaxed);
}

inline bool cas(volatile intptr_t* ptr,
                intptr_t old_value,
                intptr_t new_value) {
  memory_barrier();  // match legacy semantics
  return acquire_cas(ptr, old_value, new_value);
}

// --- exchange ---

inline intptr_t acquire_exchange(volatile intptr_t* ptr, intptr_t new_value) {
  intptr_t old_value =
      atomic_addr(ptr)->exchange(new_value, std::memory_order_relaxed);
  memory_barrier();
  return old_value;
}

inline intptr_t release_exchange(volatile intptr_t* ptr, intptr_t new_value) {
  memory_barrier();
  return atomic_addr(ptr)->exchange(new_value, std::memory_order_relaxed);
}

inline intptr_t exchange(volatile intptr_t* ptr, intptr_t new_value) {
  memory_barrier();
  intptr_t old_value =
      atomic_addr(ptr)->exchange(new_value, std::memory_order_relaxed);
  memory_barrier();
  return old_value;
}

// --- store ---

inline void relaxed_store(volatile intptr_t* ptr, intptr_t value) {
  atomic_addr(ptr)->store(value, std::memory_order_relaxed);
}

inline void acquire_store(volatile intptr_t* ptr, intptr_t value) {
  // Legacy Acquire_Store = relaxed store + barrier.
  atomic_addr(ptr)->store(value, std::memory_order_relaxed);
  memory_barrier();
}

inline void release_store(volatile intptr_t* ptr, intptr_t value) {
  atomic_addr(ptr)->store(value, std::memory_order_release);
}

inline void store(volatile intptr_t* ptr, intptr_t value) {
  // Legacy store = barrier + Acquire_Store = barrier + relaxed store + barrier.
  memory_barrier();
  atomic_addr(ptr)->store(value, std::memory_order_relaxed);
  memory_barrier();
}

// --- load ---

inline intptr_t relaxed_load(volatile const intptr_t* ptr) {
  return atomic_addr(ptr)->load(std::memory_order_relaxed);
}

inline intptr_t acquire_load(volatile const intptr_t* ptr) {
  return atomic_addr(ptr)->load(std::memory_order_acquire);
}

inline intptr_t release_load(volatile const intptr_t* ptr) {
  // Legacy Release_Load = barrier + relaxed load.
  memory_barrier();
  return atomic_addr(ptr)->load(std::memory_order_relaxed);
}

inline intptr_t load(volatile const intptr_t* ptr) {
  // Legacy load = barrier + Acquire_Load = barrier + acquire load.
  memory_barrier();
  return atomic_addr(ptr)->load(std::memory_order_acquire);
}

// ===========================================================================
// uintptr_t operations (delegate to intptr_t via cast)
// ===========================================================================

inline bool acquire_cas(volatile uintptr_t* ptr,
                        uintptr_t old_value,
                        uintptr_t new_value) {
  return acquire_cas(reinterpret_cast<volatile intptr_t*>(ptr),
                     static_cast<intptr_t>(old_value),
                     static_cast<intptr_t>(new_value));
}

inline bool release_cas(volatile uintptr_t* ptr,
                        uintptr_t old_value,
                        uintptr_t new_value) {
  return release_cas(reinterpret_cast<volatile intptr_t*>(ptr),
                     static_cast<intptr_t>(old_value),
                     static_cast<intptr_t>(new_value));
}

inline bool cas(volatile uintptr_t* ptr,
                uintptr_t old_value,
                uintptr_t new_value) {
  return cas(reinterpret_cast<volatile intptr_t*>(ptr),
             static_cast<intptr_t>(old_value),
             static_cast<intptr_t>(new_value));
}

inline uintptr_t acquire_exchange(volatile uintptr_t* ptr, uintptr_t new_value) {
  return static_cast<uintptr_t>(acquire_exchange(
      reinterpret_cast<volatile intptr_t*>(ptr),
      static_cast<intptr_t>(new_value)));
}

inline uintptr_t release_exchange(volatile uintptr_t* ptr, uintptr_t new_value) {
  return static_cast<uintptr_t>(release_exchange(
      reinterpret_cast<volatile intptr_t*>(ptr),
      static_cast<intptr_t>(new_value)));
}

inline uintptr_t exchange(volatile uintptr_t* ptr, uintptr_t new_value) {
  return static_cast<uintptr_t>(exchange(
      reinterpret_cast<volatile intptr_t*>(ptr),
      static_cast<intptr_t>(new_value)));
}

inline void relaxed_store(volatile uintptr_t* ptr, uintptr_t value) {
  relaxed_store(reinterpret_cast<volatile intptr_t*>(ptr),
                static_cast<intptr_t>(value));
}

inline void acquire_store(volatile uintptr_t* ptr, uintptr_t value) {
  acquire_store(reinterpret_cast<volatile intptr_t*>(ptr),
                static_cast<intptr_t>(value));
}

inline void release_store(volatile uintptr_t* ptr, uintptr_t value) {
  release_store(reinterpret_cast<volatile intptr_t*>(ptr),
                static_cast<intptr_t>(value));
}

inline void store(volatile uintptr_t* ptr, uintptr_t value) {
  store(reinterpret_cast<volatile intptr_t*>(ptr),
        static_cast<intptr_t>(value));
}

inline uintptr_t relaxed_load(volatile const uintptr_t* ptr) {
  return static_cast<uintptr_t>(relaxed_load(
      reinterpret_cast<volatile const intptr_t*>(ptr)));
}

inline uintptr_t acquire_load(volatile const uintptr_t* ptr) {
  return static_cast<uintptr_t>(acquire_load(
      reinterpret_cast<volatile const intptr_t*>(ptr)));
}

inline uintptr_t release_load(volatile const uintptr_t* ptr) {
  return static_cast<uintptr_t>(release_load(
      reinterpret_cast<volatile const intptr_t*>(ptr)));
}

inline uintptr_t load(volatile const uintptr_t* ptr) {
  return static_cast<uintptr_t>(load(
      reinterpret_cast<volatile const intptr_t*>(ptr)));
}

// ===========================================================================
// int32_t operations
// ===========================================================================

// --- compare-and-swap ---

inline bool acquire_cas32(volatile int32_t* ptr,
                          int32_t old_value,
                          int32_t new_value) {
  return atomic_addr(ptr)->compare_exchange_strong(
      old_value, new_value,
      std::memory_order_acquire, std::memory_order_relaxed);
}

inline bool release_cas32(volatile int32_t* ptr,
                          int32_t old_value,
                          int32_t new_value) {
  return atomic_addr(ptr)->compare_exchange_strong(
      old_value, new_value,
      std::memory_order_release, std::memory_order_relaxed);
}

inline bool cas32(volatile int32_t* ptr,
                  int32_t old_value,
                  int32_t new_value) {
  memory_barrier();
  return acquire_cas32(ptr, old_value, new_value);
}

// --- exchange32 ---

inline int32_t acquire_exchange32(volatile int32_t* ptr, int32_t new_value) {
  int32_t old_value =
      atomic_addr(ptr)->exchange(new_value, std::memory_order_relaxed);
  memory_barrier();
  return old_value;
}

inline int32_t release_exchange32(volatile int32_t* ptr, int32_t new_value) {
  memory_barrier();
  return atomic_addr(ptr)->exchange(new_value, std::memory_order_relaxed);
}

inline int32_t exchange32(volatile int32_t* ptr, int32_t new_value) {
  memory_barrier();
  int32_t old_value =
      atomic_addr(ptr)->exchange(new_value, std::memory_order_relaxed);
  memory_barrier();
  return old_value;
}

// --- store32 ---

inline void relaxed_store32(volatile int32_t* ptr, int32_t value) {
  atomic_addr(ptr)->store(value, std::memory_order_relaxed);
}

inline void acquire_store32(volatile int32_t* ptr, int32_t value) {
  atomic_addr(ptr)->store(value, std::memory_order_relaxed);
  memory_barrier();
}

inline void release_store32(volatile int32_t* ptr, int32_t value) {
  atomic_addr(ptr)->store(value, std::memory_order_release);
}

inline void store32(volatile int32_t* ptr, int32_t value) {
  memory_barrier();
  atomic_addr(ptr)->store(value, std::memory_order_relaxed);
  memory_barrier();
}

// --- load32 ---

inline int32_t relaxed_load32(volatile const int32_t* ptr) {
  return atomic_addr(ptr)->load(std::memory_order_relaxed);
}

inline int32_t acquire_load32(volatile const int32_t* ptr) {
  return atomic_addr(ptr)->load(std::memory_order_acquire);
}

inline int32_t release_load32(volatile const int32_t* ptr) {
  memory_barrier();
  return atomic_addr(ptr)->load(std::memory_order_relaxed);
}

inline int32_t load32(volatile const int32_t* ptr) {
  memory_barrier();
  return atomic_addr(ptr)->load(std::memory_order_acquire);
}

// --- increment32 ---

inline int32_t relaxed_inc32(volatile int32_t* ptr, int32_t increment) {
  // fetch_add returns the old value; the legacy NoBarrier_AtomicIncrement
  // returns the new value.
  return atomic_addr(ptr)->fetch_add(increment, std::memory_order_relaxed)
         + increment;
}

inline int32_t inc32(volatile int32_t* ptr, int32_t increment) {
  return atomic_addr(ptr)->fetch_add(increment, std::memory_order_seq_cst)
         + increment;
}

// ===========================================================================
// uint32_t operations (delegate to int32_t via cast)
// ===========================================================================

inline bool acquire_cas32(volatile uint32_t* ptr,
                          uint32_t old_value,
                          uint32_t new_value) {
  return acquire_cas32(reinterpret_cast<volatile int32_t*>(ptr),
                       static_cast<int32_t>(old_value),
                       static_cast<int32_t>(new_value));
}

inline bool release_cas32(volatile uint32_t* ptr,
                          uint32_t old_value,
                          uint32_t new_value) {
  return release_cas32(reinterpret_cast<volatile int32_t*>(ptr),
                       static_cast<int32_t>(old_value),
                       static_cast<int32_t>(new_value));
}

inline bool cas32(volatile uint32_t* ptr,
                  uint32_t old_value,
                  uint32_t new_value) {
  return cas32(reinterpret_cast<volatile int32_t*>(ptr),
               static_cast<int32_t>(old_value),
               static_cast<int32_t>(new_value));
}

inline uint32_t acquire_exchange32(volatile uint32_t* ptr, uint32_t new_value) {
  return static_cast<uint32_t>(acquire_exchange32(
      reinterpret_cast<volatile int32_t*>(ptr),
      static_cast<int32_t>(new_value)));
}

inline uint32_t release_exchange32(volatile uint32_t* ptr, uint32_t new_value) {
  return static_cast<uint32_t>(release_exchange32(
      reinterpret_cast<volatile int32_t*>(ptr),
      static_cast<int32_t>(new_value)));
}

inline uint32_t exchange32(volatile uint32_t* ptr, uint32_t new_value) {
  return static_cast<uint32_t>(exchange32(
      reinterpret_cast<volatile int32_t*>(ptr),
      static_cast<int32_t>(new_value)));
}

inline void relaxed_store32(volatile uint32_t* ptr, uint32_t value) {
  relaxed_store32(reinterpret_cast<volatile int32_t*>(ptr),
                  static_cast<int32_t>(value));
}

inline void acquire_store32(volatile uint32_t* ptr, uint32_t value) {
  acquire_store32(reinterpret_cast<volatile int32_t*>(ptr),
                  static_cast<int32_t>(value));
}

inline void release_store32(volatile uint32_t* ptr, uint32_t value) {
  release_store32(reinterpret_cast<volatile int32_t*>(ptr),
                  static_cast<int32_t>(value));
}

inline void store32(volatile uint32_t* ptr, uint32_t value) {
  store32(reinterpret_cast<volatile int32_t*>(ptr),
          static_cast<int32_t>(value));
}

inline uint32_t relaxed_load32(volatile const uint32_t* ptr) {
  return static_cast<uint32_t>(relaxed_load32(
      reinterpret_cast<volatile const int32_t*>(ptr)));
}

inline uint32_t acquire_load32(volatile const uint32_t* ptr) {
  return static_cast<uint32_t>(acquire_load32(
      reinterpret_cast<volatile const int32_t*>(ptr)));
}

inline uint32_t release_load32(volatile const uint32_t* ptr) {
  return static_cast<uint32_t>(release_load32(
      reinterpret_cast<volatile const int32_t*>(ptr)));
}

inline uint32_t load32(volatile const uint32_t* ptr) {
  return static_cast<uint32_t>(load32(
      reinterpret_cast<volatile const int32_t*>(ptr)));
}

inline int32_t relaxed_inc32(volatile uint32_t* ptr, uint32_t increment) {
  return relaxed_inc32(reinterpret_cast<volatile int32_t*>(ptr),
                       static_cast<int32_t>(increment));
}

inline int32_t inc32(volatile uint32_t* ptr, uint32_t increment) {
  return inc32(reinterpret_cast<volatile int32_t*>(ptr),
               static_cast<int32_t>(increment));
}

// ===========================================================================
// int64_t operations (ref: Go 1.15 internal/atomic/atomic_amd64.go:39-92)
// ===========================================================================

// --- compare-and-swap 64 ---

inline bool acquire_cas64(volatile int64_t* ptr,
                          int64_t old_value,
                          int64_t new_value) {
  return atomic_addr(ptr)->compare_exchange_strong(
      old_value, new_value,
      std::memory_order_acquire, std::memory_order_relaxed);
}

inline bool release_cas64(volatile int64_t* ptr,
                          int64_t old_value,
                          int64_t new_value) {
  return atomic_addr(ptr)->compare_exchange_strong(
      old_value, new_value,
      std::memory_order_release, std::memory_order_relaxed);
}

inline bool cas64(volatile int64_t* ptr,
                  int64_t old_value,
                  int64_t new_value) {
  memory_barrier();
  return acquire_cas64(ptr, old_value, new_value);
}

// --- store64 ---

inline void relaxed_store64(volatile int64_t* ptr, int64_t value) {
  atomic_addr(ptr)->store(value, std::memory_order_relaxed);
}

inline void release_store64(volatile int64_t* ptr, int64_t value) {
  atomic_addr(ptr)->store(value, std::memory_order_release);
}

inline void store64(volatile int64_t* ptr, int64_t value) {
  memory_barrier();
  atomic_addr(ptr)->store(value, std::memory_order_relaxed);
  memory_barrier();
}

// --- load64 ---

inline int64_t relaxed_load64(volatile const int64_t* ptr) {
  return atomic_addr(ptr)->load(std::memory_order_relaxed);
}

inline int64_t acquire_load64(volatile const int64_t* ptr) {
  return atomic_addr(ptr)->load(std::memory_order_acquire);
}

inline int64_t load64(volatile const int64_t* ptr) {
  memory_barrier();
  return atomic_addr(ptr)->load(std::memory_order_acquire);
}

// --- exchange64 ---

inline int64_t exchange64(volatile int64_t* ptr, int64_t new_value) {
  memory_barrier();
  int64_t old_value =
      atomic_addr(ptr)->exchange(new_value, std::memory_order_relaxed);
  memory_barrier();
  return old_value;
}

// --- increment64 ---

inline int64_t relaxed_inc64(volatile int64_t* ptr, int64_t increment) {
  return atomic_addr(ptr)->fetch_add(increment, std::memory_order_relaxed)
         + increment;
}

inline int64_t inc64(volatile int64_t* ptr, int64_t increment) {
  return atomic_addr(ptr)->fetch_add(increment, std::memory_order_seq_cst)
         + increment;
}

// ===========================================================================
// uint64_t operations (delegate to int64_t via cast)
// ===========================================================================

inline bool acquire_cas64(volatile uint64_t* ptr,
                          uint64_t old_value,
                          uint64_t new_value) {
  return acquire_cas64(reinterpret_cast<volatile int64_t*>(ptr),
                       static_cast<int64_t>(old_value),
                       static_cast<int64_t>(new_value));
}

inline bool release_cas64(volatile uint64_t* ptr,
                          uint64_t old_value,
                          uint64_t new_value) {
  return release_cas64(reinterpret_cast<volatile int64_t*>(ptr),
                       static_cast<int64_t>(old_value),
                       static_cast<int64_t>(new_value));
}

inline bool cas64(volatile uint64_t* ptr,
                  uint64_t old_value,
                  uint64_t new_value) {
  return cas64(reinterpret_cast<volatile int64_t*>(ptr),
               static_cast<int64_t>(old_value),
               static_cast<int64_t>(new_value));
}

inline void relaxed_store64(volatile uint64_t* ptr, uint64_t value) {
  relaxed_store64(reinterpret_cast<volatile int64_t*>(ptr),
                  static_cast<int64_t>(value));
}

inline void release_store64(volatile uint64_t* ptr, uint64_t value) {
  release_store64(reinterpret_cast<volatile int64_t*>(ptr),
                  static_cast<int64_t>(value));
}

inline void store64(volatile uint64_t* ptr, uint64_t value) {
  store64(reinterpret_cast<volatile int64_t*>(ptr),
          static_cast<int64_t>(value));
}

inline uint64_t relaxed_load64(volatile const uint64_t* ptr) {
  return static_cast<uint64_t>(relaxed_load64(
      reinterpret_cast<volatile const int64_t*>(ptr)));
}

inline uint64_t acquire_load64(volatile const uint64_t* ptr) {
  return static_cast<uint64_t>(acquire_load64(
      reinterpret_cast<volatile const int64_t*>(ptr)));
}

inline uint64_t load64(volatile const uint64_t* ptr) {
  return static_cast<uint64_t>(load64(
      reinterpret_cast<volatile const int64_t*>(ptr)));
}

inline uint64_t exchange64(volatile uint64_t* ptr, uint64_t new_value) {
  return static_cast<uint64_t>(exchange64(
      reinterpret_cast<volatile int64_t*>(ptr),
      static_cast<int64_t>(new_value)));
}

inline int64_t relaxed_inc64(volatile uint64_t* ptr, uint64_t increment) {
  return relaxed_inc64(reinterpret_cast<volatile int64_t*>(ptr),
                       static_cast<int64_t>(increment));
}

inline int64_t inc64(volatile uint64_t* ptr, uint64_t increment) {
  return inc64(reinterpret_cast<volatile int64_t*>(ptr),
               static_cast<int64_t>(increment));
}

// ===========================================================================
// 8-bit bitwise operations (ref: Go 1.15 atomic_amd64.go:63-66)
// ===========================================================================

inline void and8(volatile uint8_t* ptr, uint8_t v) {
  atomic_addr(ptr)->fetch_and(v, std::memory_order_relaxed);
}

inline void or8(volatile uint8_t* ptr, uint8_t v) {
  atomic_addr(ptr)->fetch_or(v, std::memory_order_relaxed);
}

}  // namespace tin::atomic
#endif  // TIN_SYNC_ATOMIC_H_
