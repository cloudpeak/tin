// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TIN_NET_DIALER_H_
#define TIN_NET_DIALER_H_
#include <absl/strings/string_view.h>
#include "tin/net/ip_address.h"
#include "tin/net/tcp_conn.h"
#include "tin/net/listener.h"

namespace tin::net {

TcpConn DialTcp(const IpAddress& address, uint16_t port);

TcpConn DialTcp(const absl::string_view& addr, uint16_t port);

TcpConn DialTcpTimeout(const IpAddress& address, uint16_t port, int64_t deadline);

TcpConn DialTcpTimeout(const absl::string_view& addr, uint16_t port,
                       int64_t deadline);

TcpListener ListenTcp(const absl::string_view& addr, uint16_t port,
                      int backlog = 511);

}  // namespace tin::net
#endif  // TIN_NET_DIALER_H_
