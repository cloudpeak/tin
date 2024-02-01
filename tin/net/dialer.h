// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once


#include <absl/strings/string_view.h>
#include "tin/net/ip_address.h"
#include "tin/net/tcp_conn.h"
#include "tin/net/listener.h"

namespace tin {
namespace net {

TcpConn DialTcp(const IPAddress& address, uint16_t port);

TcpConn DialTcp(const absl::string_view& addr, uint16_t port);

TcpConn DialTcpTimeout(const IPAddress& address, uint16_t port, int64_t deadline);

TcpConn DialTcpTimeout(const absl::string_view& addr, uint16_t port,
                       int64_t deadline);

TCPListener ListenTcp(const absl::string_view& addr, uint16_t port,
                      int backlog = 511);

}  // namespace net
}  // namespace tin

