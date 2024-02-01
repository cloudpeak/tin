// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <iostream>
#include <deque>
#include "base/memory/ref_counted.h"
// #include "base/synchronization/cancellation_flag.h"
#include "tin/sync/atomic.h"
#include "tin/runtime/raw_mutex.h"
#include "tin/runtime/semaphore.h"
#include "tin/sync/cond.h"


namespace tin {

const uint32_t kDefaultQueueCapacity = 64;
const uint32_t kMaxQueueCapacity = kuint32max;

template<class T>
class QueueImpl
  : public base::RefCountedThreadSafe<QueueImpl<T> > {
 public:
  explicit QueueImpl(uint32_t capacity = kDefaultQueueCapacity)
    : capacity_(capacity)
    , full_waiters_(0)
    , empty_waiters_(0)
    , full_cond_(&lock_)
    , empty_cond_(&lock_)
    , closed_(false) {
  }

  bool Enqueue(const T& t, size_t* size = NULL) {
    MutexGuard guard(&lock_);
    if (closed_)
      return false;
    while (queue_.size() == capacity_) {  // queue is full.
      full_waiters_++;
      // Wait will release lock automatically.
      full_cond_.Wait();
      // Wait acquired lock automatically.
      full_waiters_--;
      if (closed_)
        return false;
    }
    // now, we have space to push at least one item.
    queue_.push_back(t);
    // some consumers is waiting due to empty queue.
    if (empty_waiters_ > 0) {
      // some consumers is waiting for an item.
      empty_cond_.Signal();
    }
    if (size != NULL)
      *size = queue_.size();
    return true;
  }

  bool Dequeue(T* t, size_t* size = NULL) {
    MutexGuard guard(&lock_);
    if (closed_)
      return false;
    while (queue_.size() == 0) {  // queue is empty
      empty_waiters_++;
      // Wait will release lock automatically.
      empty_cond_.Wait();
      // Wait acquired lock automatically.
      empty_waiters_--;
      if (closed_)
        return false;
    }
    *t = queue_.front();
    queue_.pop_front();
    // some producers is waiting due to full queue.
    if (full_waiters_ > 0) {
      // some producers is waiting for a slot to put item.
      full_cond_.Signal();
    }
    if (size != NULL)
      *size = queue_.size();
    return true;
  }

  void Close() {
    MutexGuard guard(&lock_);
    if (closed_)
      return;  // already closed.
    closed_ = true;
    STLClearElements(&queue_);
    full_cond_.Broascast();
    empty_cond_.Broascast();
  }

  size_t Size() const {
    MutexGuard guard(&lock_);
    if (closed_)
      return 0;
    return queue_.size();
  }

  bool Empty() const {
    MutexGuard guard(&lock_);
    if (closed_)
      return true;
    return queue_.size() == 0;
  }

  // intrusive methods, be careful!
  void Lock() const {
    lock_.Lock();
  }

  void UnLock() const {
    lock_.Unlock();
  }

  bool IsClosedLocked() const {
    return closed_;
  }

  std::deque<T>* MutableQueueLocked() {
    return &queue_;
  }

  void NotifyConsumedLocked(int n) {
    while (full_waiters_ > 0 && n > 0) {
      full_cond_.Signal();
      full_waiters_--;
      n--;
    }
  }

 private:
  friend class base::RefCountedThreadSafe<QueueImpl<T>>;
  ~QueueImpl() {
    STLClearElements(&queue_);
    // std::cout << "Channel destructor_______\n";
  }
  DISALLOW_COPY_AND_ASSIGN(QueueImpl<T>);

 private:
  mutable Mutex lock_;
  Cond full_cond_;   // producer waiting due to queue is full.
  Cond empty_cond_;  // consumer waiting due to queue is empty.
  std::deque<T> queue_;
  uint32_t capacity_;
  int full_waiters_;
  int empty_waiters_;
  bool closed_;
};


template <typename T>
class Queue
  : public scoped_refptr<QueueImpl<T>> {
 public:
  explicit Queue(QueueImpl<T>* t)
    : scoped_refptr<QueueImpl<T>>(t) {
  }
};

template <typename T>
Queue<T> MakeQueue(uint32_t max_size =
                     kDefaultQueueCapacity) {
  return Queue<T>(new QueueImpl<T>(max_size));
}

}  // namespace tin
