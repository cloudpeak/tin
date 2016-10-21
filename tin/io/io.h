// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "tin/time/time.h"
#include "base/strings/string_piece.h"

namespace tin {
namespace io {

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

class IOReadWriter : public Reader, public Writer {
};

int ReadAtLeast(Reader* reader, void* buf, int nbytes, int min);

int ReadFull(Reader* reader, void* buf, int nbytes);

int Write(Writer* writer, const void* buf, int nbytes);

int WriteString(Writer* writer, const base::StringPiece& str);

}  // namespace io
}  // namespace tin




