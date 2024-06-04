// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstdlib>
#include <cstdint>
#include <absl/strings/string_view.h>

#include "tin/io/io.h"


namespace tin::bufio {

const int kDefaultReaderBufSize = 4096;

/*
+--------------+--------------------------------+
|              |    buffered       |    free    |
+--------------+--------------------+-----------+
                |                   |
                v                   v
             read_idx           write_idx
*/

class Reader : public tin::io::Reader {
 public:
  explicit Reader(tin::io::Reader* rd, size_t size = kDefaultReaderBufSize);
  virtual ~Reader();

  // reset a new underlying reader.
  void Reset(tin::io::Reader* rd);

  virtual int Read(void* buf, int nbytes);

  // return error code.
  int ReadSlice(uint8_t delim, absl::string_view* line);

  // return error code.
  int ReadLine(absl::string_view* line, bool* is_prefix);

  int ReadByte(uint8_t* c);
  int UnreadByte();
  int Peek(int n, absl::string_view* piece);

  // inline functions
  int buffered() const { return write_idx_ - read_idx_; }
  int free() const { return (storage_size_ - write_idx_); }

  bool empty() const { return read_idx_ == write_idx_; }
  bool full() const {
    return ((write_idx_ == storage_size_) && (read_idx_ != write_idx_));
  }

  typedef uint8_t* iterator;
  typedef const uint8_t* const_iterator;

  iterator begin() { return storage_ + read_idx_; }
  const_iterator begin() const { return storage_ + read_idx_; }

  iterator end() { return storage_ + write_idx_; }
  const_iterator end() const { return storage_ + write_idx_; }

 private:
  int ReadErr();
  void Fill();

  static absl::string_view ToStringPiece(const uint8_t* p, absl::string_view::size_type n) {
    return {reinterpret_cast<const char*>(p),n};
  }

 private:
  uint8_t* storage_;
  int storage_size_;
  int read_idx_;
  int write_idx_;
  int err_;
  tin::io::Reader* rd_;
  int last_byte_;
};

class Writer : public tin::io::Writer {
 public:
  virtual ~Writer() {}
  virtual int Write(const void* buf, int nbytes) = 0;
};



} // namespace tin::bufio





