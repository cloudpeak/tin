// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern "C" {
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
}

#include <cmath>
#include <memory>

#include "base/basictypes.h"
#include "base/sys_info.h"
#include "tin/config/config.h"
#include "tin/runtime/util.h"

#include "tin/runtime/stack/protected_fixedsize_stack.h"

namespace tin {
namespace runtime {

ProtectedFixedSizeStack::ProtectedFixedSizeStack()
  : size_(0)
  , sp_(NULL) {
}

ProtectedFixedSizeStack::~ProtectedFixedSizeStack() {
  if (sp_ != NULL) {
    void* vp = static_cast<char*>(sp_) - size_;
    // conform to POSIX.4 (POSIX.1b-1993, _POSIX_C_SOURCE=199309L)
    ::munmap(vp, size_);
  }
}

void* ProtectedFixedSizeStack::Allocate(size_t size) {
  size_t page_size = base::SysInfo::PageSize();
  size_t num_pages = static_cast<size_t>(
                       std::floor(static_cast<float>(size) / page_size)) + 1;
  if (num_pages < 2)
    num_pages = 2;
  size_ = num_pages * page_size;

#if defined(MAP_ANON)
  void* vp = mmap(0, size_,
                  PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#else
  void* vp = mmap(0, size_,
                  PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
  if (MAP_FAILED == vp)
    throw std::bad_alloc();
  const int result(::mprotect(vp, page_size, PROT_NONE));
  assert(0 == result);
  sp_ = static_cast<char*>(vp) + size_;

  return sp_;
}

}  // namespace runtime
}  // namespace tin
