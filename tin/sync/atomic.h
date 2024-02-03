// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <cstdint>
#include <atomic>
#include <type_traits>


namespace tin {
namespace atomic {


template <typename T>
inline constexpr T WrappingAdd(T a, T b) {
  static_assert(std::is_integral_v<T>);
  // Unsigned arithmetic wraps, so convert to the corresponding unsigned type.
  // Note that, if `T` is smaller than `int`, e.g. `int16_t`, the values are
  // promoted to `int`, which brings us back to undefined overflow. This is fine
  // here because the sum of any two `int16_t`s fits in `int`, but `WrappingMul`
  // will need a more complex implementation.
  using Unsigned = std::make_unsigned_t<T>;
  return static_cast<T>(static_cast<Unsigned>(a) + static_cast<Unsigned>(b));
}

// Returns `a - b` with overflow defined to wrap around, i.e. modulo 2^N where N
// is the bit width of `T`.
template <typename T>
inline constexpr T WrappingSub(T a, T b) {
  static_assert(std::is_integral_v<T>);
  // Unsigned arithmetic wraps, so convert to the corresponding unsigned type.
  // Note that, if `T` is smaller than `int`, e.g. `int16_t`, the values are
  // promoted to `int`, which brings us back to undefined overflow. This is fine
  // here because the difference of any two `int16_t`s fits in `int`, but
  // `WrappingMul` will need a more complex implementation.
  using Unsigned = std::make_unsigned_t<T>;
  return static_cast<T>(static_cast<Unsigned>(a) - static_cast<Unsigned>(b));
}


    typedef int32_t Atomic32;
    typedef int64_t Atomic64;

        typedef intptr_t Atomic64;


    typedef volatile std::atomic<Atomic32>* AtomicLocation32;
    typedef volatile std::atomic<Atomic64>* AtomicLocation64;

// Use AtomicWord for a machine-sized pointer.  It will use the Atomic32 or
// Atomic64 routines below, depending on your architecture.
        typedef intptr_t AtomicWord;


inline void MemoryBarrier() {
  std::atomic_thread_fence(std::memory_order_seq_cst);
}



inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                        Atomic64 old_value,
                                        Atomic64 new_value) {
  ((AtomicLocation64)ptr)
          ->compare_exchange_strong(old_value,
                                    new_value,
                                    std::memory_order_acquire,
                                    std::memory_order_acquire);
  return old_value;
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                        Atomic64 old_value,
                                        Atomic64 new_value) {
  ((AtomicLocation64)ptr)
          ->compare_exchange_strong(old_value,
                                    new_value,
                                    std::memory_order_release,
                                    std::memory_order_relaxed);
  return old_value;
}

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                        Atomic32 old_value,
                                        Atomic32 new_value) {
  ((AtomicLocation32)ptr)
          ->compare_exchange_strong(old_value,
                                    new_value,
                                    std::memory_order_acquire,
                                    std::memory_order_acquire);
  return old_value;
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                        Atomic32 old_value,
                                        Atomic32 new_value) {
  ((AtomicLocation32)ptr)
          ->compare_exchange_strong(old_value,
                                    new_value,
                                    std::memory_order_release,
                                    std::memory_order_relaxed);
  return old_value;
}


inline void Acquire_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;
  MemoryBarrier();
}

    inline void Acquire_Store(volatile Atomic64* ptr, Atomic64 value) {
      *ptr = value;
      MemoryBarrier();
    }

inline Atomic64 Release_Load(volatile const Atomic64* ptr) {
  MemoryBarrier();
  return *ptr;
}

inline Atomic32 Release_Load(volatile const Atomic32* ptr) {
  MemoryBarrier();
  return *ptr;
}

//------------------


inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr,
                                         Atomic32 new_value) {
  return ((AtomicLocation32)ptr)
          ->exchange(new_value, std::memory_order_relaxed);
}

inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr,
                                          Atomic64 new_value) {
  return ((AtomicLocation64)ptr)
          ->exchange(new_value, std::memory_order_relaxed);
}


inline void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value) {
  ((AtomicLocation32)ptr)->store(value, std::memory_order_relaxed);
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  ((AtomicLocation32)ptr)->store(value, std::memory_order_release);
}

inline Atomic32 NoBarrier_Load(volatile const Atomic32* ptr) {
  return ((AtomicLocation32)ptr)->load(std::memory_order_relaxed);
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  return ((AtomicLocation32)ptr)->load(std::memory_order_acquire);
}

  inline void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value) {
    ((AtomicLocation64)ptr)->store(value, std::memory_order_relaxed);
  }

  inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
    ((AtomicLocation64)ptr)->store(value, std::memory_order_release);
  }

  inline Atomic64 NoBarrier_Load(volatile const Atomic64* ptr) {
    return ((AtomicLocation64)ptr)->load(std::memory_order_relaxed);
  }

  inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
    return ((AtomicLocation64)ptr)->load(std::memory_order_acquire);
  }


inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr,
                                         Atomic32 increment) {
  return WrappingAdd(
          ((AtomicLocation32)ptr)->fetch_add(increment, std::memory_order_relaxed),
          increment);
}

inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr,
                                          Atomic64 increment) {
  return WrappingAdd(
          ((AtomicLocation64)ptr)->fetch_add(increment, std::memory_order_relaxed),
          increment);
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return WrappingAdd(((AtomicLocation32)ptr)->fetch_add(increment),
                           increment);
}


inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
return WrappingAdd(((AtomicLocation64)ptr)->fetch_add(increment),
                   increment);
}


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
  MemoryBarrier();  // add release semantics.
  return Acquire_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

// exchange.
inline intptr_t acquire_exchange(volatile intptr_t* ptr, intptr_t new_value) {
  intptr_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  MemoryBarrier();
  return old_value;
}

inline intptr_t release_exchange(volatile intptr_t* ptr, intptr_t new_value) {
  MemoryBarrier();
  intptr_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  return old_value;
}

inline intptr_t exchange(volatile intptr_t* ptr, intptr_t new_value) {
  MemoryBarrier();
  intptr_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  MemoryBarrier();
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
  MemoryBarrier();  // add release semantics.
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
  MemoryBarrier();  // add release semantics.
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
  MemoryBarrier();  // add release semantics.
  return Acquire_CompareAndSwap(ptr, old_value, new_value) == old_value;
}

// exchange32.
inline int32_t acquire_exchange32(volatile int32_t* ptr, int32_t new_value) {
  int32_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  MemoryBarrier();
  return old_value;
}

inline int32_t release_exchange32(volatile int32_t* ptr, int32_t new_value) {
  MemoryBarrier();
  int32_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  return old_value;
}

inline int32_t exchange32(volatile int32_t* ptr, int32_t new_value) {
  MemoryBarrier();
  int32_t old_value = NoBarrier_AtomicExchange(ptr, new_value);
  MemoryBarrier();
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
  MemoryBarrier();  // add release semantics.
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
  MemoryBarrier();  // add release semantics.
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
  MemoryBarrier();  // add release semantics.
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
  MemoryBarrier();  // add release semantics.
  return load32((volatile int32_t*)ptr);
}

inline int32_t relaxed_Inc32(volatile uint32_t* ptr, uint32_t increment) {
  return relaxed_Inc32((volatile int32_t*)ptr, (int32_t)increment);  // NOLINT
}

inline int32_t Inc32(volatile uint32_t* ptr, uint32_t increment) {
  return Inc32((volatile int32_t*)ptr, (int32_t)increment);  // NOLINT
}

}  // namespace atomic
}  // namespace tin










