// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// P2-1 PIMPL: Internal implementation header for TCPListenerImpl.
// The public TCPListener class is defined in include/tin/net/listener.h.
// This file is NOT part of the public API.

#pragma once

#include <memory>

#include "tin/time/time.h"
#include "tin/result.h"

namespace tin::net {

class NetFD;
class TcpConn;  // forward-declared; full definition in public tcp_conn.h

class TCPListenerImpl
  : public std::enable_shared_from_this<TCPListenerImpl> {
 public:
  TCPListenerImpl(NetFD* netfd, int backlog);
  ~TCPListenerImpl();

  TCPListenerImpl(const TCPListenerImpl&) = delete;
  TCPListenerImpl& operator=(const TCPListenerImpl&) = delete;

  Status SetDeadline(int64_t t);
  Result<TcpConn> Accept();
  Status Close();

 private:
  NetFD* netfd_;
};

}  // namespace tin::net
