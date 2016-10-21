// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/basictypes.h"
#include "base/strings/string_piece.h"
#include "tin/net/ip_address.h"
#include "tin/net/tcp_conn.h"
#include "tin/net/listener.h"

namespace tin {
namespace net {

TcpConn DialTcp(const IPAddress& address, uint16 port);

TcpConn DialTcp(const base::StringPiece& addr, uint16 port);

TcpConn DialTcpTimeout(const IPAddress& address, uint16 port, int64 deadline);

TcpConn DialTcpTimeout(const base::StringPiece& addr, uint16 port,
                       int64 deadline);

TCPListener ListenTcp(const base::StringPiece& addr, uint16 port,
                      int backlog = 511);

}  // namespace net
}  // namespace tin

