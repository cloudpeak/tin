// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <cstdint>
#include <atomic>
#include <type_traits>

#include <cliff/base/atomicops.h>



namespace tin::atomic {

using namespace cliff;
  // --------------------------------


inline bool acquire_cas(volatile intptr_t* ptr,
                        intptr_t old_value,
                        intptr_t new_value) {
  return Acquire_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

inline bool release_cas(volatile intptr_t* ptr,
                        intptr_t old_value,
                        intptr_t new_value) {
  return Release_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

inline bool cas(volatile intptr_t* ptr,
                intptr_t old_value,
                intptr_t new_value) {
  cliff::MemoryBarrier();  // add release semantics.
  return Acquire_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

// exchange.
inline intptr_t acquire_exchange(volatile intptr_t* ptr, intptr_t new_value) {
  intptr_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  cliff::MemoryBarrier();
  return old_value;
}

inline intptr_t release_exchange(volatile intptr_t* ptr, intptr_t new_value) {
  cliff::MemoryBarrier();
  intptr_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  return old_value;
}

inline intptr_t exchange(volatile intptr_t* ptr, intptr_t new_value) {
  cliff::MemoryBarrier();
  intptr_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  cliff::MemoryBarrier();
  return old_value;
}

// store.
inline void relaxed_store(volatile intptr_t* ptr, intptr_t value) {
  return NoBarrier_Store(ptr, value);
}

inline void acquire_store(volatile intptr_t* ptr, intptr_t value) {
  return Acquire_Store(ptr, value);
}

inline void release_store(volatile intptr_t* ptr, intptr_t value) {
  return Release_Store(ptr, value);
}

inline void store(volatile intptr_t* ptr, intptr_t value) {
  cliff::MemoryBarrier();  // add release semantics.
  return Acquire_Store(ptr, value);
}

// load
inline intptr_t relaxed_load(volatile const intptr_t* ptr) {
  return NoBarrier_Load(ptr);
}

inline intptr_t acquire_load(volatile const intptr_t* ptr) {
  return Acquire_Load(ptr);
}

inline intptr_t release_load(volatile const intptr_t* ptr) {
  return Release_Load(ptr);
}

inline intptr_t load(volatile const intptr_t* ptr) {
  cliff::MemoryBarrier();  // add release semantics.
  return Acquire_Load(ptr);
}

// uintptr_t

inline bool acquire_cas(volatile uintptr_t* ptr,
                        uintptr_t old_value,
                        uintptr_t new_value) {
  return acquire_cas((volatile intptr_t*)(ptr), old_value, new_value);
}

inline bool release_cas(volatile uintptr_t* ptr,
                        uintptr_t old_value,
                        uintptr_t new_value) {
  return release_cas((volatile intptr_t*)(ptr), old_value, new_value);
}

inline bool cas(volatile uintptr_t* ptr,
                uintptr_t old_value,
                uintptr_t new_value) {
  return cas((volatile intptr_t*)(ptr), old_value, new_value);
}

// exchange.
inline uintptr_t acquire_exchange(volatile uintptr_t* ptr,
                                  uintptr_t new_value) {
  return acquire_exchange((volatile intptr_t*)(ptr), new_value);
}

inline uintptr_t release_exchange(volatile uintptr_t* ptr,
                                  uintptr_t new_value) {
  return release_exchange((volatile intptr_t*)(ptr), new_value);
}

inline uintptr_t exchange(volatile uintptr_t* ptr, uintptr_t new_value) {
  return exchange((volatile intptr_t*)(ptr), new_value);
}

// store.
inline void relaxed_store(volatile uintptr_t* ptr, uintptr_t value) {
  relaxed_store((volatile intptr_t*)ptr, value);
}

inline void acquire_store(volatile uintptr_t* ptr, uintptr_t value) {
  acquire_store((volatile intptr_t*)ptr, value);
}

inline void release_store(volatile uintptr_t* ptr, uintptr_t value) {
  release_store((volatile intptr_t*)ptr, value);
}

inline void store(volatile uintptr_t* ptr, uintptr_t value) {
  store((volatile intptr_t*)ptr, value);
}

// load
inline uintptr_t relaxed_load(volatile const uintptr_t* ptr) {
  return relaxed_load((volatile intptr_t*)ptr);
}

inline uintptr_t acquire_load(volatile const uintptr_t* ptr) {
  return acquire_load((volatile intptr_t*)ptr);
}

inline uintptr_t release_load(volatile const uintptr_t* ptr) {
  return release_load((volatile intptr_t*)ptr);
}

inline uintptr_t load(volatile const uintptr_t* ptr) {
  return load((volatile intptr_t*)ptr);
}

// 32 bit ops.
inline bool acquire_cas32(volatile int32_t* ptr,
                          int32_t old_value,
                          int32_t new_value) {
  return Acquire_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

inline bool release_cas32(volatile int32_t* ptr,
                          int32_t old_value,
                          int32_t new_value) {
  return Release_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

inline bool cas32(volatile int32_t* ptr,
                  int32_t old_value,
                  int32_t new_value) {
  cliff::MemoryBarrier();  // add release semantics.
  return Acquire_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

// exchange32.
inline int32_t acquire_exchange32(volatile int32_t* ptr, int32_t new_value) {
  int32_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  cliff::MemoryBarrier();
  return old_value;
}

inline int32_t release_exchange32(volatile int32_t* ptr, int32_t new_value) {
  cliff::MemoryBarrier();
  int32_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  return old_value;
}

inline int32_t exchange32(volatile int32_t* ptr, int32_t new_value) {
  cliff::MemoryBarrier();
  int32_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  cliff::MemoryBarrier();
  return old_value;
}

// store32.
inline void relaxed_store32(volatile int32_t* ptr, int32_t value) {
  return NoBarrier_Store(ptr, value);
}

inline void acquire_store32(volatile int32_t* ptr, int32_t value) {
  return Acquire_Store(ptr, value);
}

inline void release_store32(volatile int32_t* ptr, int32_t value) {
  return Release_Store(ptr, value);
}

inline void store32(volatile int32_t* ptr, int32_t value) {
  cliff::MemoryBarrier();  // add release semantics.
  return Acquire_Store(ptr, value);
}

// load32
inline int32_t relaxed_load32(volatile const int32_t* ptr) {
  return NoBarrier_Load(ptr);
}

inline int32_t acquire_load32(volatile const int32_t* ptr) {
  return Acquire_Load(ptr);
}

inline int32_t release_load32(volatile const int32_t* ptr) {
  return Release_Load(ptr);
}

inline int32_t load32(volatile const int32_t* ptr) {
  cliff::MemoryBarrier();  // add release semantics.
  return Acquire_Load(ptr);
}

inline int32_t relaxed_Inc32(volatile int32_t* ptr, int32_t increment) {
  return NoBarrier_AtomicIncrement(ptr, increment);
}

inline int32_t Inc32(volatile int32_t* ptr, int32_t increment) {
  return Barrier_AtomicIncrement(ptr, increment);
}


// uint32_t

inline bool acquire_cas32(volatile uint32_t* ptr,
                          uint32_t old_value,
                          uint32_t new_value) {
  return acquire_cas32((volatile int32_t*)(ptr), old_value, new_value);
}

inline bool release_cas32(volatile uint32_t* ptr,
                          uint32_t old_value,
                          uint32_t new_value) {
  return release_cas32((volatile int32_t*)(ptr), old_value, new_value);
}

inline bool cas32(volatile uint32_t* ptr,
                  uint32_t old_value,
                  uint32_t new_value) {
  return cas32((volatile int32_t*)(ptr), old_value, new_value);
}

// exchange32.
inline uint32_t acquire_exchange32(volatile uint32_t* ptr, uint32_t new_value) {
  return acquire_exchange32((volatile int32_t*)(ptr), new_value);
}

inline uint32_t release_exchange32(volatile uint32_t* ptr, uint32_t new_value) {
  return release_exchange32((volatile int32_t*)(ptr), new_value);
}

inline uint32_t exchange32(volatile uint32_t* ptr, uint32_t new_value) {
  return exchange32((volatile int32_t*)(ptr), new_value);
}

// store32.
inline void relaxed_store32(volatile uint32_t* ptr, uint32_t value) {
  relaxed_store32((volatile int32_t*)ptr, value);
}

inline void acquire_store32(volatile uint32_t* ptr, uint32_t value) {
  acquire_store32((volatile int32_t*)ptr, value);
}

inline void release_store32(volatile uint32_t* ptr, uint32_t value) {
  release_store32((volatile int32_t*)ptr, value);
}

inline void store32(volatile uint32_t* ptr, uint32_t value) {
  cliff::MemoryBarrier();  // add release semantics.
  store32((volatile int32_t*)ptr, value);
}

// load32
inline uint32_t relaxed_load32(volatile const uint32_t* ptr) {
  return relaxed_load32((volatile int32_t*)ptr);
}

inline uint32_t acquire_load32(volatile const uint32_t* ptr) {
  return acquire_load32((volatile int32_t*)ptr);
}

inline uint32_t release_load32(volatile const uint32_t* ptr) {
  return release_load32((volatile int32_t*)ptr);
}

inline uint32_t load32(volatile const uint32_t* ptr) {
  cliff::MemoryBarrier();  // add release semantics.
  return load32((volatile int32_t*)ptr);
}

inline int32_t relaxed_Inc32(volatile uint32_t* ptr, uint32_t increment) {
  return relaxed_Inc32((volatile int32_t*)ptr, (int32_t)increment);  // NOLINT
}

inline int32_t Inc32(volatile uint32_t* ptr, uint32_t increment) {
  return Inc32((volatile int32_t*)ptr, (int32_t)increment);  // NOLINT
}

} // namespace tin::atomic











