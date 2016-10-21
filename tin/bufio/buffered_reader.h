// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <vector>
#include <map>

#include "base/basictypes.h"
#include "tin/io/io.h"
#include "tin/io/io_buffer.h"

namespace tin {

const int kDefaultBufferedReaderSpace = 4096;

class BufferedReader {
 public:
  BufferedReader(tin::io::Reader* reader,
                 int size = kDefaultBufferedReaderSpace);
  void SetIOReader(tin::io::Reader* reader) {
    reader_ = reader;
  }

  // read n or failed.
  bool ReadFull(int n);

  char* Data()  {
    return buffer_.begin();
  }

  int Length() const {
    return last_read_;
  }

  uint64 TotalBytesStat() const {
    return total_bytes_stat_;
  }

 private:
  IOBuffer buffer_;
  int last_read_;
  tin::io::Reader* reader_;
  uint64 total_bytes_stat_;
};

}  // namespace tin
