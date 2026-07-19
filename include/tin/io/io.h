// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include <cstddef>

#include <absl/strings/string_view.h>

#include "tin/result.h"
#include "tin/time/time.h"

namespace tin {
namespace io {

class Reader {
 public:
  virtual ~Reader() {}
  // Reads up to nbytes into buf. Returns the number of bytes read (>= 0).
  // On error, the Result's status() carries the error code.
  virtual Result<size_t> Read(void* buf, int nbytes) = 0;
};

class Writer {
 public:
  virtual ~Writer() {}
  // Writes up to nbytes from buf. Returns the number of bytes written (>= 0).
  // On error, the Result's status() carries the error code.
  virtual Result<size_t> Write(const void* buf, int nbytes) = 0;
};

class IOReadWriter : public Reader, public Writer {
};

// Reads from reader into buf until at least min bytes are read or an error
// occurs. Returns the number of bytes read and a status.
Result<size_t> ReadAtLeast(Reader* reader, void* buf, int nbytes, int min);

// Reads exactly len bytes from reader into buf.
Result<size_t> ReadFull(Reader* reader, void* buf, int len);

// Writes all len bytes from buf to writer.
Result<size_t> Write(Writer* writer, const void* buf, int len);

// Writes a string to writer.
Result<size_t> WriteString(Writer* writer, const absl::string_view& str);

}  // namespace io
}  // namespace tin
