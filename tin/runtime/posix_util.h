// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "tin/net/sys_socket.h"

namespace tin {

int Nonblock(int fd, bool set);

int Cloexec(int fd, bool set);

int Accept(int socket, struct sockaddr* address, socklen_t* address_len);

}  // namespace tin
