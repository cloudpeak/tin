// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "tin/platform/platform_posix.h"
#include "tin/runtime/posix_util.h"

namespace tin {

#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__) || \
  defined(_AIX) || defined(__DragonFly__)

int Nonblock(int fd, bool set) {
  int flag = set ? 1 : 0;
  return  HANDLE_EINTR(ioctl(fd, FIONBIO, &flag));
}

int Cloexec(int fd, bool set) {
  return HANDLE_EINTR(ioctl(fd, set ? FIOCLEX : FIONCLEX));
}

#else

// 2 syscalls version.
int Nonblock(int fd, bool set) {
  int r = HANDLE_EINTR(fcntl(fd, F_GETFL));
  if (r != -1) {
    /* Bail out now if already set/clear. */
    if (!!(r & O_NONBLOCK) == !!set)
      return 0;
    int flags =  set ? (r | O_NONBLOCK) : (r & ~O_NONBLOCK);
    r = HANDLE_EINTR(fcntl(fd, F_SETFL, flags));
  }
  return r;
}

int Cloexec(int fd, bool set) {
  int r = HANDLE_EINTR(fcntl(fd, F_GETFD));
  if (r != -1) {
    /* Bail out now if already set/clear. */
    if (!!(r & FD_CLOEXEC) == !!set)
      return 0;
    int flags = set ? (r | FD_CLOEXEC) : (r & ~FD_CLOEXEC);
    r = HANDLE_EINTR(fcntl(fd, F_SETFD, flags));
  }
  return r;
}

#endif

#if defined(OS_LINUX)
int Accept4(int fd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
#if defined(__i386__)
  unsigned long args[4];// NOLINT
  int r;

  args[0] = (unsigned long) fd;  // NOLINT
  args[1] = (unsigned long) addr;  // NOLINT
  args[2] = (unsigned long) addrlen;  // NOLINT
  args[3] = (unsigned long) flags;  // NOLINT

  r = syscall(__NR_socketcall, 18 /* SYS_ACCEPT4 */, args);

  /* socketcall() raises EINVAL when SYS_ACCEPT4 is not supported but so does
   * a bad flags argument. Try to distinguish between the two cases.
   */
  if (r == -1)
    if (errno == EINVAL)
      if ((flags & ~(TIN__SOCK_CLOEXEC | TIN__SOCK_NONBLOCK)) == 0)
        errno = ENOSYS;

  return r;
#elif defined(__NR_accept4)
  return syscall(__NR_accept4, fd, addr, addrlen, flags);
#else
  errno = ENOSYS;
  return -1;
#endif
}
#else
int Accept4(int fd, struct sockaddr* addr, socklen_t* addrlen, int flags) {
  errno = ENOSYS;
  return -1;
}
#endif

int Accept(int socket, struct sockaddr* address, socklen_t* address_len) {
  int err = 0;
  int fd;

#if defined(OS_LINUX)
  fd = HANDLE_EINTR(Accept4(socket, address, address_len,
                            TIN__SOCK_NONBLOCK | TIN__SOCK_CLOEXEC));
  if (fd != -1)
    return fd;
  err = errno;
  if (err != ENOSYS && err != EINVAL && err != EACCES && err != EFAULT) {
    return -1;
  }
#endif

  fd = HANDLE_EINTR(accept(socket, address, address_len));
  if (fd == -1)
    return -1;
  err = Cloexec(fd, true);
  if (err != -1) {
    err = Nonblock(fd, true);
  }
  if (err != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

}  // namespace tin
