// // Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PKG_IO_IO_BUFFER_H_
#define PKG_IO_IO_BUFFER_H_

#include <string>


namespace tin {

class IOBuffer {
 public:

 public:
  IOBuffer();
  explicit IOBuffer(size_t size);
  ~IOBuffer();

  std::string str() const;

  typedef char* iterator;
  typedef const char* const_iterator;

  iterator begin() { return storage_ + read_idx_; }
  const_iterator begin() const { return storage_ + read_idx_; }

  iterator end() { return storage_ + write_idx_; }
  const_iterator end() const { return storage_ + write_idx_; }

  // The following functions all override pure virtual functions
  // in BufferInterface. See buffer_interface.h for a description
  // of what they do.
  int buffered() const { return write_idx_ - read_idx_; }
  int buffer_size() const { return storage_size_; }
  int free() const { return (storage_size_ - write_idx_); }
  bool empty() const { return (read_idx_ == write_idx_); }
  bool full() const {
    return ((write_idx_ == storage_size_) && (read_idx_ != write_idx_));
  }

  // removes all data from the simple buffer
  void clear() { read_idx_ = write_idx_ = 0; }

  int Write(const void* ptr, size_t size);

  void GetWritablePtr(char** ptr, int* size) const;

  void GetReadablePtr(char** ptr, int* size) const;

  int Read(char* bytes, size_t size);

  void Reset(int size);

  // This can be an expensive operation: costing a new/delete, and copying of
  // all existing data. Even if the existing buffer does not need to be
  // resized, unread data may still need to be non-destructively copied to
  // consolidate fragmented free space.
  bool ReserveMore(int size);

  void AdvanceReadablePtr(int amount_to_advance);

  void AdvanceWritablePtr(int amount_to_advance);

  void Swap(IOBuffer* other);

 private:
  char* storage_;
  int write_idx_;
  int read_idx_;
  int storage_size_;

 public:
  IOBuffer(IOBuffer&& rvalue) {  // NOLINT
    storage_ = rvalue.storage_;
    write_idx_ = rvalue.write_idx_;
    read_idx_ = rvalue.read_idx_;
    storage_size_ = rvalue.storage_size_;

    rvalue.storage_ = NULL;
    rvalue.write_idx_ = 0;
    rvalue.read_idx_ = 0;
    rvalue.storage_size_ = 0;
  }

  void operator=(IOBuffer&& rvalue) {
    delete storage_;
    storage_ = rvalue.storage_;
    write_idx_ = rvalue.write_idx_;
    read_idx_ = rvalue.read_idx_;
    storage_size_ = rvalue.storage_size_;

    rvalue.storage_ = NULL;
    rvalue.write_idx_ = 0;
    rvalue.read_idx_ = 0;
    rvalue.storage_size_ = 0;
  }
};

}  // namespace tin

#endif  // PKG_IO_IO_BUFFER_H_
