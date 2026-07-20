// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_IO_IO_H_
#define TIN_IO_IO_H_
#include "tin/time/time.h"
#include "absl/strings/string_view.h"

namespace tin::io {

class Reader {
 public:
  virtual ~Reader() {}
  virtual int Read(void* buf, int nbytes) = 0;
};

class Writer {
 public:
  virtual ~Writer() {}
  virtual int Write(const void* buf, int nbytes) = 0;
};

class IoReadWriter : public Reader, public Writer {
};

int ReadAtLeast(Reader* reader, void* buf, int nbytes, int min);

int ReadFull(Reader* reader, void* buf, int nbytes);

int Write(Writer* writer, const void* buf, int nbytes);

int WriteString(Writer* writer, const absl::string_view& str);

}  // namespace tin::io
#endif  // TIN_IO_IO_H_
