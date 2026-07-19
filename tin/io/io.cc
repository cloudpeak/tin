// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/log/check.h>

#include "tin/error/error.h"
#include "tin/runtime/runtime.h"
#include "tin/io/io.h"


namespace tin::io {

Result<size_t> ReadAtLeast(Reader* reader, void* buf, int len, int min) {
  if (len < min) {
    return Result<size_t>::Err(TIN_EINVAL);
  }
  int n = 0;
  while (n < min) {
    auto result = reader->Read(static_cast<char*>(buf) + n, len - n);
    size_t nn = result.value_or(0);
    DCHECK_GE(static_cast<int>(nn), 0);
    if (nn > 0) {
      n += static_cast<int>(nn);
    }
    if (!result.ok()) {
      if (n >= min) {
        return Result<size_t>::Ok(static_cast<size_t>(n));
      }
      if (n > 0 && result.code() == TIN_EOF) {
        return Result<size_t>::Err(TIN_UNEXPECTED_EOF);
      }
      return Result<size_t>::Err(result.status());
    }
  }
  return Result<size_t>::Ok(static_cast<size_t>(n));
}

Result<size_t> ReadFull(Reader* reader, void* buf, int len) {
  return ReadAtLeast(reader, buf, len, len);
}

Result<size_t> Write(Writer* writer, const void* buf, int len) {
  return writer->Write(buf, len);
}

Result<size_t> WriteString(Writer* writer, const absl::string_view& str) {
  return writer->Write(str.data(), static_cast<int>(str.size()));
}

} // namespace tin::io
