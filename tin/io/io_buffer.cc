/// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/log/log.h>
#include <absl/log/check.h>
#include "tin/io/io_buffer.h"

// Some of the following member functions are marked inlined, even though they
// are virtual. This may seem counter-intuitive, since virtual functions are
// generally not eligible for inlining. Profiling results indicate that these
// large amount of runtime is spent on virtual function dispatch on these
// simple functions. They are virtual because of the interface this class
// inherits from. However, it is very unlikely that anyone will sub-class
// SimpleBuffer and change their implementation. To get rid of this baggage,
// internal implementation (e.g., Write) explicitly use SimpleBuffer:: to
// qualify the method calls, thus disabling the virtual dispatch and enable
// inlining.

namespace tin {

static const int kInitialIOBufferSize = 32;

IOBuffer::IOBuffer()
  : storage_(new char[kInitialIOBufferSize]),
    write_idx_(0),
    read_idx_(0),
    storage_size_(kInitialIOBufferSize) {
}

IOBuffer::IOBuffer(size_t size)
  : write_idx_(0),
    read_idx_(0),
    storage_size_(static_cast<int>(size)) {
  // Callers may try to allocate overly large blocks, but negative sizes are
  // obviously wrong.
  CHECK_GE(size, 0u);
  storage_ = new char[size];
}

IOBuffer::~IOBuffer() {
  delete[] storage_;
}

std::string IOBuffer::str() const {
  std::string s;
  char* readable_ptr;
  int readable_size;
  GetReadablePtr(&readable_ptr, &readable_size);
  s.append(readable_ptr, readable_ptr + readable_size);
  return s;
}

// returns the number of characters written.
// appends up-to-'size' bytes to the SimpleBuffer.
int IOBuffer::Write(const void* ptr, size_t sz) {
  int size = static_cast<int>(sz);
  const char* bytes = static_cast<const char*>(ptr);
  bool has_room = ((storage_size_ - write_idx_) >= size);
  if (!has_room) {
    (void)ReserveMore(size);
  }
  memcpy(storage_ + write_idx_, bytes, size);
  IOBuffer::AdvanceWritablePtr(size);
  return size;
}

// stores a pointer into the simple buffer in *ptr,
// and stores the number of characters which are allowed
// to be written in *size.
void IOBuffer::GetWritablePtr(char** ptr, int* size) const {
  *ptr = storage_ + write_idx_;
  *size = IOBuffer::free();
}

// stores a pointer into the simple buffer in *ptr,
// and stores the number of characters which are allowed
// to be read in *size.
void IOBuffer::GetReadablePtr(char** ptr, int* size) const {
  *ptr = storage_ + read_idx_;
  *size = write_idx_ - read_idx_;
}

// returns the number of bytes read into 'bytes'
int IOBuffer::Read(char* bytes, size_t sz) {
  int size = static_cast<int>(sz);
  char* read_ptr = NULL;
  int read_size = 0;
  GetReadablePtr(&read_ptr, &read_size);
  if (read_size > size) {
    read_size = size;
  }
  memcpy(bytes, read_ptr, read_size);
  AdvanceReadablePtr(read_size);
  return read_size;
}


void IOBuffer::Reset(int size = kInitialIOBufferSize) {
  delete[] storage_;
  storage_ = new char[size],
  write_idx_ = 0;
  read_idx_ = 0;
  storage_size_ = size;
}

// Attempts to reserve a contiguous block of buffer space by either reclaiming
// old data that is already read, and reallocate large storage as needed.
bool IOBuffer::ReserveMore(int size) {
  bool changed = false;
  if (size > 0 && free() < size) {
    char* read_ptr = NULL;
    int read_size = 0;
    GetReadablePtr(&read_ptr, &read_size);

    if (read_size + size <= buffer_size()) {
      // Can reclaim space from already read bytes by shifting
      memmove(storage_, read_ptr, read_size);
      read_idx_ = 0;
      write_idx_ = read_size;
      CHECK_GE(free(), size);
    } else {
      // what we need is to have at least size bytes available for writing.
      // This implies that the buffer needs to be at least size bytes +
      // read_size bytes long. Since we want linear time extensions in the case
      // that we're extending this thing repeatedly, we should extend to twice
      // the current size (if that is big enough), or the size + read_size
      // bytes, whichever is larger.
      int new_storage_size = 2 * storage_size_;
      if (new_storage_size < size + read_size) {
        new_storage_size = size + read_size;
      }

      // have to extend the thing
      char* new_storage = new char[new_storage_size];

      // copy still useful info to the new buffer.
      memcpy(new_storage, read_ptr, read_size);
      // reset pointers.
      read_idx_ = 0;
      write_idx_ = read_size;
      delete[] storage_;
      storage_ = new_storage;
      storage_size_ = new_storage_size;
    }
    changed = true;
  }
  return changed;
}

////////////////////////////////////////////////////////////////////////////////

// removes the oldest 'amount_to_consume' characters.
void IOBuffer::AdvanceReadablePtr(int amount_to_advance) {
  read_idx_ += amount_to_advance;
  if (read_idx_ > storage_size_) {
    read_idx_ = storage_size_;
  }
}

////////////////////////////////////////////////////////////////////////////////

// Moves the internal pointers around such that the
// amount of data specified here is expected to
// already be resident (as if it was Written)
void IOBuffer::AdvanceWritablePtr(int amount_to_advance) {
  write_idx_ += amount_to_advance;
  if (write_idx_ > storage_size_) {
    write_idx_ = storage_size_;
  }
}


void IOBuffer::Swap(IOBuffer* other) {
  char* tmp = storage_;
  storage_ = other->storage_;
  other->storage_ = tmp;

  int tmp_int = write_idx_;
  write_idx_ = other->write_idx_;
  other->write_idx_ = tmp_int;

  tmp_int = read_idx_;
  read_idx_ = other->read_idx_;
  other->read_idx_ = tmp_int;

  tmp_int = storage_size_;
  storage_size_ = other->storage_size_;
  other->storage_size_ = tmp_int;
}

}  // namespace tin
