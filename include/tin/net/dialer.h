// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once


#include <absl/strings/string_view.h>
#include "tin/net/ip_address.h"
#include "tin/net/tcp_conn.h"
#include "tin/net/listener.h"
#include "tin/result.h"

namespace tin {
namespace net {

Result<TcpConn> DialTcp(const IPAddress& address, uint16_t port);

Result<TcpConn> DialTcp(const absl::string_view& addr, uint16_t port);

Result<TcpConn> DialTcpTimeout(const IPAddress& address, uint16_t port,
                               int64_t deadline);

Result<TcpConn> DialTcpTimeout(const absl::string_view& addr, uint16_t port,
                               int64_t deadline);

Result<TCPListener> ListenTcp(const IPAddress& address, uint16_t port,
                              int backlog = 511);

Result<TCPListener> ListenTcp(const absl::string_view& addr, uint16_t port,
                              int backlog = 511);

}  // namespace net
}  // namespace tin
