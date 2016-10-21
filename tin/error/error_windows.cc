// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <WinSock2.h>
#include <windows.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tin/error/error.h"

int TinTranslateSysError(int sys_errno) {
  if (sys_errno <= 0) {
    return sys_errno;  /* If < 0 then it's already a libuv error. */
  }

  switch (sys_errno) {
  case ERROR_NOACCESS:                    return TIN_EACCES;
  case WSAEACCES:                         return TIN_EACCES;
  case ERROR_ADDRESS_ALREADY_ASSOCIATED:  return TIN_EADDRINUSE;
  case WSAEADDRINUSE:                     return TIN_EADDRINUSE;
  case WSAEADDRNOTAVAIL:                  return TIN_EADDRNOTAVAIL;
  case WSAEAFNOSUPPORT:                   return TIN_EAFNOSUPPORT;
  case WSAEWOULDBLOCK:                    return TIN_EAGAIN;
  case WSAEALREADY:                       return TIN_EALREADY;
  case ERROR_INVALID_FLAGS:               return TIN_EBADF;
  case ERROR_INVALID_HANDLE:              return TIN_EBADF;
  case ERROR_LOCK_VIOLATION:              return TIN_EBUSY;
  case ERROR_PIPE_BUSY:                   return TIN_EBUSY;
  case ERROR_SHARING_VIOLATION:           return TIN_EBUSY;
  case ERROR_OPERATION_ABORTED:           return TIN_ECANCELED;
  case WSAEINTR:                          return TIN_ECANCELED;
  case ERROR_NO_UNICODE_TRANSLATION:      return TIN_ECHARSET;
  case ERROR_CONNECTION_ABORTED:          return TIN_ECONNABORTED;
  case WSAECONNABORTED:                   return TIN_ECONNABORTED;
  case ERROR_CONNECTION_REFUSED:          return TIN_ECONNREFUSED;
  case WSAECONNREFUSED:                   return TIN_ECONNREFUSED;
  case ERROR_NETNAME_DELETED:             return TIN_ECONNRESET;
  case WSAECONNRESET:                     return TIN_ECONNRESET;
  case ERROR_ALREADY_EXISTS:              return TIN_EEXIST;
  case ERROR_FILE_EXISTS:                 return TIN_EEXIST;
  case ERROR_BUFFER_OVERFLOW:             return TIN_EFAULT;
  case WSAEFAULT:                         return TIN_EFAULT;
  case ERROR_HOST_UNREACHABLE:            return TIN_EHOSTUNREACH;
  case WSAEHOSTUNREACH:                   return TIN_EHOSTUNREACH;
  case ERROR_INSUFFICIENT_BUFFER:         return TIN_EINVAL;
  case ERROR_INVALID_DATA:                return TIN_EINVAL;
  case ERROR_INVALID_PARAMETER:           return TIN_EINVAL;
  case ERROR_SYMLINK_NOT_SUPPORTED:       return TIN_EINVAL;
  case WSAEINVAL:                         return TIN_EINVAL;
  case WSAEPFNOSUPPORT:                   return TIN_EINVAL;
  case WSAESOCKTNOSUPPORT:                return TIN_EINVAL;
  case ERROR_BEGINNING_OF_MEDIA:          return TIN_EIO;
  case ERROR_BUS_RESET:                   return TIN_EIO;
  case ERROR_CRC:                         return TIN_EIO;
  case ERROR_DEVICE_DOOR_OPEN:            return TIN_EIO;
  case ERROR_DEVICE_REQUIRES_CLEANING:    return TIN_EIO;
  case ERROR_DISK_CORRUPT:                return TIN_EIO;
  case ERROR_EOM_OVERFLOW:                return TIN_EIO;
  case ERROR_FILEMARK_DETECTED:           return TIN_EIO;
  case ERROR_GEN_FAILURE:                 return TIN_EIO;
  case ERROR_INVALID_BLOCK_LENGTH:        return TIN_EIO;
  case ERROR_IO_DEVICE:                   return TIN_EIO;
  case ERROR_NO_DATA_DETECTED:            return TIN_EIO;
  case ERROR_NO_SIGNAL_SENT:              return TIN_EIO;
  case ERROR_OPEN_FAILED:                 return TIN_EIO;
  case ERROR_SETMARK_DETECTED:            return TIN_EIO;
  case ERROR_SIGNAL_REFUSED:              return TIN_EIO;
  case WSAEISCONN:                        return TIN_EISCONN;
  case ERROR_CANT_RESOLVE_FILENAME:       return TIN_ELOOP;
  case ERROR_TOO_MANY_OPEN_FILES:         return TIN_EMFILE;
  case WSAEMFILE:                         return TIN_EMFILE;
  case WSAEMSGSIZE:                       return TIN_EMSGSIZE;
  case ERROR_FILENAME_EXCED_RANGE:        return TIN_ENAMETOOLONG;
  case ERROR_NETWORK_UNREACHABLE:         return TIN_ENETUNREACH;
  case WSAENETUNREACH:                    return TIN_ENETUNREACH;
  case WSAENOBUFS:                        return TIN_ENOBUFS;
  case ERROR_BAD_PATHNAME:                return TIN_ENOENT;
  case ERROR_DIRECTORY:                   return TIN_ENOENT;
  case ERROR_FILE_NOT_FOUND:              return TIN_ENOENT;
  case ERROR_INVALID_NAME:                return TIN_ENOENT;
  case ERROR_INVALID_DRIVE:               return TIN_ENOENT;
  case ERROR_INVALID_REPARSE_DATA:        return TIN_ENOENT;
  case ERROR_MOD_NOT_FOUND:               return TIN_ENOENT;
  case ERROR_PATH_NOT_FOUND:              return TIN_ENOENT;
  case WSAHOST_NOT_FOUND:                 return TIN_ENOENT;
  case WSANO_DATA:                        return TIN_ENOENT;
  case ERROR_NOT_ENOUGH_MEMORY:           return TIN_ENOMEM;
  case ERROR_OUTOFMEMORY:                 return TIN_ENOMEM;
  case ERROR_CANNOT_MAKE:                 return TIN_ENOSPC;
  case ERROR_DISK_FULL:                   return TIN_ENOSPC;
  case ERROR_EA_TABLE_FULL:               return TIN_ENOSPC;
  case ERROR_END_OF_MEDIA:                return TIN_ENOSPC;
  case ERROR_HANDLE_DISK_FULL:            return TIN_ENOSPC;
  case ERROR_NOT_CONNECTED:               return TIN_ENOTCONN;
  case WSAENOTCONN:                       return TIN_ENOTCONN;
  case ERROR_DIR_NOT_EMPTY:               return TIN_ENOTEMPTY;
  case WSAENOTSOCK:                       return TIN_ENOTSOCK;
  case ERROR_NOT_SUPPORTED:               return TIN_ENOTSUP;
  case ERROR_BROKEN_PIPE:                 return TIN_EOF;
  case ERROR_ACCESS_DENIED:               return TIN_EPERM;
  case ERROR_PRIVILEGE_NOT_HELD:          return TIN_EPERM;
  case ERROR_BAD_PIPE:                    return TIN_EPIPE;
  case ERROR_NO_DATA:                     return TIN_EPIPE;
  case ERROR_PIPE_NOT_CONNECTED:          return TIN_EPIPE;
  case WSAESHUTDOWN:                      return TIN_EPIPE;
  case WSAEPROTONOSUPPORT:                return TIN_EPROTONOSUPPORT;
  case ERROR_WRITE_PROTECT:               return TIN_EROFS;
  case ERROR_SEM_TIMEOUT:                 return TIN_ETIMEDOUT;
  case WSAETIMEDOUT:                      return TIN_ETIMEDOUT;
  case ERROR_NOT_SAME_DEVICE:             return TIN_EXDEV;
  case ERROR_INVALID_FUNCTION:            return TIN_EISDIR;
  case ERROR_META_EXPANSION_TOO_LONG:     return TIN_E2BIG;
  // new added
  case ERROR_HANDLE_EOF:                  return TIN_EOF;
  default:                                return TIN_UNKNOWN;
  }
}

int TinGetaddrinfoTranslateError(int sys_err) {
  switch (sys_err) {
  case 0:                       return 0;
  case WSATRY_AGAIN:            return TIN_EAI_AGAIN;
  case WSAEINVAL:               return TIN_EAI_BADFLAGS;
  case WSANO_RECOVERY:          return TIN_EAI_FAIL;
  case WSAEAFNOSUPPORT:         return TIN_EAI_FAMILY;
  case WSA_NOT_ENOUGH_MEMORY:   return TIN_EAI_MEMORY;
  case WSAHOST_NOT_FOUND:       return TIN_EAI_NONAME;
  case WSATYPE_NOT_FOUND:       return TIN_EAI_SERVICE;
  case WSAESOCKTNOSUPPORT:      return TIN_EAI_SOCKTYPE;
  default:                      return TinTranslateSysError(sys_err);
  }
}
