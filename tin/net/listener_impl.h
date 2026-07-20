// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// P2-1 PIMPL: Internal implementation header for TcpListenerImpl.
// The public TcpListener class is defined in include/tin/net/listener.h.
// This file is NOT part of the public API.

#ifndef TIN_NET_LISTENER_IMPL_H_
#define TIN_NET_LISTENER_IMPL_H_
#include <memory>

#include "tin/time/time.h"
#include "tin/result.h"

namespace tin::net {

class NetFD;
class TcpConn;  // forward-declared; full definition in public tcp_conn.h

class TcpListenerImpl
  : public std::enable_shared_from_this<TcpListenerImpl> {
 public:
  TcpListenerImpl(std::unique_ptr<NetFD> netfd, int backlog);
  ~TcpListenerImpl();

  TcpListenerImpl(const TcpListenerImpl&) = delete;
  TcpListenerImpl& operator=(const TcpListenerImpl&) = delete;

  Status SetDeadline(int64_t t);
  Result<TcpConn> Accept();
  Status Close();

 private:
  std::unique_ptr<NetFD> netfd_;
};

}  // namespace tin::net
#endif  // TIN_NET_LISTENER_IMPL_H_
