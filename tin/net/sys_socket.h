// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#ifndef NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_SOCKETS_H_
#define NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_SOCKETS_H_ 1

#include "build/build_config.h"

#if defined(OS_WIN)

#include <winsock2.h>
#include <windows.h>
#include <WS2tcpip.h>
#else

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>

#endif

#endif  // NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_SOCKETS_H_
