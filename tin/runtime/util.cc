// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cliff/build/build_config.h>
#include "tin/runtime/util.h"

#if defined(OS_WIN)
#include <windows.h>
#else
#include <sched.h>
#include <errno.h>
#endif

namespace tin {
namespace runtime {

void YieldLogicProcessor() {
#if defined(OS_WIN)
  YieldProcessor();
#elif defined(ARCH_CPU_X86_FAMILY)
  __asm__ __volatile__("pause");
#elif defined(ARCH_CPU_ARM_FAMILY)
// ARMv6K and above, disable currently.
// __asm__ __volatile__("yield");
#elif defined(ARCH_CPU_MIPS_FAMILY) && defined(ARCH_CPU_32_BITS)
  __asm__ __volatile__(".word 0x00000140");
#elif defined(ARCH_CPU_MIPS_FAMILY) && __mips_isa_rev >= 2
  __asm__ __volatile__("pause");
#else
  // do nothing.
#endif
}

void YieldLogicProcessor(int n) {
  for (int i = 0; i < n; ++i) {
    YieldLogicProcessor();
  }
}

int GetLastSystemErrorCode() {
#if defined(OS_WIN)
  return ::GetLastError();
#elif defined(OS_POSIX)
  return errno;
#else
#error Not implemented
#endif
}

}  // namespace runtime
}  // namespace tin
