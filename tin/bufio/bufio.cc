// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <absl/log/check.h>
#include <absl/log/log.h>

#include "tin/error/error.h"
#include "tin/runtime/runtime.h"
#include "tin/bufio/bufio.h"

namespace {
// const int kMinReadBufferSize = 16;
const int kMaxConsecutiveEmptyReads = 100;
}


namespace tin::bufio {

Reader::Reader(tin::io::Reader* rd, size_t size)
  : storage_(new uint8_t[size])
  , storage_size_(static_cast<int>(size))
  , read_idx_(0)
  , write_idx_(0)
  , err_(0)
  , rd_(rd)
  , last_byte_(-1) {
}

Reader::~Reader() {
  delete [] storage_;
}

int Reader::Read(void* buf, int buf_size) {
  uint8_t* p = static_cast<uint8_t*>(buf);
  int n = buf_size;
  if (n == 0) {
    tin::SetErrorCode(ReadErr());
    return 0;
  }
  DCHECK(buf != NULL);
  DCHECK_GT(buf_size, 0);

  if (empty()) {
    if (err_ != 0) {
      tin::SetErrorCode(ReadErr());
      return 0;
    }
    if (buf_size >= storage_size_) {
      // Large read, empty buffer.
      // Read directly into p to avoid copy.
      n = rd_->Read(buf, buf_size);
      err_ = tin::GetErrorCode();
      if (n < 0) {
        LOG(FATAL) << "bufio: tried to fill full buffer";
      }
      if (n > 0) {
        last_byte_ = p[n - 1];
      }
      tin::SetErrorCode(ReadErr());
      return n;
    }
    Fill();
    // buffer is empty
    if (empty()) {
      tin::SetErrorCode(ReadErr());
      return 0;
    }
  }

  // copy as much as we can
  n = std::min<int>(buffered(), buf_size);
  std::memcpy(p, begin(), n);
  read_idx_ += n;
  last_byte_ = storage_[read_idx_ - 1];
  tin::SetErrorCode(0);
  return n;
}

void Reader::Reset(tin::io::Reader* rd) {
  rd_ = rd;
  read_idx_ = 0;
  write_idx_ = 0;
  last_byte_ = -1;
}

void Reader::Fill() {
  if (read_idx_ > 0) {
    std::memmove(storage_, begin(), buffered());
    write_idx_ -= read_idx_;
    read_idx_ = 0;
  }
  if (write_idx_ >= storage_size_) {
    LOG(FATAL) << "bufio: tried to fill full buffer";
  }

  // Read new data: try a limited number of times.
  for (int i = kMaxConsecutiveEmptyReads; i > 0; i++) {
    int n = rd_->Read(end(), free());
    if (n < 0) {
      LOG(FATAL) << "bufio: tried to fill full buffer";
    }
    write_idx_ += n;
    int err = tin::GetErrorCode();
    if (err != 0) {
      err_ = err;
      return;
    }
    if (n > 0) {
      return;
    }
  }
  err_ = TIN_ENOPROGRESS;
}

int Reader::ReadErr() {
  int err = err_;
  err_ = 0;
  return err;
}



int Reader::ReadSlice(uint8_t delim, absl::string_view* line) {
  int err = 0;
  while (true) {
    const_iterator it =  std::find(begin(), end(), delim);
    if (it != end()) {
      size_t n = it - begin() + 1;
      *line = ToStringPiece(begin(), n);
      read_idx_ += static_cast<int>(n);
      break;
    }

    // Pending error?
    if (err_ != 0) {
      *line = ToStringPiece(begin(), buffered());
      read_idx_ = write_idx_;
      err = ReadErr();
      break;
    }

    // Buffer full?
    if (buffered() >= storage_size_) {
      read_idx_ = write_idx_;
      *line = ToStringPiece(begin(), buffered());
      err = TIN_EBUFFERFULL;
      break;
    }
    Fill();  // buffer is not full
  }

  // Handle last byte, if any.
  int i = static_cast<int>(line->length()) - 1;
  if (i >= 0) {
    last_byte_ = (*line)[i];
  }
  tin::SetErrorCode(err);
  return err;
}

int Reader::ReadLine(absl::string_view* line, bool* is_prefix) {
  int err = ReadSlice('\n', line);
  if (err == TIN_EBUFFERFULL) {
    // Handle the case where "\r\n" straddles the buffer.
    if (line->length() > 0 && line->back() == '\r') {
      // Put the '\r' back on buf and drop it from line.
      // Let the next call to ReadLine check for "\r\n".
      if (read_idx_ == 0) {
        // should be unreachable
        LOG(FATAL) << "bufio: tried to rewind past start of buffer";
      }
      read_idx_--;
      line->remove_suffix(1);
      *is_prefix = true;
      tin::SetErrorCode(0);
      return 0;
    }
  }

  if (line->empty()) {
    if (err != 0) {
      // line->clear();
      *line = absl::string_view();
    }
    *is_prefix = false;
    return err;
  }
  err = 0;

  if (line->back() == '\n') {
    int drop = 1;
    if (line->length() > 1 && (*line)[line->length() - 2] == '\r') {
      drop = 2;
    }
    line->remove_prefix(drop);
  }
  *is_prefix = false;
  tin::SetErrorCode(0);
  return err;
}

int Reader::Peek(int n, absl::string_view* piece) {
  DCHECK_GE(n, 0);
  if (n < 0) {
    LOG(FATAL) << "ErrNegativeCount";
  }
  int err = 0;
  if (n > storage_size_) {
    err = TIN_EBUFFERFULL;
    tin::SetErrorCode(err);
    return err;
  }

  while (buffered() < n && err_ == 0) {
    Fill();
  }

  if (buffered() < n) {
    n = buffered();
    err = ReadErr();
    if (err == 0) {
      err = TIN_EBUFFERFULL;
      tin::SetErrorCode(err);
    }
  }
  if (piece != NULL)
    *piece = ToStringPiece(begin(), n);
  return err;
}

int Reader::UnreadByte() {
  if (last_byte_ < 0 || (read_idx_ == 0 && write_idx_ > 0)) {
    return TIN_EINVAL;
  }
  // b.r > 0 || b.w == 0
  if (read_idx_ > 0) {
    read_idx_--;
  } else {
    // b.r == 0 && b.w == 0
    write_idx_ = 1;
  }
  *begin() = static_cast<uint8_t>(last_byte_);
  last_byte_ = -1;
  return 0;
}

int Reader::ReadByte(uint8_t* c) {
  while (empty()) {
    if (err_ != 0) {
      *c = 0;
      return ReadErr();
    }
    Fill();  // buffer is empty
  }
  *c = *begin();
  read_idx_++;
  last_byte_ = *c;
  return 0;
}

} // namespace tin::bufio

