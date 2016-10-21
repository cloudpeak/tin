// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "tin/error/error_inl.h"

/* Expand this list if necessary. */
#define TIN_ERRNO_MAP(XX)                                                      \
  XX(E2BIG, "argument list too long")                                         \
  XX(EACCES, "permission denied")                                             \
  XX(EADDRINUSE, "address already in use")                                    \
  XX(EADDRNOTAVAIL, "address not available")                                  \
  XX(EAFNOSUPPORT, "address family not supported")                            \
  XX(EAGAIN, "resource temporarily unavailable")                              \
  XX(EAI_ADDRFAMILY, "address family not supported")                          \
  XX(EAI_AGAIN, "temporary failure")                                          \
  XX(EAI_BADFLAGS, "bad ai_flags value")                                      \
  XX(EAI_BADHINTS, "invalid value for hints")                                 \
  XX(EAI_CANCELED, "request canceled")                                        \
  XX(EAI_FAIL, "permanent failure")                                           \
  XX(EAI_FAMILY, "ai_family not supported")                                   \
  XX(EAI_MEMORY, "out of memory")                                             \
  XX(EAI_NODATA, "no address")                                                \
  XX(EAI_NONAME, "unknown node or service")                                   \
  XX(EAI_OVERFLOW, "argument buffer overflow")                                \
  XX(EAI_PROTOCOL, "resolved protocol is unknown")                            \
  XX(EAI_SERVICE, "service not available for socket type")                    \
  XX(EAI_SOCKTYPE, "socket type not supported")                               \
  XX(EALREADY, "connection already in progress")                              \
  XX(EBADF, "bad file descriptor")                                            \
  XX(EBUSY, "resource busy or locked")                                        \
  XX(ECANCELED, "operation canceled")                                         \
  XX(ECHARSET, "invalid Unicode character")                                   \
  XX(ECONNABORTED, "software caused connection abort")                        \
  XX(ECONNREFUSED, "connection refused")                                      \
  XX(ECONNRESET, "connection reset by peer")                                  \
  XX(EDESTADDRREQ, "destination address required")                            \
  XX(EEXIST, "file already exists")                                           \
  XX(EFAULT, "bad address in system call argument")                           \
  XX(EFBIG, "file too large")                                                 \
  XX(EHOSTUNREACH, "host is unreachable")                                     \
  XX(EINTR, "interrupted system call")                                        \
  XX(EINVAL, "invalid argument")                                              \
  XX(EIO, "i/o error")                                                        \
  XX(EISCONN, "socket is already connected")                                  \
  XX(EISDIR, "illegal operation on a directory")                              \
  XX(ELOOP, "too many symbolic links encountered")                            \
  XX(EMFILE, "too many open files")                                           \
  XX(EMSGSIZE, "message too long")                                            \
  XX(ENAMETOOLONG, "name too long")                                           \
  XX(ENETDOWN, "network is down")                                             \
  XX(ENETUNREACH, "network is unreachable")                                   \
  XX(ENFILE, "file table overflow")                                           \
  XX(ENOBUFS, "no buffer space available")                                    \
  XX(ENODEV, "no such device")                                                \
  XX(ENOENT, "no such file or directory")                                     \
  XX(ENOMEM, "not enough memory")                                             \
  XX(ENONET, "machine is not on the network")                                 \
  XX(ENOPROTOOPT, "protocol not available")                                   \
  XX(ENOSPC, "no space left on device")                                       \
  XX(ENOSYS, "function not implemented")                                      \
  XX(ENOTCONN, "socket is not connected")                                     \
  XX(ENOTDIR, "not a directory")                                              \
  XX(ENOTEMPTY, "directory not empty")                                        \
  XX(ENOTSOCK, "socket operation on non-socket")                              \
  XX(ENOTSUP, "operation not supported on socket")                            \
  XX(EPERM, "operation not permitted")                                        \
  XX(EPIPE, "broken pipe")                                                    \
  XX(EPROTO, "protocol error")                                                \
  XX(EPROTONOSUPPORT, "protocol not supported")                               \
  XX(EPROTOTYPE, "protocol wrong type for socket")                            \
  XX(ERANGE, "result too large")                                              \
  XX(EROFS, "read-only file system")                                          \
  XX(ESHUTDOWN, "cannot send after transport endpoint shutdown")              \
  XX(ESPIPE, "invalid seek")                                                  \
  XX(ESRCH, "no such process")                                                \
  XX(ETIMEDOUT, "connection timed out")                                       \
  XX(ETXTBSY, "text file is busy")                                            \
  XX(EXDEV, "cross-device link not permitted")                                \
  XX(UNKNOWN, "unknown error")                                                \
  XX(EOF, "end of file")                                                      \
  XX(ENXIO, "no such device or address")                                      \
  XX(EMLINK, "too many links")                                                \
  XX(EHOSTDOWN, "host is down")                                               \
  XX(ECLOSE_INTR, "IO is interrupted by close")                               \
  XX(ETIMEOUT_INTR, "IO is interrupted by timeout")                           \
  XX(UNEXPECTED_EOF, "unexpected eof")                                        \
  XX(OBJECT_CLOSED, "object(chan etc) is closed")                             \
  XX(ENOPROGRESS, "multiple Read calls return no data or error")              \
  XX(EBUFFERFULL, "bufio: buffer full")                                       \
  XX(EBADPROTOCOL, "bad protocol")                                            \
  XX(ETOOLARGE, "read too big")


typedef enum {
#define XX(code, _) TIN_ ## code = TIN__ ## code,
  TIN_ERRNO_MAP(XX)
#undef XX
  TIN_ERRNO_MAX = TIN__EOF - 1
} tin_errno_t;

int TinTranslateSysError(int sys_errno);

int TinGetaddrinfoTranslateError(int sys_err);

class SysErrorTranslator {
 public:
  explicit SysErrorTranslator(int* error_code)
    : error_code_(error_code) {
  }
  ~SysErrorTranslator() {
    *error_code_ = TinTranslateSysError(*error_code_);
  }

 private:
  int* error_code_;
};

#define TIN_ERR_NAME_GEN(name, _) case TIN_ ## name: return #name;
inline const char* TinErrorName(int err) {
  switch (err) {
    TIN_ERRNO_MAP(TIN_ERR_NAME_GEN)
  }
  return "unkown error";
}
