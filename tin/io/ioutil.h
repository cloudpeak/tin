// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#include "tin/time/time.h"
#include "base/file_util.h"
#include "base/platform_file.h"

namespace tin {

typedef base::PlatformFile file_t;
typedef base::FilePath path_t;
typedef base::PlatformFileError file_error_t;

const file_t kInvalidFile = base::kInvalidPlatformFileValue;

file_t OpenFile(const path_t& name,
                int flags,
                bool* created,
                file_error_t* error);

bool CloseFile(file_t file);

bool TruncateFile(file_t file, int64_t length);

int ReadFile(file_t file, char* data, int size);

int WriteFile(file_t file, const char* data, int size);

bool DeleteFile(const path_t& path, bool recursive);

// handy functions

file_t OpenFileForRead(const path_t& name, file_error_t* error);

}  // namespace tin

