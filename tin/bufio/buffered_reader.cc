// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/log/check.h>
#include "tin/error/error.h"
#include "tin/runtime/runtime.h"

#include "tin/bufio/buffered_reader.h"

namespace tin {

BufferedReader::BufferedReader(tin::io::Reader* reader, int size)
  : buffer_(size)
  , last_read_(0)
  , reader_(reader)
  , total_bytes_stat_(0) {
}

bool BufferedReader::ReadFull(int n) {
  DCHECK_GT(n, 0);
  buffer_.AdvanceReadablePtr(last_read_);
  last_read_ = 0;
  int err = 0;
  if (buffer_.buffered() < n) {
    int lack = n - buffer_.buffered();
    int bytes_free = buffer_.free();
    if (bytes_free < lack) {
      buffer_.ReserveMore(lack);
    }

    DCHECK(reader_ != NULL);
    while (buffer_.buffered() < n) {
      char* write_ptr = NULL;
      int write_size = 0;
      buffer_.GetWritablePtr(&write_ptr, &write_size);
      DCHECK_GT(write_size, 0);
      int nread = reader_->Read(write_ptr, write_size);
      DCHECK_GE(nread, 0);
      buffer_.AdvanceWritablePtr(nread);
      err = tin::GetErrorCode();
      if (err != 0) {
        break;
      }
    }
  }
  if (buffer_.buffered() >= n) {
    SetErrorCode(0);
    err = 0;
  } else if (buffer_.buffered() > 0 && err == TIN_EOF) {
    SetErrorCode(TIN_UNEXPECTED_EOF);
    err = TIN_UNEXPECTED_EOF;
  }
  if (err != 0)
    n = 0;
  last_read_ = n;
  total_bytes_stat_ += n;
  return err == 0;
}

}  // namespace tin
