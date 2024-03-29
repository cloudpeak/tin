// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <utility>
#include <iostream>
#include <deque>

#include "tin/sync/atomic.h"
#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/semaphore.h"


namespace tin {
const uint32_t kDefaultChanSize = 64;

template<class T>
class Channel
  : public  std::enable_shared_from_this<Channel<T>>  {
 public:
  explicit Channel(uint32_t max_size = kDefaultChanSize)
    : max_size_(max_size)
    , free_space_sem_(max_size)
    , used_space_sem_(0)
    , closed_(0) {
    // std::cout << "Channel constructor " << rand() << std::endl;
  }

  bool Push(const T& t) {
    if (IsClosed())
      return false;
    bool ok;
    runtime::SemAcquire(&free_space_sem_);
    {
      runtime::RawMutexGuard guard(&lock_);
      ok = !IsClosed();
      if (ok) {
        queue_.push_back(t);
      }
    }
    if (ok) {
      runtime::SemRelease(&used_space_sem_);
    } else {
      runtime::SemRelease(&free_space_sem_);
    }
    return ok;
  }

  bool Pop(T* t) {
    if (IsClosed())
      return false;
    bool ok;
    runtime::SemAcquire(&used_space_sem_);
    do {
      runtime::RawMutexGuard guard(&lock_);
      ok = !IsClosed();
      if (ok) {
        *t = queue_.front();
        queue_.pop_front();
      }
    } while (0);
    if (ok) {
      runtime::SemRelease(&free_space_sem_);
    } else {
      runtime::SemRelease(&used_space_sem_);
    }
    return ok;
  }

  void Close() {
    if (atomic::exchange32(&closed_, 1) != 0) {
      // already closed.
      return;
    }
    std::deque<T> queue;
    {
      runtime::RawMutexGuard guard(&lock_);
      std::swap(queue, queue_);
    }
    ClearQueue(queue, std::is_pointer<T>());
    runtime::SemRelease(&free_space_sem_);
    runtime::SemRelease(&used_space_sem_);
  }

  bool IsClosed() {
    return atomic::acquire_load32(&closed_) != 0;
  }

 private:
  void ClearQueue(std::deque<T>& queue, std::false_type) {  // NOLINT
    queue.clear();
  }

  void ClearQueue(std::deque<T>& queue, std::true_type) {  // NOLINT
    for (typename std::deque<T>::iterator iter = queue.begin();
         iter != queue.end();
         ++iter) {
      delete *iter;
    }
    queue.clear();
  }

  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;
 private:
  uint32_t free_space_sem_;
  uint32_t used_space_sem_;
  runtime::RawMutex lock_;
  std::deque<T> queue_;
  uint32_t max_size_;
  uint32_t closed_;
};

template <typename T>
class Chan {
public:
  explicit Chan(uint32_t max_size = kDefaultChanSize) : impl_(std::make_shared<Channel<T>>(max_size)) {}

  Chan(const Chan& other) : impl_(other.impl_) {
  }

  Channel<T>* operator->() {
      return impl_.get();
  }

private:
  std::shared_ptr<Channel<T>> impl_;
};

template <typename T>
Chan<T> MakeChan(uint32_t max_size = kDefaultChanSize) {
    return Chan<T>(max_size);
}


}  // namespace tin
