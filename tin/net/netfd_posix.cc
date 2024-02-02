// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>


#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/once.h"
#include "base/strings/string_util.h"
#include "base/posix/eintr_wrapper.h"

#include "tin/error/error.h"
#include "tin/runtime/env.h"
#include "tin/runtime/posix_util.h"
#include "tin/runtime/net/pollops.h"
#include "tin/net/net.h"
#include "tin/net/sockaddr_storage.h"
#include "tin/net/ip_address.h"
#include "tin/platform/platform_posix.h"

#include "tin/net/netfd_posix.h"

namespace tin {
namespace net {

NetFD::NetFD(uintptr_t sysfd,
             AddressFamily family,
             int sotype,
             const std::string& net)
  : NetFDCommon(sysfd, family, sotype, net) {
}

NetFD::~NetFD() {
  Close();
}

int NetFD::Init() {
  return pd_.Init(sysfd_);
}

int NetFD::Read(void* buf, int len, int* nread) {
  int err = ReadLock();
  if (err != 0) {
    *nread = 0;
    return err;
  }
  err = pd_.PrepareRead();
  if (err != 0) {
    ReadUnlock();
    *nread = 0;
    return err;
  }
  while (true) {
    // non-blocking read should never set EINTR,
    // however, it's harmless to deal with it.
    int n = HANDLE_EINTR(read(IntFd(), buf, len));
    err = (n == -1) ? errno : 0;
    if (err != 0) {
      n = 0;
      if (err == EAGAIN) {
        err = pd_.WaitRead();
        if (err == 0) {
          continue;
        }
      }
    }
    err = EofError(n, err);
    if (!err)
      *nread = n;
    break;
  }
  ReadUnlock();
  return err;
}

int NetFD::Write(const void* buf, int len, int* nwritten) {
  int err = WriteLock();
  if (err != 0) {
    *nwritten = 0;
    return err;
  }
  err = pd_.PrepareWrite();
  if (err != 0) {
    WriteLock();
    *nwritten = 0;
    return err;
  }
  const char* ptr = static_cast<const char*>(buf);
  int nn = 0;
  while (true) {
    int n = HANDLE_EINTR(write(IntFd(), ptr + nn, len - nn));
    err = (n == -1) ? errno : 0;
    if (n > 0) {
      nn += n;
    }
    if (nn == len) {
      break;
    }

    if (err == EAGAIN) {
      err = pd_.WaitWrite();
      // waked up, io ready or error occurred(timeout intr, close intr, etc).
      if (err == 0) {
        continue;
      }
    }
    if (err != 0)
      break;
    if (n == 0) {
      err = TIN_UNEXPECTED_EOF;
      break;
    }
  }
  WriteUnlock();
  *nwritten = nn;
  return err;
}

void NetFD::Destroy() {
  if (sysfd_ == kInvalidSocket)
    return;
  pd_.Close();
  close(IntFd());
  sysfd_ = kInvalidSocket;
}

int NetFD::Shutdown(int how) {
  int err = Incref();
  if (err != 0) {
    return err;
  }
  err = (shutdown(IntFd(), how)) == -1 ? errno : 0;
  Decref();
  return err;
}

int NetFD::CloseRead() {
  return Shutdown(SHUT_RD);
}

int NetFD::CloseWrite() {
  return Shutdown(SHUT_WR);
}

int NetFD::Dial(IPEndPoint* local, IPEndPoint* remote, int64_t deadline) {
  int err = 0;
  SockaddrStorage lstorage;
  if (local != NULL) {
    if (local->GetFamily() != family_) {
      return EINVAL;
    }
    if (!local->ToSockAddr(lstorage.addr, &lstorage.addr_len)) {
      return EINVAL;
    }

    if (::bind(sysfd_, lstorage.addr, lstorage.addr_len) != 0) {
      return errno;
    }
  }

  SockaddrStorage rstorage;
  if (remote != NULL) {
    if (remote->GetFamily() != family_) {
      return EINVAL;
    }
    if (!remote->ToSockAddr(rstorage.addr, &rstorage.addr_len)) {
      return EINVAL;
    }
    err = Connect(local == NULL ? NULL : &lstorage, &rstorage, deadline);
    if (err != 0) {
      return err;
    }
    is_connected_ = true;
  } else {
    err = Init();
    if (err != 0) {
      return err;
    }
  }
  return 0;
}

int NetFD::Bind(const IPEndPoint& address) {
  int err = 0;
  SockaddrStorage storage;
  if (address.ToSockAddr(storage.addr, &storage.addr_len)) {
    err = bind(sysfd_, storage.addr, storage.addr_len) == -1 ? errno : 0;
    return err;
  }
  return EINVAL;
}

int NetFD::Listen(int backlog /*= 511*/) {
  int err = listen(sysfd_, backlog) == -1 ? errno : 0;
  return err;
}

int NetFD::Accept(NetFD** newfd) {
  int err = pd_.PrepareRead();
  if (err != 0) {
    return err;
  }
  int fd = -1;
  while (true) {
    fd = tin::Accept(IntFd(), NULL, NULL);
    err = fd == -1 ? errno : 0;
    if (err != 0) {
      if (err  == EAGAIN) {
        err = pd_.WaitRead();
        if (err == 0) {
          continue;
        }
      } else if (err == ECONNABORTED) {
        continue;
      }
      return err;
    }
    break;
  }
  scoped_ptr<NetFD> netfd(new NetFD(fd, family_, sotype_, net_));
  err = netfd->Init();
  if (err != 0) {
    return err;
  }
  *newfd = netfd.release();
  return 0;
}


int NetFD::AcceptImpl(NetFD** newfd) {
  int err = ReadLock();
  if (err != 0) {
    return err;
  }
  err = Accept(newfd);
  ReadUnlock();
  return err;
}

int NetFD::GetSockOpt(int level, int name, void* optval,
                      socklen_t* optlen) {
  int err = getsockopt(IntFd(), level, name, optval, optlen);
  return err == -1 ? errno : 0;
}

int NetFD::SetSockOpt(int level, int name, const void* optval,
                      socklen_t optlen) {
  int err = setsockopt(IntFd(), level, name, optval, optlen);
  return err == -1 ? errno : 0;
}

int NetFD::SetTCPKeepAlive(bool enable, int sec) {
  int on = enable ? 1 : 0;
  if (setsockopt(IntFd(), SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)))
    return -errno;

#ifdef TCP_KEEPIDLE
  if (on && setsockopt(IntFd(), IPPROTO_TCP, TCP_KEEPIDLE, &sec, sizeof(sec)))
    return errno;
#endif

  /* Solaris/SmartOS, if you don't support keep-alive,
   * then don't advertise it in your system headers...
   */
  /* FIXME(bnoordhuis) That's possibly because sizeof(delay) should be 1. */
#if defined(TCP_KEEPALIVE) && !defined(__sun)
  if (on && setsockopt(IntFd(), IPPROTO_TCP, TCP_KEEPALIVE, &sec, sizeof(sec)))
    return errno;
#endif

  return 0;
}


int NetFD::EofError(int n, int err) {
  if (n == 0 && err == 0 && sotype_ != SOCK_DGRAM && sotype_ != SOCK_RAW) {
    err = TIN_EOF;
  }
  return err;
}
int NetFD::Connect(SockaddrStorage* laddr, SockaddrStorage* raddr,
                   int64_t deadline) {
  (void)laddr;
  errno = 0;
  int err = connect(IntFd(), raddr->addr, raddr->addr_len) == -1 ? errno : 0;
  switch (err) {
  case EINPROGRESS: case EALREADY: case EINTR:
    break;
  case 0: case EISCONN: {
    return Init();
  }
  case EINVAL: {
#if defined(OS_SOLARIS)
    return 0;
#endif
  }
  default:
    return err;
  }

  err = Init();
  if (err != 0) {
    return err;
  }
  if (deadline != 0)
    SetWriteDeadline(deadline);

  while (true) {
    err = pd_.WaitWrite();
    if (err != 0) {
      break;
    }
    socklen_t errlen = sizeof(err);
    COMPILE_ASSERT(sizeof(err) == sizeof(int), sock_len_size_must_be_int_size);
    if (getsockopt(IntFd(), SOL_SOCKET, SO_ERROR, &err, &errlen) == -1) {
      err = errno;
      break;
    }
    if (err == EINPROGRESS || err == EALREADY || err == EINTR) {
      continue;
    }
    if (err == EISCONN) {
      err = 0;
    }
    break;
  }
  if (deadline != 0)
    SetWriteDeadline(0);
  return err;
}

NetFD* NewFD(AddressFamily family, int sotype, int* error_code) {
  // int sotype = SOCK_STREAM;
  // protocol:  If the protocol argument is zero,
  // the default protocol for this address family and type shall be used.
  int err = EINVAL;
  int sysfd;
#if defined(OS_LINUX)
  sysfd = socket(ConvertAddressFamily(family),
                 sotype | TIN__O_NONBLOCK | TIN__O_CLOEXEC, 0);
  err = sysfd == -1 ? errno : 0;
#endif
  if (err == EINVAL || err == EPROTONOSUPPORT) {
    sysfd = socket(ConvertAddressFamily(family), sotype, 0);
    err = sysfd == -1 ? errno : 0;
    if (err == 0) {
      err = (Cloexec(sysfd, true) == -1) ? errno : 0;
      if (err == 0) {
        err = (Nonblock(sysfd, true) == -1) ? errno : 0;
      }
    }
  }
#if defined(OS_MACOSX)
  if (err == 0 && tin::runtime::rtm_conf->IgnoreSigpipe()) {
    int value = 1;
    // note: -1 failed, 0 successful.
    if (setsockopt(sysfd, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value))) {
      err = errno;
    }
  }
#endif
  if (err != 0) {
    VLOG(1) << "NewFD failed due to " << strerror(err);
    if (sysfd) {
      close(sysfd);
    }
    if (error_code != NULL)
      *error_code = err;
    return NULL;
  }
  return new NetFD(sysfd, family, sotype, "unused");
}

}  // namespace net
}  // namespace tin
