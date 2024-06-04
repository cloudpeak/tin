// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/log/check.h>

#include "tin/error/error.h"
#include "tin/runtime/runtime.h"
#include "tin/io/io.h"


namespace tin::io {

int ReadAtLeast(Reader* reader, void* buf, int len, int min) {
  if (len < min) {
    SetErrorCode(TIN_EINVAL);
    return 0;
  }
  int err = 0;
  int n = 0;
  while (n < min && err == 0) {
    int nn = reader->Read(static_cast<char*>(buf) + n, len - n);
    DCHECK_GE(nn, 0);
    if (nn > 0)
      n += nn;
    err = GetErrorCode();
  }
  if (n >= min) {
    SetErrorCode(0);
  } else if (n > 0 && err == TIN_EOF) {
    SetErrorCode(TIN_UNEXPECTED_EOF);
  }
  return n;
}

int ReadFull(Reader* reader, void* buf, int len) {
  return ReadAtLeast(reader, buf, len, len);
}

int Write(Writer* writer, void* buf, int len) {
  return writer->Write(buf, len);
}

int WriteString(Writer* writer, const absl::string_view& str) {
  return writer->Write(str.data(), static_cast<int>(str.size()));
}

} // namespace tin::io

