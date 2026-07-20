// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_BUFIO_BUFIO_H_
#define TIN_BUFIO_BUFIO_H_
#include <cstdlib>
#include <cstdint>
#include <absl/strings/string_view.h>

#include "tin/io/io.h"
#include "tin/result.h"


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

  // Reads up to nbytes into buf. Returns the number of bytes read (>= 0).
  // On error, the Result's status() carries the error code.
  Result<size_t> Read(void* buf, int nbytes) override;

  // Reads until delim is found. Returns Status and sets *line to the slice
  // (including the delimiter). On error, *line contains the data read so far.
  Status ReadSlice(uint8_t delim, absl::string_view* line);

  // Reads a line. Returns Status and sets *line (without trailing \r\n or \n)
  // and *is_prefix (true if the line was longer than the buffer).
  Status ReadLine(absl::string_view* line, bool* is_prefix);

  // Reads a single byte. Returns Result<uint8_t>.
  Result<uint8_t> ReadByte();

  // Unreads the last byte read. Returns Status.
  Status UnreadByte();

  // Peeks n bytes without consuming. Returns Status and sets *piece.
  Status Peek(int n, absl::string_view* piece);

  // inline functions
  int buffered() const { return write_idx_ - read_idx_; }
  int free() const { return (storage_size_ - write_idx_); }

  bool empty() const { return read_idx_ == write_idx_; }
  bool full() const {
    return ((write_idx_ == storage_size_) && (read_idx_ != write_idx_));
  }

  using iterator = uint8_t*;
  using const_iterator = const uint8_t*;

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
  virtual Result<size_t> Write(const void* buf, int nbytes) = 0;
};



} // namespace tin::bufio
#endif  // TIN_BUFIO_BUFIO_H_
