// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_ERRNO_H_
#define TIN_ERRNO_H_

#include <errno.h>

#define TIN__EOF     (-6000)
#define TIN__UNKNOWN (-4094)

#define TIN__EAI_ADDRFAMILY  (-3000)
#define TIN__EAI_AGAIN       (-3001)
#define TIN__EAI_BADFLAGS    (-3002)
#define TIN__EAI_CANCELED    (-3003)
#define TIN__EAI_FAIL        (-3004)
#define TIN__EAI_FAMILY      (-3005)
#define TIN__EAI_MEMORY      (-3006)
#define TIN__EAI_NODATA      (-3007)
#define TIN__EAI_NONAME      (-3008)
#define TIN__EAI_OVERFLOW    (-3009)
#define TIN__EAI_SERVICE     (-3010)
#define TIN__EAI_SOCKTYPE    (-3011)
#define TIN__EAI_BADHINTS    (-3013)
#define TIN__EAI_PROTOCOL    (-3014)

/* Only map to the system errno on non-Windows platforms. It's apparently
 * a fairly common practice for Windows programmers to redefine errno codes.
 */
#if defined(E2BIG) && !defined(_WIN32)
# define TIN__E2BIG (-E2BIG)
#else
# define TIN__E2BIG (-4093)
#endif

#if defined(EACCES) && !defined(_WIN32)
# define TIN__EACCES (-EACCES)
#else
# define TIN__EACCES (-4092)
#endif

#if defined(EADDRINUSE) && !defined(_WIN32)
# define TIN__EADDRINUSE (-EADDRINUSE)
#else
# define TIN__EADDRINUSE (-4091)
#endif

#if defined(EADDRNOTAVAIL) && !defined(_WIN32)
# define TIN__EADDRNOTAVAIL (-EADDRNOTAVAIL)
#else
# define TIN__EADDRNOTAVAIL (-4090)
#endif

#if defined(EAFNOSUPPORT) && !defined(_WIN32)
# define TIN__EAFNOSUPPORT (-EAFNOSUPPORT)
#else
# define TIN__EAFNOSUPPORT (-4089)
#endif

#if defined(EAGAIN) && !defined(_WIN32)
# define TIN__EAGAIN (-EAGAIN)
#else
# define TIN__EAGAIN (-4088)
#endif

#if defined(EALREADY) && !defined(_WIN32)
# define TIN__EALREADY (-EALREADY)
#else
# define TIN__EALREADY (-4084)
#endif

#if defined(EBADF) && !defined(_WIN32)
# define TIN__EBADF (-EBADF)
#else
# define TIN__EBADF (-4083)
#endif

#if defined(EBUSY) && !defined(_WIN32)
# define TIN__EBUSY (-EBUSY)
#else
# define TIN__EBUSY (-4082)
#endif

#if defined(ECANCELED) && !defined(_WIN32)
# define TIN__ECANCELED (-ECANCELED)
#else
# define TIN__ECANCELED (-4081)
#endif

#if defined(ECHARSET) && !defined(_WIN32)
# define TIN__ECHARSET (-ECHARSET)
#else
# define TIN__ECHARSET (-4080)
#endif

#if defined(ECONNABORTED) && !defined(_WIN32)
# define TIN__ECONNABORTED (-ECONNABORTED)
#else
# define TIN__ECONNABORTED (-4079)
#endif

#if defined(ECONNREFUSED) && !defined(_WIN32)
# define TIN__ECONNREFUSED (-ECONNREFUSED)
#else
# define TIN__ECONNREFUSED (-4078)
#endif

#if defined(ECONNRESET) && !defined(_WIN32)
# define TIN__ECONNRESET (-ECONNRESET)
#else
# define TIN__ECONNRESET (-4077)
#endif

#if defined(EDESTADDRREQ) && !defined(_WIN32)
# define TIN__EDESTADDRREQ (-EDESTADDRREQ)
#else
# define TIN__EDESTADDRREQ (-4076)
#endif

#if defined(EEXIST) && !defined(_WIN32)
# define TIN__EEXIST (-EEXIST)
#else
# define TIN__EEXIST (-4075)
#endif

#if defined(EFAULT) && !defined(_WIN32)
# define TIN__EFAULT (-EFAULT)
#else
# define TIN__EFAULT (-4074)
#endif

#if defined(EHOSTUNREACH) && !defined(_WIN32)
# define TIN__EHOSTUNREACH (-EHOSTUNREACH)
#else
# define TIN__EHOSTUNREACH (-4073)
#endif

#if defined(EINTR) && !defined(_WIN32)
# define TIN__EINTR (-EINTR)
#else
# define TIN__EINTR (-4072)
#endif

#if defined(EINVAL) && !defined(_WIN32)
# define TIN__EINVAL (-EINVAL)
#else
# define TIN__EINVAL (-4071)
#endif

#if defined(EIO) && !defined(_WIN32)
# define TIN__EIO (-EIO)
#else
# define TIN__EIO (-4070)
#endif

#if defined(EISCONN) && !defined(_WIN32)
# define TIN__EISCONN (-EISCONN)
#else
# define TIN__EISCONN (-4069)
#endif

#if defined(EISDIR) && !defined(_WIN32)
# define TIN__EISDIR (-EISDIR)
#else
# define TIN__EISDIR (-4068)
#endif

#if defined(ELOOP) && !defined(_WIN32)
# define TIN__ELOOP (-ELOOP)
#else
# define TIN__ELOOP (-4067)
#endif

#if defined(EMFILE) && !defined(_WIN32)
# define TIN__EMFILE (-EMFILE)
#else
# define TIN__EMFILE (-4066)
#endif

#if defined(EMSGSIZE) && !defined(_WIN32)
# define TIN__EMSGSIZE (-EMSGSIZE)
#else
# define TIN__EMSGSIZE (-4065)
#endif

#if defined(ENAMETOOLONG) && !defined(_WIN32)
# define TIN__ENAMETOOLONG (-ENAMETOOLONG)
#else
# define TIN__ENAMETOOLONG (-4064)
#endif

#if defined(ENETDOWN) && !defined(_WIN32)
# define TIN__ENETDOWN (-ENETDOWN)
#else
# define TIN__ENETDOWN (-4063)
#endif

#if defined(ENETUNREACH) && !defined(_WIN32)
# define TIN__ENETUNREACH (-ENETUNREACH)
#else
# define TIN__ENETUNREACH (-4062)
#endif

#if defined(ENFILE) && !defined(_WIN32)
# define TIN__ENFILE (-ENFILE)
#else
# define TIN__ENFILE (-4061)
#endif

#if defined(ENOBUFS) && !defined(_WIN32)
# define TIN__ENOBUFS (-ENOBUFS)
#else
# define TIN__ENOBUFS (-4060)
#endif

#if defined(ENODEV) && !defined(_WIN32)
# define TIN__ENODEV (-ENODEV)
#else
# define TIN__ENODEV (-4059)
#endif

#if defined(ENOENT) && !defined(_WIN32)
# define TIN__ENOENT (-ENOENT)
#else
# define TIN__ENOENT (-4058)
#endif

#if defined(ENOMEM) && !defined(_WIN32)
# define TIN__ENOMEM (-ENOMEM)
#else
# define TIN__ENOMEM (-4057)
#endif

#if defined(ENONET) && !defined(_WIN32)
# define TIN__ENONET (-ENONET)
#else
# define TIN__ENONET (-4056)
#endif

#if defined(ENOSPC) && !defined(_WIN32)
# define TIN__ENOSPC (-ENOSPC)
#else
# define TIN__ENOSPC (-4055)
#endif

#if defined(ENOSYS) && !defined(_WIN32)
# define TIN__ENOSYS (-ENOSYS)
#else
# define TIN__ENOSYS (-4054)
#endif

#if defined(ENOTCONN) && !defined(_WIN32)
# define TIN__ENOTCONN (-ENOTCONN)
#else
# define TIN__ENOTCONN (-4053)
#endif

#if defined(ENOTDIR) && !defined(_WIN32)
# define TIN__ENOTDIR (-ENOTDIR)
#else
# define TIN__ENOTDIR (-4052)
#endif

#if defined(ENOTEMPTY) && !defined(_WIN32)
# define TIN__ENOTEMPTY (-ENOTEMPTY)
#else
# define TIN__ENOTEMPTY (-4051)
#endif

#if defined(ENOTSOCK) && !defined(_WIN32)
# define TIN__ENOTSOCK (-ENOTSOCK)
#else
# define TIN__ENOTSOCK (-4050)
#endif

#if defined(ENOTSUP) && !defined(_WIN32)
# define TIN__ENOTSUP (-ENOTSUP)
#else
# define TIN__ENOTSUP (-4049)
#endif

#if defined(EPERM) && !defined(_WIN32)
# define TIN__EPERM (-EPERM)
#else
# define TIN__EPERM (-4048)
#endif

#if defined(EPIPE) && !defined(_WIN32)
# define TIN__EPIPE (-EPIPE)
#else
# define TIN__EPIPE (-4047)
#endif

#if defined(EPROTO) && !defined(_WIN32)
# define TIN__EPROTO (-EPROTO)
#else
# define TIN__EPROTO (-4046)
#endif

#if defined(EPROTONOSUPPORT) && !defined(_WIN32)
# define TIN__EPROTONOSUPPORT (-EPROTONOSUPPORT)
#else
# define TIN__EPROTONOSUPPORT (-4045)
#endif

#if defined(EPROTOTYPE) && !defined(_WIN32)
# define TIN__EPROTOTYPE (-EPROTOTYPE)
#else
# define TIN__EPROTOTYPE (-4044)
#endif

#if defined(EROFS) && !defined(_WIN32)
# define TIN__EROFS (-EROFS)
#else
# define TIN__EROFS (-4043)
#endif

#if defined(ESHUTDOWN) && !defined(_WIN32)
# define TIN__ESHUTDOWN (-ESHUTDOWN)
#else
# define TIN__ESHUTDOWN (-4042)
#endif

#if defined(ESPIPE) && !defined(_WIN32)
# define TIN__ESPIPE (-ESPIPE)
#else
# define TIN__ESPIPE (-4041)
#endif

#if defined(ESRCH) && !defined(_WIN32)
# define TIN__ESRCH (-ESRCH)
#else
# define TIN__ESRCH (-4040)
#endif

#if defined(ETIMEDOUT) && !defined(_WIN32)
# define TIN__ETIMEDOUT (-ETIMEDOUT)
#else
# define TIN__ETIMEDOUT (-4039)
#endif

#if defined(ETXTBSY) && !defined(_WIN32)
# define TIN__ETXTBSY (-ETXTBSY)
#else
# define TIN__ETXTBSY (-4038)
#endif

#if defined(EXDEV) && !defined(_WIN32)
# define TIN__EXDEV (-EXDEV)
#else
# define TIN__EXDEV (-4037)
#endif

#if defined(EFBIG) && !defined(_WIN32)
# define TIN__EFBIG (-EFBIG)
#else
# define TIN__EFBIG (-4036)
#endif

#if defined(ENOPROTOOPT) && !defined(_WIN32)
# define TIN__ENOPROTOOPT (-ENOPROTOOPT)
#else
# define TIN__ENOPROTOOPT (-4035)
#endif

#if defined(ERANGE) && !defined(_WIN32)
# define TIN__ERANGE (-ERANGE)
#else
# define TIN__ERANGE (-4034)
#endif

#if defined(ENXIO) && !defined(_WIN32)
# define TIN__ENXIO (-ENXIO)
#else
# define TIN__ENXIO (-4033)
#endif

#if defined(EMLINK) && !defined(_WIN32)
# define TIN__EMLINK (-EMLINK)
#else
# define TIN__EMLINK (-4032)
#endif

/* EHOSTDOWN is not visible on BSD-like systems when _POSIX_C_SOURCE is
 * defined. Fortunately, its value is always 64 so it's possible albeit
 * icky to hard-code it.
 */
#if defined(EHOSTDOWN) && !defined(_WIN32)
# define TIN__EHOSTDOWN (-EHOSTDOWN)
#elif defined(__APPLE__) || \
      defined(__DragonFly__) || \
      defined(__FreeBSD__) || \
      defined(__NetBSD__) || \
      defined(__OpenBSD__)
# define TIN__EHOSTDOWN (-64)
#else
# define TIN__EHOSTDOWN (-4031)
#endif

# define TIN__ECLOSE_INTR (-5000)

# define TIN__ETIMEOUT_INTR (-5001)

# define TIN__UNEXPECTED_EOF (-5002)

# define TIN__OBJECT_CLOSED (-5003)

# define TIN__ENOPROGRESS (-5004)

# define TIN__EBUFFERFULL (-5005)

# define TIN__EBADPROTOCOL (-5006)

# define TIN__ETOOLARGE (-5007)



#endif /* TIN_ERRNO_H_ */
