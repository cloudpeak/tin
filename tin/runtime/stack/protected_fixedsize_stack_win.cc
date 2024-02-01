// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>


#include "base/sys_info.h"
#include "tin/config/config.h"
#include "tin/runtime/util.h"

#include "tin/runtime/stack/protected_fixedsize_stack.h"

namespace tin {
namespace runtime {

ProtectedFixedSizeStack::ProtectedFixedSizeStack()
  : sp_(NULL)
  , size_(0) {
}

ProtectedFixedSizeStack::~ProtectedFixedSizeStack() {
  if (sp_ != NULL) {
    void* vp = static_cast<char*>(sp_) - size_;
    ::VirtualFree(vp, 0, MEM_RELEASE);
  }
}

void* ProtectedFixedSizeStack::Allocate(size_t size) {
  size_t page_size = base::SysInfo::PageSize();
  size_t num_pages = static_cast<size_t>(
                       std::floor(static_cast<float>(size) / page_size)) + 1;
  if (num_pages < 2)
    num_pages = 2;
  size_ =  num_pages * page_size;

  void* vp = ::VirtualAlloc(0, size_, MEM_COMMIT, PAGE_READWRITE);
  if (!vp)
    throw std::bad_alloc();

  // protect bottom page from any kind of access.
  DWORD old_options;
  ::VirtualProtect(vp, page_size, PAGE_READWRITE | PAGE_GUARD , &old_options);

  sp_ = static_cast<char*>(vp) + size_;
  return sp_;
}

}  // namespace runtime
}   // namespace tin
