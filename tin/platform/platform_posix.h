// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>

#include "build/build_config.h"
#include "cstdint"

namespace tin {

#if defined(OS_LINUX)

#if defined(__alpha__)
# define TIN__O_CLOEXEC        0x200000
#elif defined(__hppa__)
# define TIN__O_CLOEXEC        0x200000
#elif defined(__sparc__)
# define TIN__O_CLOEXEC        0x400000
#else
# define TIN__O_CLOEXEC        0x80000
#endif

#if defined(__alpha__)
# define TIN__O_NONBLOCK       0x4
#elif defined(__hppa__)
# define TIN__O_NONBLOCK       O_NONBLOCK
#elif defined(__mips__)
# define TIN__O_NONBLOCK       0x80
#elif defined(__sparc__)
# define TIN__O_NONBLOCK       0x4000
#else
# define TIN__O_NONBLOCK       0x800
#endif

#define TIN__EFD_CLOEXEC       TIN__O_CLOEXEC
#define TIN__EFD_NONBLOCK      TIN__O_NONBLOCK

#define TIN__IN_CLOEXEC        TIN__O_CLOEXEC
#define TIN__IN_NONBLOCK       TIN__O_NONBLOCK

#define TIN__SOCK_CLOEXEC      TIN__O_CLOEXEC
#if defined(SOCK_NONBLOCK)
# define TIN__SOCK_NONBLOCK    SOCK_NONBLOCK
#else
# define TIN__SOCK_NONBLOCK    TIN__O_NONBLOCK
#endif

#if defined(__arm__)
# if defined(__thumb__) || defined(__ARM_EABI__)
#  define TIN_SYSCALL_BASE 0
# else
#  define TIN_SYSCALL_BASE 0x900000
# endif
#endif /* __arm__ */

#ifndef __NR_accept4
# if defined(__x86_64__)
#  define __NR_accept4 288
# elif defined(__i386__)
/* Nothing. Handled through socketcall(). */
# elif defined(__arm__)
#  define __NR_accept4 (TIN_SYSCALL_BASE + 366)
# endif
#endif /* __NR_accept4 */

#endif  // #if defined(OS_LINUX)

}  // namespace tin

