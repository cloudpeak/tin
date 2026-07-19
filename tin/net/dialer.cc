// Copyright (c) 2016 Tin Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/log/log.h>
#include <memory>
#include "tin/error/error.h"
#include "tin/time/time.h"
#include "tin/net/ip_endpoint.h"
#include "tin/net/netfd.h"
#include "tin/net/dialer.h"         // public: DialTcp, ListenTcp
#include "tin/net/tcp_conn.h"       // public: TcpConn, MakeTcpConn
#include "tin/net/listener.h"       // public: TCPListener (PIMPL)
#include "tin/net/listener_impl.h"  // internal: TCPListenerImpl
#include "tin/runtime/runtime.h"

namespace tin {
namespace net {

Result<TcpConn> DialTcpInternal(const IPAddress& address, uint16_t port,
                                int64_t deadline) {
  int err = 0;
  AddressFamily family =
    address.IsIPv4() ? ADDRESS_FAMILY_IPV4 : ADDRESS_FAMILY_IPV6;
  NetFD* netfd = NewFD(family, SOCK_STREAM, &err);
  if (netfd != nullptr) {
    IPEndPoint endpoint(address, port);
    if (deadline == -1)
      deadline = UINT64_MAX;
    err = netfd->Dial(nullptr, &endpoint, UINT64_MAX);
    if (err != 0) {
      delete netfd;
      netfd = nullptr;
    }
  }
  if (netfd == nullptr) {
    return Result<TcpConn>::Err(TinTranslateSysError(err));
  }
  return Result<TcpConn>::Ok(MakeTcpConn(netfd));
}

Result<TcpConn> DialTcpInternal(const absl::string_view& address, uint16_t port,
                                int64_t deadline) {
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(address)) {
    return Result<TcpConn>::Err(TIN_EINVAL);
  }
  return DialTcpInternal(ip_address, port, deadline);
}

Result<TcpConn> DialTcp(const IPAddress& address, uint16_t port) {
  return DialTcpInternal(address, port, -1);
}

Result<TcpConn> DialTcp(const absl::string_view& address, uint16_t port) {
  return DialTcpInternal(address, port, -1);
}

Result<TcpConn> DialTcpTimeout(const IPAddress& address, uint16_t port,
                               int64_t deadline) {
  return DialTcpInternal(address, port, deadline);
}

Result<TcpConn> DialTcpTimeout(const absl::string_view& address, uint16_t port,
                               int64_t deadline) {
  return DialTcpInternal(address, port, deadline);
}

Result<TCPListener> ListenTcp(const IPAddress& address, uint16_t port,
                              int backlog) {
  int err = 0;
  AddressFamily family =
    address.IsIPv4() ? ADDRESS_FAMILY_IPV4 : ADDRESS_FAMILY_IPV6;
  NetFD* netfd = NewFD(family, SOCK_STREAM, &err);
  if (netfd != nullptr) {
    err = netfd->Init();
    if (err == 0) {
#if defined(OS_POSIX)
      int on = 1;
      err = netfd->SetSockOpt(SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif
    }
    if (err == 0) {
      IPEndPoint endpoint(address, port);
      err = netfd->Bind(endpoint);
      if (err != 0) {
        LOG(INFO) << "Bind failed: " << TinErrorName(TinTranslateSysError(err));
      }
    }
  }
  if (err == 0) {
    err = netfd->Listen(backlog);
  }
  if (err != 0 && netfd != nullptr) {
    delete netfd;
    netfd = nullptr;
  }
  if (netfd == nullptr) {
    return Result<TCPListener>::Err(TinTranslateSysError(err));
  }
  // P2-1 PIMPL: use make_shared for single allocation.
  return Result<TCPListener>::Ok(
      TCPListener(std::make_shared<TCPListenerImpl>(netfd, backlog)));
}

Result<TCPListener> ListenTcp(const absl::string_view& address, uint16_t port,
                              int backlog) {
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(address)) {
    return Result<TCPListener>::Err(TIN_EINVAL);
  }
  return ListenTcp(ip_address, port, backlog);
}

}  // namespace net
}  // namespace tin
