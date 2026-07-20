// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_RUNTIME_POSIX_UTIL_H_
#define TIN_RUNTIME_POSIX_UTIL_H_
#include "tin/net/sys_socket.h"

namespace tin {

int Nonblock(int fd, bool set);

int Cloexec(int fd, bool set);

int Accept(int socket, struct sockaddr* address, socklen_t* address_len);

}  // namespace tin
#endif  // TIN_RUNTIME_POSIX_UTIL_H_
