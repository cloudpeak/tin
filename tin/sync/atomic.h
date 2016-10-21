// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "base/atomicops.h"

namespace tin {
namespace atomic {

using namespace base::subtle;  // NOLINT

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
  base::subtle::MemoryBarrier();  // add release semantics.
  return Acquire_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

// exchange.
inline intptr_t acquire_exchange(volatile intptr_t* ptr, intptr_t new_value) {
  intptr_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  base::subtle::MemoryBarrier();
  return old_value;
}

inline intptr_t release_exchange(volatile intptr_t* ptr, intptr_t new_value) {
  base::subtle::MemoryBarrier();
  intptr_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  return old_value;
}

inline intptr_t exchange(volatile intptr_t* ptr, intptr_t new_value) {
  base::subtle::MemoryBarrier();
  intptr_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  base::subtle::MemoryBarrier();
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
  base::subtle::MemoryBarrier();  // add release semantics.
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
  base::subtle::MemoryBarrier();  // add release semantics.
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
inline bool acquire_cas32(volatile int32* ptr,
                          int32 old_value,
                          int32 new_value) {
  return Acquire_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

inline bool release_cas32(volatile int32* ptr,
                          int32 old_value,
                          int32 new_value) {
  return Release_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

inline bool cas32(volatile int32* ptr,
                  int32 old_value,
                  int32 new_value) {
  base::subtle::MemoryBarrier();  // add release semantics.
  return Acquire_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

// exchange32.
inline int32 acquire_exchange32(volatile int32* ptr, int32 new_value) {
  int32 old_value = NoBarrier_AtomicExchange(ptr, new_value);
  base::subtle::MemoryBarrier();
  return old_value;
}

inline int32 release_exchange32(volatile int32* ptr, int32 new_value) {
  base::subtle::MemoryBarrier();
  int32 old_value = NoBarrier_AtomicExchange(ptr, new_value);
  return old_value;
}

inline int32 exchange32(volatile int32* ptr, int32 new_value) {
  base::subtle::MemoryBarrier();
  int32 old_value = NoBarrier_AtomicExchange(ptr, new_value);
  base::subtle::MemoryBarrier();
  return old_value;
}

// store32.
inline void relaxed_store32(volatile int32* ptr, int32 value) {
  return NoBarrier_Store(ptr, value);
}

inline void acquire_store32(volatile int32* ptr, int32 value) {
  return Acquire_Store(ptr, value);
}

inline void release_store32(volatile int32* ptr, int32 value) {
  return Release_Store(ptr, value);
}

inline void store32(volatile int32* ptr, int32 value) {
  base::subtle::MemoryBarrier();  // add release semantics.
  return Acquire_Store(ptr, value);
}

// load32
inline int32 relaxed_load32(volatile const int32* ptr) {
  return NoBarrier_Load(ptr);
}

inline int32 acquire_load32(volatile const int32* ptr) {
  return Acquire_Load(ptr);
}

inline int32 release_load32(volatile const int32* ptr) {
  return Release_Load(ptr);
}

inline int32 load32(volatile const int32* ptr) {
  base::subtle::MemoryBarrier();  // add release semantics.
  return Acquire_Load(ptr);
}

inline int32 relaxed_Inc32(volatile int32* ptr, int32 increment) {
  return NoBarrier_AtomicIncrement(ptr, increment);
}

inline int32 Inc32(volatile int32* ptr, int32 increment) {
  return Barrier_AtomicIncrement(ptr, increment);
}


// uint32

inline bool acquire_cas32(volatile uint32* ptr,
                          uint32 old_value,
                          uint32 new_value) {
  return acquire_cas32((volatile int32*)(ptr), old_value, new_value);
}

inline bool release_cas32(volatile uint32* ptr,
                          uint32 old_value,
                          uint32 new_value) {
  return release_cas32((volatile int32*)(ptr), old_value, new_value);
}

inline bool cas32(volatile uint32* ptr,
                  uint32 old_value,
                  uint32 new_value) {
  return cas32((volatile int32*)(ptr), old_value, new_value);
}

// exchange32.
inline uint32 acquire_exchange32(volatile uint32* ptr, uint32 new_value) {
  return acquire_exchange32((volatile int32*)(ptr), new_value);
}

inline uint32 release_exchange32(volatile uint32* ptr, uint32 new_value) {
  return release_exchange32((volatile int32*)(ptr), new_value);
}

inline uint32 exchange32(volatile uint32* ptr, uint32 new_value) {
  return exchange32((volatile int32*)(ptr), new_value);
}

// store32.
inline void relaxed_store32(volatile uint32* ptr, uint32 value) {
  relaxed_store32((volatile int32*)ptr, value);
}

inline void acquire_store32(volatile uint32* ptr, uint32 value) {
  acquire_store32((volatile int32*)ptr, value);
}

inline void release_store32(volatile uint32* ptr, uint32 value) {
  release_store32((volatile int32*)ptr, value);
}

inline void store32(volatile uint32* ptr, uint32 value) {
  base::subtle::MemoryBarrier();  // add release semantics.
  store32((volatile int32*)ptr, value);
}

// load32
inline uint32 relaxed_load32(volatile const uint32* ptr) {
  return relaxed_load32((volatile int32*)ptr);
}

inline uint32 acquire_load32(volatile const uint32* ptr) {
  return acquire_load32((volatile int32*)ptr);
}

inline uint32 release_load32(volatile const uint32* ptr) {
  return release_load32((volatile int32*)ptr);
}

inline uint32 load32(volatile const uint32* ptr) {
  base::subtle::MemoryBarrier();  // add release semantics.
  return load32((volatile int32*)ptr);
}

inline int32 relaxed_Inc32(volatile uint32* ptr, uint32 increment) {
  return relaxed_Inc32((volatile int32*)ptr, (int32)increment);  // NOLINT
}

inline int32 Inc32(volatile uint32* ptr, uint32 increment) {
  return Inc32((volatile int32*)ptr, (int32)increment);  // NOLINT
}

}  // namespace atomic
}  // namespace tin










